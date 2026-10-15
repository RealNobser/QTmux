#include "update/UpdateChecker.hpp"

#include "update/Ed25519Verify.hpp"
#include "update/UpdateKeys.hpp"

#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRandomGenerator>
#include <QUrlQuery>

#include <memory>

#if defined(Q_OS_WIN)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <cerrno>
#include <cstdio>
#endif

namespace appupdate {

namespace {

// Puts the finished temp file in place of `finalPath` in ONE step that
// replaces the NAME — whatever lies there: a regular file (its inode, and
// with it every extended attribute, goes away with it) or a symlink (the
// link is replaced, its target is never touched). On Windows a file another
// process holds open without FILE_SHARE_DELETE cannot be replaced: that is a
// failure with the system's reason, never a silent success.
//
// Why not QSaveFile, measured 2026-10-15 (docs/HISTORY.md „Downloader
// ersetzt statt zu überschreiben“): it resolves a symlink at the target name
// and replaces the link's TARGET — a link in the download folder would let
// an update write a file anywhere the user can. QFile::rename() refuses an
// existing name, and remove-then-rename would destroy the old file first.
bool replaceName(const QString& tempPath, const QString& finalPath,
                 QString* why)
{
#if defined(Q_OS_WIN)
    const QString from = QDir::toNativeSeparators(tempPath);
    const QString to = QDir::toNativeSeparators(finalPath);
    if (::MoveFileExW(reinterpret_cast<const wchar_t*>(from.utf16()),
                      reinterpret_cast<const wchar_t*>(to.utf16()),
                      MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        return true;
    }
    *why = qt_error_string(static_cast<int>(::GetLastError()));
    return false;
#else
    if (std::rename(QFile::encodeName(tempPath).constData(),
                    QFile::encodeName(finalPath).constData()) == 0) {
        return true;
    }
    *why = qt_error_string(errno);
    return false;
#endif
}

// The download's own temp file <name>.XXXXXX.part next to the target. Not a
// QTemporaryFile: its close() keeps the native handle open until the object
// dies (Qt reserves the name that way), and Windows refuses to rename a file
// with an open handle — every download failed there with a sharing
// violation (CI 38782128012, 2026-10-15; POSIX does not mind). This one is
// created exclusively (NewOnly = O_EXCL / CREATE_NEW), close() really closes,
// and it removes itself unless the download took its name.
class PartFile {
public:
    explicit PartFile(const QString& finalPath)
    {
        static const char kChars[] =
            "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
        for (int attempt = 0; attempt < 16 && !m_file.isOpen(); ++attempt) {
            QString tag;
            for (int i = 0; i < 6; ++i) {
                tag += QLatin1Char(kChars[QRandomGenerator::global()->bounded(62)]);
            }
            m_file.setFileName(finalPath + QLatin1Char('.') + tag
                               + QStringLiteral(".part"));
            if (m_file.open(QIODevice::WriteOnly | QIODevice::NewOnly)) break;
            m_lastError = m_file.errorString();
        }
    }
    ~PartFile()
    {
        if (!m_kept) discard();
    }
    PartFile(const PartFile&) = delete;
    PartFile& operator=(const PartFile&) = delete;

    bool isOpen() const { return m_file.isOpen(); }
    QFile& file() { return m_file; }
    QString path() const { return m_file.fileName(); }
    QString lastError() const { return m_lastError; }
    void discard()
    {
        m_file.close();
        if (!m_file.fileName().isEmpty()) QFile::remove(m_file.fileName());
    }
    void keep() { m_kept = true; }

private:
    QFile m_file;
    QString m_lastError;
    bool m_kept = false;
};

}  // namespace

UpdateChecker::UpdateChecker(const QUrl& baseUrl, const QString& product,
                             const QByteArray& publicKey32, QObject* parent)
    : QObject(parent)
    , m_baseUrl(baseUrl)
    , m_product(product)
    , m_publicKey(publicKey32)
{
    if (m_publicKey.isEmpty()) {
        m_publicKey = QByteArray(
            reinterpret_cast<const char*>(kUpdatePublicKey),
            static_cast<int>(sizeof(kUpdatePublicKey)));
    }
}

UpdateChecker::~UpdateChecker()
{
    abort();
}

QUrl UpdateChecker::productUrl(const QString& fileName) const
{
    QString base = m_baseUrl.toString();
    while (base.endsWith(QLatin1Char('/'))) {
        base.chop(1);
    }
    QUrl url(base + QLatin1Char('/') + m_product + QLatin1Char('/') + fileName);
    // Cache-bust only real network fetches — manifest.json is the single
    // mutable file behind possibly caching proxies. file:// must stay
    // query-free or the path lookup fails.
    const QString scheme = url.scheme().toLower();
    if (scheme == QLatin1String("http") || scheme == QLatin1String("https")) {
        QUrlQuery q(url);
        q.addQueryItem(QStringLiteral("ts"),
                       QString::number(QDateTime::currentSecsSinceEpoch()));
        url.setQuery(q);
    }
    return url;
}

QNetworkReply* UpdateChecker::get(const QUrl& url)
{
    ensureProxyApplied(url);
    QNetworkRequest req(url);
    req.setAttribute(QNetworkRequest::CacheLoadControlAttribute,
                     QNetworkRequest::AlwaysNetwork);
    QNetworkReply* reply = m_nam.get(req);
    m_activeReply = reply;
    return reply;
}

// ---- Proxy (MAC-36) --------------------------------------------------------


namespace {
// Maps a transport failure onto what the operator should DO about it.
// Lumping them together produced "signature fetch failed: ... status code
// 404" for a product that simply had no release yet — a sentence that sends
// someone hunting for a signature problem that does not exist (found
// 2026-08-06 on RAFTNG, same checker, same three call sites).
struct FailureInfo { UpdateChecker::ErrorKind kind; QString text; };

FailureInfo classifyFailure(QNetworkReply* reply, const QString& what)
{
    using EK = UpdateChecker::ErrorKind;
    switch (reply->error()) {
    case QNetworkReply::ContentNotFoundError:
        return {EK::NotPublished,
                QStringLiteral("no release published for this product yet")};
    case QNetworkReply::ProxyAuthenticationRequiredError:
        return {EK::ProxyAuthenticationRequired,
                QStringLiteral("the proxy requires authentication")};
    case QNetworkReply::ProxyConnectionRefusedError:
    case QNetworkReply::ProxyConnectionClosedError:
    case QNetworkReply::ProxyNotFoundError:
    case QNetworkReply::ProxyTimeoutError:
        return {EK::ProxyUnreachable,
                QStringLiteral("the configured proxy did not answer")};
    case QNetworkReply::ContentAccessDenied:
    case QNetworkReply::ContentOperationNotPermittedError:
    case QNetworkReply::AuthenticationRequiredError:
        return {EK::Http, QStringLiteral("%1: access denied").arg(what)};
    default:
        return {EK::Network, QStringLiteral("%1: %2").arg(what, reply->errorString())};
    }
}
}  // namespace

void UpdateChecker::setProxyConfig(const ProxyConfig& cfg)
{
    if (cfg == m_proxy) return;
    m_proxy = cfg;
    m_proxyResolved = false;   // re-resolve on the next request
    m_authAttempts  = 0;
}

void UpdateChecker::setProxyCredentialProvider(ProxyCredentialProvider provider)
{
    m_credentialProvider = std::move(provider);
    // The signal is connected lazily and exactly once: without a provider
    // there is nothing sensible to answer a challenge with, and Qt would
    // otherwise retry against an empty authenticator.
    static const char* kTag = "proxyAuthWired";
    if (!property(kTag).toBool()) {
        connect(&m_nam, &QNetworkAccessManager::proxyAuthenticationRequired,
                this, &UpdateChecker::onProxyAuthRequired);
        setProperty(kTag, true);
    }
}

void UpdateChecker::ensureProxyApplied(const QUrl& target)
{
    m_authAttempts = 0;

    switch (m_proxy.mode) {
    case ProxyConfig::Mode::None:
        m_nam.setProxy(QNetworkProxy(QNetworkProxy::NoProxy));
        return;
    case ProxyConfig::Mode::Manual:
        m_nam.setProxy(toQNetworkProxy(m_proxy));
        return;
    case ProxyConfig::Mode::System:
        break;
    }

    // System mode. Resolve at most once per checker — the worst case is
    // 64 s of blocked OS call (measured), and paying it per request would
    // be unusable. resolveSystemProxy() bounds the WAIT, not the call.
    if (m_proxyResolved) return;
    m_proxyResolved = true;

    const ResolvedProxy r = resolveSystemProxy(target);
    switch (r.status) {
    case ProxyResolution::Proxy:
        m_nam.setProxy(r.proxy);
        break;
    case ProxyResolution::NoProxy:
        m_nam.setProxy(QNetworkProxy(QNetworkProxy::NoProxy));
        break;
    case ProxyResolution::TimedOut:
    case ProxyResolution::Failed:
        // Deliberately NOT silently direct: going direct here is what turns
        // a broken PAC into "server unreachable" three seconds later. Go
        // direct so the request can still succeed on a healthy network, but
        // record why, so a later failure can name the real cause.
        m_nam.setProxy(QNetworkProxy(QNetworkProxy::NoProxy));
        m_lastErrorKind = ErrorKind::ProxyResolutionFailed;
        break;
    }
}

void UpdateChecker::onProxyAuthRequired(const QNetworkProxy& proxy,
                                        QAuthenticator* auth)
{
    if (auth == nullptr) return;
    if (!m_credentialProvider) {
        // Nobody can answer — leave the authenticator empty so Qt fails the
        // request instead of looping, and name the reason.
        m_lastErrorKind = ErrorKind::ProxyAuthenticationRequired;
        return;
    }
    if (++m_authAttempts > kMaxAuthAttempts) {
        m_lastErrorKind = ErrorKind::ProxyAuthenticationFailed;
        return;
    }

    QString user = m_proxy.user;
    QString password;
    if (!m_credentialProvider(proxy.hostName(), proxy.port(),
                              /*previousAttemptFailed*/ m_authAttempts > 1,
                              &user, &password)) {
        m_lastErrorKind = ErrorKind::ProxyAuthenticationRequired;
        return;
    }
    auth->setUser(user);
    auth->setPassword(password);
    // Not stored, not logged, not written anywhere — it lives exactly as
    // long as this authenticator.
}

// Marks the in-flight request as finished. MUST run before any callback: the
// reply is only deleteLater()'d, so the QPointer stays non-null until the next
// event-loop turn — and busy() would still say "yes" while the caller is being
// told "done". A caller that starts the next request straight from the callback
// (check -> download is the obvious one, and the update dialog appears at
// exactly that moment) would be refused with "a request is already running".
void UpdateChecker::finishActive(QNetworkReply* reply)
{
    reply->deleteLater();
    if (m_activeReply == reply) {
        m_activeReply.clear();
    }
}

void UpdateChecker::checkForUpdate(CheckCallback callback)
{
    if (busy()) {
        callback(std::nullopt, QStringLiteral("a request is already running"));
        return;
    }

    // Step 1: the 64-byte detached signature.
    QNetworkReply* sigReply = get(productUrl(QStringLiteral("manifest.json.sig")));
    connect(sigReply, &QNetworkReply::finished, this,
            [this, sigReply, callback = std::move(callback)]() mutable {
        finishActive(sigReply);
        if (sigReply->error() != QNetworkReply::NoError) {
            const auto f = classifyFailure(sigReply, QStringLiteral("signature"));
            m_lastErrorKind = f.kind;
            callback(std::nullopt, f.text);
            return;
        }
        const QByteArray sig = sigReply->readAll();
        if (sig.size() != 64) {
            callback(std::nullopt,
                     QStringLiteral("signature has %1 bytes, expected 64")
                         .arg(sig.size()));
            return;
        }

        // Step 2: the manifest itself; verify over the exact bytes BEFORE
        // parsing — unsigned JSON never reaches the parser.
        QNetworkReply* manReply = get(productUrl(QStringLiteral("manifest.json")));
        connect(manReply, &QNetworkReply::finished, this,
                [this, manReply, sig, callback = std::move(callback)]() {
            finishActive(manReply);
            if (manReply->error() != QNetworkReply::NoError) {
                const auto f = classifyFailure(manReply, QStringLiteral("manifest"));
                m_lastErrorKind = f.kind;
                callback(std::nullopt, f.text);
                return;
            }
            const QByteArray bytes = manReply->readAll();
            if (!verifyEd25519(sig, m_publicKey, bytes)) {
                callback(std::nullopt,
                         QStringLiteral("manifest signature verification failed"));
                return;
            }
            QString parseError;
            auto manifest = UpdateManifest::fromJson(bytes, &parseError);
            if (!manifest) {
                callback(std::nullopt, parseError);
                return;
            }
            callback(std::move(manifest), QString());
        });
    });
}

void UpdateChecker::downloadArtifact(const UpdateArtifact& artifact,
                                     const QString& destDir,
                                     DownloadCallback callback)
{
    if (busy()) {
        callback(QString(), QStringLiteral("a request is already running"));
        return;
    }
    if (!QDir().mkpath(destDir)) {
        callback(QString(), QStringLiteral("cannot create %1").arg(destDir));
        return;
    }

    // Manifest URLs are absolute on the real webspace; relative ones are
    // resolved against <base>/<product>/ — which is what makes file://
    // fixture trees portable.
    QUrl url(artifact.url);
    if (url.isRelative()) {
        url = productUrl(artifact.url);
    }
    const QString fileName = QFileInfo(url.path()).fileName();
    if (fileName.isEmpty()) {
        callback(QString(), QStringLiteral("artifact url carries no file name"));
        return;
    }
    const QString localPath = destDir + QLatin1Char('/') + fileName;

    // Replace, never overwrite (docs/SHARED.md „Downloader: ersetzen, nie
    // überschreiben“). The bytes go to a temp file NEXT TO the target (same
    // file system, so the final step is a rename) and only a verified
    // download takes the name. Until then whatever lies at `localPath`
    // stays exactly as it was — an abort, a network error or a hash
    // mismatch leaves neither a half file nor a damaged old one. Writing
    // into the existing name (WriteOnly|Truncate, the old way) kept that
    // file's inode with a browser's com.apple.quarantine and wrote THROUGH a
    // symlink lying there (measured, EmbyStudio EMB-280).
    auto part = std::make_shared<PartFile>(localPath);
    if (!part->isOpen()) {
        callback(QString(), QStringLiteral("cannot write %1.XXXXXX.part: %2")
                                .arg(localPath, part->lastError()));
        return;
    }

    QNetworkReply* reply = get(url);
    connect(reply, &QNetworkReply::downloadProgress, this,
            &UpdateChecker::downloadProgress);
    // Stream to disk — installers are large, buffering them in RAM helps
    // nobody.
    connect(reply, &QNetworkReply::readyRead, this,
            [reply, part]() { part->file().write(reply->readAll()); });
    connect(reply, &QNetworkReply::finished, this,
            [this, reply, part, localPath, expectedSha = artifact.sha256,
             callback = std::move(callback)]() {
        finishActive(reply);
        QFile& file = part->file();
        file.write(reply->readAll());
        const bool written = file.flush() && file.error() == QFile::NoError;
        const QString writeError = file.errorString();
        const QString partPath = part->path();
        file.close();  // really closed: Windows renames no open file

        if (reply->error() != QNetworkReply::NoError) {
            part->discard();
            callback(QString(), QStringLiteral("download failed: %1")
                                    .arg(reply->errorString()));
            return;
        }
        if (!written) {
            part->discard();
            callback(QString(), QStringLiteral("cannot write %1: %2")
                                    .arg(partPath, writeError));
            return;
        }

        // Verify the manifest's SHA-256 against what actually landed on
        // disk. A mismatch deletes the file — a corrupt installer must not
        // survive to be double-clicked later.
        //
        // The read-back handle is CLOSED before any remove(): Windows refuses
        // to unlink a file that is still open, so leaving the QFile alive
        // until the end of the lambda made the deletion a silent no-op there —
        // the caller got "file deleted" while the corrupt installer sat in
        // Downloads. Found by QTmux's Windows build (its vendored copy of this
        // file); POSIX hid it because unlinking an open file works.
        bool hashed = false;
        QByteArray digest;
        {
            QFile readBack(partPath);
            QCryptographicHash hash(QCryptographicHash::Sha256);
            hashed = readBack.open(QIODevice::ReadOnly) && hash.addData(&readBack);
            if (hashed) {
                digest = hash.result().toHex();
            }
        }
        if (!hashed) {
            part->discard();
            callback(QString(), QStringLiteral("cannot hash %1").arg(partPath));
            return;
        }
        const QString actual = QString::fromLatin1(digest);
        if (actual.compare(expectedSha, Qt::CaseInsensitive) != 0) {
            part->discard();
            callback(QString(),
                     QStringLiteral("sha256 mismatch (expected %1, got %2) — "
                                    "file deleted")
                         .arg(expectedSha, actual));
            return;
        }
        QString why;
        if (!replaceName(partPath, localPath, &why)) {
            part->discard();
            callback(QString(), QStringLiteral("cannot replace %1: %2")
                                    .arg(localPath, why));
            return;
        }
        part->keep();  // its name is the download's now
        callback(localPath, QString());
    });
}

void UpdateChecker::abort()
{
    if (m_activeReply) {
        m_activeReply->abort();
    }
}

}  // namespace appupdate
