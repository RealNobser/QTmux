#pragma once

// appupdate::UpdateShowcase — puts a REAL appupdate::UpdateFlow into any
// state of the design, for picture runs and surface tests. FOR SELF-TESTS
// ONLY: nothing in an app's normal path constructs one.
//
// Qt Core and Network only, no widgets: a Widgets surface and a QML surface
// drive the same scenes. Built as its own library (MacPCANUpdaterShowcase in
// the hub); the update core does not pull it in, a product links it only on
// request. The old include path updateui/UpdateShowcase.hpp forwards here.
//
// How, without setting a single text:
//   * its own flow, its own QSettings on an INI file in its own temp dir —
//     the app's settings and the app's flow are never touched;
//   * a file tree signed AT RUNTIME with a throwaway Ed25519 key (generated
//     from a fixed seed, never a real key) and handed to the flow through the
//     test doors setPublicKeyForTests / setBaseUrlForTests;
//   * a small local HTTP server on 127.0.0.1 that can hang (state 1), stall a
//     download at 39 % (state 4), drop it (12) or ask a proxy password (15);
//   * a stand-in InstallBackend that answers capability and never launches
//     anything;
//   * the marker of the previous run (states 6 and 13) through
//     UpdateSettings, exactly as the flow itself writes it.
//
// No picture may show internal paths, machine or person names: the only
// names here are the product's and the neutral sample user "example-user".
// Every server and proxy address is the literal 127.0.0.1 the scene servers
// listen on, never a host name: a name may resolve to the IPv6 loopback
// first, where nothing listens (a hosted macOS runner never reached scene 15
// that way). Guard: UpdateShowcaseCore.TheSourcesNameNoHostThatCouldMeanIPv6.

#include "update/UpdateFlow.hpp"

#include <QObject>
#include <QString>
#include <QStringList>

#include <functional>
#include <memory>
#include <vector>

class QSettings;
class QTemporaryDir;

namespace appupdate {

class ShowcaseServer;

class UpdateShowcase : public QObject {
    Q_OBJECT

public:
    struct Options {
        QString productSlug = QStringLiteral("demo");
        QString productName = QStringLiteral("Demo");
        QString installed = QStringLiteral("0.7.0");   // pure version
        QString buildSuffix = QStringLiteral("+4db7212");
        QString language = QStringLiteral("en");
        appupdate::UpdateFlow::QuitImpact impact;
    };

    explicit UpdateShowcase(Options options, QObject* parent = nullptr);
    ~UpdateShowcase() override;

    // Scene names: "1", "1-slow", "2", "2-notes", "3", "3-long", "4", "5",
    // "6", "7", "8", "9", "10", "10-details", "11", "12", "13", "14",
    // "14-shared", "14-filesystem" (the two long reasons), "15",
    // "15-rejected", "hint-avail", "hint-loading", "hint-ready",
    // "hint-done", "hint-failed", "settings-system", "settings-manual".
    [[nodiscard]] static QStringList scenes();

    // Drives the flow into `scene`; `done(ok)` is called once the flow is
    // there (asynchronous: network on 127.0.0.1, file:// never). Returns
    // false for an unknown scene.
    bool enter(const QString& scene, std::function<void(bool ok)> done);

    // The flow of the scene (valid after enter()).
    [[nodiscard]] appupdate::UpdateFlow* flow() const { return m_flow.get(); }
    // Whether the scene is shown with the dialog (otherwise: hint only, or
    // the settings section, or the proxy sign-in).
    [[nodiscard]] static QString surfaceFor(const QString& scene);

private:
    void makeFlow(const QString& installed);
    void publish(const QString& version, bool forThisSystem = true, bool badSignature = false,
                 const QString& notesEn = QString(), const QString& notesDe = QString());
    void writeMarker(const QString& from, const QString& to, bool failed);

    Options m_opt;
    std::unique_ptr<QTemporaryDir> m_dir;
    std::unique_ptr<QSettings> m_settings;
    std::unique_ptr<appupdate::UpdateFlow> m_flow;
    std::unique_ptr<ShowcaseServer> m_server;
    std::vector<std::shared_ptr<ShowcaseServer>> m_extraServers;  // proxies of 1-slow, 15
    QByteArray m_payload;
    bool m_manualInstall = false;
    appupdate::ManualReason m_manualReason = appupdate::ManualReason::LocationNotWritable;
};

}  // namespace appupdate
