#include "update/UpdateShowcase.hpp"

#include "update/InstallBackend.hpp"
#include "update/UpdateManifest.hpp"
#include "update/UpdateSettings.hpp"
#include "update/ed25519/monocypher-ed25519.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QHash>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPointer>
#include <QSet>
#include <QSettings>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QTimer>
#include <QVersionNumber>

#include <algorithm>
#include <cstdint>

namespace appupdate {

using Flow = appupdate::UpdateFlow;

// ---------------------------------------------------------------------------
// A tiny HTTP server on 127.0.0.1 with the misbehaviours the scenes need.
// ---------------------------------------------------------------------------
class ShowcaseServer : public QObject {
public:
    enum class Mode { Files, DropAll, HangAll, Proxy407 };

    explicit ShowcaseServer(Mode mode) : m_mode(mode)
    {
        m_server.listen(QHostAddress::LocalHost, 0);
        QObject::connect(&m_server, &QTcpServer::newConnection, this, [this] {
            while (QTcpSocket* s = m_server.nextPendingConnection()) accept(s);
        });
    }
    [[nodiscard]] quint16 port() const { return m_server.serverPort(); }
    [[nodiscard]] QUrl base() const
    {
        return QUrl(QStringLiteral("http://127.0.0.1:%1/updates").arg(port()));
    }

    QHash<QString, QByteArray> files;       // "/updates/demo/manifest.json" → bytes
    QHash<QString, qint64> stallAfter;      // path → send this many body bytes, then hold
    QHash<QString, qint64> dropAfter;       // path → send this many, then cut the line
    QSet<QString> hang;                     // path → never answer

private:
    void accept(QTcpSocket* s)
    {
        if (m_mode == Mode::DropAll) {
            s->abort();
            s->deleteLater();
            return;
        }
        auto buf = std::make_shared<QByteArray>();
        QObject::connect(s, &QTcpSocket::disconnected, s, &QObject::deleteLater);
        QObject::connect(s, &QTcpSocket::readyRead, s, [this, s, buf] {
            buf->append(s->readAll());
            while (true) {
                const qsizetype end = buf->indexOf("\r\n\r\n");
                if (end < 0) return;
                const QByteArray head = buf->left(end);
                buf->remove(0, end + 4);
                answer(s, head);
            }
        });
    }

    void answer(QTcpSocket* s, const QByteArray& head)
    {
        if (m_mode == Mode::HangAll) return;
        if (m_mode == Mode::Proxy407) {
            s->write("HTTP/1.1 407 Proxy Authentication Required\r\n"
                     "Proxy-Authenticate: Basic realm=\"updates\"\r\n"
                     "Content-Length: 0\r\nConnection: keep-alive\r\n\r\n");
            return;
        }
        const QList<QByteArray> first = head.left(head.indexOf("\r\n")).split(' ');
        QString path = first.size() >= 2 ? QString::fromLatin1(first.at(1)) : QString();
        const qsizetype q = path.indexOf(QLatin1Char('?'));
        if (q >= 0) path.truncate(q);
        if (hang.contains(path)) return;
        const auto it = files.constFind(path);
        if (it == files.constEnd()) {
            s->write("HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\n"
                     "Connection: keep-alive\r\n\r\n");
            return;
        }
        const QByteArray& body = it.value();
        s->write("HTTP/1.1 200 OK\r\nContent-Type: application/octet-stream\r\n"
                 "Content-Length: " + QByteArray::number(body.size()) + "\r\n"
                 "Connection: keep-alive\r\n\r\n");
        if (stallAfter.contains(path)) {
            // In pieces, the last one 250 ms after the others: Qt reports
            // downloadProgress at most every ~100 ms, and a single block
            // would leave the flow at 0 % forever.
            const qint64 stop = stallAfter.value(path);
            auto sent = std::make_shared<qint64>(0);
            auto* t = new QTimer(s);
            t->setInterval(10);
            QPointer<QTcpSocket> guard(s);
            QObject::connect(t, &QTimer::timeout, s, [t, sent, stop, guard, body] {
                if (!guard) return;
                const qint64 last = stop - 64 * 1024;
                const qint64 n = std::min<qint64>(512 * 1024, last - *sent);
                if (n > 0) {
                    guard->write(body.mid(*sent, n));
                    *sent += n;
                    return;
                }
                t->stop();
                // > 1 s: the hint takes a new percentage at most once a second.
                QTimer::singleShot(1100, guard, [guard, sent, stop, body] {
                    if (guard) guard->write(body.mid(*sent, stop - *sent));
                });
            });
            t->start();
            return;  // and hold
        }
        if (dropAfter.contains(path)) {
            s->write(body.left(dropAfter.value(path)));
            QPointer<QTcpSocket> guard(s);
            QTimer::singleShot(300, s, [guard] { if (guard) guard->abort(); });
            return;
        }
        s->write(body);
    }

    Mode m_mode;
    QTcpServer m_server;
};

namespace {

// Never answers prepare(), never launches: the scenes stop before that.
class ShowcaseBackend final : public appupdate::InstallBackend {
public:
    ShowcaseBackend(bool manual, appupdate::ManualReason reason) : m_manual(manual), m_reason(reason) {}
    appupdate::InstallCapability capability(const appupdate::InstallContext&) const override
    {
        appupdate::InstallCapability c;
        if (m_manual) {
            c.method = appupdate::InstallMethod::Manual;
            c.reason = m_reason;
            c.detail = QStringLiteral("self-replacement not possible: %1 (showcase)")
                           .arg(appupdate::manualReasonName(m_reason));
        } else {
            c.method = appupdate::InstallMethod::Automatic;
            c.reason = appupdate::ManualReason::None;
            c.detail = QStringLiteral("showcase");
        }
        c.reportsOutcome = !m_manual;
        return c;
    }
    void prepare(const appupdate::InstallOrder&, PrepareDone) override {}
    void cancelPrepare() override {}
    appupdate::LaunchResult launch(const appupdate::PreparedInstall&) override
    {
        return {false, QStringLiteral("showcase never launches")};
    }
    void discard(const appupdate::PreparedInstall&) override {}
    void cleanupAfterStart(const appupdate::InstallContext&) override {}

private:
    bool m_manual;
    appupdate::ManualReason m_reason;
};

struct Keys {
    std::uint8_t secret[64]{};
    std::uint8_t pub[32]{};
    Keys()
    {
        // A fixed throwaway seed: the key only ever signs showcase trees.
        std::uint8_t seed[32];
        for (int i = 0; i < 32; ++i) seed[i] = static_cast<std::uint8_t>(0x5C ^ (i * 7));
        crypto_ed25519_key_pair(secret, pub, seed);
    }
    [[nodiscard]] QByteArray sign(const QByteArray& bytes) const
    {
        std::uint8_t sig[64];
        crypto_ed25519_sign(sig, secret, reinterpret_cast<const std::uint8_t*>(bytes.constData()),
                            static_cast<size_t>(bytes.size()));
        return QByteArray(reinterpret_cast<const char*>(sig), 64);
    }
    [[nodiscard]] QByteArray publicKey() const
    {
        return QByteArray(reinterpret_cast<const char*>(pub), 32);
    }
};

const Keys& keys()
{
    static const Keys k;
    return k;
}

// 18.6 MB of deterministic bytes, shown as "18,6 MB" / "18.6 MB".
constexpr qint64 kPayloadSize = 18'634'211;

QString notesEn(const QString& product)
{
    return QStringLiteral(
               "**Online update in one step:** %1 downloads the update, quits when you ask it "
               "to and starts again in the new version.\n\n"
               "## New\n"
               "- A notice in the main window as soon as an update is ready\n"
               "- Proxy sign-in with “Remember for this session”\n"
               "- Release notes with headings, lists and [links](https://example.com/)\n\n"
               "## Improved\n"
               "- Size and progress of the download appear in the format of your language\n"
               "- Error messages name the **next step** instead of a code\n\n"
               "## Fixed\n"
               "- The option `--no-update-check` was ignored at start-up\n")
        .arg(product);
}

QString notesDe(const QString& product)
{
    return QStringLiteral(
               "**Online-Update in einem Schritt:** %1 lädt das Update, beendet sich auf Wunsch "
               "und startet in der neuen Version.\n\n"
               "## Neu\n"
               "- Hinweis im Hauptfenster, sobald ein Update bereitsteht\n"
               "- Anmeldung am Proxy mit „Für diese Sitzung merken“\n"
               "- Änderungshinweise mit Überschriften, Listen und [Links](https://example.com/)\n\n"
               "## Verbessert\n"
               "- Größe und Fortschritt des Downloads erscheinen im Format Ihrer Sprache\n"
               "- Fehlermeldungen nennen den **nächsten Schritt** statt eines Codes\n\n"
               "## Behoben\n"
               "- Die Option `--no-update-check` wurde beim Start ignoriert\n")
        .arg(product);
}

QString longNotes(const QString& base, bool de)
{
    QString s = base + QStringLiteral("\n## ") + (de ? QStringLiteral("Weitere Änderungen")
                                                      : QStringLiteral("More changes"))
                + QLatin1Char('\n');
    for (int i = 1; i <= 30; ++i) {
        s += de ? QStringLiteral("- Änderung %1 mit einem Satz, der zeigt, wie eine lange Liste rollt\n").arg(i)
                : QStringLiteral("- Change %1 with a sentence that shows how a long list scrolls\n").arg(i);
    }
    return s;
}

QString bumped(const QString& v)
{
    const QVersionNumber n = QVersionNumber::fromString(v);
    return QStringLiteral("%1.%2.0").arg(n.majorVersion()).arg(n.minorVersion() + 1);
}

QString older(const QString& v)
{
    const QVersionNumber n = QVersionNumber::fromString(v);
    const int minor = n.minorVersion() > 0 ? n.minorVersion() - 1 : 0;
    return QStringLiteral("%1.%2.2").arg(n.majorVersion()).arg(minor);
}

// Waits until `pred` holds (checked every 20 ms) or `ms` passed, then calls done.
void when(QObject* ctx, std::function<bool()> pred, int ms, std::function<void(bool)> done)
{
    auto* t = new QTimer(ctx);
    auto left = std::make_shared<int>(ms);
    t->setInterval(20);
    QObject::connect(t, &QTimer::timeout, ctx, [t, left, pred = std::move(pred),
                                               done = std::move(done)] {
        *left -= 20;
        const bool ok = pred();
        if (ok || *left <= 0) {
            t->stop();
            t->deleteLater();
            done(ok);
        }
    });
    t->start();
}

}  // namespace

UpdateShowcase::UpdateShowcase(Options options, QObject* parent)
    : QObject(parent), m_opt(std::move(options)), m_dir(std::make_unique<QTemporaryDir>())
{
    m_settings = std::make_unique<QSettings>(m_dir->path() + QStringLiteral("/showcase.ini"),
                                             QSettings::IniFormat);
    m_payload.resize(kPayloadSize);
    for (qint64 i = 0; i < kPayloadSize; ++i) {
        m_payload[i] = static_cast<char>((i * 2654435761u) >> 13);
    }
}

UpdateShowcase::~UpdateShowcase()
{
    m_flow.reset();
    m_server.reset();
}

QStringList UpdateShowcase::scenes()
{
    return {QStringLiteral("1"),           QStringLiteral("1-slow"),
            QStringLiteral("2"),           QStringLiteral("2-notes"),
            QStringLiteral("3"),           QStringLiteral("3-long"),
            QStringLiteral("4"),           QStringLiteral("5"),
            QStringLiteral("6"),           QStringLiteral("7"),
            QStringLiteral("8"),           QStringLiteral("9"),
            QStringLiteral("10"),          QStringLiteral("10-details"),
            QStringLiteral("10-proxy"),    QStringLiteral("12-proxy"),
            QStringLiteral("11"),          QStringLiteral("12"),
            QStringLiteral("13"),          QStringLiteral("14"),
            QStringLiteral("14-shared"),   QStringLiteral("14-filesystem"),
            QStringLiteral("15"),          QStringLiteral("15-rejected"),
            QStringLiteral("hint-avail"),  QStringLiteral("hint-loading"),
            QStringLiteral("hint-ready"),  QStringLiteral("hint-done"),
            QStringLiteral("hint-failed"), QStringLiteral("settings-system"),
            QStringLiteral("settings-manual")};
}

QString UpdateShowcase::surfaceFor(const QString& scene)
{
    if (scene.startsWith(QLatin1String("hint-"))) return QStringLiteral("hint");
    if (scene.startsWith(QLatin1String("settings-"))) return QStringLiteral("settings");
    if (scene.startsWith(QLatin1String("15"))) return QStringLiteral("proxy");
    return QStringLiteral("dialog");
}

void UpdateShowcase::makeFlow(const QString& installed)
{
    m_flow.reset();
    Flow::Config c;
    c.productSlug = m_opt.productSlug;
    c.productName = m_opt.productName;
    c.version = installed;
    c.buildId = installed + m_opt.buildSuffix;
    c.language = m_opt.language;
    c.requestQuit = [] { return false; };   // a showcase never quits the app
    const Flow::QuitImpact impact = m_opt.impact;
    c.quitImpact = [impact] { return impact; };
    m_flow = std::make_unique<Flow>(c, *m_settings);
    const QUrl base = m_server ? m_server->base() : QUrl(QStringLiteral("http://127.0.0.1:9/updates"));
    m_flow->setBaseUrlForTests(base);
    m_flow->setPublicKeyForTests(keys().publicKey());
    m_flow->setDownloadDirForTests(m_dir->path() + QLatin1Char('/') + m_opt.productName
                                   + QStringLiteral("-Updates"));
    m_flow->setResultDirForTests(m_dir->path() + QStringLiteral("/result"));
    m_flow->setAppLocationForTests(m_dir->path() + QLatin1Char('/') + m_opt.productName
                                   + QStringLiteral(".app"));
    m_flow->setOfflineProbeForTests([] { return false; });
    m_flow->setSilentCheckTimingForTests(24 * 60 * 60 * 1000, 24 * 60 * 60 * 1000);
    m_flow->setCleanupDelayForTests(60 * 60 * 1000);
    m_flow->setInstallBackend(std::make_unique<ShowcaseBackend>(m_manualInstall, m_manualReason));
}

void UpdateShowcase::publish(const QString& version, bool forThisSystem, bool badSignature,
                             const QString& en, const QString& de)
{
    QJsonObject art;
    art.insert(QStringLiteral("url"), version + QStringLiteral("/artifact.bin"));
    art.insert(QStringLiteral("size"), static_cast<double>(m_payload.size()));
    art.insert(QStringLiteral("sha256"),
               QString::fromLatin1(QCryptographicHash::hash(m_payload, QCryptographicHash::Sha256)
                                       .toHex()));
    art.insert(QStringLiteral("kind"), QStringLiteral("dmg"));
    QJsonObject arts;
    arts.insert(forThisSystem ? appupdate::UpdateManifest::currentArtifactKey()
                              : QStringLiteral("windows-arm64"),
                art);
    QJsonObject notes;
    notes.insert(QStringLiteral("en"), en.isNull() ? notesEn(m_opt.productName) : en);
    notes.insert(QStringLiteral("de"), de.isNull() ? notesDe(m_opt.productName) : de);
    QJsonObject m;
    m.insert(QStringLiteral("schema"), 1);
    m.insert(QStringLiteral("product"), m_opt.productSlug);
    m.insert(QStringLiteral("channel"), QStringLiteral("stable"));
    m.insert(QStringLiteral("version"), version);
    m.insert(QStringLiteral("serial"), 100);
    m.insert(QStringLiteral("published"), QStringLiteral("2026-11-18"));
    m.insert(QStringLiteral("artifacts"), arts);
    m.insert(QStringLiteral("notes"), notes);
    const QByteArray bytes = QJsonDocument(m).toJson();
    const QString dir = QStringLiteral("/updates/") + m_opt.productSlug + QLatin1Char('/');
    m_server->files.insert(dir + QStringLiteral("manifest.json"), bytes);
    m_server->files.insert(dir + QStringLiteral("manifest.json.sig"),
                           badSignature ? keys().sign(bytes + "x") : keys().sign(bytes));
    m_server->files.insert(dir + version + QStringLiteral("/artifact.bin"), m_payload);
}

void UpdateShowcase::writeMarker(const QString& from, const QString& to, bool failed)
{
    appupdate::UpdateSettings s(*m_settings);
    appupdate::PendingInstall p;
    p.fromVersion = from;
    p.toVersion = to;
    p.method = appupdate::installMethodName(appupdate::InstallMethod::Automatic);
    const QString dl = m_dir->path() + QLatin1Char('/') + m_opt.productName
                       + QStringLiteral("-Updates");
    QDir().mkpath(dl);
    p.file = dl + QLatin1Char('/') + m_opt.productName + QLatin1Char('-') + to
             + QStringLiteral(".dmg");
    QFile f(p.file);
    if (f.open(QIODevice::WriteOnly)) f.write("showcase");
    p.sha256 = QString::fromLatin1(QCryptographicHash::hash("showcase", QCryptographicHash::Sha256).toHex());
    p.size = kPayloadSize;  // the size the notes head shows in 6
    p.kind = QStringLiteral("dmg");
    p.notes = m_opt.language == QLatin1String("de") ? notesDe(m_opt.productName)
                                                    : notesEn(m_opt.productName);
    p.published = QStringLiteral("2026-11-18");
    p.startedUtc = QDateTime::currentDateTimeUtc();
    if (failed) {
        p.launchError = QStringLiteral(
            "install helper: replacing the app bundle failed at step 'swap': "
            "Operation not permitted");
    }
    s.setPendingInstall(p);
}

bool UpdateShowcase::enter(const QString& scene, std::function<void(bool)> done)
{
    if (!scenes().contains(scene)) return false;
    const QString installed = m_opt.installed;
    const QString offer = bumped(installed);
    const QString dir = QStringLiteral("/updates/") + m_opt.productSlug + QLatin1Char('/');
    const auto state = [this] { return m_flow ? m_flow->state() : Flow::State::Idle; };
    const auto finishWhen = [this, done](std::function<bool()> pred, int ms = 8000) {
        when(this, std::move(pred), ms, done);
    };

    m_manualInstall = scene.startsWith(QLatin1String("14"));
    m_manualReason = scene == QLatin1String("14-shared") ? appupdate::ManualReason::LocationShared
                   : scene == QLatin1String("14-filesystem")
                       ? appupdate::ManualReason::FileSystemUnsupported
                       : appupdate::ManualReason::LocationNotWritable;
    ShowcaseServer::Mode mode = ShowcaseServer::Mode::Files;
    if (scene == QLatin1String("10") || scene == QLatin1String("10-details")) {
        mode = ShowcaseServer::Mode::DropAll;
    }
    m_server = std::make_unique<ShowcaseServer>(mode);

    if (scene == QLatin1String("1")) {
        publish(offer);
        m_server->hang.insert(dir + QStringLiteral("manifest.json.sig"));
        makeFlow(installed);
        m_flow->checkNow();
        finishWhen([state] { return state() == Flow::State::Checking; });
    } else if (scene == QLatin1String("1-slow")) {
        // A proxy that accepts and never answers: the flow knows a proxy is
        // in use and shows its sentence after 3 s.
        publish(offer);
        auto proxy = std::make_shared<ShowcaseServer>(ShowcaseServer::Mode::HangAll);
        m_extraServers.push_back(proxy);
        makeFlow(installed);
        appupdate::ProxyConfig pc;
        pc.mode = appupdate::ProxyConfig::Mode::Manual;
        pc.host = QStringLiteral("127.0.0.1");
        pc.port = proxy->port();
        m_flow->setProxyConfig(pc);
        m_flow->checkNow();
        finishWhen([this, state, proxy] {
            return state() == Flow::State::Checking && !m_flow->view().progressNote.isEmpty();
        }, 6000);
    } else if (scene == QLatin1String("2") || scene == QLatin1String("2-notes")) {
        publish(installed);
        makeFlow(installed);
        m_flow->checkNow();
        const bool expand = scene == QLatin1String("2-notes");
        finishWhen([this, state, expand] {
            if (state() != Flow::State::UpToDate) return false;
            if (expand && !m_flow->view().showNotes) m_flow->perform(Flow::Action::ToggleNotes);
            return true;
        });
    } else if (scene == QLatin1String("3") || scene == QLatin1String("hint-avail")
               || scene == QLatin1String("3-long")) {
        if (scene == QLatin1String("3-long")) {
            publish(offer, true, false, longNotes(notesEn(m_opt.productName), false),
                    longNotes(notesDe(m_opt.productName), true));
        } else {
            publish(offer);
        }
        makeFlow(installed);
        m_flow->checkNow();
        finishWhen([state] { return state() == Flow::State::Available; });
    } else if (scene == QLatin1String("4") || scene == QLatin1String("hint-loading")) {
        publish(offer);
        const qint64 at = m_payload.size() * 393 / 1000;
        m_server->stallAfter.insert(dir + offer + QStringLiteral("/artifact.bin"), at);
        makeFlow(installed);
        m_flow->checkNow();
        auto started = std::make_shared<bool>(false);
        const bool hintOnly = scene == QLatin1String("hint-loading");
        finishWhen([this, state, started, hintOnly] {
            if (state() == Flow::State::Available && !*started) {
                *started = true;
                m_flow->download();
                return false;
            }
            if (state() != Flow::State::Downloading) return false;
            if (hintOnly) return m_flow->hint().text.contains(QLatin1String("39"));
            return m_flow->view().progressPermille >= 390;
        }, 15000);
    } else if (scene == QLatin1String("5") || scene == QLatin1String("hint-ready")
               || scene.startsWith(QLatin1String("14"))) {
        publish(offer);
        makeFlow(installed);
        m_flow->checkNow();
        auto started = std::make_shared<bool>(false);
        const Flow::State target = scene.startsWith(QLatin1String("14")) ? Flow::State::ManualInstall
                                                                : Flow::State::Ready;
        finishWhen([this, state, started, target] {
            if (state() == Flow::State::Available && !*started) {
                *started = true;
                m_flow->download();
                return false;
            }
            return state() == target;
        }, 20000);
    } else if (scene == QLatin1String("6") || scene == QLatin1String("hint-done")) {
        writeMarker(older(installed), installed, false);
        makeFlow(installed);
        m_flow->start();
        const bool open = scene == QLatin1String("6");
        finishWhen([this, open] {
            if (m_flow->hint().kind != Flow::HintKind::Updated
                && m_flow->state() != Flow::State::Updated) return false;
            if (open && m_flow->state() != Flow::State::Updated) m_flow->openFromHint();
            return true;
        });
    } else if (scene == QLatin1String("7")) {
        publish(older(installed));
        makeFlow(installed);
        m_flow->checkNow();
        finishWhen([state] { return state() == Flow::State::Downgrade; });
    } else if (scene == QLatin1String("8")) {
        publish(offer, false);
        makeFlow(installed);
        m_flow->checkNow();
        finishWhen([state] { return state() == Flow::State::NoPackage; });
    } else if (scene == QLatin1String("9")) {
        makeFlow(installed);
        m_flow->checkNow();
        finishWhen([state] { return state() == Flow::State::NotPublished; });
    } else if (scene == QLatin1String("10-proxy")) {
        // "Manual" stored without a host (e.g. a value from 0.7.0): the core
        // does not connect at all and names the reason (ProxyIncomplete).
        publish(offer);
        makeFlow(installed);
        appupdate::ProxyConfig pc;
        pc.mode = appupdate::ProxyConfig::Mode::Manual;
        m_flow->setProxyConfig(pc);
        m_flow->checkNow();
        finishWhen([state] { return state() == Flow::State::CheckFailed; });
    } else if (scene == QLatin1String("10") || scene == QLatin1String("10-details")) {
        makeFlow(installed);
        m_flow->checkNow();
        finishWhen([state] { return state() == Flow::State::CheckFailed; }, 20000);
    } else if (scene == QLatin1String("11")) {
        publish(offer, true, true);
        makeFlow(installed);
        m_flow->checkNow();
        finishWhen([state] { return state() == Flow::State::Refused; });
    } else if (scene == QLatin1String("12-proxy")) {
        // The offer was found; then "Manual" without host/port is stored and
        // the download starts: the core names the reason (ProxyIncomplete)
        // in 12 instead of "connection lost", with "Proxy settings".
        publish(offer);
        makeFlow(installed);
        m_flow->checkNow();
        auto started = std::make_shared<bool>(false);
        finishWhen([this, state, started] {
            if (state() == Flow::State::Available && !*started) {
                *started = true;
                appupdate::ProxyConfig pc;
                pc.mode = appupdate::ProxyConfig::Mode::Manual;
                m_flow->setProxyConfig(pc);
                m_flow->download();
            }
            return state() == Flow::State::DownloadFailed;
        });
    } else if (scene == QLatin1String("12")) {
        publish(offer);
        m_server->dropAfter.insert(dir + offer + QStringLiteral("/artifact.bin"),
                                   m_payload.size() * 38 / 100);
        makeFlow(installed);
        m_flow->checkNow();
        auto started = std::make_shared<bool>(false);
        finishWhen([this, state, started] {
            if (state() == Flow::State::Available && !*started) {
                *started = true;
                m_flow->download();
                return false;
            }
            return state() == Flow::State::DownloadFailed;
        }, 20000);
    } else if (scene == QLatin1String("13")) {
        writeMarker(installed, offer, true);
        makeFlow(installed);
        m_flow->start();
        finishWhen([state] { return state() == Flow::State::InstallFailed; });
    } else if (scene == QLatin1String("hint-failed")) {
        // The flow opens dialog 13 by itself one event-loop turn after start();
        // the hint stays next to it. Synchronous all the same (the gallery
        // grabs at once).
        writeMarker(installed, offer, true);
        makeFlow(installed);
        m_flow->start();
        done(m_flow->hint().kind == Flow::HintKind::InstallFailed);
    } else if (scene == QLatin1String("15") || scene == QLatin1String("15-rejected")) {
        auto proxy = std::make_shared<ShowcaseServer>(ShowcaseServer::Mode::Proxy407);
        m_extraServers.push_back(proxy);
        publish(offer);
        makeFlow(installed);
        appupdate::ProxyConfig pc;
        pc.mode = appupdate::ProxyConfig::Mode::Manual;
        pc.host = QStringLiteral("localhost");
        pc.port = proxy->port();
        pc.user = QStringLiteral("example-user");
        m_flow->setProxyConfig(pc);
        m_flow->checkNow();
        const bool reject = scene == QLatin1String("15-rejected");
        auto sent = std::make_shared<bool>(false);
        finishWhen([this, proxy, reject, sent] {
            const Flow::ProxyAuth a = m_flow->proxyAuth();
            if (!a.pending) return false;
            if (!reject) return true;
            if (!*sent) {
                *sent = true;
                m_flow->provideProxyCredentials(QStringLiteral("example-user"),
                                                QStringLiteral("not-the-password"), false);
                return false;
            }
            return a.previousRejected;
        }, 15000);
    } else if (scene == QLatin1String("settings-system")) {
        makeFlow(installed);
        appupdate::UpdateSettings(*m_settings).setSkippedVersion(QStringLiteral("0.7.1"));
        QTimer::singleShot(0, this, [done] { done(true); });
    } else if (scene == QLatin1String("settings-manual")) {
        makeFlow(installed);
        appupdate::ProxyConfig pc;
        pc.mode = appupdate::ProxyConfig::Mode::Manual;
        pc.host = QStringLiteral("proxy.example.com");
        pc.port = 3128;
        pc.user = QStringLiteral("example-user");
        m_flow->setProxyConfig(pc);
        QTimer::singleShot(0, this, [done] { done(true); });
    }
    return true;
}

}  // namespace appupdate
