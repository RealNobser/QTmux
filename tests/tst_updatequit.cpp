// „Beenden und aktualisieren“ am ECHTEN Programm (K2, Vertrag V2 §4.3 und §8).
//
// Lehre aus der Familie: In einer Schwester-App beendete „Beenden und aktualisieren“
// die App nie, weil ein Fenster sein Schließen ablehnte, und kein Test fuhr den
// echten Weg. Dieser Test fährt ihn: Er startet qtmux als Kindprozess mit eigener
// Ereignisschleife, dem echten Main.qml und dessen `onClosing`.
//
//   qtmux --selftest-update <ordner> --update-base-url <fixture>
//
// Im Selbsttest (src/app/UpdateSelfTest.cpp) prüft QTmux wie per Menü, lädt die
// Fixture-Version 9.9.9 und drückt „Beenden und aktualisieren“. Die Quelle ist
// umgestellt, darum ist die Installation ein Trockenlauf: Der Ablauf schreibt in
// aboutToQuit „dry run (update source overridden), would start: …“, statt etwas
// zu starten. Gefordert und einzeln geprüft ist:
//   • Main.qml lädt ohne QML-Fehler (sonst gäbe es kein `onClosing` zu prüfen).
//   • Die Beenden-Rückfrage war eingeschaltet (`confirmQuit=1`, der Selbsttest
//     setzt sie in seinem Profil) und mindestens eine Sitzung lief — trotzdem
//     beendet sich QTmux von selbst mit Exit 0. Die Rückfrage entfällt NUR auf dem
//     Update-Weg (UpdateQuit.cpp).
//   • Der Ablauf war im Zustand „Bereit“ (5), und die Installation kam erst in
//     aboutToQuit (die Trockenlauf-Zeile), nicht vorher.
//   • Die 15-s-Frist des Ablaufs („app did not quit within …“) griff nicht.
//   • Das Fixture `good` ist mit dem PRODUKTIONSSCHLÜSSEL signiert: Ein
//     Schlüsselwechsel macht diesen Test rot (gewollter Alarm, wie bisher).
// Dieser Fall fährt Stufe 1 (`--selftest-system-installer`): Ein Entwicklerbau
// bekäme vom Selbst-Ersetzen nur „von Hand“. Weitere Fälle: Mit der Wahl des Hubs
// fehlt der eingebettete Helfer nicht (`theRealInstallWay…`); nach einem Update auf
// diese Version schreibt QTmux die Start-Bestätigung erst nach dem Hauptfenster
// (`theNewVersionConfirms…`); ohne umgestellte Quelle bricht der Selbsttest sofort
// ab, damit er nie eine echte Installation auslösen kann. Bilder macht nur das
// Bildziel qtmux-bilder (tst_updatepictures).
//
// Jeder Lauf hat ein eigenes Profil (QTMUX_PROFILE) und räumt es am Ende ab;
// Download und Ergebnisdatei liegen im Temp-Ordner des Tests, nie in „Downloads“.

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QProcessEnvironment>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QTcpServer>
#include <QTemporaryDir>
#include <QTest>
#include <QUrl>

#include "update/InstallBackend.hpp"
#include "update/UpdateSettings.hpp"

#include <QSettings>

#include <functional>

namespace {

struct Run {
    bool finished = false;
    QProcess::ExitStatus status = QProcess::NormalExit;
    int code = -1;
    qint64 pid = 0;
    QString out;
};

// Räumt die Einstellungen und Datenordner eines Testprofils ab. Gelöscht wird nur,
// was den eindeutigen Profilnamen im Pfad trägt.
void removeProfile(const QString &profile)
{
    const QString app = QStringLiteral("QTmux-%1").arg(profile);
    // Die Einstellungen selbst legt QTmux im Selbsttest als INI-Datei in den
    // Selbsttest-Ordner (main.cpp); sie gehen mit dem QTemporaryDir. Eine native
    // Profil-Domain hier zu löschen trug nicht: cfprefsd schrieb sie verzögert neu.
    const QString before = QCoreApplication::applicationName();
    const QString beforeOrg = QCoreApplication::organizationName();
    QCoreApplication::setOrganizationName(QStringLiteral("QTmux"));
    QCoreApplication::setApplicationName(app);
    for (auto loc : {QStandardPaths::AppDataLocation, QStandardPaths::AppLocalDataLocation,
                     QStandardPaths::CacheLocation, QStandardPaths::AppConfigLocation}) {
        const QString dir = QStandardPaths::writableLocation(loc);
        if (dir.contains(app)) QDir(dir).removeRecursively();
    }
    QCoreApplication::setApplicationName(before);
    QCoreApplication::setOrganizationName(beforeOrg);
}

// Die Einstellungsdatei, die QTmux im Selbsttest liest (main.cpp: INI unter
// <ordner>/settings, Organisation „QTmux“, Anwendung „QTmux-<profil>“).
QString selfTestSettingsFile(const QTemporaryDir &work, const QString &profile)
{
    return work.filePath(QStringLiteral("settings/QTmux/QTmux-%1.ini").arg(profile));
}

// Die Datenordner, die QTmux für ein Profil im Home anlegen würde (AppData, Cache,
// Konfiguration), und unter macOS die native Einstellungsdatei der Profil-Domain.
QStringList userPlacesOfProfile(const QString &profile)
{
    const QString app = QStringLiteral("QTmux-%1").arg(profile);
    const QString before = QCoreApplication::applicationName();
    const QString beforeOrg = QCoreApplication::organizationName();
    QCoreApplication::setOrganizationName(QStringLiteral("QTmux"));
    QCoreApplication::setApplicationName(app);
    QStringList places;
    for (auto loc : {QStandardPaths::AppDataLocation, QStandardPaths::AppLocalDataLocation,
                     QStandardPaths::CacheLocation, QStandardPaths::AppConfigLocation}) {
        const QString dir = QStandardPaths::writableLocation(loc);
        if (dir.contains(app)) places << dir;
    }
    QCoreApplication::setApplicationName(before);
    QCoreApplication::setOrganizationName(beforeOrg);
#if defined(Q_OS_MACOS)
    places << QDir::homePath() + QStringLiteral("/Library/Preferences/com.qtmux.%1.plist").arg(app);
#endif
    places.removeDuplicates();
    return places;
}

// `beforeStart(profil)` läuft, bevor der Kindprozess startet (Einstellungen vorbelegen),
// `afterRun(profil)` nach seinem Ende und VOR dem Abräumen des Profils.
Run runSelfTest(const QTemporaryDir &work, const QStringList &extraArgs, int timeoutMs,
                const std::function<void(const QString &profile)> &beforeStart = {},
                bool withSelfTestArgument = true,
                const std::function<void(const QString &profile)> &afterRun = {})
{
    static int counter = 0;
    const QString profile = QStringLiteral("update-selftest-%1-%2")
                                .arg(QCoreApplication::applicationPid())
                                .arg(++counter);
    if (beforeStart) beforeStart(profile);

    // Freier MCP-Port, damit der Kindprozess keiner laufenden Instanz in die
    // Quere kommt (die Vorgabe 7345 kann belegt sein).
    QTcpServer probe;
    probe.listen(QHostAddress::LocalHost, 0);
    const quint16 port = probe.serverPort();
    probe.close();

    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert(QStringLiteral("QT_QPA_PLATFORM"), QStringLiteral("offscreen"));
    env.insert(QStringLiteral("QTMUX_NO_GPU"), QStringLiteral("1"));
    env.insert(QStringLiteral("QTMUX_MCP_PORT"), QString::number(port));
    env.insert(QStringLiteral("QTMUX_PROFILE"), profile);
    env.insert(QStringLiteral("QT_LOGGING_RULES"), QStringLiteral("appupdate.*.info=true"));
    // Unter Windows ist qtmux.exe ein GUI-Programm ohne Konsole; Qt schriebe die
    // Meldungen dann nur an den Debugger. So kommen sie über die Pipe an.
    env.insert(QStringLiteral("QT_ASSUME_STDERR_HAS_CONSOLE"), QStringLiteral("1"));
    env.insert(QStringLiteral("QT_FORCE_STDERR_LOGGING"), QStringLiteral("1"));
#if defined(Q_OS_LINUX)
    // Unter Linux tauscht der Ablauf nur ein AppImage; ohne $APPIMAGE hiesse es
    // „von Hand“, und „Bereit“ käme nie. Ein beschreibbarer Ort genügt — die
    // Installation ist ohnehin ein Trockenlauf.
    env.insert(QStringLiteral("APPIMAGE"), work.filePath(QStringLiteral("QTmux.AppImage")));
#endif

    QProcess child;
    child.setProcessEnvironment(env);
    child.setProcessChannelMode(QProcess::MergedChannels);
    child.start(QStringLiteral(QTMUX_APP_BINARY),
                (withSelfTestArgument
                     ? QStringList{QStringLiteral("--selftest-update"), work.path()}
                     : QStringList{}) + extraArgs);
    Run r;
    if (!child.waitForStarted(20000)) {
        r.out = child.errorString();
        removeProfile(profile);
        return r;
    }
    r.pid = child.processId();
    r.finished = child.waitForFinished(timeoutMs);
    if (!r.finished) {
        child.kill();
        child.waitForFinished(5000);
    }
    r.status = child.exitStatus();
    r.code = child.exitCode();
    r.out = QString::fromLocal8Bit(child.readAll());
    if (afterRun) afterRun(profile);
    removeProfile(profile);
    return r;
}

} // namespace

class TestUpdateQuit : public QObject {
    Q_OBJECT

private slots:
    void quitAndUpdateEndsTheRealApp()
    {
        QTemporaryDir work;
        QVERIFY(work.isValid());
        const QString fixture =
            QUrl::fromLocalFile(QStringLiteral(QTMUX_UPDATE_FIXTURE_DIR "/good")).toString();
        // Stufe 1 (Systeminstaller): Ein Entwicklerbau bekäme vom Selbst-Ersetzen nur
        // „von Hand“ und nie den Knopf, um den es hier geht (UpdateBridge.h).
        const Run r = runSelfTest(work,
                                  {QStringLiteral("--update-base-url"), fixture,
                                   QStringLiteral("--selftest-system-installer")},
                                  120000);
        const QString &out = r.out;

        if (!r.finished) {
            // Zwei Ursachen auseinanderhalten: nie „Bereit“ (dann wurde nie
            // „Beenden und aktualisieren“ gedrückt) oder Bereit, aber kein Ende.
            const bool ready = out.contains(QLatin1String("QTmux selftest-update: state 5"));
            QFAIL(qPrintable(QStringLiteral("%1 Ausgabe:\n%2")
                                 .arg(ready ? QStringLiteral(
                                                  "QTmux hat sich nach „Beenden und aktualisieren“ "
                                                  "nicht beendet (hängt es an einer Rückfrage?).")
                                            : QStringLiteral(
                                                  "QTmux kam nie in den Zustand „Bereit“ (5) und "
                                                  "lief bis zur Frist des Tests."),
                                      out)));
        }
        QVERIFY2(r.status == QProcess::NormalExit, qPrintable(out));
        // Das echte Main.qml muss ohne QML-Fehler geladen sein; sonst fährt der Test
        // einen Weg ohne Fenster und belegt nichts über `onClosing`. (So gefunden:
        // eine Eigenschaft, die eine FINAL-Eigenschaft von Button überschrieb, ließ
        // Main.qml nicht laden.)
        static const QRegularExpression qmlError(QStringLiteral(
            "failed to load component|Type \\w+ unavailable|Cannot override FINAL|"
            "ReferenceError|TypeError|is not defined|qrc:/qt/qml/QTmux/\\S+: (?!QML )"));
        const auto qmlHit = qmlError.match(out);
        QVERIFY2(!qmlHit.hasMatch(),
                 qPrintable(QStringLiteral("QML-Fehler „%1“:\n%2").arg(qmlHit.captured(0), out)));
        QVERIFY2(r.code == 0, qPrintable(out));
        QVERIFY2(out.contains(QLatin1String("QTmux selftest-update: state 5")),
                 qPrintable(QStringLiteral("Zustand „Bereit“ nie erreicht:\n%1").arg(out)));
        // Die Rückfrage war an, und es lief mindestens eine Sitzung: Ohne beides
        // bewiese das Beenden nichts über die Umgehung der Rückfrage.
        QVERIFY2(out.contains(QLatin1String("Beenden fuer das Update (confirmQuit=1)")),
                 qPrintable(QStringLiteral("Beenden-Rückfrage war nicht eingeschaltet:\n%1").arg(out)));
        static const QRegularExpression sessions(
            QStringLiteral("quit-and-install with (\\d+) running session"));
        const auto sm = sessions.match(out);
        QVERIFY2(sm.hasMatch() && sm.captured(1).toInt() >= 1,
                 qPrintable(QStringLiteral("Keine laufende Sitzung beim Beenden:\n%1").arg(out)));
        QVERIFY2(out.contains(QLatin1String("dry run (update source overridden), would start:")),
                 qPrintable(QStringLiteral("Keine Installation in aboutToQuit:\n%1").arg(out)));
        QVERIFY2(!out.contains(QLatin1String("app did not quit within")),
                 qPrintable(QStringLiteral("Frist des Ablaufs gegriffen:\n%1").arg(out)));
        // Der Ablauf wird abgebaut, solange die App noch lebt (Vorgabe des Hubs), nicht
        // erst in der statischen Zerstörung nach main.
        QVERIFY2(out.contains(QLatin1String("update bridge destroyed (app alive: 1)")),
                 qPrintable(QStringLiteral("Bridge nicht bei lebender App abgebaut:\n%1").arg(out)));
        // Download liegt im Ordner des Tests, nicht in „Downloads“.
        QVERIFY2(!QDir(work.filePath(QStringLiteral("downloads")))
                      .isEmpty(QDir::Files | QDir::NoDotAndDotDot),
                 qPrintable(out));
    }

    void theRealInstallWayFindsItsHelperInTheBuiltApp()
    {
        // Ohne Schalter wählt der Hub den Installationsweg (makeInstallBackendForThisSystem):
        // macOS das Selbst-Ersetzen, Windows den Helfer um msiexec, Linux das AppImage.
        // Ein Entwicklerbau erfüllt dort nicht jede Vorbedingung (nicht familien-signiert,
        // nicht per MSI installiert), dann heißt es „von Hand“ (Exit 3), sonst „Bereit“
        // und Beenden (Exit 0). Der Ablauf nennt dabei ALLE nicht erfüllten
        // Vorbedingungen. Gefordert: Der Helfer, den appupdate_embed_install_helpers()
        // ins gebaute Programm legt, fehlt nicht.
        QTemporaryDir work;
        QVERIFY(work.isValid());
        const QString fixture =
            QUrl::fromLocalFile(QStringLiteral(QTMUX_UPDATE_FIXTURE_DIR "/good")).toString();
        const Run r = runSelfTest(work, {QStringLiteral("--update-base-url"), fixture}, 120000);
        QVERIFY2(r.finished && r.status == QProcess::NormalExit, qPrintable(r.out));
        QVERIFY2(r.code == 0 || r.code == 3, qPrintable(r.out));
        // Positivprobe: Die Fähigkeit wurde wirklich gemessen und geloggt.
        static const QRegularExpression verdict(
            QStringLiteral("downloaded and verified .* install: (\\S+)(.*)"));
        const auto v = verdict.match(r.out);
        QVERIFY2(v.hasMatch(), qPrintable(r.out));
        qInfo("Installationsweg dieses Baus: %s%s", qPrintable(v.captured(1)),
              qPrintable(v.captured(2)));
        QCOMPARE(r.code == 3, v.captured(1) == QLatin1String("manual"));
#if defined(Q_OS_MACOS) || defined(Q_OS_WIN)
        // Dort wählt der Hub das Selbst-Ersetzen („automatic“ oder „manual“), nie den
        // Systeminstaller von Stufe 1. Stünde die Wahl des Hubs in der Bridge unter einer
        // Bedingung, die den Selbsttest ausnimmt, fiele der Ablauf auf die Vorgabe des
        // Kerns zurück, und hier stünde „system-installer“.
        QVERIFY2(v.captured(1) != QLatin1String("system-installer"), qPrintable(r.out));
#else
        // Linux: Der Hub wählt Stufe 1 (kLinuxSelfInstallMeasured ist aus). Das AppImage
        // wird an seinem Ort ersetzt; dafür muss nur sein Ordner beschreibbar sein, die
        // Datei selbst nicht existieren (InstallBackend.cpp, Zweig „appimage“).
        QCOMPARE(v.captured(1), QStringLiteral("system-installer"));
        QVERIFY2(r.out.contains(QLatin1String("replaces the AppImage, runs on the next start")),
                 qPrintable(r.out));
#endif
        QVERIFY2(!r.out.contains(QLatin1String("carries no swap program")), qPrintable(r.out));
        QVERIFY2(!r.out.contains(QLatin1String("update-helper.exe is missing")), qPrintable(r.out));
        // Die beiden Negativprüfungen hängen am Wortlaut des Hubs. Steht er dort nicht
        // mehr, wären sie still zahnlos: darum hier gegen den vendierten Quelltext.
        QFile src(QStringLiteral(QTMUX_UPDATER_SOURCE_DIR "/SelfInstall.cpp"));
        QVERIFY(src.open(QIODevice::ReadOnly));
        const QByteArray code = src.readAll();
        QVERIFY(code.contains("carries no swap program (Contents/Helpers/update-swap)"));
        QVERIFY(code.contains("%1 is missing next to %2 or is not a plain file"));
        QFile hdr(QStringLiteral(QTMUX_UPDATER_SOURCE_DIR "/SelfInstall.hpp"));
        QVERIFY(hdr.open(QIODevice::ReadOnly));
        QVERIFY(hdr.readAll().contains("kWindowsHelperFileName[] = \"update-helper.exe\""));
    }

    void theNewVersionConfirmsItsStartAfterTheMainWindow()
    {
        // Start-Bestätigung (Naht Revision 3): Nach einem Selbst-Update wartet der
        // Helfer auf sie; ohne sie nimmt er den Rückweg. QTmux nutzt die Vorgabe des
        // Hubs: Der Ablauf bestätigt beim ersten Lauf der Ereignisschleife nach
        // start(), und start() läuft erst, wenn Main.qml mit Fenster und Sitzungen
        // steht. Vorbelegt wird der Merker eines Updates AUF genau diese Version.
        QTemporaryDir work;
        QVERIFY(work.isValid());
        const QString fixture =
            QUrl::fromLocalFile(QStringLiteral(QTMUX_UPDATE_FIXTURE_DIR "/good")).toString();
        const QString confirm = work.filePath(QStringLiteral("start-confirmed"));
        const QString attempt = QStringLiteral("start-confirmation-probe");
        QString settingsFile;
        const Run r = runSelfTest(
            work,
            {QStringLiteral("--update-base-url"), fixture,
             QStringLiteral("--selftest-system-installer")},
            120000, [&](const QString &profile) {
                settingsFile = selfTestSettingsFile(work, profile);
                QSettings s(settingsFile, QSettings::IniFormat);
                appupdate::PendingInstall p;
                p.fromVersion = QStringLiteral("1.9.14");
                p.toVersion = QStringLiteral(QTMUX_VERSION_STRING);
                p.method = QStringLiteral("automatic");
                p.kind = QStringLiteral("dmg");
                p.confirmFile = confirm;
                p.attempt = attempt;
                p.startedUtc = QDateTime::currentDateTimeUtc();
                appupdate::UpdateSettings(s).setPendingInstall(p);
                s.sync();
            });
        QVERIFY2(r.finished && r.code == 0, qPrintable(r.out));

        QFile f(confirm);
        QVERIFY2(f.open(QIODevice::ReadOnly), qPrintable(r.out));
        const auto c = appupdate::parseStartConfirmation(f.readAll());
        QVERIFY2(c.has_value(), qPrintable(r.out));
        QCOMPARE(c->version, QStringLiteral(QTMUX_VERSION_STRING));
        QCOMPARE(c->attempt, attempt);
        QCOMPARE(c->pid, r.pid);

        // Reihenfolge: erst das Hauptfenster, dann die Auswertung des Merkers.
        const qsizetype window = r.out.indexOf(QLatin1String("QTmux selftest-update: main window loaded"));
        const qsizetype judged = r.out.indexOf(
            QStringLiteral("updated from 1.9.14 to %1").arg(QStringLiteral(QTMUX_VERSION_STRING)));
        QVERIFY2(window >= 0 && judged > window, qPrintable(r.out));
        QVERIFY2(!r.out.contains(QLatin1String("start confirmation not written")), qPrintable(r.out));

        // Der Merker ist mit der Bestätigung gegangen.
        QSettings after(settingsFile, QSettings::IniFormat);
        QVERIFY2(!appupdate::UpdateSettings(after).pendingInstall().isValid(), qPrintable(r.out));
    }

    void aSelfTestWithoutAnOverriddenSourceStopsAtOnce()
    {
        // Ohne --update-base-url/--update-channel wäre jede Installation echt. Der
        // Selbsttest bricht deshalb mit Exit 2 ab, bevor er etwas prüft.
        // `--no-update-check` hält dabei auch die stille Prüfung vom Netz fern.
        QTemporaryDir work;
        QVERIFY(work.isValid());
        const Run r = runSelfTest(work, {QStringLiteral("--no-update-check")}, 60000);
        QVERIFY2(r.finished, qPrintable(r.out));
        QVERIFY2(r.status == QProcess::NormalExit, qPrintable(r.out));
        QCOMPARE(r.code, 2);
        QVERIFY2(r.out.contains(QLatin1String("--selftest-update braucht --update-base-url")),
                 qPrintable(r.out));
        QVERIFY2(!r.out.contains(QLatin1String("QTmux selftest-update: state 1")), qPrintable(r.out));
        QVERIFY2(QDir(work.filePath(QStringLiteral("downloads")))
                     .isEmpty(QDir::Files | QDir::NoDotAndDotDot),
                 qPrintable(r.out));
    }

    void aSelfTestLeavesNothingInTheUserDataFolder()
    {
        // Ein Selbsttest-Lauf schreibt Einstellungen (INI unter <ordner>/settings) und
        // den Verlauf der Sitzungen (unter <ordner>/data/history) in den Selbsttest-
        // Ordner. Im Home des Benutzers entsteht nichts: kein Datenordner des Profils,
        // unter macOS keine Profil-Plist. Positivprobe: Der Verlaufsordner entstand
        // wirklich im Selbsttest-Ordner (das Sichern beim Beenden legt ihn immer an;
        // eine Datei darin nur, wenn die Shell schon etwas gezeichnet hatte — das
        // wackelte, gemessen 3 von 6), sonst bewiese das Fehlen nichts.
        QTemporaryDir work;
        QVERIFY(work.isValid());
        const QString fixture =
            QUrl::fromLocalFile(QStringLiteral(QTMUX_UPDATE_FIXTURE_DIR "/good")).toString();
        QStringList leftovers;
        QStringList checked;
        // Die Bundle-Domain com.qtmux.app teilt sich jeder qtmux-Bau mit der installierten
        // App; sie wird hier nur GELESEN (Inhalt und Änderungszeit vor/nach dem Lauf).
        const QString bundlePlist =
            QDir::homePath() + QStringLiteral("/Library/Preferences/com.qtmux.app.plist");
        QByteArray plistBefore;
        QDateTime plistTimeBefore;
        const Run r = runSelfTest(
            work,
            {QStringLiteral("--update-base-url"), fixture,
             QStringLiteral("--selftest-system-installer")},
            120000,
            [&](const QString &) {
#if defined(Q_OS_MACOS)
                QFile f(bundlePlist);
                if (f.open(QIODevice::ReadOnly)) plistBefore = f.readAll();
                plistTimeBefore = QFileInfo(bundlePlist).lastModified();
#endif
            },
            true, [&](const QString &profile) {
                checked = userPlacesOfProfile(profile);
                for (const QString &p : checked)
                    if (QFileInfo::exists(p)) leftovers << p;
            });
        // Kein Schreiben in die Bundle-Domain: Die Zeile des Schreibpunkts in main.cpp
        // fehlt im Log (trägt auch, wenn die Datei schon denselben Wert hat), und die
        // Datei ist nach dem Lauf unverändert (cfprefsd schreibt verzögert: kurz warten).
        QVERIFY2(!r.out.contains(QLatin1String("AppleLanguages der Bundle-Domain gesetzt")),
                 qPrintable(r.out));
#if defined(Q_OS_MACOS)
        QTest::qWait(1500);
        QByteArray plistAfter;
        {
            QFile f(bundlePlist);
            if (f.open(QIODevice::ReadOnly)) plistAfter = f.readAll();
        }
        QVERIFY2(plistAfter == plistBefore, "com.qtmux.app.plist hat sich im Selbsttest geändert");
        QCOMPARE(QFileInfo(bundlePlist).lastModified(), plistTimeBefore);
#endif
        QVERIFY2(r.finished && r.code == 0, qPrintable(r.out));
        QVERIFY2(!checked.isEmpty(), "keine Orte im Home bestimmt — die Probe misst nichts");
        QVERIFY2(leftovers.isEmpty(),
                 qPrintable(QStringLiteral("Im Home entstanden: %1").arg(leftovers.join(", "))));
        QVERIFY2(QFileInfo(work.filePath(QStringLiteral("data/history"))).isDir(),
                 qPrintable(QStringLiteral("Kein Verlaufsordner unter <ordner>/data/history:\n%1")
                                .arg(r.out)));
    }

    void theInstallerSwitchIsRefusedOutsideTheSelfTest_data()
    {
        QTest::addColumn<bool>("withSelfTest");
        QTest::addColumn<QStringList>("args");
        QTest::newRow("allein") << false
                                << QStringList{QStringLiteral("--selftest-system-installer"),
                                               QStringLiteral("--no-update-check")};
        QTest::newRow("Selbsttest ohne Quelle")
            << true << QStringList{QStringLiteral("--selftest-system-installer"),
                                   QStringLiteral("--no-update-check")};
        QTest::newRow("Quelle ohne Selbsttest")
            << false << QStringList{QStringLiteral("--selftest-system-installer"),
                                    QStringLiteral("--update-base-url"),
                                    QStringLiteral("file:///nirgends"),
                                    QStringLiteral("--no-update-check")};
    }

    void theInstallerSwitchIsRefusedOutsideTheSelfTest()
    {
        // Der Schalter für Stufe 1 gilt nur zusammen mit dem Selbsttest UND einer
        // umgestellten Quelle (dann ist jede Installation ein Trockenlauf). Sonst
        // endet QTmux sofort mit Exit 6, bevor ein Fenster, eine Einstellung oder
        // ein Download entsteht. Ohne die Abweisung liefe hier die App los und der
        // Lauf endete erst an der Frist.
        QFETCH(bool, withSelfTest);
        QFETCH(QStringList, args);
        QTemporaryDir work;
        QVERIFY(work.isValid());
        const Run r = runSelfTest(work, args, 20000, {}, withSelfTest);
        QVERIFY2(r.finished, qPrintable(r.out));
        QVERIFY2(r.status == QProcess::NormalExit, qPrintable(r.out));
        QCOMPARE(r.code, 6);
#if !defined(Q_OS_WIN)
        QVERIFY2(r.out.contains(QLatin1String("--selftest-system-installer gilt nur zusammen")),
                 qPrintable(r.out));
#endif
        QVERIFY2(!r.out.contains(QLatin1String("main window loaded")), qPrintable(r.out));
        QVERIFY2(QDir(work.path()).isEmpty(QDir::AllEntries | QDir::NoDotAndDotDot),
                 qPrintable(r.out));
    }
};

QTEST_GUILESS_MAIN(TestUpdateQuit)
#include "tst_updatequit.moc"
