#include "update/UpdateFlow.hpp"

#include "update/UpdatePolicy.hpp"

#include <algorithm>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QLoggingCategory>
#include <QMetaEnum>
#include <QNetworkInformation>
#include <QSettings>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QVersionNumber>

namespace appupdate {

Q_LOGGING_CATEGORY(lcFlow, "appupdate.flow")

namespace {

using EK = UpdateChecker::ErrorKind;

template <typename E>
QString enumName(E v)
{
    return QString::fromLatin1(QMetaEnum::fromType<E>().valueToKey(static_cast<int>(v)));
}

// The sentence of state 14 for each reason. Every value is listed and there is
// no default: a new ManualReason without a sentence is a compiler warning
// (-Wswitch) and fails UpdateFlowInstall.EveryManualReasonHasItsSentence.
const char* manualLeadKey(ManualReason r)
{
    switch (r) {
    case ManualReason::LocationNotWritable:       return "s14.lead";
    case ManualReason::RunningFromDiskImage:      return "x.s14.dmg";
    case ManualReason::AppTranslocated:           return "x.s14.translocated";
    case ManualReason::NotFamilySigned:           return "x.s14.unsigned";
    case ManualReason::NotAnAppImage:             return "x.s14.noappimage";
    case ManualReason::Quarantined:               return "x.s14.quarantined";
    case ManualReason::NotInstalledPackage:       return "x.s14.notinstalled";
    case ManualReason::OtherInstanceRunning:      return "x.s14.otherinstance";
    case ManualReason::HelperUnavailable:         return "x.s14.helper";
    case ManualReason::AppManagementDenied:       return "x.s14.appmanagement";
    case ManualReason::LocationShared:            return "x.s14.shared";
    case ManualReason::FileSystemUnsupported:     return "x.s14.filesystem";
    // By hand is simply the normal way here: no reason sentence.
    case ManualReason::NotMeasuredOnThisPlatform: return "x.s14.ready";
    case ManualReason::None:
    case ManualReason::UnsupportedKind:
    case ManualReason::Other:                     return "x.s14.other";
    }
    return "x.s14.other";
}

bool defaultOffline()
{
    // Only asked on an error path, to tell "no network" from "server down".
    // Without a backend the answer is "don't know" = not offline.
    if (!QNetworkInformation::instance()) {
        QNetworkInformation::loadBackendByFeatures(QNetworkInformation::Feature::Reachability);
    }
    const QNetworkInformation* ni = QNetworkInformation::instance();
    return ni && ni->reachability() == QNetworkInformation::Reachability::Disconnected;
}

}  // namespace

// ===========================================================================
// Construction and start
// ===========================================================================

UpdateFlow::UpdateFlow(Config config, QSettings& settings, QObject* parent)
    : QObject(parent),
      m_cfg(std::move(config)),
      m_settings(settings),
      m_texts(m_cfg.productName, m_cfg.language),
      m_backend(makeSystemInstallerBackend()),
      m_channel(QString::fromLatin1(policy::kDefaultChannel)),
      m_clock([] { return QDateTime::currentDateTimeUtc(); }),
      m_offlineProbe(defaultOffline)
{
    m_silentTimer.setSingleShot(false);
    connect(&m_silentTimer, &QTimer::timeout, this, &UpdateFlow::runSilentCheck);
    m_slowTimer.setSingleShot(true);
    m_slowTimer.setInterval(3000);
    connect(&m_slowTimer, &QTimer::timeout, this, [this] {
        // Design, state 1: the proxy sentence only after 3 s, and only when a
        // proxy is really in play.
        if (m_state == State::Checking && m_checker && m_checker->proxyInUse()) {
            m_slowHint = true;
            emitView();
        }
    });
    m_checkTimer.setSingleShot(true);
    connect(&m_checkTimer, &QTimer::timeout, this, [this] {
        if (m_op != Op::Check || m_authPending) return;
        const bool silent = m_opSilent;
        ++m_generation;  // the late answer, if any, is ignored
        if (m_checker) m_checker->abort();
        m_op = Op::None;
        const QString why = QStringLiteral("no answer from the update server within %1 s")
                                .arg(m_checkDeadlineMs / 1000.0, 0, 'f', m_checkDeadlineMs % 1000 ? 1 : 0);
        if (silent) {
            qCInfo(lcFlow).noquote() << "silent check:" << why;
            return;  // silent means silent
        }
        m_detail = why;
        m_cause = Cause::Unreachable;
        setState(State::CheckFailed);
    });
    m_prepareTimer.setSingleShot(true);
    connect(&m_prepareTimer, &QTimer::timeout, this, [this] {
        if (!m_preparing) return;
        m_backend->cancelPrepare();
        m_detail = QStringLiteral("preparing the install took longer than %1 ms")
                       .arg(m_tuning.prepareDeadlineMs);
        qCWarning(lcFlow).noquote() << m_detail;
        endPreparing(State::InstallFailed);
    });
    m_quitTimer.setSingleShot(true);
    connect(&m_quitTimer, &QTimer::timeout, this, [this] {
        if (!m_installArmed) return;  // aboutToQuit came and took it
        qCWarning(lcFlow).noquote()
            << QStringLiteral("app did not quit within %1 ms after requestQuit returned true - "
                              "the install is withdrawn")
                   .arg(m_tuning.quitDeadlineMs);
        m_quitting = false;
        disarmInstall();
        // No window of its own: the user sees "ready" again in the hint.
        setState(m_prepareFrom == State::InstallFailed ? State::InstallFailed : State::Ready);
    });
    if (QCoreApplication* app = QCoreApplication::instance()) {
        connect(app, &QCoreApplication::aboutToQuit, this, &UpdateFlow::onAboutToQuit);
    }
}

UpdateFlow::~UpdateFlow()
{
    ++m_generation;  // no callback may reach a half-destroyed flow
    // A backend working in the background must not answer into a dead flow:
    // the seam promises "no done after cancelPrepare()".
    if (m_preparing && m_backend) m_backend->cancelPrepare();
    ++m_prepareGen;
    if (m_checker) m_checker->abort();
}

void UpdateFlow::setLanguage(const QString& language)
{
    const QString lang = language.left(2).toLower();
    if (lang == m_texts.language()) return;
    if (!installUpdateTranslator(lang)) {
        // The old translator is gone either way: the sentences fall back to
        // English, so dates and sizes follow English too.
        qCWarning(lcFlow).noquote() << "update catalogue for" << lang
                                    << "missing, update texts stay English";
        m_texts = UpdateTexts(m_cfg.productName, QStringLiteral("en"));
    } else {
        m_texts = UpdateTexts(m_cfg.productName, lang);
    }
    updateHint();  // the hint is stored, not drawn on demand; emits hintChanged()
    emitView();
    emit proxyAuthChanged();
}

void UpdateFlow::applyStartOptions(const QStringList& arguments)
{
    QStringList refused;
    for (qsizetype i = 0; i < arguments.size(); ++i) {
        const QString& a = arguments.at(i);
        const auto value = [&](const QString& name, QString* out) {
            if (a == name && i + 1 < arguments.size()) { *out = arguments.at(++i); return true; }
            if (a.startsWith(name + QLatin1Char('='))) { *out = a.mid(name.size() + 1); return true; }
            return false;
        };
        QString v;
        if (a == QLatin1String("--no-update-check")) {
            m_noSilentCheck = true;
        } else if (value(QStringLiteral("--update-base-url"), &v)) {
            if (m_cfg.allowSourceOverrideForSelfTest) {
                setBaseUrlForTests(QUrl::fromUserInput(v));
                m_sourceOverridden = true;
            } else {
                refused << QStringLiteral("--update-base-url");
            }
        } else if (value(QStringLiteral("--update-channel"), &v)) {
            if (m_cfg.allowSourceOverrideForSelfTest) {
                setChannelForTests(v);
                m_sourceOverridden = true;
            } else {
                refused << QStringLiteral("--update-channel");
            }
        }
    }
    if (!refused.isEmpty()) {
        // The values are not logged: they are whatever someone typed.
        qCWarning(lcFlow).noquote()
            << "ignored" << refused.join(QStringLiteral(", "))
            << "- the update source is fixed outside the self-test";
    }
    if (m_sourceOverridden) {
        qCWarning(lcFlow).noquote()
            << "UPDATE SOURCE OVERRIDDEN for the self-test - every install is a dry run, "
               "no helper and no installer will be started";
    }
}

void UpdateFlow::start()
{
    if (m_started) return;
    m_started = true;
    if (m_settings.migrate()) {
        qCInfo(lcFlow) << "update settings migrated to the family keys";
    }
    m_serialUnreadable = m_settings.serialState(m_cfg.productSlug) == SerialState::Unreadable;
    if (m_serialUnreadable) {
        qCWarning(lcFlow).noquote()
            << "stored" << policy::lastSerialSettingsKey(m_cfg.productSlug)
            << "is unreadable — replay protection starts from 0 until a signed manifest is accepted";
        emit serialUnreadableChanged();
    }
    evaluatePendingInstall();
    if (!m_noSilentCheck) {
        QTimer::singleShot(m_startupDelayMs, this, &UpdateFlow::runSilentCheck);
        m_silentTimer.setInterval(m_periodMs);
        m_silentTimer.start();
    } else {
        qCInfo(lcFlow) << "silent update checks disabled by --no-update-check";
    }
    updateHint();
}

// ===========================================================================
// Small helpers
// ===========================================================================

QDateTime UpdateFlow::now() const { return m_clock(); }

bool UpdateFlow::busy() const { return m_op != Op::None || m_authPending; }

void UpdateFlow::setState(State s)
{
    const bool changed = s != m_state;
    m_state = s;
    if (s != State::Checking) {
        m_slowTimer.stop();
        m_slowHint = false;
    }
    if (changed) emit stateChanged();
    emitView();
    updateHint();
}

void UpdateFlow::emitView() { emit viewChanged(); }

bool UpdateFlow::isDowngradeOffer() const
{
    return m_offer && policy::isDowngrade(m_offer->version,
                                          QVersionNumber::fromString(m_cfg.version));
}

QString UpdateFlow::downloadDir() const
{
    if (!m_downloadDirOverride.isEmpty()) return m_downloadDirOverride;
    // Downloads/<Product>-Updates, else the temp folder.
    const QString sub = m_cfg.productName + QStringLiteral("-Updates");
    const QString dl = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
    if (!dl.isEmpty()) {
        const QString dir = dl + QLatin1Char('/') + sub;
        if (QDir().mkpath(dir) && QFileInfo(dir).isWritable()) return dir;
    }
    // The fallback is a PRIVATE folder with an unpredictable name (0700,
    // QTemporaryDir): on Linux the temp location is the shared /tmp, and a
    // fixed name there could be created beforehand by another user, who could
    // then swap the verified file before it is installed.
    QTemporaryDir tmp(QStandardPaths::writableLocation(QStandardPaths::TempLocation)
                      + QLatin1Char('/') + sub + QStringLiteral("-XXXXXX"));
    tmp.setAutoRemove(false);
    return tmp.isValid() ? tmp.path() : QString();
}

QString UpdateFlow::resultDir() const
{
    QString dir = m_resultDirOverride;
    if (dir.isEmpty()) {
        dir = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)
              + QStringLiteral("/update");
    }
    QDir().mkpath(dir);
    return dir;
}

QString UpdateFlow::appLocation() const
{
    if (!m_appLocationOverride.isNull()) return m_appLocationOverride;
#if defined(Q_OS_MACOS)
    // <bundle>.app/Contents/MacOS/<exe> → <bundle>.app
    QDir d(QCoreApplication::applicationDirPath());
    if (d.cdUp() && d.cdUp() && d.dirName().endsWith(QLatin1String(".app"))) return d.absolutePath();
    return QCoreApplication::applicationFilePath();
#elif defined(Q_OS_WIN)
    return QCoreApplication::applicationFilePath();
#else
    return qEnvironmentVariable("APPIMAGE");
#endif
}

InstallContext UpdateFlow::installContext(const QString& kind) const
{
    InstallContext c;
    c.kind = kind;
    c.appLocation = appLocation();
    c.verifiedFile = m_downloadedPath;
    c.productSlug = m_cfg.productSlug;
    c.windowsUpgradeCode = m_cfg.windowsUpgradeCode;
    c.previousFailureCode = m_settings.installBlockedCode();
    c.tuning = m_tuning;
    return c;
}

QHash<QString, QString> UpdateFlow::args() const
{
    QHash<QString, QString> a;
    const QString offered = m_offer ? m_offer->versionString : m_lastPending.toVersion;
    a.insert(QStringLiteral("neu"), offered);
    a.insert(QStringLiteral("alt"), m_cfg.version);
    a.insert(QStringLiteral("kennung"), m_cfg.buildId);
    a.insert(QStringLiteral("proxy"), m_authHost);
    a.insert(QStringLiteral("system"), m_texts.systemName(UpdateManifest::currentArtifactKey()));
    const QString published = m_offer ? m_offer->published : m_lastPending.published;
    a.insert(QStringLiteral("datum"), m_texts.longDateFromIso(published));
    const qint64 total = m_artifact ? m_artifact->size : (m_total > 0 ? m_total : m_lastPending.size);
    a.insert(QStringLiteral("groesse"), m_texts.size(total));
    const qint64 at = m_state == State::DownloadFailed ? m_failedAt : m_received;
    a.insert(QStringLiteral("geladen"), m_texts.amountInUnitOf(at, total));
    const int pct = total > 0 ? static_cast<int>((at * 100) / total) : 0;
    a.insert(QStringLiteral("prozent"), m_texts.locale().toString(pct));
    return a;
}

// ===========================================================================
// The checker
// ===========================================================================

void UpdateFlow::ensureChecker()
{
    if (m_checker) {
        m_checker->setProxyConfig(m_settings.proxyConfig());
        return;
    }
    m_checker = std::make_unique<UpdateChecker>(m_baseUrl, m_cfg.productSlug, m_publicKey);
    m_checker->setExpectedChannel(m_channel);
    m_checker->setDownloadStallTimeoutForTests(m_downloadStallMs);
    m_checker->setSerialStore(m_settings.serialStore());
    m_checker->setProxyConfig(m_settings.proxyConfig());
    m_checker->setProxyCredentialProvider(
        [this](const QString& host, quint16 port, bool prevFailed, QString* user, QString* pw) {
            return answerProxyChallenge(host, port, prevFailed, user, pw);
        });
    connect(m_checker.get(), &UpdateChecker::downloadProgress, this, &UpdateFlow::onProgress);
}

bool UpdateFlow::answerProxyChallenge(const QString& host, quint16 port, bool previousFailed,
                                      QString* user, QString* password)
{
    const bool sameProxy = host == m_authHost && port == m_authPort;
    if (m_opSilent) {
        // The silent check never asks — it may use
        // what is remembered, once, and never a password that just failed.
        if (previousFailed || m_sessionPassword.isEmpty() || !sameProxy) return false;
        *user = m_sessionUser;
        *password = m_sessionPassword;
        return true;
    }
    if (previousFailed) {
        // What we just sent was rejected. Never send it again (the design:
        // accounts get locked) — drop it and ask, with the rejected version.
        m_sessionPassword.clear();
        m_oneShotPassword.clear();
        m_authRejected = true;
    } else if (!m_oneShotPassword.isEmpty() && !m_oneShotUsed && sameProxy) {
        m_oneShotUsed = true;
        *user = m_sessionUser;
        *password = m_oneShotPassword;
        return true;
    } else if (!m_sessionPassword.isEmpty() && sameProxy) {
        *user = m_sessionUser;
        *password = m_sessionPassword;
        return true;
    }
    m_authPending = true;
    m_authOp = m_op;
    m_checkTimer.stop();  // the user is typing; provideProxyCredentials() starts anew
    m_authHost = host;
    m_authPort = port;
    if (m_sessionUser.isEmpty()) m_sessionUser = *user;  // ProxyConfig::user prefill
    qCInfo(lcFlow).noquote() << "proxy" << host << "asks for sign-in"
                             << (m_authRejected ? "(previous entry rejected)" : "");
    emit proxyAuthChanged();
    return false;
}

void UpdateFlow::provideProxyCredentials(const QString& user, const QString& password,
                                         bool remember)
{
    if (!m_authPending) return;
    m_authPending = false;
    m_sessionUser = user;
    if (remember) {
        m_sessionPassword = password;
        m_oneShotPassword.clear();
    } else {
        m_oneShotPassword = password;
        m_oneShotUsed = false;
        m_dropCheckerAfterOp = true;
    }
    emit proxyAuthChanged();
    // Repeat exactly the interrupted operation.
    const Op op = m_authOp;
    m_authOp = Op::None;
    if (op == Op::Download) startDownload();
    else startCheck(false);
}

void UpdateFlow::cancelProxyAuth()
{
    if (!m_authPending) return;
    m_authPending = false;
    m_authRejected = false;
    m_authOp = Op::None;
    ++m_generation;
    m_checkTimer.stop();
    if (m_checker) m_checker->abort();
    m_op = Op::None;
    m_cause = Cause::AuthCancelled;
    m_detail = QStringLiteral("proxy sign-in cancelled by the user");
    emit proxyAuthChanged();
    setState(State::CheckFailed);  // the design state 15: Abbrechen → 10 „auth“
}

// ===========================================================================
// Checking
// ===========================================================================

void UpdateFlow::runSilentCheck()
{
    if (m_noSilentCheck) return;
    // Switched off: no check and no stamp (docs/SHARED.md start-check rule).
    if (!m_settings.autoCheck()) return;
    static const QSet<State> resting = {State::Idle, State::UpToDate, State::NotPublished,
                                        State::CheckFailed, State::Refused, State::Available};
    if (m_dialogVisible || busy() || !resting.contains(m_state)) {
        qCInfo(lcFlow) << "silent check skipped: the user is busy with the update";
        return;
    }
    startCheck(true);
}

void UpdateFlow::startCheck(bool silent)
{
    ensureChecker();
    if (m_checker->busy()) {
        ++m_generation;
        m_checker->abort();  // a manual check takes over from a silent one
    }
    const int gen = ++m_generation;
    m_op = Op::Check;
    m_opSilent = silent;
    m_oneShotUsed = false;
    if (silent) {
        // Chronicle only, stamped BEFORE the request (docs/SHARED.md).
        m_settings.setLastCheck(now());
    } else {
        m_cause = Cause::None;
        m_detail.clear();
        m_notesExpanded = false;
        if (!m_authRejected) m_slowHint = false;
        setState(State::Checking);
        m_slowTimer.start();
    }
    m_checkTimer.start(m_checkDeadlineMs);
    m_checker->checkForUpdate([this, gen, silent](std::optional<UpdateManifest> m, QString e) {
        onCheckResult(gen, silent, std::move(m), e);
    });
}

void UpdateFlow::onCheckResult(int generation, bool silent, std::optional<UpdateManifest> m,
                               const QString& error)
{
    if (generation != m_generation) return;  // aborted on purpose
    m_op = Op::None;
    m_checkTimer.stop();
    if (m_dropCheckerAfterOp && !m_authPending) {
        m_oneShotPassword.clear();
        m_dropCheckerAfterOp = false;
        QTimer::singleShot(0, this, [this] { if (!busy()) m_checker.reset(); });
    }
    if (m_authPending) {
        // The request ended because we asked the user; the answer repeats it.
        emitView();
        return;
    }
    m_authRejected = false;
    if (!m) {
        const EK kind = m_checker ? m_checker->lastErrorKind() : EK::Network;
        if (silent) {
            qCInfo(lcFlow).noquote() << "silent check:" << error;
            return;  // silent means silent
        }
        failCheck(kind, error);
        return;
    }

    const QVersionNumber current = QVersionNumber::fromString(m_cfg.version);
    const auto artifact = m->currentArtifact();
    const bool newer = policy::isNewer(m->version, current);
    const bool older = policy::isDowngrade(m->version, current);

    if (silent) {
        // A find: newer, not skipped, a package for this system, once per run.
        if (!newer || !artifact) return;
        if (policy::isSkipped(m->version, m_settings.skippedVersion())) return;
        if (m_reportedVersions.contains(m->versionString)) return;
        m_reportedVersions.insert(m->versionString);
        m_offer = std::move(m);
        m_artifact = artifact;
        m_downgradeDownload = false;
        qCInfo(lcFlow).noquote() << "silent check found" << m_offer->versionString;
        setState(State::Available);
        emit updateFound(m_offer->versionString);
        return;
    }

    m_offer = std::move(m);
    m_artifact = artifact;
    // Version before package: an equal version is "up to date" even
    // when this system has no package.
    if (!newer && !older) setState(State::UpToDate);
    else if (!artifact) setState(State::NoPackage);
    else if (newer) setState(State::Available);
    else setState(State::Downgrade);
}

void UpdateFlow::failCheck(EK kind, const QString& error)
{
    m_detail = error;
    switch (kind) {
    case EK::NotPublished:
        m_cause = Cause::None;
        setState(State::NotPublished);  // not an error
        return;
    case EK::Signature:    m_cause = Cause::Signature;    setState(State::Refused); return;
    case EK::WrongProduct: m_cause = Cause::WrongProduct; setState(State::Refused); return;
    case EK::Replay:       m_cause = Cause::Replay;       setState(State::Refused); return;
    case EK::Size:         m_cause = Cause::Size;         setState(State::Refused); return;
    case EK::Parse:        m_cause = Cause::Garbled;      setState(State::CheckFailed); return;
    case EK::ProxyIncomplete:
        // Not "unreachable": nothing was tried, and the fix is in the settings.
        m_cause = Cause::ProxyIncomplete;
        setState(State::CheckFailed);
        return;
    default:
        break;
    }
    m_cause = m_offlineProbe && m_offlineProbe() ? Cause::Offline : Cause::Unreachable;
    setState(State::CheckFailed);
}

// ===========================================================================
// Downloading
// ===========================================================================

void UpdateFlow::startDownload()
{
    ensureChecker();
    const int gen = ++m_generation;
    m_op = Op::Download;
    m_opSilent = false;
    m_oneShotUsed = false;
    m_received = 0;
    m_total = m_artifact ? m_artifact->size : 0;
    m_verifying = false;
    m_failedAt = 0;
    m_cause = Cause::None;
    m_detail.clear();
    m_lastHintPercentMs = -1;
    setState(State::Downloading);
    m_checker->downloadArtifact(*m_artifact, downloadDir(),
                                [this, gen](QString path, QString e) {
                                    onDownloadResult(gen, path, e);
                                });
}

void UpdateFlow::onProgress(qint64 received, qint64 total)
{
    if (m_op != Op::Download) return;
    m_received = received;
    if (total > 0) m_total = total;
    // the design: after 100 % "Verifying…" until hash and size are confirmed.
    if (m_total > 0 && m_received >= m_total) m_verifying = true;
    emitView();
    updateHint();
}

void UpdateFlow::onDownloadResult(int generation, const QString& path, const QString& error)
{
    if (generation != m_generation) return;
    m_op = Op::None;
    m_verifying = false;
    if (m_dropCheckerAfterOp && !m_authPending) {
        m_oneShotPassword.clear();
        m_dropCheckerAfterOp = false;
        QTimer::singleShot(0, this, [this] { if (!busy()) m_checker.reset(); });
    }
    if (m_authPending) {
        emitView();
        return;
    }
    m_authRejected = false;
    if (path.isEmpty()) {
        m_detail = error;
        const EK kind = m_checker ? m_checker->lastErrorKind() : EK::Network;
        switch (kind) {
        case EK::Size:
            m_cause = Cause::Size;
            setState(State::Refused);  // the design: size goes to 11
            return;
        case EK::Hash:         m_cause = Cause::Corrupt;    break;
        case EK::File:         m_cause = Cause::FileWrite;  break;
        case EK::Http:
        case EK::NotPublished: m_cause = Cause::ServerFile; break;
        case EK::ProxyIncomplete:
            // Nothing was tried (a setting kept from 0.7.0, or changed since
            // the check): the fix is in the settings, not the connection.
            m_cause = Cause::ProxyIncomplete;
            break;
        default:
            m_cause = Cause::Connection;
            // Counted in the core; the progress signal is throttled and may
            // not have reported the bytes before the connection dropped.
            m_failedAt = m_checker ? m_checker->lastDownloadReceived() : m_received;
            break;
        }
        setState(State::DownloadFailed);
        return;
    }
    m_downloadedPath = path;
    QString quarantineError;
    m_quarantine = quarantineState(path, &quarantineError);
    if (m_quarantine == QuarantineState::Unreadable) {
        // A failed measurement is not a negative one (Quarantine.hpp): no
        // remedy for a block nobody measured, but never silently "no".
        qCWarning(lcFlow).noquote()
            << QStringLiteral("cannot tell whether %1 carries com.apple.quarantine (%2) - "
                              "no remedy is named")
                   .arg(path, quarantineError.isEmpty() ? QStringLiteral("no reason given")
                                                        : quarantineError);
    }
    m_capability = m_backend->capability(installContext(m_artifact ? m_artifact->kind : QString()));
    if (m_capability.detail.isEmpty())
        qCInfo(lcFlow).noquote() << "downloaded and verified" << path
                                 << "install:" << installMethodName(m_capability.method)
                                 << manualReasonName(m_capability.reason);
    else  // every unmet precondition, not only the first (SelfInstall.cpp)
        qCInfo(lcFlow).noquote() << "downloaded and verified" << path
                                 << "install:" << installMethodName(m_capability.method)
                                 << manualReasonName(m_capability.reason) << "detail:" << m_capability.detail;
    setState(m_capability.method == InstallMethod::Manual ? State::ManualInstall : State::Ready);
}

// ===========================================================================
// Actions
// ===========================================================================

const QStringList& UpdateFlow::iconNames()
{
    static const QStringList names{
        QStringLiteral("arrow-circle-up"), QStringLiteral("check-circle"),
        QStringLiteral("circle-notch"),    QStringLiteral("download-simple"),
        QStringLiteral("folder-open"),     QStringLiteral("info"),
        QStringLiteral("package"),         QStringLiteral("power"),
        QStringLiteral("shield-check"),    QStringLiteral("warning"),
        QStringLiteral("warning-circle"),  QStringLiteral("wifi-slash"),
    };
    return names;
}

bool UpdateFlow::canPerform(Action a) const
{
    if (m_authPending) return a == Action::SignIn || a == Action::CancelSignIn;
    switch (a) {
    case Action::None:           return false;
    case Action::CheckNow:       return m_state != State::Downloading && m_state != State::Installing;
    case Action::Cancel:         return m_state == State::Checking
                                        || (m_state == State::Installing && m_preparing);
    case Action::CancelDownload: return m_state == State::Downloading;
    case Action::HideWindow:     return m_state == State::Downloading;
    case Action::Download:       return m_state == State::Available && m_artifact.has_value();
    case Action::DownloadAnyway: return m_state == State::Downgrade && m_artifact.has_value();
    case Action::SkipVersion:    return m_state == State::Available;  // never 7
    case Action::Later:          return m_state == State::Available || m_state == State::Ready;
    case Action::Close:
        switch (m_state) {
        case State::UpToDate: case State::Updated: case State::Downgrade:
        case State::NoPackage: case State::NotPublished: case State::CheckFailed:
        case State::Refused: case State::DownloadFailed: case State::InstallFailed:
        case State::ManualInstall:
            return true;
        default:
            return false;
        }
    case Action::QuitAndInstall: return m_state == State::Ready;
    case Action::RetryCheck:     return m_state == State::CheckFailed || m_state == State::Refused;
    case Action::RetryDownload:  return m_state == State::DownloadFailed && m_artifact.has_value();
    case Action::RetryInstall:   return m_state == State::InstallFailed;
    case Action::InstallManually: return m_state == State::InstallFailed;
    case Action::ShowInFolder:   return (m_state == State::ManualInstall || m_state == State::InstallFailed)
                                        && !m_downloadedPath.isEmpty();
    case Action::ProxySettings:  return m_state == State::CheckFailed
                                        || (m_state == State::DownloadFailed
                                            && m_cause == Cause::ProxyIncomplete);
    case Action::ToggleNotes:    return m_state == State::UpToDate;
    case Action::SignIn:
    case Action::CancelSignIn:   return false;  // only while m_authPending
    }
    return false;
}

bool UpdateFlow::perform(Action a)
{
    if (!canPerform(a)) {
        qCInfo(lcFlow).noquote() << "action" << enumName(a) << "refused in state" << enumName(m_state);
        return false;
    }
    switch (a) {
    case Action::CheckNow:       return checkNow();
    case Action::Cancel:
    case Action::CancelDownload: return cancel();
    case Action::Download:
    case Action::DownloadAnyway: return download();
    case Action::SkipVersion:    return skipVersion();
    case Action::QuitAndInstall: return quitAndInstall();
    case Action::RetryCheck:     return retryCheck();
    case Action::RetryDownload:  return retryDownload();
    case Action::RetryInstall:   return retryInstall();
    case Action::InstallManually: return installManually();
    case Action::ShowInFolder:   return showInFolder();
    case Action::ProxySettings:  emit proxySettingsRequested(); return true;
    case Action::ToggleNotes:    m_notesExpanded = !m_notesExpanded; emitView(); return true;
    case Action::HideWindow:     emit closeDialogRequested(); return true;  // download goes on
    case Action::Later:          emit closeDialogRequested(); return true;  // hint stays
    case Action::Close:
        m_offer.reset();
        m_artifact.reset();
        setState(State::Idle);
        emit closeDialogRequested();
        return true;
    case Action::SignIn:
    case Action::CancelSignIn:
    case Action::None:
        return false;
    }
    return false;
}

bool UpdateFlow::checkNow()
{
    if (!canPerform(Action::CheckNow)) return false;
    m_authRejected = false;
    startCheck(false);
    return true;
}

bool UpdateFlow::openByHand()
{
    switch (m_state) {
    case State::Available:      // a silent find: no second request
    case State::Downloading:
    case State::Ready:
    case State::Installing:
    case State::DownloadFailed: // failed in the background: show it, keep it
    case State::InstallFailed:
    case State::ManualInstall:
        break;
    default:
        if (!checkNow()) return false;
        break;
    }
    emit dialogRequested();
    return true;
}

bool UpdateFlow::retryCheck()
{
    if (!canPerform(Action::RetryCheck)) return false;
    startCheck(false);
    return true;
}

bool UpdateFlow::download()
{
    if (!canPerform(Action::Download) && !canPerform(Action::DownloadAnyway)) return false;
    m_downgradeDownload = m_state == State::Downgrade;
    startDownload();
    return true;
}

bool UpdateFlow::retryDownload()
{
    if (!canPerform(Action::RetryDownload)) return false;
    startDownload();
    return true;
}

bool UpdateFlow::cancel()
{
    if (m_state == State::Checking) {
        ++m_generation;
        m_checkTimer.stop();
        if (m_checker) m_checker->abort();
        m_op = Op::None;
        setState(State::Idle);
        emit closeDialogRequested();  // the design state 1: stop, window closes
        return true;
    }
    if (m_state == State::Installing && m_preparing) {
        m_backend->cancelPrepare();
        endPreparing(m_prepareFrom);
        return true;
    }
    if (m_state == State::Downloading) {
        ++m_generation;
        if (m_checker) m_checker->abort();  // the checker deletes the .part
        m_op = Op::None;
        m_verifying = false;
        setState(m_downgradeDownload ? State::Downgrade : State::Available);
        return true;
    }
    return false;
}

bool UpdateFlow::skipVersion()
{
    if (!canPerform(Action::SkipVersion) || !m_offer) return false;
    m_settings.setSkippedVersion(m_offer->versionString);
    m_offer.reset();
    m_artifact.reset();
    setState(State::Idle);
    emit closeDialogRequested();
    return true;
}

bool UpdateFlow::installManually()
{
    if (!canPerform(Action::InstallManually)) return false;
    m_capability.method = InstallMethod::Manual;
    m_capability.reason = ManualReason::Other;
    setState(State::ManualInstall);
    return true;
}

bool UpdateFlow::showInFolder()
{
    if (!canPerform(Action::ShowInFolder)) return false;
    emit revealFileRequested(m_downloadedPath);
    return true;
}

// ===========================================================================
// „Beenden und aktualisieren"
// ===========================================================================

bool UpdateFlow::quitAndInstall()
{
    if (m_state != State::Ready && m_state != State::InstallFailed) return false;
    const QString kind = m_artifact ? m_artifact->kind : m_lastPending.kind;
    InstallOrder order;
    order.context = installContext(kind);
    order.verifiedFile = m_downloadedPath;
    order.expectedSha256 = m_artifact ? m_artifact->sha256 : m_lastPending.sha256;
    order.expectedSize = m_artifact ? m_artifact->size : m_lastPending.size;
    order.fromVersion = m_cfg.version;
    order.toVersion = m_offer ? m_offer->versionString : m_lastPending.toVersion;
    order.productSlug = m_cfg.productSlug;
    order.productName = m_cfg.productName;
    order.appPid = QCoreApplication::applicationPid();
    const QString base = resultDir() + QLatin1Char('/') + m_cfg.productSlug;
    order.resultFilePath = base + QStringLiteral("-install-result.txt");
    order.startConfirmFilePath = base + QStringLiteral("-start-confirm.txt");
    // A new mark for every attempt, a retry included: a result file of an
    // earlier attempt must never judge this one.
    order.attemptId = newInstallAttemptId();
    // Stale files must not judge this run.
    QFile::remove(order.resultFilePath);
    QFile::remove(order.startConfirmFilePath);

    // 2. prepare: asynchronous, may check and stage, starts nothing.
    m_prepareFrom = m_state;
    m_preparing = true;
    const int gen = ++m_prepareGen;
    setState(State::Installing);
    m_prepareTimer.start(m_tuning.prepareDeadlineMs);
    m_backend->prepare(order, [this, gen](PrepareResult r) { onPrepared(gen, std::move(r)); });
    return true;
}

void UpdateFlow::endPreparing(State back)
{
    ++m_prepareGen;  // a late answer is ignored
    m_prepareTimer.stop();
    m_preparing = false;
    setState(back);
}

void UpdateFlow::onPrepared(int generation, PrepareResult r)
{
    if (generation != m_prepareGen || !m_preparing) return;
    m_prepareTimer.stop();
    m_preparing = false;
    if (!r.prepared) {
        m_detail = r.error;
        if (r.manual != ManualReason::None) {
            m_capability.method = InstallMethod::Manual;
            m_capability.reason = r.manual;
            m_capability.detail = r.error;
            setState(State::ManualInstall);
        } else {
            qCWarning(lcFlow).noquote() << "install not prepared:" << r.error;
            m_cause = Cause::None;
            setState(State::InstallFailed);
        }
        return;
    }

    // 5. the marker, before anything quits.
    const InstallOrder& order = r.prepared->order;
    PendingInstall p;
    p.fromVersion = order.fromVersion;
    p.toVersion = order.toVersion;
    p.method = installMethodName(r.prepared->method);
    p.file = order.verifiedFile;
    p.sha256 = order.expectedSha256;
    p.size = order.expectedSize;
    p.kind = order.context.kind;
    p.resultFile = order.resultFilePath;
    p.confirmFile = order.startConfirmFilePath;
    p.attempt = order.attemptId;
    p.notes = m_offer ? m_offer->notes(m_texts.language() == QLatin1String("de")
                                           ? QStringLiteral("de") : QStringLiteral("en"))
                      : m_lastPending.notes;
    p.published = m_offer ? m_offer->published : m_lastPending.published;
    p.startedUtc = now();
    m_settings.setPendingInstall(p);
    m_prepared = std::move(r.prepared);
    emitView();
    qCInfo(lcFlow).noquote() << "install of" << p.toVersion << "over" << p.fromVersion
                             << "prepared:" << m_prepared->description;

    // 3. the surface lets go first: an open dialog that refused its close
    // would otherwise make the app's normal quit (closeAllWindows) fail.
    m_quitting = true;
    emitView();
    emit quitStarting();
    // the app quits through its normal way - its prompt may refuse.
    const bool going = m_cfg.requestQuit ? m_cfg.requestQuit() : false;
    if (!going) {
        // 4. Refused: nothing happened. No helper, no changed file, no marker.
        m_quitting = false;
        disarmInstall();
        qCInfo(lcFlow) << "quit was cancelled - back to" << enumName(m_prepareFrom);
        setState(m_prepareFrom == State::InstallFailed ? State::InstallFailed : State::Ready);
        emit dialogRequested();  // the surface had let go of it
        return;
    }
    // Armed only NOW, after the app said "going": an aboutToQuit that comes
    // while requestQuit() still runs (the app's own question open in an inner
    // loop, and Windows ending the session with WM_ENDSESSION, which makes Qt
    // send aboutToQuit at once) must not start the install - the user may
    // still answer "Cancel". Then the session ends without an install, and
    // the marker is judged "unknown" at the next start.
    // "true" promises aboutToQuit; an app that keeps running gets it withdrawn.
    if (!m_prepared) return;  // withdrawn while requestQuit() ran
    m_installArmed = true;
    m_quitTimer.start(m_tuning.quitDeadlineMs);
}

void UpdateFlow::disarmInstall()
{
    m_installArmed = false;
    m_quitTimer.stop();
    if (m_prepared) m_backend->discard(*m_prepared);
    m_prepared.reset();
    m_settings.clearPendingInstall();
}

bool UpdateFlow::retryInstall()
{
    if (!canPerform(Action::RetryInstall)) return false;
    if (m_downloadedPath.isEmpty() || !QFileInfo(m_downloadedPath).isFile()) {
        m_detail = QStringLiteral("the verified file is gone");  // no local path in "Details"
        m_capability.method = InstallMethod::Manual;
        m_capability.reason = ManualReason::Other;
        setState(State::ManualInstall);
        return false;
    }
    return quitAndInstall();
}

void UpdateFlow::onAboutToQuit()
{
    // The ONLY place that starts the install — after the app's own prompt
    // has let it go.
    m_quitTimer.stop();
    if (!m_installArmed || !m_prepared) return;
    m_installArmed = false;
    // An overridden source (self-test only) never reaches a real install:
    // the switch is for pictures and tests, not a way in for other bytes.
    if (m_settings.dryRunLaunch() || m_sourceOverridden) {
        qCInfo(lcFlow).noquote()
            << (m_sourceOverridden ? "dry run (update source overridden), would start:"
                                   : "dry run, would start:")
            << m_prepared->description;
        m_backend->discard(*m_prepared);
        m_settings.clearPendingInstall();
        return;
    }
    const LaunchResult lr = m_backend->launch(*m_prepared);
    if (!lr.started) {
        qCWarning(lcFlow).noquote() << "install could not be started:" << lr.error;
        PendingInstall p = m_settings.pendingInstall();
        p.launchError = lr.error.isEmpty() ? QStringLiteral("launch failed") : lr.error;
        m_settings.setPendingInstall(p);
    }
}

void UpdateFlow::simulateAboutToQuitForTests() { onAboutToQuit(); }

void UpdateFlow::confirmStarted()
{
    if (!m_cfg.confirmStartExplicitly) return;
    if (!m_started) {
        m_confirmRequested = true;  // evaluatePendingInstall() picks it up
        return;
    }
    writeConfirmationNow();
}

void UpdateFlow::writeConfirmationNow()
{
    if (!m_confirmWaiting) return;  // nothing pending, or already written
    m_confirmWaiting = false;
    StartConfirmation c;
    c.version = m_cfg.version;
    c.attempt = m_lastPending.attempt;
    c.pid = QCoreApplication::applicationPid();
    QString err;
    if (!writeStartConfirmation(m_lastPending.confirmFile, c, &err)) {
        qCWarning(lcFlow).noquote() << "start confirmation not written:" << err;
    }
    m_settings.clearPendingInstall();
}

void UpdateFlow::evaluatePendingInstall()
{
    const PendingInstall p = m_settings.pendingInstall();
    if (!p.isValid()) return;
    const QVersionNumber running = QVersionNumber::fromString(m_cfg.version);
    const bool isNew = QVersionNumber::compare(running, QVersionNumber::fromString(p.toVersion)) == 0;
    const bool isOld = QVersionNumber::compare(running, QVersionNumber::fromString(p.fromVersion)) == 0;

    // A helper may be waiting for exactly this line. It is written on the
    // first pass of the event loop, not here: everything the app does
    // synchronously after start() (show the window, restore the last project,
    // connect) has passed by then. A new version that dies or hangs before
    // that never confirms, and the helper takes the way back. The marker goes
    // in the same pass, so the old version, started again, still judges the
    // attempt from the result file. With Config::confirmStartExplicitly both
    // wait for confirmStarted().
    m_lastPending = p;
    if (isNew && !p.confirmFile.isEmpty()) {
        m_confirmWaiting = true;
        if (!m_cfg.confirmStartExplicitly || m_confirmRequested) {
            QTimer::singleShot(0, this, [this] { writeConfirmationNow(); });
        } else {
            qCInfo(lcFlow) << "start confirmation waits for the app (confirmStartExplicitly)";
        }
    } else {
        m_settings.clearPendingInstall();  // judged once, in either case
    }
    // Later: a helper may still be finishing.
    QTimer::singleShot(m_cleanupDelayMs, this, [this, kind = p.kind] {
        m_backend->cleanupAfterStart(installContext(kind));
    });

    std::optional<InstallOutcome> outcome;
    QString outcomeError;
    if (!p.resultFile.isEmpty()) {
        outcome = readInstallOutcome(p.resultFile, &outcomeError);
        if (outcome) QFile::remove(p.resultFile);
    }

    if (isNew && p.launchError.isEmpty()) {
        qCInfo(lcFlow).noquote() << "updated from" << p.fromVersion << "to" << p.toVersion;
        m_settings.setInstallBlockedCode(QString());
        m_updatedHint = true;
        // after a confirmed install the download goes - but only a file
        // inside our own download folders, never a path the marker names
        // elsewhere.
        const QString dir = QFileInfo(p.file).absolutePath();
        const QString sub = m_cfg.productName + QStringLiteral("-Updates");
        const bool ours = !m_downloadDirOverride.isEmpty()
                              ? dir == QFileInfo(m_downloadDirOverride).absoluteFilePath()
                              : (QFileInfo(dir).fileName() == sub
                                 || QFileInfo(dir).fileName().startsWith(sub + QLatin1Char('-')));
        if (ours && QFileInfo(p.file).isFile()) QFile::remove(p.file);
        return;
    }
    if (isOld) {
        const bool automatic = p.method == installMethodName(InstallMethod::Automatic);
        if (!automatic && p.launchError.isEmpty()) {
            // The system installer was started; whether the user finished it
            // is not ours to know. No failure state.
            qCInfo(lcFlow).noquote() << "still" << p.fromVersion
                                     << "after handing" << p.toVersion << "to the system installer";
            return;
        }
        // The verdict for the old version. A missing or contradicting result
        // is "unknown" - never "failed", never state 13.
        const auto judge = [this, p](std::optional<InstallOutcome> outcome) {
            // Only a result of THIS attempt counts. A marker of an older
            // stand has no mark; then the result is taken as before.
            if (outcome && !p.attempt.isEmpty() && outcome->attempt != p.attempt) {
                qCWarning(lcFlow).noquote()
                    << "install of" << p.toVersion << "- the result file"
                    << (outcome->attempt.isEmpty() ? QStringLiteral("carries no attempt mark")
                                                   : QStringLiteral("belongs to another attempt"))
                    << "and is not taken";
                outcome.reset();
            }
            if (p.launchError.isEmpty()
                && (!outcome || outcome->result == InstallOutcome::Result::Ok)) {
                qCWarning(lcFlow).noquote()
                    << "install of" << p.toVersion << "- outcome unknown:"
                    << (outcome ? "helper says ok, but" : "no readable result, and")
                    << p.fromVersion << "is running";
                return;
            }
            if (outcome && !outcome->code.isEmpty()) {
                m_settings.setInstallBlockedCode(outcome->code);  // Manual next time
            }
            m_cause = outcome && outcome->result == InstallOutcome::Result::Cancelled
                          ? Cause::InstallCancelled : Cause::None;
            m_detail = !p.launchError.isEmpty() ? p.launchError
                     : QStringLiteral("%1 at step '%2': %3")
                           .arg(installOutcomeResultName(outcome->result), outcome->step,
                                outcome->reason);
            qCWarning(lcFlow).noquote() << "install of" << p.toVersion << "failed:" << m_detail;
            m_failedHint = true;
            // A late verdict (after the grace wait) may meet a flow that moved
            // on: in a resting state it takes over; while the user is busy
            // with an update (4, 5, Installing ...) only the hint tells it.
            static const QSet<State> resting = {State::Idle, State::UpToDate, State::Available,
                                                State::NotPublished, State::CheckFailed,
                                                State::Refused};
            if (!resting.contains(m_state)) {
                updateHint();
                return;
            }
            // 13 is about the MARKER's file and version: an offer a silent
            // check found meanwhile would mix another artifact into "retry".
            m_offer.reset();
            m_artifact.reset();
            m_downloadedPath = p.file;
            setState(State::InstallFailed);
            // Design, state 13: opens once by itself at this start.
            QTimer::singleShot(0, this, [this] {
                if (m_state == State::InstallFailed) {
                    // The hint stays next to the dialog (design, hint table):
                    // it goes after a click, with its x, or after a good update.
                    updateHint();
                    emit dialogRequested();
                }
            });
        };
        if (!p.launchError.isEmpty() || outcome) {
            judge(outcome);
            return;
        }
        // No readable result yet: read again for a while before judging.
        const int grace = m_tuning.outcomeGraceMs;
        auto* poll = new QTimer(this);
        auto waited = std::make_shared<QElapsedTimer>();
        waited->start();
        poll->setInterval(250);
        connect(poll, &QTimer::timeout, this, [poll, p, judge, waited, grace] {
            QString err;
            const std::optional<InstallOutcome> late = readInstallOutcome(p.resultFile, &err);
            if (late) QFile::remove(p.resultFile);
            if (late || waited->elapsed() >= grace) {
                poll->stop();
                poll->deleteLater();
                if (!late) {
                    qCInfo(lcFlow).noquote() << "no install result after" << grace << "ms:" << err;
                }
                judge(late);
            }
        });
        poll->start();
        return;
    }
    qCInfo(lcFlow).noquote() << "pending install" << p.fromVersion << "->" << p.toVersion
                             << "does not match the running" << m_cfg.version << "- dropped";
}

// ===========================================================================
// Hint and dialog
// ===========================================================================

void UpdateFlow::setDialogVisible(bool visible)
{
    if (visible == m_dialogVisible) return;
    m_dialogVisible = visible;
    updateHint();
}

void UpdateFlow::updateHint()
{
    Hint h;
    const QHash<QString, QString> a = args();
    const auto make = [&](HintKind k, const char* key, const char* icon, const char* role,
                          const char* surface, bool dismissable) {
        h.kind = k;
        h.text = m_texts.get(key, a);
        h.version = a.value(QStringLiteral("neu"));
        h.iconName = QString::fromLatin1(icon);
        h.iconRole = QString::fromLatin1(role);
        h.surfaceRole = QString::fromLatin1(surface);
        h.dismissable = dismissable;
        if (dismissable) h.dismissLabel = m_texts.get("n.dismiss");
    };
    if (m_state == State::Ready || m_state == State::Installing) {
        make(HintKind::Ready, "n.ready", "check-circle", "accent", "accentTint", false);
    } else if (m_state == State::Downloading && !m_dialogVisible) {
        // The percentage changes at most once a second (the design).
        const qint64 ms = now().toMSecsSinceEpoch();
        const int pct = m_total > 0 ? static_cast<int>((m_received * 100) / m_total) : 0;
        if (m_hint.kind != HintKind::Loading || m_lastHintPercentMs < 0
            || ms - m_lastHintPercentMs >= 1000) {
            m_hintPercent = pct;
            m_lastHintPercentMs = ms;
            if (m_hintCatchUp) m_hintCatchUp->stop();
        } else if (pct != m_hintPercent) {
            // Held back by the one-second rule. If no further progress comes
            // (a stalled download), nothing would ever show it: look again
            // when the second is over, so the hint is at most a second late.
            if (!m_hintCatchUp) {
                m_hintCatchUp = new QTimer(this);
                m_hintCatchUp->setSingleShot(true);
                connect(m_hintCatchUp, &QTimer::timeout, this, &UpdateFlow::updateHint);
            }
            if (!m_hintCatchUp->isActive()) {
                const qint64 left = 1000 - (ms - m_lastHintPercentMs);
                m_hintCatchUp->start(static_cast<int>(std::clamp<qint64>(left, 1, 1000)));
            }
        }
        QHash<QString, QString> la = a;
        la.insert(QStringLiteral("prozent"), m_texts.locale().toString(m_hintPercent));
        make(HintKind::Loading, "n.loading", "download-simple", "muted", "raised", false);
        h.text = m_texts.get("n.loading", la);
    } else if (m_failedHint && !m_failedDismissed) {
        make(HintKind::InstallFailed, "n.failed", "warning-circle", "danger", "dangerTint", true);
    } else if (m_updatedHint && !m_updatedDismissed) {
        make(HintKind::Updated, "n.done", "check-circle", "success", "successTint", true);
    } else if ((m_state == State::Available
                || (m_state == State::DownloadFailed && m_artifact))  // failed in the background
               && !m_dialogVisible && m_offer
               && !m_dismissedAvail.contains(m_offer->versionString)
               && !policy::isSkipped(m_offer->version, m_settings.skippedVersion())) {
        make(HintKind::Available, "n.avail", "arrow-circle-up", "accent", "accentTint", true);
        h.tooltip = m_texts.get("n.tip.avail");
    }
    if (m_hintCatchUp && h.kind != HintKind::Loading) m_hintCatchUp->stop();
    const bool changed = h.kind != m_hint.kind || h.text != m_hint.text;
    m_hint = h;
    if (changed) emit hintChanged();
}

bool UpdateFlow::openFromHint()
{
    switch (m_hint.kind) {
    case HintKind::None:
        return false;
    case HintKind::Available:
    case HintKind::Loading:
    case HintKind::Ready:
        break;  // the state is already the one the hint names
    case HintKind::Updated:
        m_updatedDismissed = true;
        m_offer.reset();
        m_artifact.reset();
        setState(State::Updated);
        break;
    case HintKind::InstallFailed:
        m_failedDismissed = true;
        setState(State::InstallFailed);
        break;
    }
    updateHint();
    emit dialogRequested();
    return true;
}

void UpdateFlow::dismissHint()
{
    switch (m_hint.kind) {
    case HintKind::Available:
        if (m_offer) m_dismissedAvail.insert(m_offer->versionString);  // until next start
        break;
    case HintKind::Updated:       m_updatedDismissed = true; break;
    case HintKind::InstallFailed: m_failedDismissed = true;  break;
    default:
        return;  // not dismissable
    }
    updateHint();
}

// ===========================================================================
// Settings page
// ===========================================================================

bool UpdateFlow::autoCheck() const { return m_settings.autoCheck(); }
void UpdateFlow::setAutoCheck(bool on) { m_settings.setAutoCheck(on); }
QString UpdateFlow::skippedVersion() const { return m_settings.skippedVersion(); }
void UpdateFlow::resetSkippedVersion() { m_settings.clearSkippedVersion(); updateHint(); }
ProxyConfig UpdateFlow::proxyConfig() const { return m_settings.proxyConfig(); }

void UpdateFlow::setProxyConfig(const ProxyConfig& cfg)
{
    const ProxyConfig old = m_settings.proxyConfig();
    if (old.mode != cfg.mode || old.host != cfg.host || old.user != cfg.user) {
        forgetSessionPassword();  // it belonged to another proxy or user
    }
    m_settings.setProxyConfig(cfg);
    if (m_checker && !busy()) m_checker->setProxyConfig(cfg);
}

void UpdateFlow::forgetSessionPassword()
{
    m_sessionPassword.clear();
    m_oneShotPassword.clear();
    // QNetworkAccessManager caches proxy credentials itself; a fresh one is
    // the only way to make "forgotten" true.
    if (m_checker && !busy()) {
        m_checker.reset();
    } else {
        m_dropCheckerAfterOp = true;
    }
    emit proxyAuthChanged();
}

void UpdateFlow::restoreDefaults()
{
    m_settings.setAutoCheck(true);
    m_settings.clearSkippedVersion();
    setProxyConfig(ProxyConfig{});
    forgetSessionPassword();
    updateHint();  // an offer skipped before is one again
    emit settingsRestored();
}

// ===========================================================================
// Doors
// ===========================================================================

void UpdateFlow::setBaseUrlForTests(const QUrl& url) { m_baseUrl = url; m_checker.reset(); }
void UpdateFlow::setPublicKeyForTests(const QByteArray& key32) { m_publicKey = key32; m_checker.reset(); }
void UpdateFlow::setChannelForTests(const QString& channel) { m_channel = channel; m_checker.reset(); }
void UpdateFlow::setDownloadDirForTests(const QString& dir) { m_downloadDirOverride = dir; }
void UpdateFlow::setInstallBackend(std::unique_ptr<InstallBackend> b) { if (b) m_backend = std::move(b); }
void UpdateFlow::setClockForTests(std::function<QDateTime()> c) { if (c) m_clock = std::move(c); }
void UpdateFlow::setOfflineProbeForTests(std::function<bool()> p) { m_offlineProbe = std::move(p); }
void UpdateFlow::setAppLocationForTests(const QString& l) { m_appLocationOverride = l.isNull() ? QString(QLatin1String("")) : l; }
void UpdateFlow::setResultDirForTests(const QString& dir) { m_resultDirOverride = dir; }
void UpdateFlow::setInstallTuningForTests(const InstallTuning& t) { m_tuning = t; }
void UpdateFlow::setCleanupDelayForTests(int ms) { m_cleanupDelayMs = ms; }
void UpdateFlow::setCheckDeadlineForTests(int ms) { m_checkDeadlineMs = ms; }
void UpdateFlow::setDownloadStallTimeoutForTests(int ms)
{
    m_downloadStallMs = ms;
    if (m_checker) m_checker->setDownloadStallTimeoutForTests(ms);
}
void UpdateFlow::setSilentCheckTimingForTests(int startupDelayMs, int periodMs)
{
    m_startupDelayMs = startupDelayMs;
    m_periodMs = periodMs;
    if (m_silentTimer.isActive()) m_silentTimer.start(m_periodMs);
}

// ===========================================================================
// What the surfaces draw
// ===========================================================================

ReleaseNotes UpdateFlow::releaseNotes() const
{
    const QString lang = m_texts.language() == QLatin1String("de") ? QStringLiteral("de")
                                                                     : QStringLiteral("en");
    if (m_state == State::Updated) return parseReleaseNotes(m_lastPending.notes);
    if (!m_offer) return {};
    return parseReleaseNotes(m_offer->notes(lang));
}

QStringList UpdateFlow::quitFootnote(bool retry) const
{
    QStringList lines;
    const QString kind = m_artifact ? m_artifact->kind : m_lastPending.kind;
    if (retry) {
        lines << m_texts.get("s13.foot", args());
    } else {
        const bool self = m_capability.method == InstallMethod::Automatic;
        const auto a = args();
        if (kind == QLatin1String("msi")) lines << m_texts.get("s5.win", a);
        else if (self) lines << m_texts.get("s5.mac", a);
        else if (kind == QLatin1String("dmg")) lines << m_texts.get("x.s5.mac.dmg", a);
        else lines << m_texts.get("x.s5.lin.appimage", a);
    }
    if (m_cfg.quitImpact) {
        const QuitImpact q = m_cfg.quitImpact();
        if (q.runningSessions >= 2) {
            lines << m_texts.get("s5.qtmux.n", {{QStringLiteral("n"), m_texts.locale().toString(q.runningSessions)}});
        } else if (q.runningSessions == 1) {
            lines << m_texts.get("s5.qtmux.1");
        }
        if (q.unsavedWorkPossible) lines << m_texts.get("s5.work");
        if (q.busConnected) lines << m_texts.get("s5.bus");
    }
    return lines;
}

UpdateFlow::View UpdateFlow::view() const
{
    View v;
    v.state = m_state;
    v.cause = m_cause;
    v.windowTitle = m_texts.get("win.title");
    const auto a = args();
    const auto btn = [&](Action act, ButtonRole role, const char* key) {
        v.buttons.append(Button{act, role, m_texts.get(key, a), canPerform(act)});
    };
    const auto notesBlock = [&](const char* headKey) {
        v.showNotes = true;
        v.notesHead = m_texts.get(headKey, a);
        v.notesMeta = m_texts.get("cl.meta", a);
        if (releaseNotes().isEmpty()) v.notesEmpty = m_texts.get("cl.empty");
    };
    const auto icon = [&](const char* name, const char* role) {
        v.iconName = QString::fromLatin1(name);
        v.iconRole = QString::fromLatin1(role);
    };

    switch (m_state) {
    case State::Idle:
        break;
    case State::Checking:
        icon("circle-notch", "accent");
        v.title = m_texts.get("s1.title");
        v.lead = m_texts.get("s1.lead");
        if (m_slowHint) v.progressNote = m_texts.get("s1.slow");
        v.showProgress = true;
        v.progressIndeterminate = true;
        btn(Action::Cancel, ButtonRole::Secondary, "s1.cancel");
        v.escapeAction = Action::Cancel;  // Enter does nothing (the design)
        break;
    case State::UpToDate:
        icon("check-circle", "success");
        v.title = m_texts.get("s2.title");
        v.lead = m_texts.get("s2.lead", a);
        if (m_notesExpanded) notesBlock("s2.notes");
        if (!releaseNotes().isEmpty()) btn(Action::ToggleNotes, ButtonRole::Quiet, "s2.notes");
        btn(Action::Close, ButtonRole::Primary, "btn.close");
        v.enterAction = v.escapeAction = Action::Close;
        break;
    case State::Available:
        icon("arrow-circle-up", "accent");
        v.title = m_texts.get("s3.title");
        v.lead = m_texts.get("s3.lead", a);
        notesBlock("cl.head");
        btn(Action::SkipVersion, ButtonRole::Quiet, "s3.skip");
        btn(Action::Later, ButtonRole::Secondary, "btn.later");
        btn(Action::Download, ButtonRole::Primary, "s3.download");
        v.enterAction = Action::Download;
        v.escapeAction = Action::Later;
        break;
    case State::Downloading:
        icon("download-simple", "accent");
        v.title = m_texts.get("s4.title");
        v.lead = m_texts.get(m_downgradeDownload ? "s7.lead" : "s3.lead", a);
        notesBlock("cl.head");
        v.showProgress = true;
        v.progressIndeterminate = m_verifying;
        v.progressPermille = m_total > 0 ? static_cast<int>((m_received * 1000) / m_total) : 0;
        if (m_verifying) {
            v.progressLeft = m_texts.get("s4.verify");
        } else {
            v.progressLeft = m_texts.get("s4.amount", a);
            v.progressRight = m_texts.get("s4.percent", a);
        }
        v.footnoteIcon = QStringLiteral("info");
        v.footnote << m_texts.get("s4.hint");
        btn(Action::CancelDownload, ButtonRole::Secondary, "s4.cancel");
        v.escapeAction = Action::HideWindow;  // the download goes on
        break;
    case State::Ready:
    case State::Installing:
        icon("check-circle", "success");
        v.title = m_texts.get(m_downgradeDownload ? "s7.ready" : "s5.title", a);
        v.lead = m_texts.get("s5.lead", a);
        notesBlock("cl.head");
        v.footnoteIcon = QStringLiteral("power");
        v.footnoteBoxed = true;
        v.footnote = quitFootnote(false);
        if (m_state == State::Installing && m_preparing) {
            v.showProgress = true;
            v.progressIndeterminate = true;
            v.progressLeft = m_texts.get("x.s5.preparing");
            btn(Action::Cancel, ButtonRole::Secondary, "s1.cancel");
            v.escapeAction = Action::Cancel;
        }
        if (m_state == State::Ready) {
            btn(Action::Later, ButtonRole::Secondary, "btn.later");
            btn(Action::QuitAndInstall, ButtonRole::Primary, m_downgradeDownload ? "s7.go" : "s5.go");
            v.enterAction = Action::QuitAndInstall;
            v.escapeAction = Action::Later;
        }
        break;
    case State::Updated:
        icon("check-circle", "success");
        v.title = m_texts.get("s6.title", a);
        v.lead = m_texts.get("s6.lead", {{QStringLiteral("alt"), m_lastPending.fromVersion}});
        notesBlock("cl.head");
        btn(Action::Close, ButtonRole::Primary, "btn.close");
        v.enterAction = v.escapeAction = Action::Close;
        break;
    case State::Downgrade:
        icon("warning", "warn");
        v.title = m_texts.get("s7.title");
        v.lead = m_texts.get("s7.lead", a);
        v.extra << m_texts.get("s7.meta", a);
        v.warning = m_texts.get("s7.warn", a);
        btn(Action::DownloadAnyway, ButtonRole::Warning, "s7.anyway");
        btn(Action::Close, ButtonRole::Primary, "btn.close");
        v.enterAction = v.escapeAction = Action::Close;
        break;
    case State::NoPackage:
        icon("package", "muted");
        v.title = m_texts.get("s8.title");
        v.lead = m_texts.get("s8.lead", a);
        v.extra << m_texts.get("s8.next");
        btn(Action::Close, ButtonRole::Primary, "btn.close");
        v.enterAction = v.escapeAction = Action::Close;
        break;
    case State::NotPublished:
        icon("info", "muted");
        v.title = m_texts.get("s9.title");
        v.lead = m_texts.get("s9.lead", a);
        btn(Action::Close, ButtonRole::Primary, "btn.close");
        v.enterAction = v.escapeAction = Action::Close;
        break;
    case State::CheckFailed: {
        icon("wifi-slash", "danger");
        v.title = m_texts.get("s10.title");
        const char* lead = m_cause == Cause::Offline ? "s10.offline"
                         : m_cause == Cause::Garbled ? "s10.garbled"
                         : m_cause == Cause::AuthCancelled ? "s10.auth"
                         : m_cause == Cause::ProxyIncomplete ? "x.s10.proxy" : "s10.unreachable";
        v.lead = m_texts.get(lead, a);
        v.details = m_detail;
        btn(Action::ProxySettings, ButtonRole::Quiet, "s10.settings");
        btn(Action::Close, ButtonRole::Secondary, "btn.close");
        btn(Action::RetryCheck, ButtonRole::Primary, "s10.retry");
        v.enterAction = Action::RetryCheck;
        v.escapeAction = Action::Close;
        break;
    }
    case State::Refused: {
        icon("shield-check", "accent");
        v.title = m_texts.get("s11.title");
        v.lead = m_texts.get("s11.lead");
        const char* why = m_cause == Cause::Signature ? "s11.sig"
                        : m_cause == Cause::WrongProduct ? "s11.product"
                        : m_cause == Cause::Size ? "s11.size" : "s11.replay";
        v.reasonBox = m_texts.get(why);
        v.reasonIcon = QStringLiteral("shield-check");
        v.reasonRole = QStringLiteral("muted");
        v.extra << m_texts.get("s11.calm");
        v.details = m_detail;
        btn(Action::RetryCheck, ButtonRole::Secondary, "s11.retry");
        btn(Action::Close, ButtonRole::Primary, "btn.close");
        v.enterAction = v.escapeAction = Action::Close;
        break;
    }
    case State::DownloadFailed: {
        icon("warning-circle", "danger");
        v.title = m_texts.get("s12.title");
        const char* lead = m_cause == Cause::Corrupt ? "s12.corrupt"
                         : m_cause == Cause::FileWrite ? "x.s12.file"
                         : m_cause == Cause::ServerFile ? "x.s12.server"
                         : m_cause == Cause::ProxyIncomplete ? "x.s10.proxy" : "s12.conn";
        v.lead = m_texts.get(lead, a);
        v.details = m_detail;
        if (m_cause == Cause::ProxyIncomplete) {
            btn(Action::ProxySettings, ButtonRole::Quiet, "s10.settings");
        }
        btn(Action::Close, ButtonRole::Secondary, "btn.close");
        btn(Action::RetryDownload, ButtonRole::Primary, "s12.retry");
        v.enterAction = Action::RetryDownload;
        v.escapeAction = Action::Close;
        break;
    }
    case State::InstallFailed:
        icon("warning-circle", "danger");
        if (m_cause == Cause::InstallCancelled) {
            v.title = m_texts.get("x.s13c.title");
            v.lead = m_texts.get("x.s13c.lead", a);
        } else {
            v.title = m_texts.get("s13.title");
            v.lead = m_texts.get("s13.lead", a);
        }
        v.extra << m_texts.get("s13.next");
        v.details = m_detail;
        v.footnoteIcon = QStringLiteral("power");
        v.footnoteBoxed = true;
        v.footnote = quitFootnote(true);
        btn(Action::InstallManually, ButtonRole::InText, "s13.manual");
        btn(Action::Close, ButtonRole::Secondary, "btn.close");
        btn(Action::RetryInstall, ButtonRole::Primary, "s13.go");
        v.enterAction = Action::RetryInstall;
        v.escapeAction = Action::Close;
        break;
    case State::ManualInstall: {
        icon("folder-open", "accent");
        v.title = m_texts.get("s14.title");
        const char* lead = manualLeadKey(m_capability.reason);
        v.lead = m_texts.get(lead, a);
        const QString kind = m_artifact ? m_artifact->kind : m_lastPending.kind;
        v.steps << m_texts.get("s14.step1");
        if (kind == QLatin1String("dmg")) v.steps << m_texts.get("s14.mac2");
        else if (kind == QLatin1String("msi")) v.steps << m_texts.get("s14.win2");
        else v.steps << m_texts.get("s14.lin2");
        if (m_quarantine == QuarantineState::Present
            || m_capability.reason == ManualReason::Quarantined) {
            v.notice = m_texts.get("x.s14.quarantine");
        }
        v.details = !m_capability.detail.isEmpty() ? m_capability.detail : m_detail;
        btn(Action::Close, ButtonRole::Secondary, "btn.close");
#if defined(Q_OS_MACOS)
        btn(Action::ShowInFolder, ButtonRole::Primary, "x.s14.show.mac");
#else
        btn(Action::ShowInFolder, ButtonRole::Primary, "s14.show");
#endif
        v.enterAction = Action::ShowInFolder;
        v.escapeAction = Action::Close;
        break;
    }
    }
    if (m_serialUnreadable && v.notice.isEmpty() && m_state != State::Idle) {
        v.notice = m_texts.get("x.serial");
    }
    v.quitting = m_quitting;
    return v;
}

QVariantMap UpdateFlow::viewMap() const
{
    const View v = view();
    QVariantMap m;
    m.insert(QStringLiteral("state"), static_cast<int>(v.state));
    m.insert(QStringLiteral("stateName"), enumName(v.state));
    m.insert(QStringLiteral("cause"), enumName(v.cause));
    m.insert(QStringLiteral("windowTitle"), v.windowTitle);
    m.insert(QStringLiteral("iconName"), v.iconName);
    m.insert(QStringLiteral("iconRole"), v.iconRole);
    m.insert(QStringLiteral("title"), v.title);
    m.insert(QStringLiteral("lead"), v.lead);
    m.insert(QStringLiteral("extra"), v.extra);
    m.insert(QStringLiteral("warning"), v.warning);
    m.insert(QStringLiteral("notice"), v.notice);
    m.insert(QStringLiteral("steps"), v.steps);
    m.insert(QStringLiteral("showNotes"), v.showNotes);
    m.insert(QStringLiteral("notesHead"), v.notesHead);
    m.insert(QStringLiteral("notesMeta"), v.notesMeta);
    m.insert(QStringLiteral("notesEmpty"), v.notesEmpty);
    m.insert(QStringLiteral("showProgress"), v.showProgress);
    m.insert(QStringLiteral("progressIndeterminate"), v.progressIndeterminate);
    m.insert(QStringLiteral("progressPermille"), v.progressPermille);
    m.insert(QStringLiteral("progressLeft"), v.progressLeft);
    m.insert(QStringLiteral("progressRight"), v.progressRight);
    m.insert(QStringLiteral("footnoteIcon"), v.footnoteIcon);
    m.insert(QStringLiteral("footnoteBoxed"), v.footnoteBoxed);
    m.insert(QStringLiteral("quitting"), v.quitting);
    m.insert(QStringLiteral("footnote"), v.footnote);
    m.insert(QStringLiteral("details"), v.details);
    m.insert(QStringLiteral("reasonBox"), v.reasonBox);
    m.insert(QStringLiteral("reasonIcon"), v.reasonIcon);
    m.insert(QStringLiteral("reasonRole"), v.reasonRole);
    m.insert(QStringLiteral("progressNote"), v.progressNote);
    QVariantList buttons;
    for (const Button& b : v.buttons) {
        QVariantMap bm;
        bm.insert(QStringLiteral("action"), static_cast<int>(b.action));
        bm.insert(QStringLiteral("actionName"), enumName(b.action));
        bm.insert(QStringLiteral("role"), enumName(b.role));
        bm.insert(QStringLiteral("label"), b.label);
        bm.insert(QStringLiteral("enabled"), b.enabled);
        buttons.append(bm);
    }
    m.insert(QStringLiteral("buttons"), buttons);
    m.insert(QStringLiteral("enterAction"), static_cast<int>(v.enterAction));
    m.insert(QStringLiteral("escapeAction"), static_cast<int>(v.escapeAction));
    return m;
}

QVariantMap UpdateFlow::hintMap() const
{
    QVariantMap m;
    m.insert(QStringLiteral("kind"), enumName(m_hint.kind));
    m.insert(QStringLiteral("visible"), m_hint.kind != HintKind::None);
    m.insert(QStringLiteral("text"), m_hint.text);
    m.insert(QStringLiteral("version"), m_hint.version);
    m.insert(QStringLiteral("iconName"), m_hint.iconName);
    m.insert(QStringLiteral("iconRole"), m_hint.iconRole);
    m.insert(QStringLiteral("surfaceRole"), m_hint.surfaceRole);
    m.insert(QStringLiteral("tooltip"), m_hint.tooltip);
    m.insert(QStringLiteral("dismissable"), m_hint.dismissable);
    m.insert(QStringLiteral("dismissLabel"), m_hint.dismissLabel);
    return m;
}

UpdateFlow::ProxyAuth UpdateFlow::proxyAuth() const
{
    ProxyAuth p;
    p.pending = m_authPending;
    p.host = m_authHost;
    p.port = m_authPort;
    p.previousRejected = m_authRejected;
    p.user = m_sessionUser;
    const auto a = args();
    p.title = m_texts.get("s15.title");
    p.lead = m_texts.get("s15.lead", a);
    p.userLabel = m_texts.get("s15.user");
    p.passwordLabel = m_texts.get("s15.pass");
    p.rememberLabel = m_texts.get("s15.remember");
    p.never = m_texts.get("s15.never");
    if (m_authRejected) p.rejected = m_texts.get("s15.rejected");
    p.signIn = m_texts.get("s15.ok");
    p.cancel = m_texts.get("s15.cancel");
    return p;
}

QVariantMap UpdateFlow::proxyAuthMap() const
{
    const ProxyAuth p = proxyAuth();
    QVariantMap m;
    m.insert(QStringLiteral("pending"), p.pending);
    m.insert(QStringLiteral("host"), p.host);
    m.insert(QStringLiteral("port"), p.port);
    m.insert(QStringLiteral("previousRejected"), p.previousRejected);
    m.insert(QStringLiteral("user"), p.user);
    m.insert(QStringLiteral("title"), p.title);
    m.insert(QStringLiteral("lead"), p.lead);
    m.insert(QStringLiteral("userLabel"), p.userLabel);
    m.insert(QStringLiteral("passwordLabel"), p.passwordLabel);
    m.insert(QStringLiteral("rememberLabel"), p.rememberLabel);
    m.insert(QStringLiteral("never"), p.never);
    m.insert(QStringLiteral("rejected"), p.rejected);
    m.insert(QStringLiteral("signIn"), p.signIn);
    m.insert(QStringLiteral("cancel"), p.cancel);
    return m;
}

}  // namespace appupdate
