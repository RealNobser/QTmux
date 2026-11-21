// QTmux' Verdrahtung des gemeinsamen Update-Ablaufs (K2, Vertrag V2 §4.6 und §5).
//
// Den Ablauf selbst (Zustände, Texte, Fristen, Schlüssel) belegt der Hub mit eigenen
// Suiten. Hier steht nur, was QTmux dazugibt und was nur in QTmux schiefgehen kann:
//   • Die alten QTmux-Schlüssel werden beim ersten Start übernommen, und ein
//     bewusst ABGESCHALTETER Schalter bleibt aus (Vertrag §5, Mutationsprobe §8).
//   • Die Update-Quelle `update/baseUrl` verschwindet (E2).
//   • Die Seite „Updates“ schreibt die Familien-Schlüssel sofort; ein Port außerhalb
//     0…65535 wird nicht gespeichert.
//   • Der UpgradeCode im Code ist derselbe wie in installer/QTmux.wxs.
//   • Eine Aktion, die im Zustand nicht erlaubt ist, tut nichts.
//   • Ein für die Sitzung behaltenes Proxy-Passwort landet nie in der
//     Einstellungsdatei (Mutationsprobe K2: schreibt die Brücke es hinein, fällt
//     der Fall).
// Kein Fall geht ins Netz: Jeder Start läuft mit --no-update-check; der Proxy-Fall
// spricht nur mit einem Schein-Proxy auf 127.0.0.1.

#include <QCoreApplication>
#include <QFile>
#include <QRegularExpression>
#include <QSettings>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QTest>
#include <QUrl>

#include "UpdateBridge.h"
#include "update/InstallBackend.hpp"
#include "update/UpdateSettings.hpp"

using Flow = appupdate::UpdateFlow;

namespace {

// Installationsweg, der die Vorbereitung festhält: Der Test entscheidet, wann sie fertig
// ist, und kann dazwischen eine Sitzung anlegen (zwischen Klick und Scharfschalten).
class HeldBackend : public appupdate::InstallBackend {
public:
    PrepareDone held;
    appupdate::InstallOrder order;
    int launches = 0;
    int discards = 0;

    appupdate::InstallCapability capability(const appupdate::InstallContext &) const override
    {
        appupdate::InstallCapability c;
        c.method = appupdate::InstallMethod::SystemInstaller;
        c.reason = appupdate::ManualReason::None;
        return c;
    }
    void prepare(const appupdate::InstallOrder &o, PrepareDone done) override
    {
        order = o;
        held = std::move(done);
    }
    void cancelPrepare() override { held = nullptr; }
    appupdate::LaunchResult launch(const appupdate::PreparedInstall &) override
    {
        ++launches;
        return {true, {}};
    }
    void discard(const appupdate::PreparedInstall &) override { ++discards; }

    void finishPrepare()
    {
        appupdate::PreparedInstall p;
        p.order = order;
        p.method = appupdate::InstallMethod::SystemInstaller;
        p.stagedFile = order.verifiedFile;
        p.description = QStringLiteral("test");
        appupdate::PrepareResult r;
        r.prepared = p;
        auto done = std::move(held);
        held = nullptr;
        if (done) done(std::move(r));
    }
};

} // namespace

class TestUpdateBridge : public QObject {
    Q_OBJECT

    QTemporaryDir m_settingsDir;

    static qtmux::UpdateBridge::Wiring wiring()
    {
        qtmux::UpdateBridge::Wiring w;
        w.language = QStringLiteral("en");
        w.requestQuit = [] { return false; };
        return w;
    }
    static QStringList noNetwork() { return {QStringLiteral("qtmux"), QStringLiteral("--no-update-check")}; }

private slots:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        // Einstellungen als INI-Datei in einem eigenen Ordner: Die Datei steht nach
        // sync() wirklich auf der Platte (unter macOS schreibt das native Format
        // verzögert über cfprefsd, eine gelesene Datei bewiese dann nichts).
        QVERIFY(m_settingsDir.isValid());
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, m_settingsDir.path());
        QCoreApplication::setOrganizationName(QStringLiteral("QTmuxTest"));
        QCoreApplication::setApplicationName(QStringLiteral("QTmux-updatebridge-test"));
    }
    void init() { QSettings().clear(); }
    void cleanupTestCase() { QSettings().clear(); }

    void oldQtmuxKeysAreTakenOverOnce()
    {
        {
            QSettings s;
            s.setValue(QStringLiteral("update/skippedVersion"), QStringLiteral("1.9.20"));
            s.setValue(QStringLiteral("update/proxyMode"), 2);
            s.setValue(QStringLiteral("update/proxyType"), 1);
            s.setValue(QStringLiteral("update/proxyHost"), QStringLiteral("proxy.example"));
            s.setValue(QStringLiteral("update/proxyPort"), 3128);
            s.setValue(QStringLiteral("update/proxyUser"), QStringLiteral("anna"));
            s.setValue(QStringLiteral("update/baseUrl"), QStringLiteral("file:///anywhere"));
        }
        qtmux::UpdateBridge bridge(wiring());
        bridge.start(noNetwork());
        QCOMPARE(bridge.skippedVersion(), QStringLiteral("1.9.20"));
        QCOMPARE(bridge.proxyMode(), 2);
        QCOMPARE(bridge.proxyType(), 1);
        QCOMPARE(bridge.proxyHost(), QStringLiteral("proxy.example"));
        QCOMPARE(bridge.proxyPort(), 3128);
        QCOMPARE(bridge.proxyUser(), QStringLiteral("anna"));
        QSettings s;
        for (const char *gone : {"update/skippedVersion", "update/proxyMode", "update/proxyType",
                                 "update/proxyHost", "update/proxyPort", "update/proxyUser",
                                 "update/baseUrl"})
            QVERIFY2(!s.contains(QLatin1String(gone)), gone);
        QCOMPARE(s.value(QLatin1String(appupdate::settingskey::kProxyMode)).toString(),
                 QStringLiteral("manual"));
        // E2: Die alte, umgestellte Quelle wirkt nicht — der Ablauf fragt die feste
        // Produktionsquelle im Familien-Kanal (bisher tst_updateviewmodel /
        // tst_updatehardening `theKeyAndChannelAreTheFamilyContract`).
        // Gegen die Hub-Konstante: Belegt ist, dass die alte Quelle nicht wirkt, nicht
        // die Adresse selbst (die gehört dem Hub).
        QCOMPARE(bridge.flow().baseUrl(), QUrl(QString::fromLatin1(appupdate::kProductionBaseUrl)));
        QCOMPARE(bridge.flow().channel(), QStringLiteral("stable"));

        // „Einmal“: Ein zweiter Start findet keine alten Schlüssel mehr und ändert
        // nichts; eine inzwischen geänderte Einstellung bleibt.
        bridge.setProxyPort(8080);
        qtmux::UpdateBridge second(wiring());
        second.start(noNetwork());
        QCOMPARE(second.proxyPort(), 8080);
        QCOMPARE(second.skippedVersion(), QStringLiteral("1.9.20"));
        for (const char *gone : {"update/skippedVersion", "update/proxyPort", "update/baseUrl"})
            QVERIFY2(!QSettings().contains(QLatin1String(gone)), gone);
    }

    void aSwitchedOffCheckStaysOff_data()
    {
        QTest::addColumn<QString>("key");
        QTest::newRow("bis 1.9.3") << QStringLiteral("update/autoCheck");
        QTest::newRow("ab 1.9.4") << QStringLiteral("update/auto_check");
    }
    void aSwitchedOffCheckStaysOff()
    {
        QFETCH(QString, key);
        QSettings().setValue(key, false);
        qtmux::UpdateBridge bridge(wiring());
        bridge.start(noNetwork());
        QVERIFY(!bridge.autoCheck());
        QCOMPARE(QSettings().value(QLatin1String(appupdate::settingskey::kAutoCheck)).toBool(),
                 false);
    }

    void settersWriteTheFamilyKeysAtOnce()
    {
        qtmux::UpdateBridge bridge(wiring());
        bridge.start(noNetwork());
        QSignalSpy changed(&bridge, &qtmux::UpdateBridge::settingsChanged);
        bridge.setProxyMode(2);
        bridge.setProxyHost(QStringLiteral(" proxy.example "));
        bridge.setProxyPort(8080);
        bridge.setAutoCheck(false);
        QCOMPARE(changed.count(), 4);
        QSettings s;
        QCOMPARE(s.value(QLatin1String(appupdate::settingskey::kProxyMode)).toString(),
                 QStringLiteral("manual"));
        QCOMPARE(s.value(QLatin1String(appupdate::settingskey::kProxyHost)).toString(),
                 QStringLiteral("proxy.example"));
        QCOMPARE(s.value(QLatin1String(appupdate::settingskey::kProxyPort)).toInt(), 8080);
        QCOMPARE(s.value(QLatin1String(appupdate::settingskey::kAutoCheck)).toBool(), false);
        // Außerhalb des Bereichs: nichts gespeichert, kein Signal.
        bridge.setProxyPort(70000);
        bridge.setProxyPort(-1);
        QCOMPARE(bridge.proxyPort(), 8080);
        QCOMPARE(changed.count(), 4);
        // Unverändert gesetzt: kein Signal.
        bridge.setProxyMode(2);
        QCOMPARE(changed.count(), 4);
    }

    void restoreDefaultsGoesBackToTheFamilyDefaults()
    {
        qtmux::UpdateBridge bridge(wiring());
        bridge.start(noNetwork());
        bridge.setAutoCheck(false);
        bridge.setProxyMode(2);
        QSignalSpy changed(&bridge, &qtmux::UpdateBridge::settingsChanged);
        bridge.restoreDefaults();
        QVERIFY(changed.count() >= 1);
        QVERIFY(bridge.autoCheck());
        QCOMPARE(bridge.proxyMode(), 0);
    }

    void anActionOutsideItsStateDoesNothing()
    {
        qtmux::UpdateBridge bridge(wiring());
        bridge.start(noNetwork());
        QCOMPARE(bridge.flow().state(), Flow::State::Idle);
        QVERIFY(!bridge.perform(static_cast<int>(Flow::Action::QuitAndInstall)));
        QVERIFY(!bridge.perform(static_cast<int>(Flow::Action::Download)));
        QCOMPARE(bridge.flow().state(), Flow::State::Idle);
    }

    void aChangedSessionCountRefreshesTheView()
    {
        // Der Satz „N Sitzungen werden beendet“ steht fertig in `flow.view`; die
        // Oberfläche liest `view` zusammen mit `textRevision`. Ändert sich die Zahl,
        // muss die Revision steigen, sonst bliebe im Zustand „Bereit“ die alte Zahl.
        qtmux::UpdateBridge bridge(wiring());
        bridge.start(noNetwork());
        QSignalSpy texts(&bridge, &qtmux::UpdateBridge::textsChanged);
        const int before = bridge.textRevision();
        bridge.setRunningSessions(3);
        QCOMPARE(texts.count(), 1);
        QVERIFY(bridge.textRevision() > before);
        bridge.setRunningSessions(3);
        QCOMPARE(texts.count(), 1);
    }

    void aSessionAddedAfterConsentStopsTheQuit_data()
    {
        QTest::addColumn<int>("sessionsAfterClick");
        QTest::addColumn<bool>("quits");
        QTest::newRow("eine Sitzung mehr") << 3 << false;
        QTest::newRow("eine Sitzung weniger") << 1 << true;
        QTest::newRow("gleich viele") << 2 << true;
    }

    void aSessionAddedAfterConsentStopsTheQuit()
    {
        // Zugestimmt hat der Anwender zu der Zahl, die „Beenden und aktualisieren“ beim
        // Klick nannte. Kommt bis zum Scharfschalten eine Sitzung dazu, beendet QTmux
        // nicht: keine Installation, kein Beenden, „Bereit“, Dialog neu mit neuer Zahl.
        QFETCH(int, sessionsAfterClick);
        QFETCH(bool, quits);
        QTemporaryDir work;
        QVERIFY(work.isValid());
        int quitCalls = 0;
        qtmux::UpdateBridge::Wiring w = wiring();
        w.selfTest = true;
        w.selfTestDir = work.path();
        w.requestQuit = [&quitCalls] {
            ++quitCalls;
            return true;
        };
        qtmux::UpdateBridge bridge(std::move(w));
        auto backend = std::make_unique<HeldBackend>();
        HeldBackend *held = backend.get();
        bridge.flow().setInstallBackend(std::move(backend));
        bridge.setRunningSessions(2);
        const QString fixture =
            QUrl::fromLocalFile(QStringLiteral(QTMUX_UPDATE_FIXTURE_DIR "/good")).toString();
        bridge.start({QStringLiteral("qtmux"), QStringLiteral("--no-update-check"),
                      QStringLiteral("--update-base-url"), fixture});
        QVERIFY(bridge.flow().sourceOverridden());
        QVERIFY(bridge.openByHand());
        QTRY_COMPARE_WITH_TIMEOUT(bridge.flow().state(), Flow::State::Available, 20000);
        QVERIFY(bridge.perform(static_cast<int>(Flow::Action::Download)));
        QTRY_COMPARE_WITH_TIMEOUT(bridge.flow().state(), Flow::State::Ready, 20000);

        QVERIFY(bridge.perform(static_cast<int>(Flow::Action::QuitAndInstall)));
        QCOMPARE(bridge.flow().state(), Flow::State::Installing);
        QVERIFY(held->held);
        bridge.setRunningSessions(sessionsAfterClick);
        QSignalSpy dialog(&bridge.flow(), &Flow::dialogRequested);
        held->finishPrepare();

        QCOMPARE(held->launches, 0);   // ausgelöst würde sie erst bei aboutToQuit
        if (quits) {
            QCOMPARE(quitCalls, 1);
            QCOMPARE(bridge.flow().state(), Flow::State::Installing);
            QCOMPARE(dialog.count(), 0);
            return;
        }
        QCOMPARE(quitCalls, 0);
        QCOMPARE(bridge.flow().state(), Flow::State::Ready);
        QCOMPARE(held->discards, 1);
        {
            QSettings raw;
            QVERIFY(!appupdate::UpdateSettings(raw).pendingInstall().isValid());
        }
        QCOMPARE(dialog.count(), 1);
        const QStringList foot = bridge.flow().view().footnote;
        QVERIFY2(foot.join(QLatin1Char(' ')).contains(QLatin1String("3 sessions are running")),
                 qPrintable(foot.join(QLatin1String(" | "))));
        // Ein zweiter Klick nimmt die neue Zahl als Zustimmung.
        QVERIFY(bridge.perform(static_cast<int>(Flow::Action::QuitAndInstall)));
        held->finishPrepare();
        QCOMPARE(quitCalls, 1);
    }

    void aLanguageSwitchReachesFlowAndSettingsPage()
    {
        // Der Sprachwechsel geht durch den Ablauf (er tauscht Katalog und Texte und
        // meldet viewChanged); die Seite „Updates“ liest ihre Sätze einzeln und
        // folgt dem Revisionszähler. Beides muss nach dem Wechsel die neue Sprache
        // tragen, ohne Neustart.
        qtmux::UpdateBridge bridge(wiring());
        bridge.start(noNetwork());
        const QString en = QStringLiteral("Check for updates automatically at startup");
        QCOMPARE(bridge.text(QStringLiteral("set.auto")), en);
        QSignalSpy view(&bridge.flow(), &Flow::viewChanged);
        QSignalSpy texts(&bridge, &qtmux::UpdateBridge::textsChanged);
        bridge.setLanguage(QStringLiteral("de"));
        QCOMPARE(bridge.text(QStringLiteral("set.auto")),
                 QStringLiteral("Beim Start automatisch nach Updates suchen"));
        QCOMPARE(bridge.flow().texts().language(), QStringLiteral("de"));
        QVERIFY(view.count() >= 1);
        QCOMPARE(texts.count(), 1);
        bridge.setLanguage(QStringLiteral("en"));
        QCOMPARE(bridge.text(QStringLiteral("set.auto")), en);
    }

    void aRememberedProxyPasswordNeverReachesTheSettingsFile()
    {
        // QTMUX-129 (bisher tst_updateviewmodel): Ein Passwort, das die Proxy-Anmeldung
        // „für diese Sitzung“ behält, lebt nur im Speicher. Ein Schein-Proxy auf
        // 127.0.0.1 verlangt die Anmeldung (407); nichts verlässt den Rechner.
        QTcpServer proxy;
        QVERIFY(proxy.listen(QHostAddress::LocalHost, 0));
        int requests = 0;
        connect(&proxy, &QTcpServer::newConnection, &proxy, [&] {
            while (QTcpSocket* s = proxy.nextPendingConnection()) {
                connect(s, &QTcpSocket::readyRead, s, [s, &requests] {
                    if (!s->readAll().contains("\r\n\r\n")) return;
                    ++requests;
                    s->write("HTTP/1.1 407 Proxy Authentication Required\r\n"
                             "Proxy-Authenticate: Basic realm=\"test\"\r\n"
                             "Content-Length: 0\r\nConnection: close\r\n\r\n");
                    s->disconnectFromHost();
                });
                connect(s, &QTcpSocket::disconnected, s, &QObject::deleteLater);
            }
        });

        const QString secret = QStringLiteral("streng-geheim-4711");
        qtmux::UpdateBridge bridge(wiring());
        bridge.start(noNetwork());
        bridge.setProxyMode(2);
        bridge.setProxyHost(QStringLiteral("127.0.0.1"));
        bridge.setProxyPort(proxy.serverPort());

        QVERIFY(bridge.openByHand());
        QTRY_VERIFY_WITH_TIMEOUT(bridge.flow().proxyAuth().pending, 20000);
        bridge.provideProxyCredentials(QStringLiteral("nutzer"), secret, true);
        QVERIFY2(bridge.hasSessionPassword(), "Passwort wurde nicht für die Sitzung behalten");
        QTRY_VERIFY_WITH_TIMEOUT(requests >= 2, 20000);   // die Wiederholung lief
        QSettings().sync();

        QFile f(QSettings().fileName());
        QVERIFY2(f.open(QIODevice::ReadOnly), qPrintable(f.fileName()));
        const QByteArray bytes = f.readAll();
        const QByteArray utf16(reinterpret_cast<const char*>(secret.utf16()),
                               secret.size() * qsizetype(sizeof(char16_t)));
        QVERIFY2(!bytes.contains(secret.toUtf8()) && !bytes.contains(utf16),
                 "Proxy-Passwort in der Einstellungsdatei");
        // Gegenprobe: Es ist die Datei, in der die Seite „Updates“ schreibt.
        QVERIFY2(bytes.contains("127.0.0.1"), qPrintable(f.fileName()));
    }

    void versionAndBuildIdComeFromTheBuild()
    {
        qtmux::UpdateBridge bridge(wiring());
        QVERIFY(!bridge.currentVersion().isEmpty());
        QVERIFY(!bridge.currentVersion().contains(QLatin1Char('+')));   // Vergleichsgröße
        QVERIFY(bridge.buildId().startsWith(bridge.currentVersion()));  // Anzeige
    }

    void everyIconTheFlowNamesIsEmbedded()
    {
        // Die Namen kommen zur Laufzeit aus dem Ablauf (view.iconName, hint.iconName …);
        // tst_icons sieht sie im QML nicht. Darum hier: jeder Name aus
        // UpdateFlow::iconNames() liegt als SVG in resources/icons/, und iconSource()
        // liefert für ihn einen Pfad — für einen fremden Namen nicht. Dass jede
        // Datei dort auch EINGEBETTET ist, belegt tst_icons
        // (everyIconFileOnDiskIsEmbedded); erst beide zusammen tragen den Namen.
        qtmux::UpdateBridge bridge(wiring());
        for (const QString &name : Flow::iconNames()) {
            QVERIFY2(QFile::exists(QStringLiteral(QTMUX_ICON_DIR "/") + name + QStringLiteral(".svg")),
                     qPrintable(name));
            QCOMPARE(bridge.iconSource(name), QStringLiteral("qrc:/icons/") + name + QStringLiteral(".svg"));
        }
        QVERIFY(bridge.iconSource(QStringLiteral("not-an-icon")).isEmpty());
    }

    void upgradeCodeMatchesTheInstaller()
    {
        QFile wxs(QStringLiteral(QTMUX_WXS_FILE));
        QVERIFY(wxs.open(QIODevice::ReadOnly));
        static const QRegularExpression re(QStringLiteral(R"re(UpgradeCode="([0-9A-Fa-f-]{36})")re"));
        const auto m = re.match(QString::fromUtf8(wxs.readAll()));
        QVERIFY(m.hasMatch());
        QCOMPARE(qtmux::UpdateBridge::windowsUpgradeCode().toUpper(), m.captured(1).toUpper());
    }
};

QTEST_GUILESS_MAIN(TestUpdateBridge)
#include "tst_updatebridge.moc"
