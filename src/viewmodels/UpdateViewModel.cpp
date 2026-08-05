#include "UpdateViewModel.h"

#include "qtmux_version.h"

#include "update/InstallerLauncher.hpp"
#include "update/UpdateChecker.hpp"
#include "update/UpdatePolicy.hpp"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QLocale>
#include <QSettings>
#include <QStandardPaths>
#include <QVersionNumber>

namespace qtmux {

namespace {

// Produktname auf dem Webspace: /updates/<product>/manifest.json
constexpr auto kProduct = "qtmux";

// Basis-URL OHNE Produktteil — den hängt der Kern selbst an. Die Vollform
// https://nobser.de/updates/qtmux/manifest.json ergibt sich daraus.
constexpr auto kDefaultBaseUrl = "https://nobser.de/updates";

constexpr auto kKeyAutoCheck = "update/autoCheck";
constexpr auto kKeyLastCheck = "update/lastCheck";
constexpr auto kKeySkipped   = "update/skippedVersion";
constexpr auto kKeyBaseUrl   = "update/baseUrl";
// Proxy (QTMUX-129) — deckt sich mit appupdate::ProxyConfig. KEIN Passwort-
// Schlüssel: das lebt nur im Sitzungsspeicher (ProxyCredentials.h).
constexpr auto kKeyProxyMode = "update/proxyMode";
constexpr auto kKeyProxyType = "update/proxyType";
constexpr auto kKeyProxyHost = "update/proxyHost";
constexpr auto kKeyProxyPort = "update/proxyPort";
constexpr auto kKeyProxyUser = "update/proxyUser";

QVersionNumber currentVersionNumber() {
    return QVersionNumber(QTMUX_VERSION_MAJOR, QTMUX_VERSION_MINOR,
                          QTMUX_VERSION_PATCH);
}

} // namespace

UpdateViewModel::UpdateViewModel(QObject *parent) : QObject(parent) {
    // Sprache aus den Einstellungen vorbelegen, damit `notes` auch dann etwas
    // Sinnvolles liefert, wenn niemand die Property bindet (Tests, Kopfloses).
    QSettings s;
    const QString sys = QLocale::system().name().left(2);
    m_language = s.value(QStringLiteral("ui/language"),
                         sys == QLatin1String("de") ? QStringLiteral("de")
                                                    : QStringLiteral("en"))
                     .toString();
}

UpdateViewModel::~UpdateViewModel() = default;

// --- Einstellungen ----------------------------------------------------------
bool UpdateViewModel::autoCheck() const {
    return QSettings().value(QLatin1String(kKeyAutoCheck), true).toBool();
}

void UpdateViewModel::setAutoCheck(bool on) {
    if (on == autoCheck()) return;
    QSettings().setValue(QLatin1String(kKeyAutoCheck), on);
    emit autoCheckChanged();
}

QString UpdateViewModel::baseUrl() const {
    return QSettings()
        .value(QLatin1String(kKeyBaseUrl), QLatin1String(kDefaultBaseUrl))
        .toString();
}

void UpdateViewModel::setBaseUrl(const QString &url) {
    const QString v = url.trimmed().isEmpty() ? QString::fromLatin1(kDefaultBaseUrl)
                                              : url.trimmed();
    if (v == baseUrl()) return;
    QSettings().setValue(QLatin1String(kKeyBaseUrl), v);
    emit baseUrlChanged();
    // Der Checker hält die Basis-URL im Konstruktor fest — beim nächsten Lauf
    // muss er also neu gebaut werden, sonst zeigte ein Dry-Run weiter auf den
    // Produktivserver (und die Messung ginge an der falschen Quelle vorbei).
    m_checker.reset();
}

void UpdateViewModel::setLanguage(const QString &lang) {
    if (lang.isEmpty() || lang == m_language) return;
    m_language = lang;
    emit notesChanged();
}

// --- Abgeleitete Anzeigewerte -----------------------------------------------
QString UpdateViewModel::currentVersion() const {
    return QStringLiteral(QTMUX_VERSION_STRING);
}

QString UpdateViewModel::remoteVersion() const {
    return m_manifest ? m_manifest->versionString : QString();
}

QString UpdateViewModel::published() const {
    return m_manifest ? m_manifest->published : QString();
}

QString UpdateViewModel::notes() const {
    return m_manifest ? m_manifest->notes(m_language) : QString();
}

qint64 UpdateViewModel::downloadSize() const {
    if (!m_manifest) return 0;
    const auto art = m_manifest->currentArtifact();
    return art ? art->size : 0;
}

bool UpdateViewModel::hasPackageForThisSystem() const {
    return m_manifest && m_manifest->currentArtifact().has_value();
}

QString UpdateViewModel::downloadDir() const {
    QString base = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
    if (base.isEmpty())
        base = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    return base + QStringLiteral("/QTmux-Updates");
}

QString UpdateViewModel::launchPlanDescription() const {
    if (m_downloadedPath.isEmpty() || !m_manifest) return {};
    const auto art = m_manifest->currentArtifact();
    if (!art) return {};
    const auto plan = appupdate::installerLaunchPlan(
        m_downloadedPath, art->kind, QString::fromLocal8Bit(qgetenv("APPIMAGE")));
    return plan ? plan->description : QString();
}

bool UpdateViewModel::canLaunchInstaller() const {
    return !launchPlanDescription().isEmpty();
}

// --- Zustandsführung ---------------------------------------------------------
void UpdateViewModel::setState(State s) {
    if (m_state == s) return;
    m_state = s;
    emit stateChanged();
}

void UpdateViewModel::setError(const QString &err) {
    m_lastError = err;
    emit lastErrorChanged();
}

void UpdateViewModel::rebuildChecker() {
    const QString base = baseUrl();
    if (m_checker && m_checkerBase == base) return;
    m_checker = std::make_unique<appupdate::UpdateChecker>(QUrl(base),
                                                           QLatin1String(kProduct));
    m_checkerBase = base;
    connect(m_checker.get(), &appupdate::UpdateChecker::downloadProgress, this,
            [this](qint64 got, qint64 total) {
                m_progress = (total > 0) ? double(got) / double(total) : 0.0;
                emit downloadProgressChanged();
            });
}

// --- Prüfen ------------------------------------------------------------------
void UpdateViewModel::checkNow() { startCheck(/*manual=*/true); }

void UpdateViewModel::checkOnStartup() {
    if (!autoCheck()) return;
    const QString last = QSettings().value(QLatin1String(kKeyLastCheck)).toString();
    if (!appupdate::policy::autoCheckDue(last, QDateTime::currentDateTime())) return;
    startCheck(/*manual=*/false);
}

void UpdateViewModel::startCheck(bool manual) {
    if (busy()) return;
    // Merken, was läuft — eine Proxy-Anmeldung mittendrin muss genau DAS
    // wiederholen können (QTMUX-129).
    m_proxyRetryIsDownload = false;
    m_proxyRetryManual = manual;
    m_manual = manual;
    m_available = false;
    m_downgrade = false;
    m_manifest.reset();
    m_downloadedPath.clear();
    m_progress = 0.0;
    setError(QString());
    emit resultChanged();
    emit notesChanged();
    emit downloadedPathChanged();
    emit downloadProgressChanged();
    setState(Checking);

    rebuildChecker();
    m_checker->checkForUpdate([this, manual](std::optional<appupdate::UpdateManifest> m,
                                            QString error) {
        // Der Zeitstempel wird auch bei einem FEHLSCHLAG geschrieben: sonst
        // versucht es der Start-Check bei jedem Start erneut, obwohl der Server
        // unerreichbar ist — aus „höchstens 1×/Tag" würde „bei jedem Start".
        QSettings().setValue(QLatin1String(kKeyLastCheck),
                             QDateTime::currentDateTime().toString(Qt::ISODate));

        if (!m) {
            // Still scheitern, wenn niemand danach gefragt hat.
            if (manual) {
                setError(error);
                setState(Failed);
            } else {
                setState(Idle);
            }
            emit checkFinished(manual);
            return;
        }

        m_manifest = std::move(m);
        const QVersionNumber cur = currentVersionNumber();
        const bool newer = appupdate::policy::isNewer(m_manifest->version, cur);
        const bool older = appupdate::policy::isDowngrade(m_manifest->version, cur);
        const QString skipped =
            QSettings().value(QLatin1String(kKeySkipped)).toString();

        // Übersprungen zählt NUR beim stillen Check — wer von Hand nachsieht,
        // will das Ergebnis sehen, egal was er einmal weggeklickt hat.
        const bool suppressed =
            !manual && appupdate::policy::isSkipped(m_manifest->version, skipped);

        // Downgrade wird nur auf ausdrückliche Anforderung angeboten (Owner:
        // „zulassen mit Warnung"). Beim Start-Check bliebe sonst jeder
        // Entwickler-Build täglich an derselben Rückstufungs-Frage hängen.
        m_downgrade = older && manual;
        m_available = (newer && !suppressed) || m_downgrade;

        emit resultChanged();
        emit notesChanged();
        setState(m_available ? Available : UpToDate);
        if (m_available) emit updateFound();
        emit checkFinished(manual);
    });
}

// --- Herunterladen -----------------------------------------------------------
void UpdateViewModel::download() {
    if (busy() || !m_manifest) return;
    m_proxyRetryIsDownload = true;   // s. startCheck()
    const auto art = m_manifest->currentArtifact();
    if (!art) {
        setError(tr("Für dieses Betriebssystem liegt kein Paket bereit."));
        setState(Failed);
        return;
    }
    m_progress = 0.0;
    emit downloadProgressChanged();
    setError(QString());
    setState(Downloading);

    rebuildChecker();
    m_checker->downloadArtifact(*art, downloadDir(),
                                [this](QString localPath, QString error) {
        if (!error.isEmpty()) {
            // Bei SHA-Abweichung hat der Kern die Datei bereits gelöscht — ein
            // beschädigter Installer darf nicht liegen bleiben.
            setError(error);
            setState(Failed);
            return;
        }
        m_downloadedPath = std::move(localPath);
        emit downloadedPathChanged();
        m_progress = 1.0;
        emit downloadProgressChanged();
        setState(Ready);
    });
}

bool UpdateViewModel::launchInstaller() {
    if (m_downloadedPath.isEmpty() || !m_manifest) return false;
    const auto art = m_manifest->currentArtifact();
    if (!art) return false;
    if (!canLaunchInstaller()) {
        // Der einzige reale Fall: Linux-AppImage, aber QTmux läuft nicht aus
        // einem ($APPIMAGE leer) — es gibt nichts zu ersetzen. Die interne
        // Meldung des Kerns („no launch plan for kind 'appimage'") hilft dem
        // Anwender nicht; er braucht den Pfad und den nächsten Schritt.
        setError(tr("QTmux läuft nicht aus einem AppImage und kann sich deshalb "
                    "nicht selbst ersetzen. Die geprüfte Datei liegt hier — bitte "
                    "von Hand installieren:\n%1").arg(m_downloadedPath));
        setState(Failed);
        return false;
    }
    QString err;
    if (appupdate::launchInstaller(m_downloadedPath, art->kind, &err)) return true;
    setError(err);
    setState(Failed);
    return false;
}

void UpdateViewModel::skipVersion() {
    if (!m_manifest) return;
    QSettings().setValue(QLatin1String(kKeySkipped), m_manifest->versionString);
    m_available = false;
    emit resultChanged();
    setState(UpToDate);
}

void UpdateViewModel::abort() {
    if (m_checker) m_checker->abort();
    if (busy()) setState(Idle);
}

// ---- Proxy-Einstellungen (QTMUX-129) ---------------------------------------
// Bewusst ohne Zwischenspeicher direkt aus/in QSettings — genau wie autoCheck
// und baseUrl daneben. So kann `SettingsIo` (Reset/Import) die Schlüssel
// ändern, ohne dass hier ein veralteter Wert stehen bliebe.

int UpdateViewModel::proxyMode() const {
    return QSettings().value(QLatin1String(kKeyProxyMode), 0).toInt();
}
void UpdateViewModel::setProxyMode(int mode) {
    if (mode == proxyMode()) return;
    QSettings().setValue(QLatin1String(kKeyProxyMode), mode);
    // Ein Moduswechsel macht ein gemerktes Passwort gegenstandslos — es gehörte
    // zu einem anderen Proxy.
    m_proxyCreds.clear();
    emit proxyChanged();
}

int UpdateViewModel::proxyType() const {
    return QSettings().value(QLatin1String(kKeyProxyType), 0).toInt();
}
void UpdateViewModel::setProxyType(int type) {
    if (type == proxyType()) return;
    QSettings().setValue(QLatin1String(kKeyProxyType), type);
    emit proxyChanged();
}

QString UpdateViewModel::proxyHost() const {
    return QSettings().value(QLatin1String(kKeyProxyHost)).toString();
}
void UpdateViewModel::setProxyHost(const QString &host) {
    if (host == proxyHost()) return;
    QSettings().setValue(QLatin1String(kKeyProxyHost), host);
    m_proxyCreds.clear();   // anderer Proxy → altes Passwort ungültig
    emit proxyChanged();
}

int UpdateViewModel::proxyPort() const {
    return QSettings().value(QLatin1String(kKeyProxyPort), 0).toInt();
}
void UpdateViewModel::setProxyPort(int port) {
    if (port == proxyPort()) return;
    QSettings().setValue(QLatin1String(kKeyProxyPort), port);
    emit proxyChanged();
}

QString UpdateViewModel::proxyUser() const {
    return QSettings().value(QLatin1String(kKeyProxyUser)).toString();
}
void UpdateViewModel::setProxyUser(const QString &user) {
    if (user == proxyUser()) return;
    QSettings().setValue(QLatin1String(kKeyProxyUser), user);
    m_proxyCreds.clear();   // anderer Benutzer → altes Passwort ungültig
    emit proxyChanged();
}

// ---- Proxy-Anmeldung (QTMUX-129) -------------------------------------------
//
// Der Weg im Ganzen: Lib fragt synchron → `answerProxyChallenge` antwortet aus
// dem Sitzungsspeicher, ohne zu blockieren → liegt nichts vor, feuert sie
// `proxyAuthenticationNeeded` und die Anfrage endet → QML fragt den Menschen →
// `provideProxyCredentials` legt ab und **wiederholt** den Vorgang.
//
// ⚠️ Noch NICHT an `appupdate` angeschlossen: Der vendierte Kern kennt
// `setProxyCredentialProvider` in dieser Arbeitskopie noch nicht (das Nachziehen
// ist eine eigene, angekündigte Runde). Die App-Hälfte ist bewusst vollständig
// und getestet, damit dann nur eine Lambda-Zeile fehlt.

bool UpdateViewModel::answerProxyChallenge(const QString &host, int port,
                                           bool previousAttemptFailed,
                                           QString *user, QString *password) {
    if (!user || !password) return false;

    if (m_proxyCreds.mayAnswer(previousAttemptFailed)) {
        *user = m_proxyCreds.user();
        *password = m_proxyCreds.password();
        return true;
    }

    // Kein (brauchbares) Passwort da. Den Menschen fragen — aber NUR melden,
    // nicht warten: Wir stehen in einem synchronen Callback, jedes Blockieren
    // hier friert das Netzwerk-Ereignis ein.
    // 🔑 Queued, damit das Signal die Event-Loop erreicht, nachdem der Callback
    // zurückgekehrt ist und die Lib ihre Anfrage sauber beendet hat.
    QMetaObject::invokeMethod(this, [this, host, port]() {
        emit proxyAuthenticationNeeded(host, port, m_proxyCreds.lastAttemptRejected());
    }, Qt::QueuedConnection);
    return false;
}

void UpdateViewModel::provideProxyCredentials(const QString &user, const QString &password) {
    m_proxyCreds.set(user, password);
    // Genau den Vorgang wiederholen, der unterbrochen wurde. Ohne das müsste der
    // Mensch nach der Anmeldung von Hand neu anstoßen — und würde beim Download
    // wieder ganz vorne anfangen.
    if (m_proxyRetryIsDownload)
        download();
    else
        startCheck(m_proxyRetryManual);
}

void UpdateViewModel::cancelProxyAuthentication() {
    m_proxyCreds.clear();
    setState(Idle);
}

void UpdateViewModel::forgetProxyCredentials() { m_proxyCreds.clear(); }

bool UpdateViewModel::hasProxyCredentials() const {
    return m_proxyCreds.hasUsablePassword();
}

} // namespace qtmux
