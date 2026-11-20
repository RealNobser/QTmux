#pragma once

// appupdate::SelfInstall — the automatic install backend (contract V2 §6,
// phase H2): the app replaces itself and starts the new version.
//
// CANONICAL: QTmux vendors src/update/ byte-identically. Qt6::Core only (plus
// the system frameworks Security, CoreFoundation and DiskArbitration on
// macOS) — no Widgets, no Network, no QSettings.
//
// The rules that carry the design (each one measured on throwaway bundles):
//
//   * The only input is the file the flow verified against the SIGNED
//     manifest; prepare() hashes it again, and the helper checks again
//     right before it changes anything.
//   * macOS: the new bundle must be valid (strict, nested, all
//     architectures) AND satisfy the designated requirement of the RUNNING
//     app. A plain validity check is NOT enough — measured: it accepts a
//     foreign identity, ad hoc and a foreign identifier. An OLDER bundle of
//     the same identity satisfies the requirement, so CFBundleVersion must
//     equal the manifest version.
//   * The bundle is staged as a SIBLING of the target (same folder, hence
//     same volume, and only renames inside one folder — moving a bundle into
//     another folder needs write access to the bundle itself, measured).
//   * The helper is a constant shell text passed as `/bin/sh -c <text> <name>
//     <args…>` — no script file on disk that could be swapped, values only
//     as arguments, fixed PATH and LC_ALL. It learns that the app is gone
//     from EOF on its stdin (the "lifeline": a pipe whose write end only the
//     app holds, close-on-exec), then also waits for the pid.
//   * macOS swaps in ONE step (renamex_np RENAME_SWAP, the program
//     update-swap from Contents/Helpers of the running bundle): the name of
//     the app is never free. prepare() copies that program through a chain of
//     descriptors into a fresh 0700 folder of the user's own temp directory
//     and checks the COPY against the identity of the running app plus the
//     identifier "<bundle identifier>.update-swap"; the helper only ever runs
//     that copy, there and on the way back. Without it: Manual
//     (HelperUnavailable), no second way.
//   * After the swap the old bundle sits under the staging name until the new
//     version confirmed its start (InstallOrder::startConfirmFilePath);
//     without a confirmation the helper swaps back.
//   * Whatever fails before anything changed (or after a swap back), the helper
//     starts the old app again — only when it is provably the unchanged old
//     app (macOS: the running app's requirement and its version; Linux: the
//     same inode) and nothing runs from it yet. The result file says so in its
//     own key restart=, written BEFORE the start.
//   * The private copy of update-swap is checked again right before EACH
//     start (codesign -R with the requirement as an argument); what remains
//     is the moment between that check and the start, open only to a process
//     of the same user.
//   * Quarantine marks are never removed: a marked file means ManualInstall
//     (Quarantined).
//
// What this file offers besides the backend: the PURE decisions and plans
// (what WOULD happen), testable on every platform, the same split as
// installerLaunchPlan() in InstallerLauncher.hpp.

#include "update/InstallBackend.hpp"
#include "update/Quarantine.hpp"

#include <QList>
#include <QString>
#include <QStringList>

#include <functional>
#include <memory>
#include <optional>

namespace appupdate {

// ---- Pure decisions --------------------------------------------------------

enum class SelfInstallPlatform : std::uint8_t { MacOS, Windows, Linux };

[[nodiscard]] SelfInstallPlatform currentSelfInstallPlatform();

// What the backend measured about the running app. capability() is
// selfInstallDecision(context, measure(context)); tests fill this directly.
struct SelfInstallFacts {
    // all systems
    bool locationExists = false;     // the bundle / exe / AppImage is there
    bool locationWritable = false;   // its parent folder accepts a rename
    bool locationShared = false;     // others may write there (isSharedParent)
    QString locationSharedRule;      // which rule applied, for the detail text
    bool platformMeasured = true;    // the swap was measured on this system
    bool otherInstanceRunning = false;
    bool helperAvailable = false;    // /bin/sh resp. the Windows helper
    // macOS
    QString runningRequirement;      // designated requirement text; empty = unreadable
    bool translocated = false;
    bool onDiskImage = false;
    bool onReadOnlyVolume = false;
    bool onNetworkVolume = false;    // statfs without MNT_LOCAL: owner and mode say nothing
    bool swapToolPresent = false;    // Contents/Helpers/update-swap is a plain file
    QuarantineState quarantine = QuarantineState::Unreadable;
    // Windows
    bool installedPackage = false;   // running from the MSI-installed product
};

// The capability for `platform`. Order of the checks is part of the contract:
// the most specific reason wins (a translocated app is also read-only, but
// "move it out of Downloads" is the useful sentence, not "not writable").
[[nodiscard]] InstallCapability selfInstallDecision(SelfInstallPlatform platform,
                                                    const InstallContext& context,
                                                    const SelfInstallFacts& facts);

// The parent folder of the app may be written by someone else, so a name
// freed for a moment could be taken. True when (a) everyone may write,
// (b) it is sticky (shared folders like /tmp or /Users/Shared), (c) it is
// owned by neither the user nor root, (d) it is owned by root and writable
// for a group other than `adminGid`, or (e) it is the user's own and
// writable for its group at all — on macOS the primary group is usually
// "staff", and every local user is in it. /Applications (root:admin, 775)
// and the user's own folders (755) pass: another administrator can do
// anything anyway. Pass kNoAdminGroup where no group administers the
// machine (Linux). `myPrimaryGroup` is kept for callers and logs.
// Measured 2026-11-20: /Applications root:admin drwxrwxr-x; /Users/Shared
// and /private/tmp root:wheel drwxrwxrwt.
inline constexpr unsigned kNoAdminGroup = 0xFFFFFFFFu;
[[nodiscard]] bool isSharedParent(unsigned owner, unsigned group, unsigned mode, unsigned me,
                                  unsigned myPrimaryGroup, unsigned adminGid);
// The same decision, naming the rule that applied ("world-writable",
// "sticky", "foreign-owner", "group-writable") or nullptr — for the detail
// text, so a support question can be answered.
[[nodiscard]] const char* sharedParentRule(unsigned owner, unsigned group, unsigned mode, unsigned me,
                                           unsigned myPrimaryGroup, unsigned adminGid);

// Linux: the AppImage swap is not yet measured on a real desktop, so
// capability() says NotMeasuredOnThisPlatform until this is true. A switch
// in the code, never a setting.
inline constexpr bool kLinuxSelfInstallMeasured = false;

// True when `requirement` binds an identifier to a certificate (the family
// signature, or any real identity) and is not a CDHash list (ad hoc).
[[nodiscard]] bool isCertificateAnchoredRequirement(const QString& requirement);

// The `code=` word the macOS helper writes when the system's App Management
// permission refused the swap ("Operation not permitted" on the first mv).
inline constexpr char kCodeAppManagementDenied[] = "app-management-denied";
// ... when the file system cannot do the atomic swap (macOS RENAME_SWAP
// answers ENOTSUP, Linux cannot make the hard link).
inline constexpr char kCodeFileSystemUnsupported[] = "file-system-unsupported";

// ---- macOS names next to the target ----------------------------------------

struct MacSwapPaths {
    QString target;         // /Applications/MacPCAN.app
    QString stagingPrefix;  // /Applications/.MacPCAN.app.update-staged-
    QString swapTool;       // /Applications/MacPCAN.app/Contents/Helpers/update-swap
};

// The requirement the private copy of update-swap must satisfy: the running
// app's designated requirement with its identifier replaced by
// "<identifier>.update-swap" — the same identity, the expected program.
// Empty when `appRequirement` does not start with an identifier clause or is
// not certificate-anchored.
[[nodiscard]] QString swapToolRequirement(const QString& appRequirement);
[[nodiscard]] MacSwapPaths macSwapPaths(const QString& bundle);

// One entry in the root of a mounted disk image.
struct DiskImageEntry {
    QString name;
    bool isDir = false;
    bool isSymlink = false;
    QString symlinkTarget;  // raw link text
};

// The root must hold exactly one directory `expectedBundleName` (not a link);
// besides it only the symlink `Applications -> /Applications` that every
// family DMG carries (it is never followed) and hidden entries (".…", never
// used). Anything else: refused with `why`. Returns the bundle name.
[[nodiscard]] std::optional<QString> validateDiskImageRoot(
    const QList<DiskImageEntry>& entries, const QString& expectedBundleName,
    QString* why = nullptr);

// First symlink inside `bundle` whose target is absolute or resolves outside
// the bundle; empty when there is none. POSIX only (always empty elsewhere).
[[nodiscard]] QString firstEscapingSymlink(const QString& bundle);

// ---- The helper ------------------------------------------------------------

// The constant helper texts. Never formatted, never written to disk.
[[nodiscard]] QString macHelperScript();
[[nodiscard]] QString linuxHelperScript();

// Checks what launch() hands to a helper: a pid > 1, clean absolute paths
// without line breaks (target, staged copy, result and confirmation file), a
// plain version string and, if set, a plain product slug. launch() refuses
// anything else instead of passing it on.
[[nodiscard]] bool validatePreparedForHelper(const PreparedInstall& prepared,
                                             QString* why = nullptr);

// `slug` when it is a plain name ([a-z0-9-], up to 32), otherwise "app" —
// for helper and temp-folder names.
[[nodiscard]] QString helperNamePart(const QString& slug);

struct HelperPlan {
    QString program;          // "/bin/sh"
    QStringList arguments;    // "-c", <script>, <name>, <args…>
    QString description;      // for the dry-run log; never the script text
};

// macOS: `/bin/sh -c <macHelperScript> <slug>-update-helper <target> <staged>
// <requirement> <version> <result> <confirm> <pid> <waitTicks> <confirmTicks>
// <swapTool>` (ticks of 100 ms; swapTool = the checked private copy).
[[nodiscard]] HelperPlan macHelperPlan(const PreparedInstall& prepared,
                                       const QString& requirement,
                                       const QString& swapTool);

// Linux: `/bin/sh -c <linuxHelperScript> <slug>-update-helper <target>
// <staged> <sha256> <version> <result> <confirm> <pid> <waitTicks>
// <confirmTicks>`.
[[nodiscard]] HelperPlan linuxHelperPlan(const PreparedInstall& prepared);

// ---- Windows plans (the helper program itself is not part of this file) ---

// msiexec arguments for plans and logs: /i <native msi> /passive /norestart
// INSTALLFOLDER=<native folder> /l*v <native log> — as separate list items,
// WITHOUT quotes. Not for starting msiexec through QProcess (its quoting of
// "INSTALLFOLDER=C:\Program Files\…" is not measured); the helper program
// builds its own command line with the quoting msiexec expects
// (winhelper::msiexecCommandLine). Native separators (1619 otherwise).
[[nodiscard]] QStringList msiexecArguments(const QString& msi,
                                           const QString& installFolder,
                                           const QString& logFile);

// Maps an msiexec exit code to the result file. `codeOut` gets a machine
// word ("restart-required", "user-cancelled", "another-install-running",
// "msiexec-<n>"). Measured unattended: 0, 1601 (after the Restart Manager
// ended a process holding an installed file), 1603 (the package was held open
// without write/delete sharing), 1619 (forward slashes, missing file). The
// codes for a declined elevation prompt and the Cancel button follow the
// Windows Installer documentation and are not measured.
[[nodiscard]] InstallOutcome::Result msiexecExitToResult(int exitCode,
                                                         QString* codeOut = nullptr);

// ---- The backend -----------------------------------------------------------

// Seams for tests only; production passes nothing.
struct SelfInstallHooks {
    // Replaces the measurement in capability() and prepare().
    std::function<SelfInstallFacts(const InstallContext&)> facts;
    // launch() normally keeps the lifeline's write end open until the
    // process ends (that IS the signal). A test receives it here and closes
    // it to play "the app has quit".
    std::function<void(int writeFd)> takeLifeline;
    // Tests of the AppImage swap run it although kLinuxSelfInstallMeasured
    // is false. Production never sets this.
    bool assumePlatformMeasuredForTests = false;
};

// macOS → the bundle swap; Linux → the AppImage swap; Windows → nullptr
// until the helper program exists (the caller keeps the system-installer
// backend there — an explicit decision, not a silent fallback per case).
[[nodiscard]] std::unique_ptr<InstallBackend> makeSelfInstallBackend(
    SelfInstallHooks hooks = {});

}  // namespace appupdate
