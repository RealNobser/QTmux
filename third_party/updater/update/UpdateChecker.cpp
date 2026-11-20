#include "update/UpdateChecker.hpp"

#include <QPointer>
#include <QTimer>

#include "update/Ed25519Verify.hpp"
#include "update/ErrorText.hpp"
#include "update/UpdateKeys.hpp"
#include "update/UpdatePolicy.hpp"

#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMetaEnum>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRandomGenerator>
#include <QRegularExpression>
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
#include <fcntl.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace appupdate {

namespace {

// How old (last write) a <name>.XXXXXX.part must be before a later download
// of the same name may take it for a remnant (sweepStaleParts). Not the
// proof that nobody uses it — that is the lock — but the cover for the one
// instant without a lock: between NewOnly-create and flock() of another
// instance, microseconds. Ten minutes are far beyond that and still clear a
// crash's remnant at the next attempt that is not within the same minutes.
constexpr qint64 kStalePartAgeSeconds = 10 * 60;

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
//
// While it lives it is marked IN USE for sweepStaleParts() of every other
// instance: POSIX holds an flock() on a second descriptor from creation
// until keep()/discard() — across close(), hashing and rename, and the
// kernel drops it with the process, however that ends. Windows needs no
// extra lock: Qt opens without FILE_SHARE_DELETE, so nobody can delete the
// file while the download writes it.
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
            m_lastError = fileErrorName(m_file.error());
        }
#if !defined(Q_OS_WIN)
        if (m_file.isOpen()) {
            m_lockFd = ::open(QFile::encodeName(m_file.fileName()).constData(),
                              O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
            if (m_lockFd < 0 || ::flock(m_lockFd, LOCK_EX | LOCK_NB) != 0) {
                qWarning("update: no lock on %s (%s) — after %d min of silence "
                         "another instance would take it for a remnant",
                         qPrintable(m_file.fileName()), qPrintable(qt_error_string(errno)),
                         int(kStalePartAgeSeconds / 60));
            }
        }
#endif
    }
    ~PartFile()
    {
        if (!m_kept) discard();
        unlock();
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
        unlock();
    }
    void keep()
    {
        m_kept = true;
        unlock();
    }

private:
    void unlock()
    {
#if !defined(Q_OS_WIN)
        if (m_lockFd >= 0) ::close(m_lockFd);  // releases the flock
        m_lockFd = -1;
#endif
    }

    QFile m_file;
    QString m_lastError;
    bool m_kept = false;
#if !defined(Q_OS_WIN)
    int m_lockFd = -1;
#endif
};

// Remnants of an earlier download of the SAME name in the SAME folder
// (2026-10-16, EmbyStudio EMB-283): an app that dies between creating its
// <name>.XXXXXX.part and renaming it (SIGKILL, crash, power) leaves the part
// behind — an update DMG is 30–40 MB. The next download of that name clears
// them, and only what is provably such a remnant:
//   - the hub's exact pattern <name>.<6 of [A-Za-z0-9]>.part, nothing else;
//   - a regular file — no symlink, no junction, no folder;
//   - last written more than kStalePartAgeSeconds ago. A live download
//     writes it all the time; the age only covers the instant between
//     creating and locking (below) and must not let a sweep delete a part
//     another instance has just created;
//   - not in use: POSIX takes the flock() a living PartFile holds (a
//     stalled download keeps an OLD part locked — no timeout in this
//     downloader, so age alone proves nothing); Windows: a file a download
//     writes cannot be deleted (no FILE_SHARE_DELETE), the removal fails
//     and the file stays.
// Every removal is logged with qWarning, and so is every remnant that stays
// for any other reason than its age.
// Guard: UpdateDownloadReplace.ARemnant* / AStalledDownload* (both run on
// every platform).
void sweepStaleParts(const QString& destDir, const QString& fileName)
{
    const QRegularExpression pattern(
        QStringLiteral("\\A%1\\.[A-Za-z0-9]{6}\\.part\\z")
            .arg(QRegularExpression::escape(fileName)));
    const QDateTime now = QDateTime::currentDateTime();
    const QFileInfoList candidates = QDir(destDir).entryInfoList(
        {QStringLiteral("*.part")},
        QDir::Files | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot);
    for (const QFileInfo& fi : candidates) {
        if (!pattern.match(fi.fileName()).hasMatch()) continue;
        const QString path = fi.absoluteFilePath();
        if (fi.isSymLink() || fi.isJunction() || !fi.isFile()) {
            qWarning("update: %s matches a download part but is no regular "
                     "file — left alone", qPrintable(path));
            continue;
        }
        const QDateTime written = fi.lastModified();
        if (!written.isValid() || written.secsTo(now) <= kStalePartAgeSeconds) {
            continue;  // young: possibly another instance's download
        }
#if defined(Q_OS_WIN)
        QFile file(path);
        if (!file.remove()) {
            qWarning("update: download remnant %s stays (%s) — another "
                     "download may still write it",
                     qPrintable(path), qPrintable(file.errorString()));
            continue;
        }
#else
        const QByteArray native = QFile::encodeName(path);
        const int fd = ::open(native.constData(),
                              O_RDONLY | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK);
        if (fd < 0) {
            qWarning("update: download remnant %s stays (cannot open: %s)",
                     qPrintable(path), qPrintable(qt_error_string(errno)));
            continue;
        }
        struct stat st {};
        if (::fstat(fd, &st) != 0 || !S_ISREG(st.st_mode)) {
            ::close(fd);
            qWarning("update: %s is no regular file — left alone", qPrintable(path));
            continue;
        }
        if (::flock(fd, LOCK_EX | LOCK_NB) != 0) {
            const int err = errno;
            ::close(fd);
            if (err != EWOULDBLOCK) {
                qWarning("update: download remnant %s stays (no lock: %s — "
                         "cannot prove it unused)",
                         qPrintable(path), qPrintable(qt_error_string(err)));
            }
            continue;  // EWOULDBLOCK: a living download holds it
        }
        // Holding the lock: no download can own it until it is gone.
        const bool removed = ::unlink(native.constData()) == 0;
        const int err = errno;
        ::close(fd);
        if (!removed) {
            qWarning("update: download remnant %s stays (%s)",
                     qPrintable(path), qPrintable(qt_error_string(err)));
            continue;
        }
#endif
        qWarning("update: removed download remnant %s (%lld bytes, last "
                 "written %s)",
                 qPrintable(path), static_cast<long long>(fi.size()),
                 qPrintable(written.toUTC().toString(Qt::ISODate)));
    }
}

}  // namespace

UpdateChecker::UpdateChecker(const QUrl& baseUrl, const QString& product,
                             const QByteArray& publicKey32, QObject* parent)
    : QObject(parent)
    , m_baseUrl(baseUrl)
    , m_product(product)
    , m_publicKey(publicKey32)
    , m_channel(QString::fromLatin1(policy::kDefaultChannel))
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

// "<step>: <NetworkError name>[ (HTTP <status>)]" - the same in every
// language, without the URL (it may carry user info) and without a path.
// QNetworkReply::errorString() is translated by Qt's catalogue (ErrorText.hpp).
QString networkErrorText(QNetworkReply* reply, const QString& what)
{
    const QNetworkReply::NetworkError e = reply->error();
    QString name = QString::fromLatin1(
        QMetaEnum::fromType<QNetworkReply::NetworkError>().valueToKey(static_cast<int>(e)));
    if (name.isEmpty()) name = QStringLiteral("NetworkError(%1)").arg(static_cast<int>(e));
    QString text = what + QStringLiteral(": ") + name;
    const int http = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (http > 0) text += QStringLiteral(" (HTTP %1)").arg(http);
    return text;
}

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
        return {EK::Network, networkErrorText(reply, what)};
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

bool UpdateChecker::proxyInUse() const
{
    const QNetworkProxy::ProxyType t = m_nam.proxy().type();
    return t != QNetworkProxy::NoProxy && t != QNetworkProxy::DefaultProxy;
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

bool UpdateChecker::manualProxyIncomplete(const QUrl& target) const
{
    // Only the network uses a proxy; a file:// tree is read either way.
    const QString scheme = target.scheme().toLower();
    if (scheme != QLatin1String("http") && scheme != QLatin1String("https")) return false;
    return m_proxy.mode == ProxyConfig::Mode::Manual && !isUsable(m_proxy);
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
    // The stall deadline of a running download rests while the provider
    // runs — an app may ask the user here (with a nested event loop, even).
    // Guarded: in that nested loop the reply may be aborted and deleted, and
    // the timer, its child, with it.
    QPointer<QTimer> stall = m_activeReply
        ? m_activeReply->findChild<QTimer*>(QStringLiteral("appupdateStallTimer")) : nullptr;
    const bool stallWasActive = stall && stall->isActive();
    if (stall) stall->stop();
    const bool answered = m_credentialProvider(proxy.hostName(), proxy.port(),
                                               /*previousAttemptFailed*/ m_authAttempts > 1,
                                               &user, &password);
    if (stall && stallWasActive) stall->start();
    if (!answered) {
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

void UpdateChecker::setExpectedChannel(const QString& channel)
{
    m_channel = channel;
}

void UpdateChecker::setSerialStore(SerialStore store)
{
    m_serialStore = std::move(store);
}

void UpdateChecker::checkForUpdate(CheckCallback callback)
{
    if (busy()) {
        callback(std::nullopt, QStringLiteral("a request is already running"));
        return;
    }
    // Every check answers for itself. Before contract V1 the kind was only
    // ever SET on transport failures, so a signature or parse failure kept
    // whatever the previous request had left behind.
    m_lastErrorKind = ErrorKind::None;
    if (manualProxyIncomplete(productUrl(QStringLiteral("manifest.json.sig")))) {
        m_lastErrorKind = ErrorKind::ProxyIncomplete;
        callback(std::nullopt, QStringLiteral("manual proxy incomplete: host or port missing"));
        return;
    }
    if (!hasSerialStore() && !m_warnedNoSerialStore) {
        m_warnedNoSerialStore = true;
        qWarning("update: %s is checked without a serial store — no replay "
                 "protection (docs/SHARED.md „Update-Härtung“)",
                 qPrintable(m_product));
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
            m_lastErrorKind = ErrorKind::Signature;
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
                m_lastErrorKind = ErrorKind::Signature;
                callback(std::nullopt,
                         QStringLiteral("manifest signature verification failed"));
                return;
            }
            QString parseError;
            auto manifest = UpdateManifest::fromJson(bytes, &parseError);
            if (!manifest) {
                m_lastErrorKind = ErrorKind::Parse;
                callback(std::nullopt, parseError);
                return;
            }

            // Contract V1, rule A2.1: a signature proves the OWNER wrote the
            // manifest — not that it is the one for THIS product and channel.
            // Every product is signed with the same key, so without this a
            // manifest copied from another product's directory would be
            // offered as an update here.
            const QString mismatch =
                policy::productMismatch(*manifest, m_product, m_channel);
            if (!mismatch.isEmpty()) {
                m_lastErrorKind = ErrorKind::WrongProduct;
                callback(std::nullopt, mismatch);
                return;
            }

            // Rule A2.2: an old manifest, validly signed, must not come back.
            const quint32 last =
                hasSerialStore() ? m_serialStore.load(m_product) : 0;
            switch (policy::checkSerial(last, manifest->serial)) {
            case policy::SerialVerdict::ReplayWithoutSerial:
                m_lastErrorKind = ErrorKind::Replay;
                callback(std::nullopt,
                         QStringLiteral("manifest carries no serial, but serial "
                                        "%1 was accepted before — refusing an "
                                        "outdated manifest (replay)").arg(last));
                return;
            case policy::SerialVerdict::ReplayOlderSerial:
                m_lastErrorKind = ErrorKind::Replay;
                callback(std::nullopt,
                         QStringLiteral("manifest serial %1 is older than the "
                                        "accepted %2 — refusing an outdated "
                                        "manifest (replay)")
                             .arg(*manifest->serial).arg(last));
                return;
            case policy::SerialVerdict::Accept:
                break;
            }
            // Stored now, before the callback, whether or not anything gets
            // installed: an accepted serial is a fact about the server.
            const quint32 next = policy::nextLastSerial(last, manifest->serial);
            if (hasSerialStore() && next != last) {
                m_serialStore.store(m_product, next);
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
    m_lastErrorKind = ErrorKind::None;
    // Contract V1, rule A3: `size` is the bound, so without one there is
    // nothing to hold the server to — refused before a byte is fetched.
    if (artifact.size <= 0) {
        m_lastErrorKind = ErrorKind::Size;
        callback(QString(), QStringLiteral("the manifest gives no size for this "
                                           "artifact — refusing an unbounded "
                                           "download"));
        return;
    }
    if (!QDir().mkpath(destDir)) {
        m_lastErrorKind = ErrorKind::File;
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
        m_lastErrorKind = ErrorKind::Parse;
        callback(QString(), QStringLiteral("artifact url carries no file name"));
        return;
    }
    if (manualProxyIncomplete(url)) {
        m_lastErrorKind = ErrorKind::ProxyIncomplete;
        callback(QString(), QStringLiteral("manual proxy incomplete: host or port missing"));
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
    sweepStaleParts(destDir, fileName);  // remnants of a hard end, see there
    auto part = std::make_shared<PartFile>(localPath);
    if (!part->isOpen()) {
        m_lastErrorKind = ErrorKind::File;
        callback(QString(), QStringLiteral("cannot write %1.XXXXXX.part: %2")
                                .arg(QFileInfo(localPath).fileName(), part->lastError()));
        return;
    }

    // Rule A3: never more than `size` bytes reach the disk. Counted here,
    // not taken from Content-Length or downloadProgress — the server's own
    // claim is exactly what is not trusted, and Qt throttles the progress
    // signal. The chunk that would cross the bound is not written; the
    // reply is aborted on the spot, so an endless body costs at most one
    // read buffer, not the disk.
    struct Budget {
        qint64 limit = 0;
        qint64 received = 0;
        bool exceeded = false;
        bool stalled = false;
    };
    auto budget = std::make_shared<Budget>();
    budget->limit = artifact.size;
    m_lastDownloadReceived = 0;
    QNetworkReply* reply = get(url);
    // Stall deadline: a child of the reply, so it lives exactly as long as
    // this download. Restarted by every chunk (drain), paused around the
    // proxy credential callback (onProxyAuthRequired).
    QTimer* stall = nullptr;
    if (m_downloadStallTimeoutMs > 0) {
        stall = new QTimer(reply);
        stall->setObjectName(QStringLiteral("appupdateStallTimer"));
        stall->setSingleShot(true);
        stall->setInterval(m_downloadStallTimeoutMs);
        connect(stall, &QTimer::timeout, reply, [reply, budget]() {
            budget->stalled = true;
            reply->abort();
        });
        stall->start();
    }
    const auto drain = [reply, part, budget, stall]() {
        if (budget->exceeded || budget->stalled) {
            reply->readAll();  // discard; abort is already under way
            return;
        }
        const QByteArray chunk = reply->readAll();
        if (budget->received + chunk.size() > budget->limit) {
            budget->exceeded = true;
            budget->received += chunk.size();
            reply->abort();
            return;
        }
        budget->received += chunk.size();
        part->file().write(chunk);
        if (stall && !chunk.isEmpty()) stall->start();  // data came: the clock restarts
    };
    connect(reply, &QNetworkReply::downloadProgress, this,
            &UpdateChecker::downloadProgress);
    // Stream to disk — installers are large, buffering them in RAM helps
    // nobody.
    connect(reply, &QNetworkReply::readyRead, this, drain);
    connect(reply, &QNetworkReply::finished, this,
            [this, reply, part, budget, drain, localPath,
             expectedSha = artifact.sha256, callback = std::move(callback)]() {
        finishActive(reply);
        QFile& file = part->file();
        drain();
        const bool written = file.flush() && file.error() == QFile::NoError;
        const QString writeError = fileErrorName(file.error());
        const QString partPath = part->path();
        file.close();  // really closed: Windows renames no open file

        // Before the transport error: the abort above IS a transport error
        // (OperationCanceled), and reporting it as "download failed" would
        // hide that the server overstepped the manifest.
        if (QTimer* t = reply->findChild<QTimer*>(QStringLiteral("appupdateStallTimer"))) t->stop();
        m_lastDownloadReceived = budget->received;
        if (budget->stalled && !budget->exceeded) {
            // Before the transport error for the same reason as the size
            // bound: the abort above reads as OperationCanceled otherwise.
            part->discard();
            m_lastErrorKind = ErrorKind::Network;
            callback(QString(),
                     QStringLiteral("download stalled: no data for %1 ms after %2 bytes — "
                                    "aborted, file deleted")
                         .arg(m_downloadStallTimeoutMs).arg(budget->received));
            return;
        }
        if (budget->exceeded) {
            part->discard();
            m_lastErrorKind = ErrorKind::Size;
            callback(QString(),
                     QStringLiteral("the server sent more than the %1 bytes the "
                                    "manifest declares (at least %2) — download "
                                    "aborted, file deleted")
                         .arg(budget->limit).arg(budget->received));
            return;
        }
        if (reply->error() != QNetworkReply::NoError) {
            part->discard();
            m_lastErrorKind = classifyFailure(reply, QStringLiteral("download")).kind;
            if (m_lastErrorKind == ErrorKind::NotPublished) {
                m_lastErrorKind = ErrorKind::Http;  // a listed artifact is missing
            }
            callback(QString(), QStringLiteral("download failed: %1")
                                    .arg(networkErrorText(reply, QStringLiteral("download"))));
            return;
        }
        if (!written) {
            part->discard();
            m_lastErrorKind = ErrorKind::File;
            // The file name only - a local path stays out of "Details".
            callback(QString(), QStringLiteral("cannot write %1: %2")
                                    .arg(QFileInfo(partPath).fileName(), writeError));
            return;
        }
        if (budget->received < budget->limit) {
            part->discard();
            m_lastErrorKind = ErrorKind::Size;
            callback(QString(),
                     QStringLiteral("the server sent %1 of the %2 bytes the "
                                    "manifest declares — file deleted")
                         .arg(budget->received).arg(budget->limit));
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
            m_lastErrorKind = ErrorKind::File;
            callback(QString(), QStringLiteral("cannot hash %1").arg(partPath));
            return;
        }
        const QString actual = QString::fromLatin1(digest);
        if (actual.compare(expectedSha, Qt::CaseInsensitive) != 0) {
            part->discard();
            m_lastErrorKind = ErrorKind::Hash;
            callback(QString(),
                     QStringLiteral("sha256 mismatch (expected %1, got %2) — "
                                    "file deleted")
                         .arg(expectedSha, actual));
            return;
        }
        QString why;
        if (!replaceName(partPath, localPath, &why)) {
            part->discard();
            m_lastErrorKind = ErrorKind::File;
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
