#pragma once

// appupdate::UpdateSettings — the ONE set of update keys for all four apps
// (update contract V2, 2026-11-19, docs/SHARED.md).
//
// CANONICAL: QTmux vendors src/update/ byte-identically. Qt6::Core only.
//
// ⚠️ This is a deliberate change of the vendoring contract (docs/SHARED.md,
// rule 2 used to say "no QSettings in src/update/"): the keys, defaults and the
// migration live here because their drifting apart WAS the finding. UpdateChecker itself stays QSettings-free.
//
// The class is a thin view over a QSettings the app owns (tests pass one on an
// INI file in a temp dir). It never caches: every getter reads, so a settings
// import or reset done elsewhere is seen at once.
//
// THE PASSWORD IS NOT HERE. It lives in UpdateFlow's memory only. There is
// no key for it, no setter, and the export filter below would drop one anyway.

#include "update/ProxyConfig.hpp"
#include "update/SerialStore.hpp"

#include <QDateTime>
#include <QString>
#include <QVariantMap>

#include <cstdint>

class QSettings;

namespace appupdate {

namespace settingskey {
inline constexpr const char* kAutoCheck      = "update/auto_check";
inline constexpr const char* kLastCheck      = "update/last_check";
inline constexpr const char* kSkippedVersion = "update/skipped_version";
inline constexpr const char* kLastSerialPrefix = "update/last_serial/";
inline constexpr const char* kProxyMode      = "update/proxy_mode";
inline constexpr const char* kProxyType      = "update/proxy_type";
inline constexpr const char* kProxyHost      = "update/proxy_host";
inline constexpr const char* kProxyPort      = "update/proxy_port";
inline constexpr const char* kProxyUser      = "update/proxy_user";
inline constexpr const char* kDryRunLaunch   = "update/dry_run_launch";
inline constexpr const char* kPendingInstall = "update/pending_install";
// the machine word of the last failed install (result file `code=`).
inline constexpr const char* kInstallBlockedCode = "update/install_blocked_code";
// Removed on every start, never read: the update source is
// not a setting. Both spellings — the family's and QTmux'.
inline constexpr const char* kBaseUrlDropped       = "update/base_url";
inline constexpr const char* kBaseUrlDroppedQtmux  = "update/baseUrl";
}  // namespace settingskey

// What a stored lastSerial looks like. Unreadable is LOUD: a
// value is there, but it is no number in range — the app's settings were
// edited or damaged, and replay protection silently starting from 0 would be
// the wrong answer to keep quiet about.
enum class SerialState : std::uint8_t { Absent, Valid, Unreadable };

// The marker written right before „Beenden und aktualisieren" quits the app
// (the update contract). Everything the next start needs to judge the
// outcome — and to retry without a new download (state InstallFailed).
struct PendingInstall {
    QString fromVersion;
    QString toVersion;
    QString method;         // installMethodName(): "automatic" | "system-installer"
    QString file;           // the verified artifact
    QString sha256;
    qint64  size = 0;
    QString kind;           // "dmg" | "msi" | "appimage"
    QString resultFile;     // where an Automatic helper reports
    QString confirmFile;    // where the new version confirms its start
    QString launchError;    // set when launch() itself failed in aboutToQuit
    QString notes;          // release notes of toVersion (state 6 after restart)
    QString published;      // its release date, ISO
    QDateTime startedUtc;

    [[nodiscard]] bool isValid() const
    {
        return !fromVersion.isEmpty() && !toVersion.isEmpty() && !method.isEmpty();
    }
};

class UpdateSettings {
public:
    // `settings` must outlive this view.
    explicit UpdateSettings(QSettings& settings);

    // ---- Start-up housekeeping ---------------------------------------------
    // Runs migrateLegacyKeys() over the live settings and drops the update
    // source. Idempotent; call once per start, before the first read.
    // Returns true when anything was changed.
    bool migrate();

    // ---- Plain keys ---------------------------------------------------------
    [[nodiscard]] bool autoCheck() const;               // default ON
    void setAutoCheck(bool on);

    [[nodiscard]] QDateTime lastCheck() const;          // invalid = never
    void setLastCheck(const QDateTime& when);

    [[nodiscard]] QString skippedVersion() const;       // empty = none
    void setSkippedVersion(const QString& version);
    void clearSkippedVersion();

    [[nodiscard]] bool dryRunLaunch() const;            // default OFF

    // ---- Proxy (no password, ever) ------------------------------------------
    [[nodiscard]] ProxyConfig proxyConfig() const;
    void setProxyConfig(const ProxyConfig& cfg);

    // ---- lastSerial (contract V1) --------------------------------------------
    [[nodiscard]] SerialState serialState(const QString& productSlug) const;
    // The SerialStore for UpdateChecker. load() answers 0 for Absent AND for
    // Unreadable (the checker then accepts the next signed manifest — the
    // flow reports Unreadable loudly, see UpdateFlow::serialUnreadable);
    // store() only ever writes a higher value than a VALID stored one.
    [[nodiscard]] SerialStore serialStore() const;

    // ---- Marker for „Beenden und aktualisieren" ------------------------------
    [[nodiscard]] PendingInstall pendingInstall() const;  // !isValid() = none
    void setPendingInstall(const PendingInstall& p);
    void clearPendingInstall();

    // remembered `code=` of the last failed attempt; empty = none.
    [[nodiscard]] QString installBlockedCode() const;
    void setInstallBlockedCode(const QString& code);

    // ---- Pure helpers (also for imports and exports) --------------------------

    // Maps QTmux' old spellings onto the family keys and removes them
    // (update contract V2): autoCheck → auto_check, lastCheck →
    // last_check, skippedVersion → skipped_version, proxyMode 0/1/2 →
    // system/none/manual, proxyType 0/1 → http/socks5, proxyHost/Port/User →
    // proxy_host/port/user; drops base_url and baseUrl.
    // A family key that is already there WINS — it is the newer value — and
    // the old one is only removed. That is what keeps a deliberately
    // switched-off auto_check off. Works on a settings snapshot or on the key
    // map of an imported export file alike. Returns true when it changed
    // anything.
    static bool migrateLegacyKeys(QVariantMap& keys);

    // False for keys that must never travel in an export file: lastSerial
    // (replay state of THIS machine), the proxy user, the install
    // marker, and the dropped update source.
    [[nodiscard]] static bool isExportable(const QString& key);

    // The stored words for ProxyConfig::Mode / type ("system" | "none" |
    // "manual", "http" | "socks5"). Unknown words read as the default.
    [[nodiscard]] static QString proxyModeWord(ProxyConfig::Mode m);
    [[nodiscard]] static ProxyConfig::Mode proxyModeFromWord(const QString& w);

private:
    QSettings& m_s;
};

}  // namespace appupdate
