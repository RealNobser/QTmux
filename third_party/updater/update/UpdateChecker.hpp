#pragma once

// appupdate::UpdateChecker — thin QNetworkAccessManager wrapper (modelled on
// RAFTNG's raftng::ai::AiClient): one in-flight request at a time, abort()
// cancels it, all callbacks run on the owner's thread via QNetworkReply
// signals.
//
// Check flow: GET <base>/<product>/manifest.json.sig (64 raw bytes), then
// GET <base>/<product>/manifest.json, verify the Ed25519 signature over the
// EXACT manifest bytes BEFORE parsing, then parse. http(s) fetches are
// cache-busted (?ts=<epoch> + AlwaysNetwork) because manifest.json is the
// one mutable file on the webspace; file:// URLs are left untouched, which
// is what makes fixture-based dry-runs free.
//
// Download flow: streams the artifact to <destDir>/<filename>, emits
// progress, and verifies the SHA-256 from the manifest on completion — a
// mismatch deletes the file and reports an error, it never hands the caller
// a corrupt installer.

#include "update/ProxyConfig.hpp"
#include "update/UpdateManifest.hpp"

#include <QAuthenticator>
#include <QByteArray>
#include <QNetworkAccessManager>
#include <QObject>
#include <QPointer>
#include <QUrl>

#include <functional>
#include <optional>

class QFile;
class QNetworkReply;

namespace appupdate {

class UpdateChecker : public QObject {
    Q_OBJECT

public:
    // `publicKey32`: raw 32-byte Ed25519 public key. Empty selects the
    // built-in production key (UpdateKeys.hpp); tests inject their fixture
    // key here.
    explicit UpdateChecker(const QUrl& baseUrl, const QString& product,
                           const QByteArray& publicKey32 = QByteArray(),
                           QObject* parent = nullptr);
    ~UpdateChecker() override;

    using CheckCallback =
        std::function<void(std::optional<UpdateManifest>, QString error)>;
    void checkForUpdate(CheckCallback callback);

    using DownloadCallback =
        std::function<void(QString localPath, QString error)>;
    void downloadArtifact(const UpdateArtifact& artifact,
                          const QString& destDir, DownloadCallback callback);

    // ---- Proxy support (MAC-36) -------------------------------------------
    // Applies to BOTH network paths — manifest fetch and artifact download.
    // Set per instance, never through QNetworkProxyFactory's process-wide
    // switch: this library lives inside three applications, and a global
    // side effect from a vendored lib would hit RAFTNG's AiClient and
    // QTmux's own network paths too.
    void setProxyConfig(const ProxyConfig& cfg);
    [[nodiscard]] ProxyConfig proxyConfig() const { return m_proxy; }

    // Asked when the proxy demands authentication. Return true after
    // filling user/password, false to give up (which ends the request with
    // ErrorKind::ProxyAuthenticationRequired instead of retrying forever).
    // `previousAttemptFailed` is true from the second challenge onwards, so
    // an app can say "wrong password" rather than asking again blankly.
    //
    // The library never stores what it receives here beyond the request,
    // and never writes it anywhere. Whether the app keeps it in session
    // memory or a keychain is the app's decision.
    using ProxyCredentialProvider = std::function<bool(
        const QString& proxyHost, quint16 proxyPort,
        bool previousAttemptFailed,
        QString* user, QString* password)>;
    void setProxyCredentialProvider(ProxyCredentialProvider provider);

    // Why the last operation failed. Additive: the existing QString error
    // in both callbacks is unchanged, so RAFTNG and QTmux keep compiling
    // and can adopt this at their own pace.
    enum class ErrorKind : std::uint8_t {
        None = 0,
        Network,      // transport failed for a non-proxy reason
        Http,         // server answered, but not with 200
        Signature,    // Ed25519 over the manifest bytes did not verify
        Parse,        // manifest unreadable
        Hash,         // artifact SHA-256 mismatch
        File,         // could not write the download
        // Proxy-specific, so a misconfigured proxy never reads as
        // "server unreachable" — the operator would search in the wrong
        // place, which is exactly what these exist to prevent.
        ProxyAuthenticationRequired,  // challenged, no credentials supplied
        ProxyAuthenticationFailed,    // credentials supplied and rejected
        ProxyUnreachable,             // proxy itself did not answer
        ProxyResolutionFailed,        // system proxy could not be resolved
    };
    [[nodiscard]] ErrorKind lastErrorKind() const { return m_lastErrorKind; }

    [[nodiscard]] bool busy() const { return !m_activeReply.isNull(); }
    void abort();

signals:
    void downloadProgress(qint64 receivedBytes, qint64 totalBytes);

private:
    QNetworkReply* get(const QUrl& url);
    // Clears the in-flight marker and schedules the reply for deletion. Call
    // this BEFORE invoking a user callback — see the definition for why.
    void finishActive(QNetworkReply* reply);
    [[nodiscard]] QUrl productUrl(const QString& fileName) const;

    // Applies m_proxy to m_nam. For Mode::System this runs the blocking
    // resolution on a worker thread with a timeout (see ProxyConfig.hpp)
    // and caches the outcome for the lifetime of this checker, so the
    // 64-second worst case can be paid at most once.
    void ensureProxyApplied(const QUrl& target);
    void onProxyAuthRequired(const QNetworkProxy& proxy, QAuthenticator* auth);

    QNetworkAccessManager m_nam;
    QPointer<QNetworkReply> m_activeReply;
    QUrl m_baseUrl;
    QString m_product;
    QByteArray m_publicKey;

    ProxyConfig m_proxy;
    ProxyCredentialProvider m_credentialProvider;
    ErrorKind m_lastErrorKind = ErrorKind::None;
    bool m_proxyResolved = false;      // System mode resolved this session
    int  m_authAttempts  = 0;          // guards the 407 loop
    static constexpr int kMaxAuthAttempts = 3;
};

}  // namespace appupdate
