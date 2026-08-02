// QTMUX-125: die QTmux-Hälfte des Online-Updates — `UpdateViewModel` über dem
// vendierten Kern. Der Test läuft gegen einen committeten Fixture-Baum
// (tests/fixtures/update/), den der **Produktionsschlüssel des Owners** signiert
// hat; das ViewModel baut seinen Checker ohne Schlüssel-Parameter, prüft also mit
// den in `UpdateKeys.hpp` eingebauten Bytes.
//
// 🔑 Damit ist das hier zugleich der Interop-Nachweis der ausgelieferten App:
// `openssl pkeyutl -sign -rawin` erzeugt die Signatur, das vendierte Monocypher
// akzeptiert sie. Ein Schlüsselwechsel macht diesen Test rot — genau richtig,
// dann ist die vendierte `UpdateKeys.hpp` veraltet (s. fixtures/update/README.md).
//
// Hermetisch wie tst_settingsio: INI-Format in einem QTemporaryDir, eigene
// Organisations-/Anwendungsnamen — der Test fasst weder die echten Einstellungen
// noch das Netz an.

#include <QtTest>

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QSettings>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTimer>
#include <QUrl>

#include "UpdateViewModel.h"

using namespace qtmux;

namespace {

// Dreht den Event-Loop, bis `done` kippt oder die Wache zuschlägt.
void spinUntil(bool &done, int guardMs = 5000) {
    QEventLoop loop;
    QTimer guard;
    guard.setSingleShot(true);
    QObject::connect(&guard, &QTimer::timeout, &loop, &QEventLoop::quit);
    guard.start(guardMs);
    while (!done && guard.isActive())
        loop.processEvents(QEventLoop::AllEvents, 20);
}

} // namespace

class tst_updateviewmodel : public QObject {
    Q_OBJECT

private:
    QTemporaryDir m_settingsDir;
    QString m_fixtures;

    // Basis-URL eines Fixture-Baums. Der Kern haengt `/qtmux/manifest.json` an —
    // jeder Baum traegt darum ein eigenes `qtmux/`-Unterverzeichnis.
    QString baseFor(const QString &tree) const {
        return QUrl::fromLocalFile(m_fixtures + QLatin1Char('/') + tree).toString();
    }

    // Setzt die Tagesdrosselung zurueck. Noetig, weil AUCH ein Check von Hand den
    // Zeitstempel schreibt (er hat ja gerade geprueft) — ohne das kaeme der stille
    // Check in denselben Test nie zum Zug.
    static void clearThrottle() {
        QSettings().remove(QStringLiteral("update/lastCheck"));
    }

    // Führt einen Check aus und wartet auf `checkFinished`.
    bool runCheck(UpdateViewModel &vm, bool manual) {
        bool done = false;
        auto c = connect(&vm, &UpdateViewModel::checkFinished,
                         [&done](bool) { done = true; });
        if (manual) vm.checkNow(); else vm.checkOnStartup();
        spinUntil(done);
        disconnect(c);
        return done;
    }

private slots:
    void initTestCase() {
        QVERIFY(m_settingsDir.isValid());
        QStandardPaths::setTestModeEnabled(true);
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope,
                           m_settingsDir.path());
        QCoreApplication::setOrganizationName(QStringLiteral("QTmuxTest"));
        QCoreApplication::setApplicationName(QStringLiteral("tst_updateviewmodel"));

        m_fixtures = QStringLiteral(QTMUX_UPDATE_FIXTURE_DIR);
        QVERIFY2(QFile::exists(m_fixtures + QStringLiteral("/good/qtmux/manifest.json")),
                 qPrintable(QStringLiteral("Fixtures fehlen unter %1").arg(m_fixtures)));
    }

    void cleanup() {
        QSettings s;
        s.clear();
        s.sync();
    }

    // Ohne gesetzte Basis-URL zeigt das ViewModel auf den echten Webspace —
    // wer das ändert, bricht die Auslieferung, ohne dass es sonst auffiele.
    void defaultBaseUrlPointsAtTheProductionWebspace() {
        UpdateViewModel vm;
        QCOMPARE(vm.baseUrl(), QStringLiteral("https://nobser.de/updates"));
        // Der Kern hängt Produkt und Dateiname an: .../qtmux/manifest.json.
        QVERIFY(vm.autoCheck());   // Vorgabe AN (Owner-Entscheidung)
    }

    // Die volle Kette: echt signiertes Manifest -> Version neuer -> Download ->
    // SHA-256 stimmt -> Start-Plan steht.
    void realSignatureIsAcceptedAndArtifactDownloads() {
        UpdateViewModel vm;
        vm.setBaseUrl(baseFor(QStringLiteral("good")));
        QVERIFY(runCheck(vm, /*manual=*/true));

        QCOMPARE(vm.state(), int(UpdateViewModel::Available));
        QVERIFY(vm.updateAvailable());
        QVERIFY(!vm.isDowngrade());
        QCOMPARE(vm.remoteVersion(), QStringLiteral("9.9.9"));
        QVERIFY(vm.hasPackageForThisSystem());
        QVERIFY(vm.lastError().isEmpty());

        // Anmerkungen folgen der Sprache und fallen nie auf leer zurück.
        vm.setLanguage(QStringLiteral("de"));
        QVERIFY(vm.notes().contains(QStringLiteral("QTMUX-125")));
        vm.setLanguage(QStringLiteral("en"));
        QVERIFY(vm.notes().contains(QStringLiteral("Fixture release")));

        bool ready = false;
        auto c = connect(&vm, &UpdateViewModel::stateChanged, [&] {
            if (vm.state() == UpdateViewModel::Ready
                || vm.state() == UpdateViewModel::Failed) ready = true;
        });
        vm.download();
        spinUntil(ready);
        disconnect(c);
        QVERIFY2(vm.state() == UpdateViewModel::Ready, qPrintable(vm.lastError()));
        QVERIFY(QFile::exists(vm.downloadedPath()));
        QCOMPARE(vm.downloadProgress(), 1.0);
        // Der Start-Plan muss zur Paketart dieser Plattform passen.
        QVERIFY(!vm.launchPlanDescription().isEmpty());
        QFile::remove(vm.downloadedPath());
    }

    // Signatur über ANDERE Bytes: das Manifest ist syntaktisch einwandfrei, darf
    // den Parser aber nie erreichen. Bei einem Check von Hand muss der Fehler
    // SICHTBAR werden — hier hat der Anwender ja danach gefragt.
    void forgedSignatureFailsVisiblyOnAManualCheck() {
        UpdateViewModel vm;
        vm.setBaseUrl(baseFor(QStringLiteral("sigfail")));
        QSignalSpy found(&vm, &UpdateViewModel::updateFound);
        QVERIFY(runCheck(vm, /*manual=*/true));
        QCOMPARE(vm.state(), int(UpdateViewModel::Failed));
        QVERIFY(!vm.updateAvailable());
        QVERIFY(vm.remoteVersion().isEmpty());   // nichts geparst
        QCOMPARE(found.count(), 0);
        QVERIFY(vm.lastError().contains(QStringLiteral("signature"), Qt::CaseInsensitive));

        // Positivkontrolle mit demselben Aufbau: derselbe Code, korrekt signiert,
        // muss durchgehen — sonst „bestünde" der Test auch bei kaputtem Abruf.
        UpdateViewModel ok;
        ok.setBaseUrl(baseFor(QStringLiteral("good")));
        QVERIFY(runCheck(ok, true));
        QCOMPARE(ok.state(), int(UpdateViewModel::Available));
    }

    // Ein unerreichbarer Server beim START darf NICHTS zeigen — sonst begrüßt
    // ein Rechner ohne Netz den Anwender jeden Morgen mit einem Fehlerdialog.
    void startupCheckStaysSilentOnFailure() {
        UpdateViewModel vm;
        vm.setBaseUrl(QUrl::fromLocalFile(m_fixtures + QStringLiteral("/gibtesnicht"))
                          .toString());
        QSignalSpy found(&vm, &UpdateViewModel::updateFound);
        QVERIFY(runCheck(vm, /*manual=*/false));
        QCOMPARE(vm.state(), int(UpdateViewModel::Idle));
        QVERIFY(vm.lastError().isEmpty());
        QCOMPARE(found.count(), 0);
    }

    // Der Start-Check läuft höchstens einmal am Tag — und der Zeitstempel wird
    // auch nach einem Fehlschlag geschrieben, sonst würde bei unerreichbarem
    // Server bei JEDEM Start erneut gefragt.
    void startupCheckIsThrottledToOncePerDay() {
        UpdateViewModel vm;
        vm.setBaseUrl(baseFor(QStringLiteral("good")));
        QVERIFY(runCheck(vm, false));
        QVERIFY(vm.updateAvailable());
        const QString stamp = QSettings().value(QStringLiteral("update/lastCheck")).toString();
        QVERIFY(!stamp.isEmpty());

        // Zweiter Anlauf am selben Tag: die Drosselung greift, es passiert nichts.
        UpdateViewModel vm2;
        QSignalSpy finished(&vm2, &UpdateViewModel::checkFinished);
        vm2.checkOnStartup();
        QTest::qWait(200);
        QCOMPARE(finished.count(), 0);
        QCOMPARE(vm2.state(), int(UpdateViewModel::Idle));

        // Gestern geprüft -> wieder fällig.
        QSettings().setValue(QStringLiteral("update/lastCheck"),
                             QDateTime::currentDateTime().addDays(-1).toString(Qt::ISODate));
        UpdateViewModel vm3;
        QVERIFY(runCheck(vm3, false));
        QVERIFY(vm3.updateAvailable());
    }

    // Abgeschaltet heißt abgeschaltet — auch wenn die Drosselung fällig wäre.
    void startupCheckObeysTheOffSwitch() {
        UpdateViewModel vm;
        vm.setBaseUrl(baseFor(QStringLiteral("good")));
        vm.setAutoCheck(false);
        QSignalSpy finished(&vm, &UpdateViewModel::checkFinished);
        vm.checkOnStartup();
        QTest::qWait(200);
        QCOMPARE(finished.count(), 0);
        // Von Hand geht es weiterhin — der Schalter betrifft nur den Start.
        QVERIFY(runCheck(vm, true));
        QVERIFY(vm.updateAvailable());
    }

    // „Version überspringen" bindet NUR den stillen Check; der Menüpunkt zeigt
    // sie weiter, sonst wäre ein Fehlklick unwiderruflich.
    void skippedVersionSilencesOnlyTheStartupCheck() {
        UpdateViewModel vm;
        vm.setBaseUrl(baseFor(QStringLiteral("good")));
        QVERIFY(runCheck(vm, true));
        QVERIFY(vm.updateAvailable());
        vm.skipVersion();
        QVERIFY(!vm.updateAvailable());
        QCOMPARE(QSettings().value(QStringLiteral("update/skippedVersion")).toString(),
                 QStringLiteral("9.9.9"));

        clearThrottle();
        UpdateViewModel silent;
        silent.setBaseUrl(baseFor(QStringLiteral("good")));
        QSignalSpy found(&silent, &UpdateViewModel::updateFound);
        QVERIFY(runCheck(silent, /*manual=*/false));
        QCOMPARE(silent.state(), int(UpdateViewModel::UpToDate));
        QCOMPARE(found.count(), 0);

        UpdateViewModel manual;
        manual.setBaseUrl(baseFor(QStringLiteral("good")));
        QVERIFY(runCheck(manual, /*manual=*/true));
        QVERIFY(manual.updateAvailable());
    }

    // Downgrade: erlaubt (Owner-Entscheidung), aber nur auf ausdrückliche
    // Anforderung und mit gesetzter Warnflagge.
    void downgradeIsOfferedOnlyOnAManualCheck() {
        UpdateViewModel manual;
        manual.setBaseUrl(baseFor(QStringLiteral("old")));
        QVERIFY(runCheck(manual, /*manual=*/true));
        QCOMPARE(manual.remoteVersion(), QStringLiteral("0.0.1"));
        QVERIFY(manual.isDowngrade());
        QVERIFY(manual.updateAvailable());          // installierbar …
        QCOMPARE(manual.state(), int(UpdateViewModel::Available));

        // … aber der stille Start-Check schweigt dazu: sonst fragte er jeden
        // Entwickler-Build taeglich, ob er sich zurueckstufen will.
        clearThrottle();
        UpdateViewModel silent;
        silent.setBaseUrl(baseFor(QStringLiteral("old")));
        QSignalSpy found(&silent, &UpdateViewModel::updateFound);
        QVERIFY(runCheck(silent, /*manual=*/false));
        QVERIFY(!silent.isDowngrade());
        QVERIFY(!silent.updateAvailable());
        QCOMPARE(silent.state(), int(UpdateViewModel::UpToDate));
        QCOMPARE(found.count(), 0);
    }

    // Der Download landet in einem eigenen Unterordner, nicht lose in Downloads.
    void downloadDirIsItsOwnFolder() {
        UpdateViewModel vm;
        QVERIFY(vm.downloadDir().endsWith(QStringLiteral("/QTmux-Updates")));
    }
};

QTEST_MAIN(tst_updateviewmodel)
#include "tst_updateviewmodel.moc"
