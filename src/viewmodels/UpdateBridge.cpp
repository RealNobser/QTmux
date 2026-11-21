#include "UpdateBridge.h"

#include "FileManagerReveal.h"
#include "qtmux_buildid.h"
#include "qtmux_version.h"

#include "update/SelfInstall.hpp"
#include "update/UpdateTexts.hpp"

#include <QCoreApplication>
#include <QDir>
#include <QLoggingCategory>

namespace qtmux {

namespace {

// UpgradeCode von installer/QTmux.wxs — dieselbe Quelle, ein Test vergleicht beide
// (tst_updatebridge, `upgradeCodeMatchesTheInstaller`). Der Ablauf braucht ihn unter
// Windows, um das installierte Produkt zu erkennen (Vertrag V2 §6, E8).
constexpr const char *kWindowsUpgradeCode = "84D77034-7166-4138-B80F-2B2DCEFD1602";

using Flow = appupdate::UpdateFlow;

} // namespace

UpdateBridge::UpdateBridge(Wiring wiring, QObject *parent)
    : QObject(parent), m_selfTest(wiring.selfTest)
{
    Flow::Config cfg;
    cfg.productSlug = QStringLiteral("qtmux");
    cfg.productName = QStringLiteral("QTmux");
    cfg.version = QStringLiteral(QTMUX_VERSION_STRING);
    cfg.buildId = QStringLiteral(QTMUX_BUILD_ID);
    cfg.language = wiring.language;
    cfg.requestQuit = std::move(wiring.requestQuit);
    // Was beim Beenden endet: die laufenden Sitzungen (Daten, der Satz kommt aus
    // dem Hub-Katalog, Schlüssel s5.qtmux.n / s5.qtmux.1).
    cfg.quitImpact = [this] {
        Flow::QuitImpact q;
        q.runningSessions = m_runningSessions;
        return q;
    };
#if defined(Q_OS_WIN)
    cfg.windowsUpgradeCode = windowsUpgradeCode();
#endif
    cfg.allowSourceOverrideForSelfTest = wiring.selfTest;

    // Der Hub-Katalog muss VOR dem ersten Text installiert sein; danach fragt der
    // Ablauf jeden Satz zur Laufzeit über QCoreApplication::translate ab.
    if (!appupdate::installUpdateTranslator(wiring.language))
        qWarning("QTmux: Update-Texte fuer \"%s\" fehlen im Bau (Hub-Katalog).",
                 qPrintable(wiring.language));

    if (wiring.externalFlow) {
        // Bildziel: Der Ablauf gehört dem Szenen-Baustein (eigene Einstellungen,
        // eigener Installationsweg); die Bridge zeigt ihn nur.
        m_flow = wiring.externalFlow;
    } else {
        m_ownFlow = std::make_unique<Flow>(std::move(cfg), m_settings, nullptr);
        m_flow = m_ownFlow.get();
        // Installationsweg: Die Wahl trifft der Hub, einmal für jede App (macOS: die
        // App ersetzt sich selbst; Windows: der Helfer um msiexec, sobald der Hub ihn
        // freigibt; Linux: das AppImage wird an seinem Ort ersetzt). Die Helfer bettet
        // appupdate_embed_install_helpers(qtmux) ein (CMakeLists.txt).
        // Start-Bestätigung: die Vorgabe des Hubs (erster Lauf der Ereignisschleife
        // nach start()). Hauptfenster, Sitzungen und PTYs entstehen synchron in
        // engine.loadFromModule(), und start() läuft erst danach (src/app/main.cpp).
        // Genau dieser eine Aufruf, ohne Bedingung. Wächter: Regel 5 von
        // test_update_consumer_guard (Anzahl, Zeile allein, keine einzeilige Bedingung
        // davor) und tst_updatequit theRealInstallWayFindsItsHelperInTheBuiltApp (der
        // Selbsttest zeigt unter macOS/Windows den Weg des Hubs). Eine Bedingung als
        // Klammerblock sieht keiner der beiden.
        m_flow->setInstallBackend(appupdate::makeInstallBackendForThisSystem());
    }

    if (!wiring.externalFlow && wiring.selfTest && !wiring.selfTestDir.isEmpty()) {
        QDir().mkpath(wiring.selfTestDir + QStringLiteral("/downloads"));
        QDir().mkpath(wiring.selfTestDir + QStringLiteral("/results"));
        m_flow->setDownloadDirForTests(wiring.selfTestDir + QStringLiteral("/downloads"));
        m_flow->setResultDirForTests(wiring.selfTestDir + QStringLiteral("/results"));
    }

    // „Im Ordner zeigen“ ist Gui-Code und gehört darum nicht in den Kern.
    connect(m_flow, &Flow::revealFileRequested, this,
            [](const QString &path) { FileManagerReveal::reveal(path); });
    connect(m_flow, &Flow::viewChanged, this, &UpdateBridge::notesChanged);
    // Zurücksetzen über den Ablauf: Die Seite liest ihre Werte neu.
    connect(m_flow, &Flow::settingsRestored, this, &UpdateBridge::settingsChanged);
    connect(m_flow, &Flow::settingsRestored, this, &UpdateBridge::settingsReplaced);
    connect(m_flow, &Flow::settingsRestored, this, &UpdateBridge::sessionPasswordChanged);
    // Der Ablauf verwirft ein Sitzungspasswort auch selbst (nach einer Ablehnung):
    // Die Anzeige auf der Seite folgt dem Stand der Anmeldung.
    connect(m_flow, &Flow::proxyAuthChanged, this, &UpdateBridge::sessionPasswordChanged);
}

UpdateBridge::~UpdateBridge()
{
    // Messpunkt für tst_updatequit: Der Ablauf (mit Backend, Timern, Einstellungen) muss
    // abgebaut werden, solange die App noch lebt (Vorgabe des Hubs; main.cpp hält die
    // Bridge darum zwischen app und engine).
    if (m_selfTest)
        qInfo("QTmux selftest-update: update bridge destroyed (app alive: %d)",
              QCoreApplication::instance() ? 1 : 0);
}

QString UpdateBridge::currentVersion() const { return QStringLiteral(QTMUX_VERSION_STRING); }
QString UpdateBridge::buildId() const { return m_flow->installedBuildId(); }
QString UpdateBridge::windowsUpgradeCode() { return QString::fromLatin1(kWindowsUpgradeCode); }

void UpdateBridge::start(const QStringList &arguments)
{
    m_flow->applyStartOptions(arguments);
    m_flow->start();
    // start() übernimmt alte QTmux-Schlüssel (UpdateSettings::migrate) — die
    // Seite soll danach die übernommenen Werte zeigen.
    emit settingsChanged();
}

// --- Einstellungen -----------------------------------------------------------

bool UpdateBridge::autoCheck() const { return m_flow->autoCheck(); }
void UpdateBridge::setAutoCheck(bool on)
{
    if (on == autoCheck()) return;
    m_flow->setAutoCheck(on);
    emit settingsChanged();
}

void UpdateBridge::setProxy(const std::function<void(appupdate::ProxyConfig &)> &change)
{
    appupdate::ProxyConfig cfg = m_flow->proxyConfig();
    const appupdate::ProxyConfig before = cfg;
    change(cfg);
    if (cfg == before) return;
    // Der Ablauf speichert und verwirft dabei ein Sitzungspasswort, wenn Modus,
    // Host oder Benutzer wechseln (Vertrag §5, A9).
    m_flow->setProxyConfig(cfg);
    emit settingsChanged();
    emit sessionPasswordChanged();
}

int UpdateBridge::proxyMode() const
{
    switch (m_flow->proxyConfig().mode) {
    case appupdate::ProxyConfig::Mode::None:   return 1;
    case appupdate::ProxyConfig::Mode::Manual: return 2;
    case appupdate::ProxyConfig::Mode::System: break;
    }
    return 0;
}
void UpdateBridge::setProxyMode(int mode)
{
    setProxy([mode](appupdate::ProxyConfig &c) {
        c.mode = mode == 1 ? appupdate::ProxyConfig::Mode::None
               : mode == 2 ? appupdate::ProxyConfig::Mode::Manual
                           : appupdate::ProxyConfig::Mode::System;
    });
}

int UpdateBridge::proxyType() const
{
    return m_flow->proxyConfig().type == QNetworkProxy::Socks5Proxy ? 1 : 0;
}
void UpdateBridge::setProxyType(int type)
{
    setProxy([type](appupdate::ProxyConfig &c) {
        c.type = type == 1 ? QNetworkProxy::Socks5Proxy : QNetworkProxy::HttpProxy;
    });
}

QString UpdateBridge::proxyHost() const { return m_flow->proxyConfig().host; }
void UpdateBridge::setProxyHost(const QString &host)
{
    setProxy([&host](appupdate::ProxyConfig &c) { c.host = host.trimmed(); });
}

int UpdateBridge::proxyPort() const { return m_flow->proxyConfig().port; }
void UpdateBridge::setProxyPort(int port)
{
    // Der Bereich wird an der Oberfläche markiert, nicht hier still beschnitten:
    // ein Wert außerhalb 0…65535 wird gar nicht erst gespeichert.
    if (port < 0 || port > 65535) return;
    setProxy([port](appupdate::ProxyConfig &c) { c.port = static_cast<quint16>(port); });
}

QString UpdateBridge::proxyUser() const { return m_flow->proxyConfig().user; }
void UpdateBridge::setProxyUser(const QString &user)
{
    setProxy([&user](appupdate::ProxyConfig &c) { c.user = user.trimmed(); });
}

QString UpdateBridge::skippedVersion() const { return m_flow->skippedVersion(); }
bool UpdateBridge::hasSessionPassword() const { return m_flow->hasSessionPassword(); }

void UpdateBridge::resetSkippedVersion()
{
    m_flow->resetSkippedVersion();
    emit settingsChanged();
}

void UpdateBridge::forgetSessionPassword()
{
    m_flow->forgetSessionPassword();
    emit sessionPasswordChanged();
}

void UpdateBridge::restoreDefaults() { m_flow->restoreDefaults(); }

// Nach Import oder Zurücksetzen über die Einstellungsdatei: Modus oder Host können
// sich geändert haben, ohne dass der Ablauf es sah. Ein Sitzungspasswort gehört zum
// alten Proxy und wird verworfen (wie beim Umschalten auf der Seite).
void UpdateBridge::reloadSettings()
{
    m_flow->forgetSessionPassword();
    emit settingsChanged();
    emit settingsReplaced();
    emit sessionPasswordChanged();
}

void UpdateBridge::setRunningSessions(int n)
{
    if (n == m_runningSessions) return;
    m_runningSessions = n;
    emit runningSessionsChanged();
    // Der Satz „N Sitzungen werden beendet“ steht fertig in `flow.view`; die
    // Oberfläche liest `view` mit `textRevision` und bekommt so die neue Zahl.
    ++m_textRevision;
    emit textsChanged();
}

QString UpdateBridge::notesHighlight() const { return m_flow->releaseNotes().highlightHtml; }

QString UpdateBridge::text(const QString &key, const QVariantMap &args) const
{
    QHash<QString, QString> a;
    for (auto it = args.constBegin(); it != args.constEnd(); ++it)
        a.insert(it.key(), it.value().toString());
    const QByteArray k = key.toLatin1();
    return m_flow->texts().get(k.constData(), a);
}

QString UpdateBridge::iconSource(const QString &name) const
{
    // Die Oberfläche nutzt zusätzlich diese drei (Proxy-Anmeldung, Details).
    static const QStringList surfaceIcons = {QStringLiteral("lock-key"),
                                             QStringLiteral("caret-right"),
                                             QStringLiteral("caret-down")};
    if (name.isEmpty()) return {};
    if (!Flow::iconNames().contains(name) && !surfaceIcons.contains(name)) {
        qWarning("QTmux: unbekanntes Update-Icon \"%s\"", qPrintable(name));
        return {};
    }
    return QStringLiteral("qrc:/icons/") + name + QStringLiteral(".svg");
}

// --- Aktionen ----------------------------------------------------------------

bool UpdateBridge::perform(int action)
{
    const bool ok = m_flow->perform(static_cast<Flow::Action>(action));
    // „Version überspringen“ und „Später“ ändern sichtbare Einstellungen.
    emit settingsChanged();
    return ok;
}

bool UpdateBridge::openByHand() { return m_flow->openByHand(); }
bool UpdateBridge::openFromHint() { return m_flow->openFromHint(); }
void UpdateBridge::dismissHint() { m_flow->dismissHint(); }
void UpdateBridge::setDialogVisible(bool visible) { m_flow->setDialogVisible(visible); }
bool UpdateBridge::checkNow() { return m_flow->checkNow(); }

void UpdateBridge::provideProxyCredentials(const QString &user, const QString &password,
                                           bool remember)
{
    m_flow->provideProxyCredentials(user, password, remember);
    emit settingsChanged();
    emit sessionPasswordChanged();
}

void UpdateBridge::cancelProxyAuth() { m_flow->cancelProxyAuth(); }

void UpdateBridge::setLanguage(const QString &language)
{
    // Der Ablauf tauscht Katalog und Texte selbst und meldet viewChanged (darüber
    // auch notesChanged), hintChanged und proxyAuthChanged. Die Seite „Updates“
    // liest ihre Sätze nicht aus dem Ablauf, sondern einzeln aus dem Katalog: Sie
    // folgt dem Revisionszähler. Bekannte Grenze des Hubs: Ein schon vorbereitetes
    // Selbst-Update behält die Änderungshinweise in der Sprache der Vorbereitung.
    m_flow->setLanguage(language);
    ++m_textRevision;
    emit textsChanged();
}

} // namespace qtmux
