#pragma once

// appupdate::UpdateFlow — THE update flow of all four desktop apps
// (update contract V2, 2026-11-19, docs/SHARED.md; behaviour and wording:
// the update design of the same day).
//
// CANONICAL: QTmux vendors src/update/ byte-identically. Qt6::Core and
// Qt6::Network only. No window, no nested event loop (no exec()), ever — the
// proxy sign-in is two-step: the flow raises proxyAuthPending, the surface
// asks, provideProxyCredentials() repeats exactly the interrupted operation.
//
// One object per app, alive as long as the app, independent of any dialog.
// The surfaces (Widgets in the hub, QML in QTmux) only BIND: everything a
// surface needs to draw a state — headline, sentences, which buttons in which
// order, which one is the primary, what Enter and Esc do — comes out of
// view() / viewMap(). A surface that decides something is a defect (the
// "identical" criterion of the update contract).
//
// What the app gives is Config, nothing else. Key, base URL
// and channel belong to the hub; tests and acceptance runs reach them through
// the doors at the end of this header, or the start options.

#include "update/InstallBackend.hpp"
#include "update/ProxyConfig.hpp"
#include "update/Quarantine.hpp"
#include "update/ReleaseNotes.hpp"
#include "update/UpdateChecker.hpp"
#include "update/UpdateManifest.hpp"
#include "update/UpdateSettings.hpp"
#include "update/UpdateTexts.hpp"

#include <QDateTime>
#include <QList>
#include <QObject>
#include <QSet>
#include <QStringList>
#include <QTimer>
#include <QUrl>
#include <QVariantMap>

#include <functional>
#include <memory>
#include <optional>

class QSettings;

namespace appupdate {

// Fixed production source (the update contract not a setting anywhere).
inline constexpr const char* kProductionBaseUrl = "https://nobser.de/updates";

// A check that has no answer after this long ends: by hand in state 10
// (cause Unreachable), silently without any change. Measured reason: a
// request whose connection is refused can stay without any reply for a long
// time (Qt 6.10.3, macOS), and the dialog would sit in "Checking" for good.
// Not for the download — there the progress counts. Not while the user is
// asked for a proxy password; the answer starts it anew.
inline constexpr int kCheckDeadlineMs = 60'000;

class UpdateFlow : public QObject {
    Q_OBJECT
    Q_PROPERTY(int state READ stateInt NOTIFY stateChanged)
    Q_PROPERTY(QVariantMap view READ viewMap NOTIFY viewChanged)
    Q_PROPERTY(QVariantMap hint READ hintMap NOTIFY hintChanged)
    Q_PROPERTY(QVariantMap proxyAuth READ proxyAuthMap NOTIFY proxyAuthChanged)
    Q_PROPERTY(QVariantList notes READ notesList NOTIFY viewChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY stateChanged)
    Q_PROPERTY(bool serialUnreadable READ serialUnreadable NOTIFY serialUnreadableChanged)

public:
    // the design, numbered 1…15 like the design (Idle and Installing are the
    // flow's own: nothing is shown / the app is on its way out).
    enum class State {
        Idle = 0,
        Checking = 1,
        UpToDate = 2,
        Available = 3,
        Downloading = 4,
        Ready = 5,
        Updated = 6,
        Downgrade = 7,
        NoPackage = 8,
        NotPublished = 9,
        CheckFailed = 10,
        Refused = 11,
        DownloadFailed = 12,
        InstallFailed = 13,
        ManualInstall = 14,
        Installing = 16,
    };
    Q_ENUM(State)

    // Every button of every state, by meaning — never one button with three
    // meanings.
    enum class Action {
        None = 0,
        CheckNow, Cancel, CancelDownload, Download, DownloadAnyway,
        SkipVersion, Later, Close, HideWindow, QuitAndInstall,
        RetryCheck, RetryDownload, RetryInstall,
        InstallManually, ShowInFolder, ProxySettings, ToggleNotes,
        SignIn, CancelSignIn,
    };
    Q_ENUM(Action)

    enum class ButtonRole { Primary, Secondary, Warning, Quiet, InText };
    Q_ENUM(ButtonRole)

    // Why a check failed (10) / an offer was refused (11) / a download failed
    // (12) — one value per sentence of the design.
    enum class Cause {
        None = 0,
        Offline, Unreachable, Garbled, AuthCancelled,         // 10
        ProxyIncomplete,                                     // 10, manual proxy without host/port
        Signature, WrongProduct, Replay, Size,               // 11
        Connection, Corrupt, FileWrite, ServerFile,          // 12
        InstallCancelled,                                    // 13, result=cancelled
    };
    Q_ENUM(Cause)

    enum class HintKind { None = 0, Available, Loading, Ready, Updated, InstallFailed };
    Q_ENUM(HintKind)

    // What ends when the app quits — DATA, the sentence comes from the
    // catalogue (keys s5.qtmux.n, s5.qtmux.1, s5.work, s5.bus).
    struct QuitImpact {
        int runningSessions = -1;       // QTmux; -1 = not applicable
        bool unsavedWorkPossible = false;  // RAFTNG, EmbyStudio
        bool busConnected = false;         // MacPCAN
    };

    struct Config {
        QString productSlug;      // "macpcan"
        QString productName;      // "MacPCAN"
        QString version;          // pure version for the comparison, "0.7.0"
        QString buildId;          // full id for display, "0.7.0+4db7212"
        QString language;         // UI language: "de" | "en" | "sv"
        // Asks the app to quit through its NORMAL way (closeAllWindows() or
        // the app's own closing path), so its "unsaved work?" prompt runs.
        // Returns false when the quit was refused (prompt cancelled) — then
        // nothing happens and the flow returns to Ready.
        // Returning true means: the app is on its way out; the install
        // starts in aboutToQuit, never before.
        std::function<bool()> requestQuit;
        std::function<QuitImpact()> quitImpact;  // optional
        // Windows: the MSI UpgradeCode, from the app's code - same source as
        // its .wxs. Empty for other systems or a product without MSI.
        QString windowsUpgradeCode;
        // The update source is fixed. Only the app's self-test may point the
        // flow at a fixture with --update-base-url / --update-channel; every
        // other run ignores both (and says so once in the log). Set it in the
        // app's CODE for the self-test run only - never from settings, the
        // environment or an argument.
        bool allowSourceOverrideForSelfTest = false;
    };

    struct Button {
        Action action = Action::None;
        ButtonRole role = ButtonRole::Secondary;
        QString label;
        bool enabled = true;
    };

    // Everything a surface needs for the current state (the design).
    struct View {
        State state = State::Idle;
        QString windowTitle;
        QString iconName;      // Phosphor: "circle-notch", "check-circle", …
        QString iconRole;      // "accent" | "success" | "warn" | "danger" | "muted"
        QString title;
        QString lead;
        QStringList extra;     // further sentences, in order (Zusatzsatz)
        QString warning;       // warn box (7), empty = none
        QString notice;        // info box, e.g. the quarantine remedy (14)
        // Reason box (11): why the offer was refused, drawn in its own box
        // with this icon - the surface never picks it out of `extra`.
        QString reasonBox;
        QString reasonIcon;    // Phosphor name: "shield-check" (as the design draws it)
        QString reasonRole;    // colour role: "muted" (grey in the box, accent in the title)
        QStringList steps;     // numbered steps (14)
        bool showNotes = false;     // release-notes block (3, 4, 5, 6; 2 on demand)
        QString notesHead;     // "What’s new in 0.8.0"
        QString notesMeta;     // "November 18, 2026 · 98.4 MB"
        QString notesEmpty;    // sentence when the release has no notes
        bool showProgress = false;
        bool progressIndeterminate = false;  // 1 and the verifying moment of 4
        int progressPermille = 0;
        QString progressLeft;  // "38.2 of 98.4 MB" or "Verifying…"
        QString progressRight; // "39%"
        // Line under the progress bar (1: "can take longer through a proxy"
        // after 3 s), so the bar does not move when it appears.
        QString progressNote;
        QString footnoteIcon;  // "power" (5, 13) | "info" (4) | empty
        bool footnoteBoxed = false;  // the footnote is drawn in its box (5, 13)
        QStringList footnote;  // one or two lines
        QString details;       // raw English message, under "Details" (10–14)
        QList<Button> buttons; // left → right
        Action enterAction = Action::None;
        Action escapeAction = Action::None;
        // The flow is quitting the app for the install (quitStarting() came).
        // The surface accepts its window being closed and runs no action.
        bool quitting = false;
        Cause cause = Cause::None;
    };

    struct Hint {
        HintKind kind = HintKind::None;
        QString text;
        QString version;
        QString iconName;
        QString iconRole;
        QString surfaceRole;   // fill: "accentTint" | "raised" | "successTint" | "dangerTint"
        QString tooltip;
        bool dismissable = false;
        QString dismissLabel;
    };

    struct ProxyAuth {
        bool pending = false;
        QString host;
        quint16 port = 0;
        bool previousRejected = false;
        QString user;          // prefilled
        QString title, lead, userLabel, passwordLabel, rememberLabel, never,
                rejected, signIn, cancel;
    };

    UpdateFlow(Config config, QSettings& settings, QObject* parent = nullptr);
    ~UpdateFlow() override;

    // Call once after the main window is up: migrates the settings,
    // evaluates a pending install from the previous run, and starts
    // the silent checks (first after startupDelay, then every 6 h).
    // After a self-update the start confirmation the helper waits for is
    // written on the first pass of the event loop, not inside start().
    void start();

    // Start options: --no-update-check (always), --update-base-url <url> and
    // --update-channel <name> (only with Config::allowSourceOverrideForSelfTest,
    // otherwise ignored with one log line). Unknown arguments are ignored.
    // Call before start().
    void applyStartOptions(const QStringList& arguments);

    // ---- State ---------------------------------------------------------------
    [[nodiscard]] State state() const { return m_state; }
    [[nodiscard]] int stateInt() const { return static_cast<int>(m_state); }
    [[nodiscard]] bool busy() const;
    [[nodiscard]] View view() const;
    [[nodiscard]] QVariantMap viewMap() const;
    [[nodiscard]] Hint hint() const { return m_hint; }
    [[nodiscard]] QVariantMap hintMap() const;
    [[nodiscard]] ProxyAuth proxyAuth() const;
    [[nodiscard]] QVariantMap proxyAuthMap() const;
    [[nodiscard]] ReleaseNotes releaseNotes() const;
    [[nodiscard]] QVariantList notesList() const { return notesToVariantList(releaseNotes()); }
    [[nodiscard]] bool serialUnreadable() const { return m_serialUnreadable; }
    // True from quitStarting() until the quit is refused or withdrawn.
    [[nodiscard]] bool isQuitting() const { return m_quitting; }
    // A start option moved the update source (self-test only, see
    // Config::allowSourceOverrideForSelfTest). Then every install is a dry
    // run: aboutToQuit never starts a helper or an installer.
    [[nodiscard]] bool sourceOverridden() const { return m_sourceOverridden; }
    [[nodiscard]] Cause cause() const { return m_cause; }
    [[nodiscard]] QString errorDetail() const { return m_detail; }
    [[nodiscard]] QString downloadedFile() const { return m_downloadedPath; }
    [[nodiscard]] QuarantineState quarantine() const { return m_quarantine; }
    [[nodiscard]] InstallCapability installCapability() const { return m_capability; }
    [[nodiscard]] std::optional<UpdateManifest> offer() const { return m_offer; }
    [[nodiscard]] UpdateTexts texts() const { return m_texts; }

    // Every icon name the flow can hand out (view: iconName, footnoteIcon,
    // reasonIcon; hint: iconName) - Phosphor "regular" names. A surface checks
    // its resources against this list; a name outside it is a defect.
    [[nodiscard]] static const QStringList& iconNames();

    // Is `action` allowed in the current state? perform() refuses the rest.
    [[nodiscard]] bool canPerform(Action action) const;

    // ---- Actions (the update contract: named, unambiguous) --------------------------
    // Dispatcher for surfaces: does what the button means. Returns false (and
    // logs) when the action is not allowed in this state.
    Q_INVOKABLE bool perform(Action action);

    bool checkNow();         // by hand (menu, settings › Check now)
    // The menu entry "Check for updates": shows the state as it is where
    // there is something to show (3 from a silent find, 4, 5, 12, 13, 14 -
    // a download that failed in the background must not be replaced by a new
    // check), otherwise checks. Emits dialogRequested() in both cases.
    Q_INVOKABLE bool openByHand();
    bool download();         // 3, and "Download anyway" in 7
    bool cancel();           // 1 → Idle; 4 → back to 3/7
    bool skipVersion();      // 3 only (never 7)
    bool quitAndInstall();   // 5
    bool retryCheck();       // 10, 11
    bool retryDownload();    // 12
    bool retryInstall();     // 13: re-checks the file, then quits again
    bool showInFolder();     // 14 (and 13): emits revealFileRequested
    bool installManually();  // 13 → 14

    // Proxy sign-in (the design). Each entry is sent exactly ONCE: a rejected
    // password is never offered again, the dialog comes back with
    // previousRejected. `remember` keeps it in memory until the app quits.
    Q_INVOKABLE void provideProxyCredentials(const QString& user, const QString& password,
                                             bool remember);
    Q_INVOKABLE void cancelProxyAuth();  // → 10 with cause AuthCancelled

    // ---- Dialog and hint ------------------------------------------------------
    // The surface tells the flow whether the dialog is on screen: the hint
    // "Downloading · 39 %" exists only while it is not (the design).
    Q_INVOKABLE void setDialogVisible(bool visible);
    [[nodiscard]] bool dialogVisible() const { return m_dialogVisible; }
    // Click on the hint: puts the flow into the state the hint opens and
    // emits dialogRequested(). Returns false without a hint.
    Q_INVOKABLE bool openFromHint();
    // The × of the hint (only where Hint::dismissable).
    Q_INVOKABLE void dismissHint();

    // ---- Settings page (the design) --------------------------------------------
    [[nodiscard]] bool autoCheck() const;
    void setAutoCheck(bool on);
    [[nodiscard]] QString skippedVersion() const;
    void resetSkippedVersion();
    [[nodiscard]] ProxyConfig proxyConfig() const;
    // Applies and stores. A new mode, host or user drops the session password
    //.
    void setProxyConfig(const ProxyConfig& cfg);
    [[nodiscard]] bool hasSessionPassword() const { return !m_sessionPassword.isEmpty(); }
    void forgetSessionPassword();
    // "Restore defaults" of a settings page: automatic checks on, no skipped
    // version, proxy "System" (type, host, port and user cleared), the session
    // password forgotten. The flow's own bookkeeping stays (last check,
    // lastSerial, the install marker, the code of a failed install), and so
    // does a proxy sign-in that is open right now — it belongs to the running
    // operation, not to the settings. Emits settingsRestored(), so an open
    // settings section also drops a manual entry that was not stored yet.
    void restoreDefaults();
    [[nodiscard]] QString installedBuildId() const { return m_cfg.buildId; }

    // ---- Test and acceptance doors ----------------------------
    void setBaseUrlForTests(const QUrl& url);
    void setPublicKeyForTests(const QByteArray& key32);
    void setChannelForTests(const QString& channel);
    void setDownloadDirForTests(const QString& dir);
    void setInstallBackend(std::unique_ptr<InstallBackend> backend);
    void setClockForTests(std::function<QDateTime()> nowUtc);
    void setSilentCheckTimingForTests(int startupDelayMs, int periodMs);
    void setCheckDeadlineForTests(int ms);  // kCheckDeadlineMs
    void setDownloadStallTimeoutForTests(int ms);  // kDownloadStallTimeoutMs (UpdateChecker.hpp)
    void setOfflineProbeForTests(std::function<bool()> isOffline);
    // Where the running app lives (InstallContext::appLocation). Default:
    // macOS bundle of applicationFilePath, Windows the exe, Linux $APPIMAGE.
    void setAppLocationForTests(const QString& location);
    void setResultDirForTests(const QString& dir);
    void setInstallTuningForTests(const InstallTuning& tuning);
    void setCleanupDelayForTests(int ms);
    // Stand-in for QCoreApplication::aboutToQuit in tests: runs exactly what
    // the real signal runs.
    void simulateAboutToQuitForTests();

    [[nodiscard]] QUrl baseUrl() const { return m_baseUrl; }
    [[nodiscard]] QString channel() const { return m_channel; }
    [[nodiscard]] bool silentChecksDisabled() const { return m_noSilentCheck; }

signals:
    void stateChanged();
    void viewChanged();
    void hintChanged();
    void proxyAuthChanged();
    void serialUnreadableChanged();
    // The surface should show the dialog (hint clicked; or once at start
    // after a failed install, the design on state 13).
    void dialogRequested();
    // The surface should close/hide the dialog (Close, Later, HideWindow,
    // Cancel in 1).
    void closeDialogRequested();
    // Show the file in the platform file manager (Gui code lives in the app
    // or the surface — QDesktopServices is not Qt Core).
    void revealFileRequested(const QString& path);
    // Open Settings › Updates (10, "Proxy settings").
    void proxySettingsRequested();
    // A silent check found `version` (also reflected in hint()).
    void updateFound(const QString& version);
    // "Quit and update": the flow is about to ask the app to quit
    // (Config::requestQuit). The surface hides the dialog and the proxy
    // sign-in NOW, without running an escape action; while isQuitting(), its
    // closeEvent accepts. If the quit is refused, dialogRequested() follows.
    void quitStarting();
    // restoreDefaults() has run; a settings surface shows the stored values
    // again, including fields that have the focus.
    void settingsRestored();

private:
    enum class Op { None, Check, Download };

    void setState(State s);
    void emitView();
    void updateHint();
    void runSilentCheck();
    void startCheck(bool silent);
    void onCheckResult(int generation, bool silent, std::optional<UpdateManifest> m,
                       const QString& error);
    void startDownload();
    void onDownloadResult(int generation, const QString& path, const QString& error);
    void onProgress(qint64 received, qint64 total);
    void onAboutToQuit();
    void evaluatePendingInstall();
    void ensureChecker();
    bool answerProxyChallenge(const QString& host, quint16 port, bool previousFailed,
                              QString* user, QString* password);
    void failCheck(UpdateChecker::ErrorKind kind, const QString& error);
    QString downloadDir() const;
    QString resultDir() const;
    QString appLocation() const;
    QDateTime now() const;
    InstallContext installContext(const QString& kind) const;
    [[nodiscard]] bool isDowngradeOffer() const;
    void onPrepared(int generation, PrepareResult r);
    void endPreparing(State back);
    // Withdraws an armed or prepared install: discard, marker gone.
    void disarmInstall();
    [[nodiscard]] QStringList quitFootnote(bool retry) const;
    [[nodiscard]] QHash<QString, QString> args() const;

    Config m_cfg;
    UpdateSettings m_settings;
    UpdateTexts m_texts;
    std::unique_ptr<UpdateChecker> m_checker;
    std::unique_ptr<InstallBackend> m_backend;

    QUrl m_baseUrl{QString::fromLatin1(kProductionBaseUrl)};
    QByteArray m_publicKey;
    QString m_channel;
    QString m_downloadDirOverride;
    QString m_resultDirOverride;
    QString m_appLocationOverride;
    std::function<QDateTime()> m_clock;
    std::function<bool()> m_offlineProbe;
    bool m_noSilentCheck = false;
    bool m_started = false;

    State m_state = State::Idle;
    Cause m_cause = Cause::None;
    QString m_detail;
    std::optional<UpdateManifest> m_offer;   // the manifest of the current offer
    std::optional<UpdateArtifact> m_artifact;
    bool m_downgradeDownload = false;       // 4/5 came from 7
    QString m_downloadedPath;
    QuarantineState m_quarantine = QuarantineState::NotApplicable;
    InstallCapability m_capability;
    std::optional<PreparedInstall> m_prepared;
    bool m_preparing = false;               // prepare() runs (Installing, "Preparing…")
    int m_prepareGen = 0;
    State m_prepareFrom = State::Ready;     // where Cancel/deadline return to
    QTimer m_prepareTimer;
    InstallTuning m_tuning;
    int m_cleanupDelayMs = kCleanupDelayMs;
    bool m_installArmed = false;            // aboutToQuit may launch
    bool m_quitting = false;                // between quitStarting() and the end
    bool m_sourceOverridden = false;        // --update-base-url/--update-channel took effect
    QTimer m_quitTimer;                     // kQuitDeadlineMs after requestQuit() said true
    bool m_notesExpanded = false;           // 2: "What's new in {alt}"
    bool m_slowHint = false;                // 1: s1.slow after 3 s with proxy
    bool m_verifying = false;               // 4: after 100 %, before 5
    qint64 m_received = 0;
    qint64 m_total = 0;
    qint64 m_failedAt = 0;                  // 12: where the connection dropped
    bool m_serialUnreadable = false;
    PendingInstall m_lastPending;           // evaluated at start (6, 13)

    // Silent checks
    QTimer m_silentTimer;
    QTimer m_slowTimer;
    QTimer m_checkTimer;                    // kCheckDeadlineMs for a running check
    int m_checkDeadlineMs = kCheckDeadlineMs;
    int m_downloadStallMs = kDownloadStallTimeoutMs;
    int m_startupDelayMs = 3000;
    int m_periodMs = 6 * 60 * 60 * 1000;
    QSet<QString> m_reportedVersions;       // once per run

    // Operation bookkeeping
    Op m_op = Op::None;
    Op m_authOp = Op::None;                 // the op a sign-in interrupted
    bool m_opSilent = false;
    int m_generation = 0;                   // results of an aborted op are ignored

    // Proxy sign-in
    bool m_authPending = false;
    bool m_authRejected = false;
    bool m_authAsked = false;               // the challenge of this op was raised
    QString m_authHost;
    quint16 m_authPort = 0;
    QString m_sessionUser;
    QString m_sessionPassword;              // memory only, never written
    QString m_oneShotPassword;              // "remember" off: this op only
    bool m_oneShotUsed = false;
    bool m_dropCheckerAfterOp = false;      // "remember" off: forget QNAM's cache too

    // Hint
    Hint m_hint;
    bool m_dialogVisible = false;
    QSet<QString> m_dismissedAvail;         // × on "available", until next start
    bool m_updatedHint = false;             // 6: previous run installed m_lastPending.toVersion
    bool m_failedHint = false;              // 13: previous run's install failed
    bool m_updatedDismissed = false;
    bool m_failedDismissed = false;
    qint64 m_lastHintPercentMs = -1;
    int m_hintPercent = -1;
    QTimer* m_hintCatchUp = nullptr;        // shows a held-back percentage late
};

}  // namespace appupdate
