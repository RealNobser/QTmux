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
#include <QUrlQuery>

#include <memory>

namespace appupdate {

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
    QNetworkRequest req(url);
    req.setAttribute(QNetworkRequest::CacheLoadControlAttribute,
                     QNetworkRequest::AlwaysNetwork);
    QNetworkReply* reply = m_nam.get(req);
    m_activeReply = reply;
    return reply;
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
        sigReply->deleteLater();
        if (sigReply->error() != QNetworkReply::NoError) {
            callback(std::nullopt, QStringLiteral("signature fetch failed: %1")
                                       .arg(sigReply->errorString()));
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
            manReply->deleteLater();
            if (manReply->error() != QNetworkReply::NoError) {
                callback(std::nullopt,
                         QStringLiteral("manifest fetch failed: %1")
                             .arg(manReply->errorString()));
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

    auto file = std::make_shared<QFile>(localPath);
    if (!file->open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        callback(QString(), QStringLiteral("cannot write %1").arg(localPath));
        return;
    }

    QNetworkReply* reply = get(url);
    connect(reply, &QNetworkReply::downloadProgress, this,
            &UpdateChecker::downloadProgress);
    // Stream to disk — installers are large, buffering them in RAM helps
    // nobody.
    connect(reply, &QNetworkReply::readyRead, this,
            [reply, file]() { file->write(reply->readAll()); });
    connect(reply, &QNetworkReply::finished, this,
            [reply, file, localPath, expectedSha = artifact.sha256,
             callback = std::move(callback)]() {
        reply->deleteLater();
        file->write(reply->readAll());
        file->close();

        if (reply->error() != QNetworkReply::NoError) {
            QFile::remove(localPath);
            callback(QString(), QStringLiteral("download failed: %1")
                                    .arg(reply->errorString()));
            return;
        }

        // Verify the manifest's SHA-256 against what actually landed on
        // disk. A mismatch deletes the file — a corrupt installer must not
        // survive to be double-clicked later.
        QFile readBack(localPath);
        QCryptographicHash hash(QCryptographicHash::Sha256);
        if (!readBack.open(QIODevice::ReadOnly) || !hash.addData(&readBack)) {
            QFile::remove(localPath);
            callback(QString(), QStringLiteral("cannot hash %1").arg(localPath));
            return;
        }
        const QString actual = QString::fromLatin1(hash.result().toHex());
        if (actual.compare(expectedSha, Qt::CaseInsensitive) != 0) {
            QFile::remove(localPath);
            callback(QString(),
                     QStringLiteral("sha256 mismatch (expected %1, got %2) — "
                                    "file deleted")
                         .arg(expectedSha, actual));
            return;
        }
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
