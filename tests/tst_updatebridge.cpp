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
#include "update/UpdateSettings.hpp"

using Flow = appupdate::UpdateFlow;

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
