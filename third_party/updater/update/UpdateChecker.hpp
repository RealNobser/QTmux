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

#include "update/UpdateManifest.hpp"

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

    QNetworkAccessManager m_nam;
    QPointer<QNetworkReply> m_activeReply;
    QUrl m_baseUrl;
    QString m_product;
    QByteArray m_publicKey;
};

}  // namespace appupdate
