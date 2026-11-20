#pragma once

// Internal to SelfInstall*.cpp — not part of the public appupdate surface.

#include "update/SelfInstall.hpp"

#include <atomic>
#include <functional>
#include <memory>

namespace appupdate {

#if defined(Q_OS_UNIX)
namespace posixhelper {

// Starts `plan` detached (own session), stdin = the read end of the
// lifeline pipe, stdout /dev/null, stderr appended to `stderrLog`, minimal
// environment. The write end goes to `takeLifeline` when set (tests);
// otherwise it stays open in this process until the process ends.
bool spawnDetachedHelper(const HelperPlan& plan, const QString& stderrLog,
                         const std::function<void(int)>& takeLifeline,
                         QString* errorOut);

// Removes `path` without ever following a symlink: a link (also at the top)
// is unlinked; a directory is walked through descriptors (openat with
// O_NOFOLLOW, fstatat, unlinkat), each level checked by device and inode,
// never crossing into another file system. Clean absolute paths only. True
// when nothing is left.
bool removeTreeNoFollow(const QString& path);
// Tests only: called with each directory name between its fstatat and its
// openat, so a test can swap the directory there. Production never sets it.
void setRemoveTreeHookForTests(std::function<void(const QByteArray& name)> hook);

// isSharedParent() for the folder that holds `path` AND every folder above
// it up to "/" (whoever may rename an ancestor can swap the whole folder),
// measured after resolving links (realpath) through a descriptor (fstat),
// never by name. On macOS an ACL that ALLOWS someone other than the user to
// add or delete entries counts as shared too (deny entries do not). A
// failed measurement counts as shared.
// `rule` (optional) names what applied: a sharedParentRule() name, "acl",
// "ignore-ownership" (a volume mounted without owners: every user counts as
// owner), "unmeasurable", each prefixed "ancestor " when it was not the
// parent itself.
bool parentIsShared(const QString& path, unsigned adminGid, QString* rule = nullptr);

// Opens `path` for reading without blocking on a FIFO or device and without
// following a link (O_NONBLOCK|O_NOFOLLOW), then checks AT THE DESCRIPTOR
// that it is a regular file of `expectedSize` bytes and switches back to
// blocking reads. -1 with `why` otherwise. (The family rule: a path from
// outside is an input; isFile() on the name and open() afterwards leave a
// window in which a FIFO can be put in.)
int openRegularNoFollow(const QString& path, qint64 expectedSize, QString* why);

// Copies exactly `expectedSize` bytes from `inFd` into a NEW file `dst`
// (O_CREAT|O_EXCL|O_NOFOLLOW, then fchmod to `mode`) and hashes what it
// wrote. Lowercase hex SHA-256, or empty with `why` (and `dst` removed).
// `cancel` may be null.
QString copyAndHashFd(int inFd, const QString& dst, unsigned mode, qint64 expectedSize,
                      const std::atomic<bool>* cancel, QString* why);

}  // namespace posixhelper

std::unique_ptr<InstallBackend> makeAppImageSwapBackend(SelfInstallHooks hooks);
#endif

#if defined(Q_OS_MACOS)
std::unique_ptr<InstallBackend> makeMacBundleSwapBackend(SelfInstallHooks hooks);

namespace posixhelper {
// The user's own temp folder (confstr _CS_DARWIN_USER_TEMP_DIR, resolved):
// 0700 and independent of $TMPDIR, which the environment may point anywhere.
// Private copies of the self-installation live here. Empty when unknown.
QString userTempDir();
}  // namespace posixhelper
#endif

}  // namespace appupdate
