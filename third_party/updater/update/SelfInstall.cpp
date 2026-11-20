#include "update/SelfInstall.hpp"
#include "update/SelfInstallPosix.hpp"
#include "update/winhelper/UpdateHelperCore.hpp"

#include <QCryptographicHash>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QMetaObject>
#include <QObject>
#include <QPointer>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QThread>

#include <atomic>
#include <memory>
#include <vector>

#if defined(Q_OS_UNIX)
#include <cerrno>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <spawn.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

#if defined(Q_OS_UNIX)
#include <dirent.h>
#endif
#if defined(Q_OS_MACOS)
#include <membership.h>
#include <sys/acl.h>
#include <sys/mount.h>
#endif

namespace appupdate {

// ============================================================================
// Pure decisions
// ============================================================================

SelfInstallPlatform currentSelfInstallPlatform()
{
#if defined(Q_OS_MACOS)
    return SelfInstallPlatform::MacOS;
#elif defined(Q_OS_WIN)
    return SelfInstallPlatform::Windows;
#else
    return SelfInstallPlatform::Linux;
#endif
}

const char* sharedParentRule(unsigned owner, unsigned group, unsigned mode, unsigned me, unsigned myPrimaryGroup,
                             unsigned adminGid)
{
    constexpr unsigned kGroupWrite = 0020, kOtherWrite = 0002, kSticky = 01000;
    Q_UNUSED(myPrimaryGroup);
    if (mode & kOtherWrite) return "world-writable";                                           // (a)
    if (mode & kSticky) return "sticky";                                                       // (b)
    if (owner != me && owner != 0) return "foreign-owner";                                     // (c)
    if (owner == 0 && owner != me && (mode & kGroupWrite) && group != adminGid) return "group-writable";  // (d)
    if (owner == me && (mode & kGroupWrite)) return "group-writable";  // (e) staff = every local user
    return nullptr;
}

bool isSharedParent(unsigned owner, unsigned group, unsigned mode, unsigned me, unsigned myPrimaryGroup,
                    unsigned adminGid)
{
    return sharedParentRule(owner, group, mode, me, myPrimaryGroup, adminGid) != nullptr;
}

bool isCertificateAnchoredRequirement(const QString& requirement)
{
    const QString r = requirement.simplified();
    if (r.isEmpty()) return false;
    if (r.contains(QLatin1String("cdhash"))) return false;  // ad hoc
    // A certificate CLAUSE alone could bind to a field (a name) that any
    // self-made certificate may carry: a hash anchor or Apple's anchor is
    // required.
    static const QRegularExpression hashAnchor(
        QStringLiteral("certificate (root|leaf|[0-9]+) = H\"[0-9a-fA-F]{40}\""));
    const bool anchored = hashAnchor.match(r).hasMatch() || r.contains(QLatin1String("anchor apple"));
    return r.contains(QLatin1String("identifier \"")) && anchored;
}

namespace {

InstallCapability manual(ManualReason reason, QString detail)
{
    InstallCapability c;
    c.method = InstallMethod::Manual;
    c.reason = reason;
    c.detail = std::move(detail);
    c.reportsOutcome = false;
    return c;
}

InstallCapability automatic(QString detail)
{
    InstallCapability c;
    c.method = InstallMethod::Automatic;
    c.reason = ManualReason::None;
    c.detail = std::move(detail);
    c.reportsOutcome = true;
    return c;
}

}  // namespace

InstallCapability selfInstallDecision(SelfInstallPlatform platform,
                                      const InstallContext& context,
                                      const SelfInstallFacts& facts)
{
    const QString kind = context.kind.toLower();
    switch (platform) {
    case SelfInstallPlatform::MacOS:
        if (kind != QLatin1String("dmg"))
            return manual(ManualReason::UnsupportedKind,
                          QStringLiteral("kind '%1' is not a disk image").arg(context.kind));
        if (context.previousFailureCode == QLatin1String(kCodeAppManagementDenied))
            return manual(ManualReason::AppManagementDenied,
                          QStringLiteral("the last attempt was refused by App Management"));
        if (context.previousFailureCode == QLatin1String(kCodeFileSystemUnsupported))
            return manual(ManualReason::FileSystemUnsupported,
                          QStringLiteral("the file system at %1 cannot swap atomically").arg(context.appLocation));
        if (!facts.locationExists)
            return manual(ManualReason::Other,
                          QStringLiteral("the running bundle %1 was not found").arg(context.appLocation));
        if (facts.translocated)
            return manual(ManualReason::AppTranslocated,
                          QStringLiteral("the app runs translocated from %1").arg(context.appLocation));
        if (facts.onDiskImage)
            return manual(ManualReason::RunningFromDiskImage,
                          QStringLiteral("the app runs from a mounted disk image"));
        if (!isCertificateAnchoredRequirement(facts.runningRequirement))
            return manual(ManualReason::NotFamilySigned,
                          facts.runningRequirement.isEmpty()
                              ? QStringLiteral("the designated requirement of the running app is unreadable")
                              : QStringLiteral("the running app is not signed with an identity (%1)")
                                    .arg(facts.runningRequirement));
        if (facts.onReadOnlyVolume || !facts.locationWritable)
            return manual(ManualReason::LocationNotWritable,
                          QStringLiteral("the folder of %1 does not accept a rename").arg(context.appLocation));
        if (facts.onNetworkVolume)
            return manual(ManualReason::FileSystemUnsupported,
                          QStringLiteral("%1 lies on a network volume; who writes there is decided by the server")
                              .arg(context.appLocation));
        if (facts.locationShared)
            return manual(ManualReason::LocationShared,
                          QStringLiteral("others may write in the folder of %1 (rule: %2)")
                              .arg(context.appLocation, facts.locationSharedRule));
        if (facts.quarantine != QuarantineState::Absent)
            return manual(ManualReason::Quarantined,
                          QStringLiteral("the downloaded file's quarantine mark is %1")
                              .arg(quarantineStateName(facts.quarantine)));
        if (facts.otherInstanceRunning)
            return manual(ManualReason::OtherInstanceRunning,
                          QStringLiteral("another process runs from %1").arg(context.appLocation));
        if (!facts.helperAvailable)
            return manual(ManualReason::HelperUnavailable,
                          QStringLiteral("/bin/sh, codesign, ditto or hdiutil is missing"));
        if (!facts.swapToolPresent)
            return manual(ManualReason::HelperUnavailable,
                          QStringLiteral("the bundle carries no swap program (Contents/Helpers/update-swap)"));
        return automatic(QStringLiteral("replace %1 and restart").arg(context.appLocation));

    case SelfInstallPlatform::Linux:
        if (kind != QLatin1String("appimage"))
            return manual(ManualReason::UnsupportedKind,
                          QStringLiteral("kind '%1' is not an AppImage").arg(context.kind));
        if (context.appLocation.isEmpty())
            return manual(ManualReason::NotAnAppImage,
                          QStringLiteral("the app is not running from an AppImage"));
        if (!facts.locationExists)
            return manual(ManualReason::Other,
                          QStringLiteral("the running AppImage %1 was not found").arg(context.appLocation));
        if (context.previousFailureCode == QLatin1String(kCodeFileSystemUnsupported))
            return manual(ManualReason::FileSystemUnsupported,
                          QStringLiteral("no hard link is possible at %1").arg(context.appLocation));
        if (!facts.platformMeasured)
            return manual(ManualReason::NotMeasuredOnThisPlatform,
                          QStringLiteral("replacing an AppImage is not yet measured on a Linux desktop"));
        if (!facts.locationWritable)
            return manual(ManualReason::LocationNotWritable,
                          QStringLiteral("the folder of %1 does not accept a rename").arg(context.appLocation));
        if (facts.locationShared)
            return manual(ManualReason::LocationShared,
                          QStringLiteral("others may write in the folder of %1 (rule: %2)")
                              .arg(context.appLocation, facts.locationSharedRule));
        if (facts.otherInstanceRunning)
            return manual(ManualReason::OtherInstanceRunning,
                          QStringLiteral("another process runs from %1").arg(context.appLocation));
        if (!facts.helperAvailable)
            return manual(ManualReason::HelperUnavailable, QStringLiteral("/bin/sh is missing"));
        return automatic(QStringLiteral("replace %1 and restart").arg(context.appLocation));

    case SelfInstallPlatform::Windows:
        if (kind != QLatin1String("msi"))
            return manual(ManualReason::UnsupportedKind,
                          QStringLiteral("kind '%1' is not an MSI").arg(context.kind));
        if (!facts.installedPackage)
            return manual(ManualReason::NotInstalledPackage,
                          QStringLiteral("%1 is not the MSI-installed product").arg(context.appLocation));
        if (facts.otherInstanceRunning)
            return manual(ManualReason::OtherInstanceRunning,
                          QStringLiteral("another process runs from the install folder"));
        if (!facts.helperAvailable)
            return manual(ManualReason::HelperUnavailable,
                          QStringLiteral("the update helper cannot be started here"));
        return automatic(QStringLiteral("run msiexec and restart"));
    }
    return manual(ManualReason::Other, QStringLiteral("unknown platform"));
}

// ============================================================================
// Names and disk-image rules
// ============================================================================

MacSwapPaths macSwapPaths(const QString& bundle)
{
    const QString clean = QDir::cleanPath(bundle);
    const QFileInfo fi(clean);
    MacSwapPaths p;
    p.target = clean;
    p.stagingPrefix = fi.path() + QStringLiteral("/.") + fi.fileName() +
                      QStringLiteral(".update-staged-");
    p.swapTool = clean + QStringLiteral("/Contents/Helpers/update-swap");
    return p;
}

QString swapToolRequirement(const QString& appRequirement)
{
    static const QRegularExpression re(QStringLiteral("^identifier \"([A-Za-z0-9.-]{1,200})\" and (.+)$"));
    const QRegularExpressionMatch m = re.match(appRequirement.trimmed());
    if (!m.hasMatch() || !isCertificateAnchoredRequirement(appRequirement)) return {};
    // Parenthesised: the rest keeps its own meaning even if it holds an "or".
    return QStringLiteral("identifier \"%1.update-swap\" and (%2)").arg(m.captured(1), m.captured(2));
}

std::optional<QString> validateDiskImageRoot(const QList<DiskImageEntry>& entries,
                                             const QString& expectedBundleName,
                                             QString* why)
{
    const auto fail = [why](const QString& w) -> std::optional<QString> {
        if (why) *why = w;
        return std::nullopt;
    };
    if (why) why->clear();
    int bundles = 0;
    for (const DiskImageEntry& e : entries) {
        if (e.name.startsWith(QLatin1Char('.'))) continue;  // .fseventsd, .background …
        if (e.name == expectedBundleName) {
            if (e.isSymlink || !e.isDir)
                return fail(QStringLiteral("%1 in the disk image is not a plain folder").arg(e.name));
            ++bundles;
            continue;
        }
        if (e.name == QLatin1String("Applications") && e.isSymlink &&
            e.symlinkTarget == QLatin1String("/Applications"))
            continue;  // the drag target every family DMG carries; never followed
        return fail(QStringLiteral("unexpected entry '%1' in the disk image").arg(e.name));
    }
    if (bundles != 1)
        return fail(QStringLiteral("the disk image does not contain %1").arg(expectedBundleName));
    return expectedBundleName;
}

QString firstEscapingSymlink(const QString& bundle)
{
#if defined(Q_OS_UNIX)
    const QString root = QDir::cleanPath(QFileInfo(bundle).absoluteFilePath());
    char* rr = ::realpath(QFile::encodeName(root).constData(), nullptr);
    if (!rr) return root;
    const QString realRoot = QFile::decodeName(QByteArray(rr));
    std::free(rr);
    QDirIterator it(root, QDir::AllEntries | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot,
                    QDirIterator::Subdirectories);  // does not follow symlinks
    while (it.hasNext()) {
        const QString path = it.next();
        const QFileInfo fi = it.fileInfo();
        if (!fi.isSymLink()) continue;
        char buf[PATH_MAX + 1];
        const ssize_t n = ::readlink(QFile::encodeName(path).constData(), buf, PATH_MAX);
        if (n < 0) return path;  // unreadable link: treat as escaping
        const QString raw = QFile::decodeName(QByteArray(buf, int(n)));
        if (raw.startsWith(QLatin1Char('/'))) return path;
        // Resolve PHYSICALLY: a chain of links that each look harmless as text
        // (A/B/L -> ../.., A/B/M -> L/..) can still lead out. A link that
        // does not resolve counts as escaping.
        char* real = ::realpath(QFile::encodeName(path).constData(), nullptr);
        if (!real) return path;
        const QString resolved = QFile::decodeName(QByteArray(real));
        std::free(real);
        if (resolved != realRoot && !resolved.startsWith(realRoot + QLatin1Char('/'))) return path;
    }
#else
    Q_UNUSED(bundle);
#endif
    return {};
}

// ============================================================================
// Helper texts. Constants: never formatted, never written to a file.
// Arguments: see macHelperPlan() / linuxHelperPlan().
// Exit 0 = ok, 1 = failed (nothing changed), 3 = rolled back, 64 = misuse.
// ============================================================================

QString macHelperScript()
{
    return QStringLiteral(R"SH(set -u
PATH=/usr/bin:/bin:/usr/sbin:/sbin; export PATH
LC_ALL=C; export LC_ALL
umask 077
[ "$#" -eq 11 ] || exit 64
target=$1 staged=$2 dr=$3 ver=$4 result=$5 confirm=$6 pid=$7 waitticks=$8 confirmticks=$9 tool=${10} toolreq=${11}
for p in "$target" "$staged" "$result" "$confirm" "$tool"; do
    case "$p" in /*) ;; *) exit 64 ;; esac
done
for n in "$pid" "$waitticks" "$confirmticks"; do
    case "$n" in ''|*[!0-9]*) exit 64 ;; esac
done
[ "$pid" -gt 1 ] || exit 64
toolDir=${tool%/*}
restart=
one() { printf '%s' "$1" | tr '\r\n' '  '; }
out() {
    [ -L "$result" ] && rm -f "$result"
    t=$(mktemp "$result.XXXXXX") || return 1
    { printf 'result=%s\nstep=%s\nreason=%s\nversion=%s\ncode=%s\n' "$(one "$1")" "$(one "$2")" "$(one "$3")" \
        "$(one "$4")" "$(one "${5:-}")"
      [ -z "$restart" ] || printf 'restart=%s\n' "$restart"; } >"$t" &&
        mv -f "$t" "$result"
}
# The private copy of the swap program goes with the helper.
finish() { rm -rfx "$toolDir" 2>/dev/null; exit "$1"; }
ver_of() { /usr/libexec/PlistBuddy -c 'Print :CFBundleVersion' "$1/Contents/Info.plist" 2>/dev/null; }
running_from() { ps -axo comm= | grep -qF "$1/Contents/MacOS/"; }
genuine() {
    [ ! -L "$1" ] && [ -d "$1" ] && codesign --verify --deep --strict -R "=$dr" "$1" 2>/dev/null &&
        [ "$(ver_of "$1")" = "$2" ]
}
# The copy was checked when it was prepared and is checked again right before
# EACH start (identity of the app, identifier <id>.update-swap). What remains
# is the moment between this check and the start, open only to a process of
# the same user (who owns the 0700 folder).
tool_ok() {
    [ ! -L "$tool" ] && [ -f "$tool" ] && [ -x "$tool" ] && [ -n "$toolreq" ] &&
        codesign --verify --strict -R "=$toolreq" "$tool" 2>/dev/null
}
restore() {  # the old bundle sits under the staging name: swap back; succeeds only if it IS back
    if tool_ok && "$tool" "$target" "$staged" && genuine "$target" "$oldver"; then
        rm -rfx "$staged"
        return 0
    fi
    return 1
}
# Nothing (any more) changed: the old app comes back, but only when it is
# provably the unchanged old app (the running app's requirement and its own
# version) and nothing runs from it yet. The result is written BEFORE the
# start; restart= says what happened, a start that fails rewrites it.
end_unchanged() {  # <result> <step> <reason> <code> <exit>
    if [ -L "$target" ]; then restart=skipped-link
    elif running_from "$target"; then restart=skipped-running
    elif ! genuine "$target" "$oldver"; then restart=skipped-not-genuine
    else restart=started; fi
    out "$1" "$2" "$3" "$oldver" "$4"
    if [ "$restart" = started ] && ! open -n "$target" 2>/dev/null; then
        restart=failed; out "$1" "$2" "$3" "$oldver" "$4"
    fi
    finish "$5"
}
# The lifeline: EOF on stdin = the app is gone. Bounded, then the pid.
exec 3<&0
cat <&3 >/dev/null &
lp=$!
exec 3<&- 0</dev/null
i=0
while kill -0 "$lp" 2>/dev/null; do
    i=$((i + 1))
    if [ "$i" -gt "$waitticks" ]; then kill "$lp" 2>/dev/null; break; fi
    sleep 0.1
done
while kill -0 "$pid" 2>/dev/null; do
    i=$((i + 1))
    if [ "$i" -gt "$waitticks" ]; then out failed wait-exit "the app was still running" "$(ver_of "$target")"; finish 1; fi
    sleep 0.1
done
oldver=$(ver_of "$target")
case "$oldver" in ''|*[!0-9A-Za-z.+_-]*) oldver=unknown ;; esac
if [ -L "$target" ]; then
    out failed replace "a link stands where the app was" "$oldver" in-the-way; finish 1
fi
if running_from "$target"; then
    out failed replace "the app was started again" "$oldver" app-running; finish 1
fi
if [ -L "$tool" ] || [ ! -f "$tool" ] || [ ! -x "$tool" ]; then
    end_unchanged failed replace "the checked copy of the swap program is gone" swap-tool-missing 1
fi
if ! genuine "$staged" "$ver"; then
    rm -rfx "$staged"
    end_unchanged failed verify "the new bundle fails its signature, designated requirement or version check" "" 1
fi
if [ "$(stat -f %d "$staged")" != "$(stat -f %d "$target")" ]; then
    end_unchanged failed verify "the new bundle is on another volume" "" 1
fi
if ! genuine "$target" "$oldver"; then
    out failed verify "the running bundle fails its own check" "$oldver" running-not-genuine; finish 1
fi
if ! tool_ok; then
    end_unchanged failed replace "the copy of the swap program fails its check right before the start" \
        swap-tool-unverified 1
fi
# One atomic swap: the name of the app is never free. Whatever the program
# answers, what stands at the target decides.
err=$("$tool" "$target" "$staged" 2>&1); rc=$?
err=$(printf '%s' "$err" | tr '\r\n' '  ')
if genuine "$target" "$ver"; then
    [ "$rc" -eq 0 ] || echo "the swap program ended with $rc after the swap ($err)" >&2
elif genuine "$target" "$oldver"; then
    case "$rc" in
        4) end_unchanged failed replace "the file system cannot swap atomically ($err)" file-system-unsupported 1 ;;
        5) end_unchanged failed replace "the system refused the swap ($err)" app-management-denied 1 ;;
        6) end_unchanged failed replace "access to the folder was denied ($err)" swap-access-denied 1 ;;
        1) end_unchanged failed replace "the swap did not happen ($err)" swap-failed 1 ;;
        0) end_unchanged failed replace "the swap program reported a swap that did not happen" swap-not-done 1 ;;
        *) end_unchanged failed replace "the swap program refused ($rc: $err)" swap-refused 1 ;;
    esac
elif genuine "$staged" "$oldver"; then
    # Swapped, but what stands at the target fails its check: back.
    if restore; then
        end_unchanged rolled-back replace "the bundle in place fails its check" changed-after-check 3
    fi
    out failed replace "the bundle in place fails its check and the old one could not be restored" "" rollback-failed
    finish 1
else
    out failed replace "after the swap program ($rc) neither version stands checked in place" "" swap-state-unknown
    finish 1
fi
rm -f "$confirm"
if open -n "$target" 2>/dev/null; then
    i=0
    while [ "$i" -lt "$confirmticks" ]; do
        if [ ! -L "$confirm" ] && grep -qxF "version=$ver" "$confirm" 2>/dev/null; then
            rm -rfx "$staged"
            out ok launch "" "$ver"; finish 0
        fi
        i=$((i + 1)); sleep 0.1
    done
    if running_from "$target"; then
        out failed launch "the new version runs but did not confirm its start" "$ver" start-unconfirmed; finish 1
    fi
fi
if restore; then
    end_unchanged rolled-back launch "the new version did not start" start-failed 3
fi
out failed launch "the new version did not start and the old one could not be restored" "" rollback-failed
finish 1
)SH");
}

QString linuxHelperScript()
{
    return QStringLiteral(R"SH(set -u
PATH=/usr/bin:/bin:/usr/sbin:/sbin; export PATH
LC_ALL=C; export LC_ALL
umask 077
[ "$#" -eq 9 ] || exit 64
target=$1 staged=$2 sha=$3 ver=$4 result=$5 confirm=$6 pid=$7 waitticks=$8 confirmticks=$9
for p in "$target" "$staged" "$result" "$confirm"; do
    case "$p" in /*) ;; *) exit 64 ;; esac
done
for n in "$pid" "$waitticks" "$confirmticks"; do
    case "$n" in ''|*[!0-9]*) exit 64 ;; esac
done
[ "$pid" -gt 1 ] || exit 64
old="$target.update-old"
restart=
absent() { [ ! -e "$1" ] && [ ! -L "$1" ]; }
one() { printf '%s' "$1" | tr '\r\n' '  '; }
out() {
    [ -L "$result" ] && rm -f "$result"
    t=$(mktemp "$result.XXXXXX") || return 1
    { printf 'result=%s\nstep=%s\nreason=%s\nversion=%s\ncode=%s\n' "$(one "$1")" "$(one "$2")" "$(one "$3")" \
        "$(one "$4")" "$(one "${5:-}")"
      [ -z "$restart" ] || printf 'restart=%s\n' "$restart"; } >"$t" &&
        mv -f "$t" "$result"
}
inode_of() { ls -di "$1" 2>/dev/null | awk '{print $1}'; }
hash_of() {
    if command -v sha256sum >/dev/null 2>&1; then sha256sum "$1" | cut -d' ' -f1
    else shasum -a 256 "$1" | cut -d' ' -f1; fi
}
start() {  # the app gets the user's language and PATH back, never the helper's
    (
        # setsid/nohup are looked up with the helper's fixed PATH, before the
        # user's PATH is set for the app.
        launcher=$(command -v setsid 2>/dev/null) || launcher=$(command -v nohup 2>/dev/null) || launcher=
        unset LC_ALL
        [ -n "${APPUPDATE_APP_LC_ALL:-}" ] && LC_ALL=$APPUPDATE_APP_LC_ALL && export LC_ALL
        [ -n "${APPUPDATE_APP_PATH:-}" ] && PATH=$APPUPDATE_APP_PATH && export PATH
        unset APPUPDATE_APP_LC_ALL APPUPDATE_APP_PATH
        if [ -n "$launcher" ]; then exec "$launcher" "$1" </dev/null >/dev/null 2>&1; fi
        exec "$1" </dev/null >/dev/null 2>&1
    ) &
}
running_from() {
    if [ ! -d /proc/self ]; then
        # Darwin's ps prints the full executable path (the tests of this text
        # run there); anywhere else without /proc there is no answer: running.
        [ "$(uname -s)" = Darwin ] || return 0
        ps -axo comm= | grep -qxF "$1"
        return $?
    fi
    for p in /proc/[0-9]*; do
        [ "$(readlink "$p/exe" 2>/dev/null)" = "$1" ] && return 0
    done
    return 1
}
genuine() { [ ! -L "$1" ] && [ -f "$1" ] && [ "$(hash_of "$1" 2>/dev/null)" = "$sha" ]; }
# Nothing (any more) changed: the old image comes back, but only when it is
# provably the same file that ran (same inode as when the helper began — an
# AppImage carries no signature to check against) and nothing runs from it
# yet. The result is written BEFORE the start; restart= says what happened.
end_unchanged() {  # <result> <step> <reason> <code> <exit>
    if [ -L "$target" ] || [ ! -f "$target" ]; then restart=skipped-link
    elif running_from "$target"; then restart=skipped-running
    elif [ "$(inode_of "$target")" != "$oldino" ]; then restart=skipped-not-genuine
    else restart=started; fi
    out "$1" "$2" "$3" "" "$4"
    [ "$restart" = started ] && start "$target"
    exit "$5"
}
# Replacing a FILE by rename(2) is atomic: the name of the app is never
# free. -T: never move INTO a directory planted under the name (GNU/BusyBox;
# Darwin's mv, used only by the tests of this text, has no -T).
replace() {
    if [ "$(uname -s)" = Darwin ]; then mv -f "$1" "$2"; else mv -f -T "$1" "$2"; fi
}
restore() {  # the old image (a hard link) goes back atomically; succeeds only if it IS back
    if [ ! -L "$old" ] && [ -f "$old" ] && replace "$old" "$target" 2>/dev/null &&
        [ ! -L "$target" ] && [ -f "$target" ]; then
        return 0
    fi
    return 1
}
# The file the app runs from, taken while it still runs: only this one is
# "the old image" that may be started again.
oldino=$(inode_of "$target")
# The lifeline: EOF on stdin = the app is gone. Bounded, then the pid.
exec 3<&0
cat <&3 >/dev/null &
lp=$!
exec 3<&- 0</dev/null
i=0
while kill -0 "$lp" 2>/dev/null; do
    i=$((i + 1))
    if [ "$i" -gt "$waitticks" ]; then kill "$lp" 2>/dev/null; break; fi
    sleep 0.1
done
while kill -0 "$pid" 2>/dev/null; do
    i=$((i + 1))
    if [ "$i" -gt "$waitticks" ]; then out failed wait-exit "the app was still running" ""; exit 1; fi
    sleep 0.1
done
if ! genuine "$staged"; then
    rm -f "$staged"; end_unchanged failed verify "the new image does not match the manifest hash" "" 1
fi
if ! absent "$old" || [ -L "$target" ]; then
    end_unchanged failed replace "leftovers of an earlier attempt or a link are in the way" in-the-way 1
fi
if running_from "$target"; then
    out failed replace "the app was started again" "" app-running; exit 1
fi
# Keep the old image as a hard link, then replace the name atomically.
if ! ln "$target" "$old" 2>/dev/null; then
    end_unchanged failed replace "no hard link is possible here" file-system-unsupported 1
fi
if [ -L "$target" ] || [ ! -f "$target" ] || ! replace "$staged" "$target" 2>/dev/null; then
    rm -f "$old"
    end_unchanged failed replace "cannot put the new image in place; the old one stays" move-failed 1
fi
if ! genuine "$target"; then
    if restore; then end_unchanged rolled-back replace "the image in place fails its check" changed-after-check 3; fi
    out failed replace "the image in place fails its check and the old one could not be restored" "" rollback-failed; exit 1
fi
rm -f "$confirm"
start "$target"
i=0
while [ "$i" -lt "$confirmticks" ]; do
    if [ ! -L "$confirm" ] && grep -qxF "version=$ver" "$confirm" 2>/dev/null; then
        rm -f "$old"; out ok launch "" "$ver"; exit 0
    fi
    i=$((i + 1)); sleep 0.1
done
if running_from "$target"; then
    out failed launch "the new version runs but did not confirm its start" "$ver" start-unconfirmed; exit 1
fi
if restore; then
    end_unchanged rolled-back launch "the new version did not start" start-failed 3
fi
out failed launch "the new version did not start and the old one could not be restored" "" rollback-failed
exit 1
)SH");
}

namespace {

QString ticks(int ms) { return QString::number((qMax(ms, 0) + 99) / 100); }

QString shortSlug(const PreparedInstall& p) { return helperNamePart(p.order.productSlug); }

}  // namespace

HelperPlan macHelperPlan(const PreparedInstall& prepared, const QString& requirement,
                         const QString& swapTool)
{
    const InstallOrder& o = prepared.order;
    const InstallTuning& t = o.context.tuning;
    const QString target = QDir::cleanPath(o.context.appLocation);
    HelperPlan plan;
    plan.program = QStringLiteral("/bin/sh");
    plan.arguments = {QStringLiteral("-c"), macHelperScript(),
                      shortSlug(prepared) + QStringLiteral("-update-helper"),
                      target, prepared.stagedFile, requirement, o.toVersion,
                      o.resultFilePath, o.startConfirmFilePath, QString::number(o.appPid),
                      ticks(t.helperWaitExitMs), ticks(t.startConfirmTimeoutMs), swapTool,
                      swapToolRequirement(requirement)};
    plan.description = QStringLiteral("helper: wait for pid %1, verify %2 against \"%3\", "
                                      "swap into %4 with the checked copy of update-swap, start %5, "
                                      "confirm within %6 ms")
                           .arg(o.appPid).arg(prepared.stagedFile, requirement, target, o.toVersion)
                           .arg(t.startConfirmTimeoutMs);
    return plan;
}

HelperPlan linuxHelperPlan(const PreparedInstall& prepared)
{
    const InstallOrder& o = prepared.order;
    const InstallTuning& t = o.context.tuning;
    HelperPlan plan;
    plan.program = QStringLiteral("/bin/sh");
    plan.arguments = {QStringLiteral("-c"), linuxHelperScript(),
                      shortSlug(prepared) + QStringLiteral("-update-helper"),
                      o.context.appLocation, prepared.stagedFile, o.expectedSha256.toLower(),
                      o.toVersion, o.resultFilePath, o.startConfirmFilePath,
                      QString::number(o.appPid), ticks(t.helperWaitExitMs),
                      ticks(t.startConfirmTimeoutMs)};
    plan.description = QStringLiteral("helper: wait for pid %1, check sha256 of %2, swap into %3, "
                                      "start, confirm within %4 ms")
                           .arg(o.appPid).arg(prepared.stagedFile, o.context.appLocation)
                           .arg(t.startConfirmTimeoutMs);
    return plan;
}

// ============================================================================
// Windows plans
// ============================================================================

QStringList msiexecArguments(const QString& msi, const QString& installFolder,
                             const QString& logFile)
{
    QString folder = QDir::toNativeSeparators(QDir::cleanPath(installFolder));
    // msiexec reads a trailing backslash before the closing quote as an
    // escaped quote; the folder never ends in one.
    while (folder.endsWith(QLatin1Char('\\')) && folder.size() > 3) folder.chop(1);
    return {QStringLiteral("/i"), QDir::toNativeSeparators(msi), QStringLiteral("/passive"),
            QStringLiteral("/norestart"), QStringLiteral("INSTALLFOLDER=") + folder,
            QStringLiteral("/l*v"), QDir::toNativeSeparators(logFile)};
}

InstallOutcome::Result msiexecExitToResult(int exitCode, QString* codeOut)
{
    // One mapping for both sides: the helper program uses the same function.
    const winhelper::Outcome o =
        winhelper::outcomeForMsiexec(static_cast<unsigned long>(exitCode < 0 ? 0xFFFFFFFFu : unsigned(exitCode)), {});
    if (codeOut) *codeOut = QString::fromStdString(o.code);
    if (o.result == "ok") return InstallOutcome::Result::Ok;
    if (o.result == "cancelled") return InstallOutcome::Result::Cancelled;
    return InstallOutcome::Result::Failed;
}

// ============================================================================
// What may go to a helper
// ============================================================================

bool validatePreparedForHelper(const PreparedInstall& prepared, QString* why)
{
    const auto fail = [why](const QString& w) { if (why) *why = w; return false; };
    if (why) why->clear();
    const InstallOrder& o = prepared.order;
    if (o.appPid <= 1) return fail(QStringLiteral("no valid pid of the running app"));
    const QString paths[] = {o.context.appLocation, prepared.stagedFile, o.resultFilePath,
                             o.startConfirmFilePath};
    for (const QString& p : paths) {
        if (p.isEmpty() || !QDir::isAbsolutePath(p) || QDir::cleanPath(p) != p ||
            p.contains(QLatin1Char('\n')) || p.contains(QLatin1Char('\r')))
            return fail(QStringLiteral("not a clean absolute path: '%1'").arg(p));
    }
    static const QRegularExpression version(QStringLiteral("^[0-9A-Za-z][0-9A-Za-z.+_-]{0,63}$"));
    if (!version.match(o.toVersion).hasMatch())
        return fail(QStringLiteral("unusable version '%1'").arg(o.toVersion));
    if (!o.productSlug.isEmpty() && helperNamePart(o.productSlug) != o.productSlug)
        return fail(QStringLiteral("unusable product slug '%1'").arg(o.productSlug));
    return true;
}

QString helperNamePart(const QString& slug)
{
    static const QRegularExpression ok(QStringLiteral("^[a-z0-9][a-z0-9-]{0,31}$"));
    return ok.match(slug).hasMatch() ? slug : QStringLiteral("app");
}

// ============================================================================
// POSIX helper start (shared by the macOS and the AppImage backend)
// ============================================================================

#if defined(Q_OS_UNIX)

namespace posixhelper {

bool spawnDetachedHelper(const HelperPlan& plan, const QString& stderrLog,
                         const std::function<void(int)>& takeLifeline, QString* errorOut)
{
    const auto fail = [errorOut](const QString& why) {
        if (errorOut) *errorOut = why;
        return false;
    };
    int fds[2];
    // Both ends close-on-exec from the start where the system can (no window
    // in which another thread's fork inherits the write end); the write end
    // stays in THIS process only, so no child the app starts later (a CLI,
    // a viewer …) keeps the helper waiting.
#if defined(Q_OS_LINUX)
    if (::pipe2(fds, O_CLOEXEC) != 0)
        return fail(QStringLiteral("pipe2: %1").arg(QString::fromLocal8Bit(std::strerror(errno))));
#else
    if (::pipe(fds) != 0) return fail(QStringLiteral("pipe: %1").arg(QString::fromLocal8Bit(std::strerror(errno))));
    ::fcntl(fds[0], F_SETFD, FD_CLOEXEC);
    ::fcntl(fds[1], F_SETFD, FD_CLOEXEC);
#endif

    posix_spawn_file_actions_t fa;
    posix_spawnattr_t attr;
    if (posix_spawn_file_actions_init(&fa) != 0) {
        ::close(fds[0]);
        ::close(fds[1]);
        return fail(QStringLiteral("posix_spawn_file_actions_init failed"));
    }
    if (posix_spawnattr_init(&attr) != 0) {
        posix_spawn_file_actions_destroy(&fa);
        ::close(fds[0]);
        ::close(fds[1]);
        return fail(QStringLiteral("posix_spawnattr_init failed"));
    }
    const QByteArray logPath = QFile::encodeName(stderrLog.isEmpty() ? QStringLiteral("/dev/null") : stderrLog);
    bool actionsOk = posix_spawn_file_actions_adddup2(&fa, fds[0], 0) == 0 &&
                     posix_spawn_file_actions_addclose(&fa, fds[0]) == 0 &&
                     posix_spawn_file_actions_addclose(&fa, fds[1]) == 0 &&
                     posix_spawn_file_actions_addopen(&fa, 1, "/dev/null", O_WRONLY, 0) == 0 &&
                     posix_spawn_file_actions_addopen(&fa, 2, logPath.constData(),
                                                      O_WRONLY | O_CREAT | O_APPEND | O_NOFOLLOW, 0600) == 0;
    short flags = POSIX_SPAWN_SETSID;
#if defined(POSIX_SPAWN_CLOEXEC_DEFAULT)
    flags |= POSIX_SPAWN_CLOEXEC_DEFAULT;  // macOS: no other descriptor survives
#elif defined(__GLIBC__) && (__GLIBC__ > 2 || (__GLIBC__ == 2 && __GLIBC_MINOR__ >= 34))
    actionsOk = actionsOk && posix_spawn_file_actions_addclosefrom_np(&fa, 3) == 0;  // Linux, glibc >= 2.34
#else
    // No closefrom: close every open descriptor above 2 one by one, so none
    // of the app's (a socket, a file) reaches the helper or the new app.
    {
        long maxFd = ::sysconf(_SC_OPEN_MAX);
        if (maxFd < 0 || maxFd > 65536) maxFd = 65536;
        for (int fd = 3; fd < maxFd && actionsOk; ++fd)
            if (fd != fds[0] && fd != fds[1] && ::fcntl(fd, F_GETFD) != -1)
                actionsOk = posix_spawn_file_actions_addclose(&fa, fd) == 0;
    }
#endif
    actionsOk = actionsOk && posix_spawnattr_setflags(&attr, flags) == 0;
    if (!actionsOk) {
        posix_spawn_file_actions_destroy(&fa);
        posix_spawnattr_destroy(&attr);
        ::close(fds[0]);
        ::close(fds[1]);
        return fail(QStringLiteral("cannot prepare the helper's start"));
    }

    QList<QByteArray> argBytes;
    argBytes << QFile::encodeName(plan.program);
    for (const QString& a : plan.arguments) argBytes << a.toUtf8();
    std::vector<char*> argv;
    for (QByteArray& b : argBytes) argv.push_back(b.data());
    argv.push_back(nullptr);

    // Minimal environment: the helper sets PATH and LC_ALL itself; HOME is
    // what `open` and LaunchServices expect; the desktop session variables
    // are what a restarted app needs to open a window on Linux (without
    // them the new version never confirms). The user's language and PATH
    // reach the restarted app, not the helper: the helper runs with LC_ALL=C
    // and a fixed PATH, and hands the originals to the app it starts (Linux:
    // start(); macOS starts through LaunchServices, which does not inherit
    // them). A fixed positive list, values without line breaks.
    QList<QByteArray> envBytes;
    envBytes << QByteArrayLiteral("PATH=/usr/bin:/bin:/usr/sbin:/sbin");
    const auto pass = [&envBytes](const char* name, const QByteArray& as) {
        const QByteArray v = qgetenv(name);
        if (!v.isEmpty() && !v.contains('\n') && !v.contains('\r')) envBytes << as + '=' + v;
    };
    for (const char* name : {"HOME", "DISPLAY", "WAYLAND_DISPLAY", "XDG_RUNTIME_DIR", "DBUS_SESSION_BUS_ADDRESS",
                             "XAUTHORITY", "XDG_SESSION_TYPE", "LANG", "LANGUAGE", "LC_CTYPE", "LC_MESSAGES",
                             "LC_NUMERIC", "LC_TIME", "LC_COLLATE", "LC_MONETARY"})
        pass(name, QByteArray(name));
    pass("LC_ALL", QByteArrayLiteral("APPUPDATE_APP_LC_ALL"));
    pass("PATH", QByteArrayLiteral("APPUPDATE_APP_PATH"));
    std::vector<char*> envp;
    for (QByteArray& b : envBytes) envp.push_back(b.data());
    envp.push_back(nullptr);

    pid_t child = 0;
    const int rc = ::posix_spawn(&child, argBytes.first().constData(), &fa, &attr, argv.data(), envp.data());
    posix_spawn_file_actions_destroy(&fa);
    posix_spawnattr_destroy(&attr);
    ::close(fds[0]);
    if (rc != 0) {
        ::close(fds[1]);
        return fail(QStringLiteral("posix_spawn %1: %2").arg(plan.program, QString::fromLocal8Bit(std::strerror(rc))));
    }
    if (takeLifeline) {
        takeLifeline(fds[1]);
    }
    // Otherwise the write end is deliberately never closed: the kernel closes
    // it when this process ends, and that EOF is the helper's signal.
    return true;
}

int openRegularNoFollow(const QString& path, qint64 expectedSize, QString* why)
{
    const auto fail = [why](const QString& w) { if (why) *why = w; return -1; };
    const int fd = ::open(QFile::encodeName(path).constData(), O_RDONLY | O_NONBLOCK | O_NOFOLLOW | O_CLOEXEC);
    if (fd < 0) return fail(QStringLiteral("cannot open %1 (%2)").arg(path, QString::fromLatin1(std::strerror(errno))));
    struct stat st {};
    if (::fstat(fd, &st) != 0 || !S_ISREG(st.st_mode)) {
        ::close(fd);
        return fail(QStringLiteral("%1 is not a regular file").arg(path));
    }
    if (st.st_size != expectedSize) {
        ::close(fd);
        return fail(QStringLiteral("%1 has %2 bytes, the manifest says %3")
                        .arg(path).arg(qint64(st.st_size)).arg(expectedSize));
    }
    const int fl = ::fcntl(fd, F_GETFL);
    if (fl < 0 || ::fcntl(fd, F_SETFL, fl & ~O_NONBLOCK) != 0) {
        ::close(fd);
        return fail(QStringLiteral("cannot read %1").arg(path));
    }
    return fd;
}

QString copyAndHashFd(int inFd, const QString& dst, unsigned mode, qint64 expectedSize,
                      const std::atomic<bool>* cancel, QString* why)
{
    const QByteArray d = QFile::encodeName(dst);
    const int out = ::open(d.constData(), O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, 0600);
    if (out < 0) {
        if (why) *why = QStringLiteral("cannot create %1 (%2)").arg(dst, QString::fromLatin1(std::strerror(errno)));
        return {};
    }
    const auto fail = [&](const QString& w) {
        ::close(out);
        ::unlink(d.constData());
        if (why) *why = w;
        return QString();
    };
    QCryptographicHash h(QCryptographicHash::Sha256);
    std::vector<char> buf(1 << 20);
    qint64 total = 0;
    for (;;) {
        if (cancel && cancel->load()) return fail(QStringLiteral("cancelled"));
        const ssize_t n = ::read(inFd, buf.data(), buf.size());
        if (n < 0) {
            if (errno == EINTR) continue;
            return fail(QStringLiteral("read error"));
        }
        if (n == 0) break;
        total += n;
        if (total > expectedSize) return fail(QStringLiteral("the file grew while it was read"));
        h.addData(QByteArrayView(buf.data(), n));
        for (ssize_t off = 0; off < n;) {
            const ssize_t w = ::write(out, buf.data() + off, size_t(n - off));
            if (w < 0) {
                if (errno == EINTR) continue;
                return fail(QStringLiteral("write error"));
            }
            off += w;
        }
    }
    if (total != expectedSize) return fail(QStringLiteral("the file shrank while it was read"));
    if (::fchmod(out, mode_t(mode)) != 0 || ::fsync(out) != 0) return fail(QStringLiteral("cannot finish the copy"));
    if (::close(out) != 0) {
        ::unlink(d.constData());
        if (why) *why = QStringLiteral("cannot finish the copy");
        return {};
    }
    return QString::fromLatin1(h.result().toHex());
}

#if defined(Q_OS_MACOS)
namespace {
// An ACL entry that ALLOWS another principal to add or delete entries.
bool aclAllowsOthersToWrite(int fd)
{
    acl_t acl = ::acl_get_fd_np(fd, ACL_TYPE_EXTENDED);
    if (!acl) return errno != ENOENT;  // no ACL: fine; an error is no "no"
    uuid_t me;
    if (::mbr_uid_to_uuid(::getuid(), me) != 0) {
        ::acl_free(acl);
        return true;
    }
    bool others = false;
    acl_entry_t entry = nullptr;
    for (int which = ACL_FIRST_ENTRY; ::acl_get_entry(acl, which, &entry) == 0; which = ACL_NEXT_ENTRY) {
        acl_tag_t tag;
        if (::acl_get_tag_type(entry, &tag) != 0) { others = true; break; }  // unreadable entry: no "no"
        if (tag != ACL_EXTENDED_ALLOW) continue;                              // deny entries grant nothing
        acl_permset_t perms;
        if (::acl_get_permset(entry, &perms) != 0) { others = true; break; }
        // Anything beyond reading counts: whoever may change the security
        // information or the owner can give themselves write access.
        static const acl_perm_t kChanging[] = {ACL_ADD_FILE, ACL_ADD_SUBDIRECTORY, ACL_DELETE_CHILD, ACL_DELETE,
                                               ACL_WRITE_ATTRIBUTES, ACL_WRITE_EXTATTRIBUTES, ACL_WRITE_SECURITY,
                                               ACL_CHANGE_OWNER};
        bool writes = false;
        for (acl_perm_t p : kChanging) writes = writes || ::acl_get_perm_np(perms, p) != 0;
        if (!writes) continue;
        auto* who = static_cast<unsigned char*>(::acl_get_qualifier(entry));
        const bool isMe = who && std::memcmp(who, me, sizeof(uuid_t)) == 0;
        if (who) ::acl_free(who);
        if (!isMe) { others = true; break; }
    }
    ::acl_free(acl);
    return others;
}
}  // namespace
#endif

bool parentIsShared(const QString& path, unsigned adminGid, QString* rule)
{
    const auto shared = [rule](const QString& r, bool ancestor) {
        if (rule) *rule = ancestor ? QStringLiteral("ancestor ") + r : r;
        return true;
    };
    if (rule) rule->clear();
    const QByteArray parent = QFile::encodeName(QFileInfo(path).absolutePath());
    char* real = ::realpath(parent.constData(), nullptr);
    if (!real) return shared(QStringLiteral("unmeasurable"), false);
    QString dir = QFile::decodeName(QByteArray(real));
    std::free(real);
    for (int level = 0; level < 128; ++level) {
        const bool ancestor = level > 0;
        const int fd = ::open(QFile::encodeName(dir).constData(), O_RDONLY | O_DIRECTORY | O_CLOEXEC);
        if (fd < 0) return shared(QStringLiteral("unmeasurable"), ancestor);
        struct stat st {};
        if (::fstat(fd, &st) != 0) {
            ::close(fd);
            return shared(QStringLiteral("unmeasurable"), ancestor);
        }
        QString found;
#if defined(Q_OS_MACOS)
        // Without owners every user counts as owner: the mode bits say
        // nothing there, so this comes first.
        struct statfs sfs {};
        if (::fstatfs(fd, &sfs) != 0)
            found = QStringLiteral("unmeasurable");
        else if (sfs.f_flags & MNT_IGNORE_OWNERSHIP)
            found = QStringLiteral("ignore-ownership");
#endif
        if (found.isEmpty())
            if (const char* r = sharedParentRule(st.st_uid, st.st_gid, st.st_mode, ::getuid(), ::getgid(), adminGid))
                found = QString::fromLatin1(r);
#if defined(Q_OS_MACOS)
        if (found.isEmpty() && aclAllowsOthersToWrite(fd)) found = QStringLiteral("acl");
#endif
        ::close(fd);
        if (!found.isEmpty()) return shared(found, ancestor);
        if (dir == QLatin1String("/")) return false;
        dir = QFileInfo(dir).path();
    }
    return shared(QStringLiteral("unmeasurable"), true);
}

namespace {

// Removes the entry `name` inside the directory `parentFd`, never following
// a link and never leaving device `dev`. A directory is opened relative to
// its parent with O_NOFOLLOW and then checked by device and inode against
// what fstatat saw, so a directory swapped for a link (or another
// directory) between the two calls is noticed and left alone. No chdir (it
// would hold for every thread of the app).
std::function<void(const QByteArray&)>& removeHookForTests()
{
    static std::function<void(const QByteArray&)> hook;
    return hook;
}

bool removeAt(int parentFd, const char* name, dev_t dev, int depth)
{
    if (depth > 256) return false;
    struct stat st {};
    if (::fstatat(parentFd, name, &st, AT_SYMLINK_NOFOLLOW) != 0) return errno == ENOENT;
    if (!S_ISDIR(st.st_mode)) return ::unlinkat(parentFd, name, 0) == 0;  // files and links themselves
    if (st.st_dev != dev) return false;  // another file system is mounted here
    if (removeHookForTests()) removeHookForTests()(QByteArray(name));  // tests: change it between the looks
    const int fd = ::openat(parentFd, name, O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
    if (fd < 0) return false;
    struct stat opened {};
    if (::fstat(fd, &opened) != 0 || opened.st_dev != st.st_dev || opened.st_ino != st.st_ino) {
        ::close(fd);
        return false;  // changed between the two looks: do not touch it
    }
    const int listFd = ::dup(fd);
    DIR* d = listFd >= 0 ? ::fdopendir(listFd) : nullptr;
    if (!d) {
        if (listFd >= 0) ::close(listFd);
        ::close(fd);
        return false;
    }
    std::vector<QByteArray> names;
    while (dirent* e = ::readdir(d)) {
        if (std::strcmp(e->d_name, ".") == 0 || std::strcmp(e->d_name, "..") == 0) continue;
        names.emplace_back(e->d_name);
    }
    ::closedir(d);
    bool ok = true;
    for (const QByteArray& n : names) ok = removeAt(fd, n.constData(), dev, depth + 1) && ok;
    ::close(fd);
    return ::unlinkat(parentFd, name, AT_REMOVEDIR) == 0 && ok;
}

}  // namespace

void setRemoveTreeHookForTests(std::function<void(const QByteArray& name)> hook)
{
    removeHookForTests() = std::move(hook);
}

bool removeTreeNoFollow(const QString& path)
{
    if (path.isEmpty() || !QDir::isAbsolutePath(path) || QDir::cleanPath(path) != path) return false;
    const QFileInfo fi(path);
    const QByteArray parent = QFile::encodeName(fi.path());
    const QByteArray name = QFile::encodeName(fi.fileName());
    if (name.isEmpty() || name == "." || name == "..") return false;
    const int parentFd = ::open(parent.constData(), O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    if (parentFd < 0) return false;
    struct stat ps {};
    bool ok = false;
    if (::fstat(parentFd, &ps) == 0) {
        struct stat st {};
        if (::fstatat(parentFd, name.constData(), &st, AT_SYMLINK_NOFOLLOW) != 0)
            ok = errno == ENOENT;
        else
            ok = removeAt(parentFd, name.constData(), st.st_dev, 0);
    }
    ::close(parentFd);
    return ok;
}

}  // namespace posixhelper

#endif  // Q_OS_UNIX

// ============================================================================
// AppImage swap (Linux; compiled on every POSIX system so its tests run on
// the macOS CI too)
// ============================================================================

#if defined(Q_OS_UNIX)

namespace {

class AppImageSwapBackend final : public InstallBackend {
public:
    explicit AppImageSwapBackend(SelfInstallHooks hooks) : hooks_(std::move(hooks)) {}

    InstallCapability capability(const InstallContext& context) const override
    {
        return selfInstallDecision(SelfInstallPlatform::Linux, context, measure(context));
    }

    void prepare(const InstallOrder& order, PrepareDone done) override
    {
        const int gen = ++generation_;
        PrepareResult result = prepareNow(order);
        QMetaObject::invokeMethod(&ctx_, [this, gen, result, done = std::move(done)]() {
            if (gen == generation_.load()) {
                done(result);
            } else if (result.prepared) {
                QFile::remove(result.prepared->stagedFile);  // cancelled: leave nothing behind
            }
        }, Qt::QueuedConnection);
    }

    void cancelPrepare() override { ++generation_; }

    LaunchResult launch(const PreparedInstall& prepared) override
    {
        LaunchResult r;
        if (launched_) {
            r.error = QStringLiteral("a helper was already started for this install");
            return r;
        }
        if (!validatePreparedForHelper(prepared, &r.error)) return r;
        if (prepared.stagedFile != prepared.order.context.appLocation + QStringLiteral(".update-new")) {
            r.error = QStringLiteral("the prepared install is not one of ours");
            return r;
        }
        const HelperPlan plan = linuxHelperPlan(prepared);
        r.started = posixhelper::spawnDetachedHelper(plan, prepared.order.resultFilePath + QStringLiteral(".log"),
                                                     hooks_.takeLifeline, &r.error);
        launched_ = r.started;
        return r;
    }

    void discard(const PreparedInstall& prepared) override
    {
        const QString staged = prepared.stagedFile;
        if (staged == prepared.order.context.appLocation + QStringLiteral(".update-new"))
            QFile::remove(staged);
    }

    void cleanupAfterStart(const InstallContext& context) override
    {
        if (context.appLocation.isEmpty()) return;
        const QString target = context.appLocation;
        const QString old = target + QStringLiteral(".update-old");
        const QFileInfo t(target), o(old);
        // A helper that died between the renames: the old image comes back.
        if (!t.exists() && !t.isSymLink() && o.isFile() && !o.isSymLink()) {
            ::rename(QFile::encodeName(old).constData(), QFile::encodeName(target).constData());
            return;
        }
        // Remove the old image only when a real file stands at the target.
        if (!(t.isFile() && !t.isSymLink())) return;
        for (const char* suffix : {".update-old", ".update-failed", ".update-new"}) {
            const QFileInfo fi(target + QLatin1String(suffix));
            if (fi.isFile() || fi.isSymLink()) QFile::remove(fi.absoluteFilePath());
        }
    }

private:
    SelfInstallFacts measure(const InstallContext& c) const
    {
        if (hooks_.facts) return hooks_.facts(c);
        SelfInstallFacts f;
        const QFileInfo fi(c.appLocation);
        f.locationExists = !c.appLocation.isEmpty() && fi.isFile();
        f.locationWritable = f.locationExists &&
                             ::access(QFile::encodeName(fi.absolutePath()).constData(), W_OK) == 0;
        f.helperAvailable = ::access("/bin/sh", X_OK) == 0;
        f.otherInstanceRunning = otherProcessRuns(fi.absoluteFilePath());
        f.platformMeasured = kLinuxSelfInstallMeasured || hooks_.assumePlatformMeasuredForTests;
        f.locationShared = posixhelper::parentIsShared(fi.absoluteFilePath(), kNoAdminGroup, &f.locationSharedRule);
        return f;
    }

    // Compiled on every POSIX system (so it is at least type-checked on the
    // macOS CI), called on Linux only: macOS has no /proc.
    static bool scanProcForOtherInstance(const QString& exe)
    {
        const QByteArray want = QFile::encodeName(exe);
        // The AppImage runtime that mounted this app is one of our ANCESTORS,
        // and its /proc/<pid>/exe is the AppImage file itself: not "another
        // instance". Collect the chain of parents first. (Reasoned from the
        // runtime's design, not measured on a Linux desktop.)
        QList<pid_t> ours{::getpid()};
        for (pid_t p = ::getpid(); p > 1 && ours.size() < 64;) {
            char statPath[64];
            std::snprintf(statPath, sizeof statPath, "/proc/%d/stat", int(p));
            QFile st(QString::fromLatin1(statPath));
            if (!st.open(QIODevice::ReadOnly)) break;
            const QByteArray line = st.read(4096);
            const int close = line.lastIndexOf(')');  // the name may contain spaces
            const QList<QByteArray> rest = line.mid(close + 2).split(' ');
            if (close < 0 || rest.size() < 2) break;
            p = static_cast<pid_t>(rest.at(1).toInt());
            if (p > 1) ours << p;
        }
        DIR* d = ::opendir("/proc");
        if (!d) return true;  // unknown is no "no"
        bool found = false;
        while (dirent* e = ::readdir(d)) {
            const pid_t pid = static_cast<pid_t>(std::strtol(e->d_name, nullptr, 10));
            if (pid <= 0 || ours.contains(pid)) continue;
            char link[64];
            std::snprintf(link, sizeof link, "/proc/%d/exe", int(pid));
            char buf[PATH_MAX + 1];
            const ssize_t n = ::readlink(link, buf, PATH_MAX);
            if (n > 0 && QByteArray(buf, int(n)) == want) { found = true; break; }
        }
        ::closedir(d);
        return found;
    }

    static bool otherProcessRuns(const QString& exe)
    {
#if defined(Q_OS_LINUX)
        return scanProcForOtherInstance(exe);
#else
        Q_UNUSED(exe);
        return false;
#endif
    }

    PrepareResult prepareNow(const InstallOrder& order) const
    {
        PrepareResult res;
        const InstallCapability cap = capability(order.context);
        if (cap.method != InstallMethod::Automatic) {
            res.manual = cap.reason;
            res.error = cap.detail;
            return res;
        }
        if (QDir::cleanPath(order.context.verifiedFile) != QDir::cleanPath(order.verifiedFile)) {
            res.error = QStringLiteral("the order names two different files");
            return res;
        }
        // Read through ONE descriptor (no FIFO, no link), write a new file
        // next to the app (O_EXCL|O_NOFOLLOW) and hash what was written.
        QString err;
        const int in = posixhelper::openRegularNoFollow(order.verifiedFile, order.expectedSize, &err);
        if (in < 0) {
            res.error = err;
            return res;
        }
        const QString staged = order.context.appLocation + QStringLiteral(".update-new");
        if (QFileInfo(staged).isSymLink() || QFileInfo::exists(staged)) QFile::remove(staged);
        const QString got = posixhelper::copyAndHashFd(in, staged, 0755, order.expectedSize, nullptr, &err);
        ::close(in);
        if (got.isEmpty() || got != order.expectedSha256.toLower()) {
            QFile::remove(staged);
            res.error = err.isEmpty() ? QStringLiteral("the downloaded file no longer matches the manifest hash") : err;
            return res;
        }
        PreparedInstall p;
        p.order = order;
        p.method = InstallMethod::Automatic;
        p.stagedFile = staged;
        p.description = linuxHelperPlan(p).description;
        res.prepared = p;
        return res;
    }

    SelfInstallHooks hooks_;
    QObject ctx_;
    std::atomic<int> generation_{0};
    bool launched_ = false;
};

}  // namespace

std::unique_ptr<InstallBackend> makeAppImageSwapBackend(SelfInstallHooks hooks)
{
    return std::make_unique<AppImageSwapBackend>(std::move(hooks));
}

#endif  // Q_OS_UNIX

// ============================================================================
// Factory
// ============================================================================

std::unique_ptr<InstallBackend> makeSelfInstallBackend(SelfInstallHooks hooks)
{
#if defined(Q_OS_MACOS)
    return makeMacBundleSwapBackend(std::move(hooks));
#elif defined(Q_OS_UNIX)
    return makeAppImageSwapBackend(std::move(hooks));
#else
    Q_UNUSED(hooks);
    return nullptr;
#endif
}

}  // namespace appupdate
