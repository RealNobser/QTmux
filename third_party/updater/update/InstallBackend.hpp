#pragma once

// appupdate::InstallBackend — the seam between the update FLOW (UpdateFlow,
// update contract V2, docs/SHARED.md) and whatever actually installs a verified
// artifact (stage 2: self-installation; until then: the system installer via
// InstallerLauncher).
//
// CANONICAL: QTmux vendors src/update/ byte-identically. Qt6::Core only —
// no Widgets, no Network, no QSettings here.
//
// Revision 2 (2026-11-19): prepare() is asynchronous with a hard deadline, the automatic
// method is called Automatic (it covers the macOS self-replacement AND the
// Windows helper around msiexec), more manual reasons, a start confirmation,
// cleanupAfterStart, a machine-readable code, test doors and deadlines.
//
// The flow asks, hands over one order, and judges the outcome:
//
//   1. capability(context)   "Can this build install by itself HERE, and if
//                            not, why not?" Asked when the flow reaches Ready,
//                            so the dialog can say before the click what the
//                            button does. May run a HARMLESS probe (e.g.
//                            Windows `helper --probe` against AppLocker) —
//                            nothing that changes anything.
//   2. prepare(order, done)  Started by quitAndInstall() BEFORE the app quits.
//                            ASYNCHRONOUS: may check, mount read-only, copy to
//                            a staging place and verify the copy (about 2 s on
//                            macOS, measured), but starts NOTHING
//                            irreversible and no helper. `done` is called
//                            exactly once on the owner's thread — unless
//                            cancelPrepare() came first, then never.
//                            Everything it creates is undone by discard().
//                            The flow enforces kPrepareDeadlineMs itself and
//                            calls cancelPrepare() when it passes.
//   3. launch(prepared)      Called from QCoreApplication::aboutToQuit ONLY,
//                            i.e. after the app's own "unsaved work?" prompt
//                            has passed. Starts the detached helper (or the
//                            system installer) and returns at once. Must not
//                            block, open a window or spin an event loop.
//   4. discard(prepared)     The quit was cancelled, or launch() is not going
//                            to be called: remove whatever prepare() staged.
//   5. cleanupAfterStart(c)  On the next start, after the flow has judged the
//                            marker and the result file: remove leftovers of
//                            an earlier attempt. Their names belong to the
//                            backend: the macOS bundle swap keeps the old
//                            bundle as .<Name>.app.update-staged-<hex> next
//                            to the target, plus private temp folders; the
//                            Linux image swap keeps <target>.update-old (and
//                            may leave .update-new). The old copy goes only
//                            when what stands at the target is the genuine
//                            new one — until then it is the way back. Called
//                            kCleanupDelayMs after start, because a helper
//                            may still be finishing (it deletes the old copy
//                            itself after the confirmation). Must tolerate
//                            files that are already gone.
//
// After the restart the flow writes `version=<own version>` into
// order.startConfirmFilePath — that is how the helper learns the new
// version really runs — and reads the helper's result file
// (order.resultFilePath) with readInstallOutcome() below. The FORMAT of both
// files is part of this seam.
//
// Threading: all calls on the thread that owns the UpdateFlow (the GUI
// thread); a backend that works in the background delivers `done` back there.

#include <QByteArray>
#include <QString>
#include <QtGlobal>

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>

namespace appupdate {

// ---- Deadlines (in the hub, not only in a helper text) ----------------

// The helper waits this long for the app to be really gone (EOF on the
// lifeline, then the pid).
inline constexpr int kHelperWaitExitMs = 120'000;
// The helper waits this long for the start confirmation of the new version,
// counted from the moment it started that version. Without a confirmation it
// rolls back only when the new version no longer runs; if it still runs, the
// helper ends without a way back and reports start-unconfirmed (the flow of
// the running new version counts that as an update).
inline constexpr int kStartConfirmTimeoutMs = 60'000;
// The flow gives prepare() this long; then cancelPrepare() and InstallFailed.
inline constexpr int kPrepareDeadlineMs = 120'000;
// The flow calls cleanupAfterStart() this long after start().
inline constexpr int kCleanupDelayMs = 10'000;
// When the OLD version runs after an Automatic install and the result file
// is missing or unreadable, the flow reads it again for this long before it
// judges - and then a missing result is "unknown", never "failed". The helper
// writes the file (via .tmp + rename) BEFORE it restarts the old version, so
// it is normally there at once; 5 s cover a slow file system or a virus
// scanner holding the file, 50 times the helper's 100-ms tick.
inline constexpr int kOutcomeGraceMs = 5'000;
// After the app's requestQuit() said true, aboutToQuit must follow within
// this long; otherwise the flow withdraws the armed install (discard, marker
// gone, back to Ready) instead of letting it start at some later, unrelated
// quit. Between "true" and aboutToQuit only the app's own shutdown runs (its
// prompt has already been answered inside requestQuit); 15 s leave room for
// a slow save or ending child processes, and are short enough that the user
// still connects the withdrawal with the click. Not measured per app.
inline constexpr int kQuitDeadlineMs = 15'000;

// ---- 1. What is possible here ---------------------------------------------

enum class InstallMethod : std::uint8_t {
    // The backend replaces the app (macOS: bundle swap; Linux: AppImage swap;
    // Windows: helper around msiexec) and starts the new version. A helper
    // reports through the result file — reportsOutcome is true, and that is
    // what the flow judges by.
    Automatic,
    // The backend hands the file to the system installer (stage 1: `open <dmg>`,
    // `msiexec /i`, the in-place AppImage swap) and the app quits. Whether
    // the user finishes the installer is unknown; no result file.
    SystemInstaller,
    // Nothing can be started from here. The verified file stays where it is
    // and the flow enters ManualInstall with `reason`. NOT an error state.
    Manual,
};

// Why nothing automatic is possible. One value per distinct remedy (each has
// its own sentence in UpdateTexts); `detail` is only for "Details" and the log.
enum class ManualReason : std::uint8_t {
    None = 0,             // method != Manual
    UnsupportedKind,      // artifact kind this backend does not handle
    NotAnAppImage,        // Linux: the app is not running from an AppImage
    LocationNotWritable,  // the app's own location cannot be replaced
    RunningFromDiskImage, // macOS: started straight from the mounted .dmg
    AppTranslocated,      // macOS: App Translocation (randomised read-only path)
    NotFamilySigned,      // macOS: the running app does not carry the family
                          // identity, so the new bundle cannot be checked
                          // against its designated requirement
    HelperUnavailable,    // the helper cannot be started (missing, AppLocker)
    Quarantined,          // macOS: the downloaded file carries (or may
                          // carry) the quarantine mark, which is never removed
    NotInstalledPackage,  // Windows: not running from the MSI-installed
                          // product (portable ZIP, copied folder)
    OtherInstanceRunning, // another process runs from the same bundle/folder
    AppManagementDenied,  // macOS: an earlier attempt was refused by the
                          // system's "App Management" permission
    LocationShared,       // the app's parent folder is not the user's own, is
                          // writable for group/others or carries the sticky bit
    NotMeasuredOnThisPlatform, // self-replacement not measured on this system
                          // (for now Linux; a switch in the code) - "by hand"
                          // is simply the normal way here, no reason sentence
    FileSystemUnsupported, // the file system there cannot do the atomic swap
    // New values go HERE, before Other: the flow maps every value explicitly
    // (no default), and a test walks the whole range.
    Other,                // see detail
};

struct InstallCapability {
    InstallMethod method = InstallMethod::Manual;
    ManualReason reason = ManualReason::UnsupportedKind;
    QString detail;  // English, for "Details" and the log; never a UI sentence
    // True when the backend will write a result file that the next start can
    // evaluate (Automatic). False for SystemInstaller and Manual.
    bool reportsOutcome = false;
};

// Test doors and deadlines. Defaults are production; tests set them.
struct InstallTuning {
    int helperWaitExitMs = kHelperWaitExitMs;
    int startConfirmTimeoutMs = kStartConfirmTimeoutMs;
    // The flow's own deadline for prepare() (kPrepareDeadlineMs).
    int prepareDeadlineMs = kPrepareDeadlineMs;
    // How long the flow waits for a result file before "unknown" (kOutcomeGraceMs).
    int outcomeGraceMs = kOutcomeGraceMs;
    // How long the flow waits for aboutToQuit after requestQuit() said true (kQuitDeadlineMs).
    int quitDeadlineMs = kQuitDeadlineMs;
    // macOS: the designated requirement to treat as the RUNNING app's
    // (test programs are no bundles; SecCodeCopySelf would answer ad hoc).
    // Empty = read it from the running code.
    QString runningRequirementForTests;
    // Where prepare() stages; empty = next to the target (same volume).
    QString stagingDirForTests;
};

// What the backend needs to know about the running app. The flow fills it
// (with a test door); backends never look it up themselves, so a test can
// describe any situation without being in it.
struct InstallContext {
    QString kind;         // manifest artifact kind: "dmg" | "msi" | "appimage"
    // Where the running app lives, per platform:
    //   macOS   the .app bundle directory
    //   Windows the running executable (QCoreApplication::applicationFilePath)
    //   Linux   $APPIMAGE (empty when not running from an AppImage)
    QString appLocation;
    // The verified download (capability() may look at its quarantine mark).
    QString verifiedFile;
    // The product as the app's code names it (same as InstallOrder::productSlug).
    QString productSlug;
    // Windows: the MSI UpgradeCode of this product, "{XXXXXXXX-…}", from the
    // app's code - the same source as its installer definition (.wxs). Never
    // from settings, environment or arguments. Empty = not known (other
    // systems, or a product without MSI): the backend falls back safely.
    QString windowsUpgradeCode;
    // the `code=` of the last FAILED attempt, remembered by the flow
    // (e.g. "app-management-denied"). The backend decides whether it means
    // Manual this time; the flow forgets it after a successful install.
    QString previousFailureCode;
    InstallTuning tuning;
};

// ---- 2./3. The order and its prepared form --------------------------------

struct InstallOrder {
    InstallContext context;
    QString verifiedFile;     // the downloaded artifact, hash already checked
    QString expectedSha256;   // lowercase hex, from the SIGNED manifest
    qint64  expectedSize = 0; // from the signed manifest
    QString fromVersion;      // the running version, e.g. "0.7.0"
    QString toVersion;        // the manifest version, e.g. "0.8.0"
    QString productSlug;      // "macpcan"
    QString productName;      // "MacPCAN", for the helper's own messages
    qint64  appPid = 0;       // QCoreApplication::applicationPid()
    // Where the helper writes its outcome (format: readInstallOutcome()).
    // Chosen by the flow under the app's data location; the backend never
    // invents a path.
    QString resultFilePath;
    // where the NEW version writes `version=<its version>` at its start
    // (writeStartConfirmation()). The helper waits for it
    // (startConfirmTimeoutMs) before it deletes the old app.
    QString startConfirmFilePath;
};

struct PreparedInstall {
    InstallOrder order;
    InstallMethod method = InstallMethod::Manual;
    // The file or bundle the helper / installer will actually use. Equals
    // order.verifiedFile unless prepare() staged a copy (then it is the
    // staged copy, verified after copying).
    QString stagedFile;
    // Human-readable, for the dry-run log (`update/dry_run_launch`) and
    // "Details": what launch() WOULD start. Never contains a secret.
    QString description;
    // Backend-private bookkeeping for discard() (e.g. a staging directory).
    QByteArray backendState;
};

struct PrepareResult {
    std::optional<PreparedInstall> prepared;  // nullopt = refused
    // When refused: why. `manual` set → fall back to ManualInstall with that
    // reason (e.g. the location turned read-only since capability());
    // otherwise InstallFailed with `error` (e.g. the hash no longer matches,
    // the staged bundle fails its signature check).
    ManualReason manual = ManualReason::None;
    QString error;  // English, for "Details" and the log
};

struct LaunchResult {
    bool started = false;
    QString error;  // English; the app still quits, the flow records it
};

// ---- The seam itself ------------------------------------------------------

class InstallBackend {
public:
    using PrepareDone = std::function<void(PrepareResult)>;

    virtual ~InstallBackend() = default;

    [[nodiscard]] virtual InstallCapability capability(
        const InstallContext& context) const = 0;

    // Asynchronous; see the header comment for the rules of `done`.
    virtual void prepare(const InstallOrder& order, PrepareDone done) = 0;
    // Stops a running prepare(); `done` is not called afterwards. Whatever it
    // already staged is removed (or left to cleanupAfterStart). Idempotent.
    // The flow also calls it when it is destroyed mid-prepare; a backend that
    // is itself destroyed must not call `done` afterwards either.
    virtual void cancelPrepare() = 0;

    // From aboutToQuit only. Must return promptly.
    [[nodiscard]] virtual LaunchResult launch(const PreparedInstall& prepared) = 0;

    // Undo prepare(). Idempotent; safe to call after a failed launch.
    virtual void discard(const PreparedInstall& prepared) = 0;

    // Leftovers of an earlier attempt. Default: nothing to do.
    virtual void cleanupAfterStart(const InstallContext& context) { Q_UNUSED(context); }
};

// The stage-1 backend: today's mechanics behind the new seam. dmg → `open`,
// msi → `msiexec /i` (native separators), appimage → in-place replace with
// .old fallback (InstallerLauncher.hpp). Reports SystemInstaller for dmg/msi
// and for appimage (the replaced image runs on the next start; no result
// file), Manual/NotAnAppImage when appLocation is empty for kind appimage,
// Manual/LocationNotWritable when its folder is read-only,
// Manual/UnsupportedKind otherwise. prepare() re-checks the file's size and
// SHA-256 against the order, stages nothing, and answers on the next turn of
// the event loop.
[[nodiscard]] std::unique_ptr<InstallBackend> makeSystemInstallerBackend();

// ---- Start confirmation ----------------------------------------------
//
// Written by the flow of the NEW version at its start, read by the helper:
// one line `version=<version>` (LF), written via <path>.tmp + rename.
bool writeStartConfirmation(const QString& path, const QString& version,
                            QString* errorOut = nullptr);

// ---- Result file (written by the helper, read on the next start) ----------
//
// Format, deliberately trivial so a shell or batch helper can write it
// without a JSON encoder: UTF-8, one `key=value` per line, LF or CRLF, no
// quoting; unknown keys are ignored, a value runs to the end of its line.
// Written via <path>.tmp + rename (atomic).
//
//   result=ok | failed | rolled-back | cancelled
//   step=<short machine word: wait-exit | mount | verify | replace | launch …>
//   reason=<one line of English for "Details">
//   version=<the version the helper installed or restored>
//   code=<optional machine word for the flow, app-management-denied …>
//
// `result` is mandatory; a file without a valid one is unreadable (nullopt
// with errorOut), never "ok". So is a file in which ANY key occurs twice: two
// results are no result. Read with a hard limit (4 KiB, the readFile-
// Guarded rule of the family): the path is under the app's data location, but
// a file there is still an input.

struct InstallOutcome {
    enum class Result : std::uint8_t {
        Ok,          // new version installed (and started, for Automatic)
        Failed,      // a step failed and nothing was changed
        RolledBack,  // a step failed after a change; the old app was restored
        Cancelled,   // the user declined (e.g. the Windows elevation prompt)
    };
    Result result = Result::Failed;
    QString step;
    QString reason;
    QString version;
    QString code;
};

inline constexpr qint64 kInstallOutcomeMaxBytes = 4096;

[[nodiscard]] std::optional<InstallOutcome> parseInstallOutcome(
    const QByteArray& bytes, QString* errorOut = nullptr);

[[nodiscard]] std::optional<InstallOutcome> readInstallOutcome(
    const QString& path, QString* errorOut = nullptr);

// Writes the format above (for the test fake and for helpers written in C++).
// Line breaks in values are replaced by spaces.
[[nodiscard]] QByteArray serializeInstallOutcome(const InstallOutcome& outcome);

// Stable lower-case names for logs and the files ("ok", "rolled-back", …).
[[nodiscard]] QString installOutcomeResultName(InstallOutcome::Result r);
[[nodiscard]] QString manualReasonName(ManualReason r);
[[nodiscard]] QString installMethodName(InstallMethod m);  // "automatic" | …

}  // namespace appupdate
