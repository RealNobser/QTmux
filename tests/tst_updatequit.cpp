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
// Zweiter Fall: Ohne umgestellte Quelle bricht der Selbsttest sofort ab, damit er
// nie eine echte Installation auslösen kann.
//
// Jeder Lauf hat ein eigenes Profil (QTMUX_PROFILE) und räumt es am Ende ab;
// Download und Ergebnisdatei liegen im Temp-Ordner des Tests, nie in „Downloads“.

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QProcessEnvironment>
#include <QRegularExpression>
#include <QSettings>
#include <QStandardPaths>
#include <QTcpServer>
#include <QTemporaryDir>
#include <QTest>
#include <QUrl>

namespace {

struct Run {
    bool finished = false;
    QProcess::ExitStatus status = QProcess::NormalExit;
    int code = -1;
    QString out;
};

// Räumt die Einstellungen und Datenordner eines Testprofils ab. Gelöscht wird nur,
// was den eindeutigen Profilnamen im Pfad trägt.
void removeProfile(const QString &profile)
{
    const QString app = QStringLiteral("QTmux-%1").arg(profile);
    // Erst den Dateinamen holen, das QSettings-Objekt abbauen und DANN löschen: Ein
    // lebendes Objekt (oder clear()) schriebe die Datei beim Abbau leer wieder hin
    // (unter macOS über cfprefsd, gemessen: 44 leere Profile nach einigen Läufen).
    const QString file = QSettings(QStringLiteral("QTmux"), app).fileName();
    if (file.contains(app)) QFile::remove(file);
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

Run runSelfTest(const QTemporaryDir &work, const QStringList &extraArgs, int timeoutMs)
{
    static int counter = 0;
    const QString profile = QStringLiteral("update-selftest-%1-%2")
                                .arg(QCoreApplication::applicationPid())
                                .arg(++counter);

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
                QStringList{QStringLiteral("--selftest-update"), work.path()} + extraArgs);
    Run r;
    if (!child.waitForStarted(20000)) {
        r.out = child.errorString();
        removeProfile(profile);
        return r;
    }
    r.finished = child.waitForFinished(timeoutMs);
    if (!r.finished) {
        child.kill();
        child.waitForFinished(5000);
    }
    r.status = child.exitStatus();
    r.code = child.exitCode();
    r.out = QString::fromLocal8Bit(child.readAll());
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
        const Run r = runSelfTest(work, {QStringLiteral("--update-base-url"), fixture}, 120000);
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
        // Download liegt im Ordner des Tests, nicht in „Downloads“.
        QVERIFY2(!QDir(work.filePath(QStringLiteral("downloads")))
                      .isEmpty(QDir::Files | QDir::NoDotAndDotDot),
                 qPrintable(out));
    }

    void thePictureModeWritesAnImageOfTheScene()
    {
        // Bildmodus (UpdateSelfTest.h): Der Ablauf erreicht die Szene, dann entsteht
        // das Bild; eine unbekannte Szene endet mit Exit 2, ohne Bild.
        QTemporaryDir work;
        QVERIFY(work.isValid());
        const QString fixture =
            QUrl::fromLocalFile(QStringLiteral(QTMUX_UPDATE_FIXTURE_DIR "/good")).toString();
        const QString png = work.filePath(QStringLiteral("scene-3.png"));
        const Run r = runSelfTest(work,
                                  {QStringLiteral("--update-base-url"), fixture,
                                   QStringLiteral("--update-picture"), QStringLiteral("3"),
                                   QStringLiteral("--screenshot"), png},
                                  90000);
        QVERIFY2(r.finished && r.code == 0, qPrintable(r.out));
        QVERIFY2(r.out.contains(QLatin1String("QTmux update-picture: state 3")), qPrintable(r.out));
        QVERIFY2(!r.out.contains(QLatin1String("qrc:/qt/qml/QTmux/")), qPrintable(r.out));
        QVERIFY2(QFileInfo(png).size() > 10000, qPrintable(r.out));

        const QString none = work.filePath(QStringLiteral("none.png"));
        const Run bad = runSelfTest(work,
                                    {QStringLiteral("--update-base-url"), fixture,
                                     QStringLiteral("--update-picture"), QStringLiteral("no-such"),
                                     QStringLiteral("--screenshot"), none},
                                    60000);
        QVERIFY2(bad.finished, qPrintable(bad.out));
        QCOMPARE(bad.code, 2);
        QVERIFY(!QFileInfo::exists(none));
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
};

QTEST_GUILESS_MAIN(TestUpdateQuit)
#include "tst_updatequit.moc"
