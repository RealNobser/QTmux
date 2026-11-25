/* update-swap — swaps the app bundle with its staged successor in ONE step
 * (macOS, renamex_np with RENAME_SWAP), so the name of the app is never
 * free. Part of the self-installation (src/update/SelfInstall.*); it lives
 * inside every family bundle under Contents/Helpers/ and is signed with it,
 * identifier "<bundle identifier>.update-swap". The app never runs it from
 * there: prepare() copies it into a private folder and checks the COPY
 * against the identity of the running app; only that copy is started.
 *
 *   update-swap <target.app> <staged>
 *
 * <target> is the bundle the app runs from, <staged> its sibling named
 * ".<Name>.app.update-staged-<hex>". After the swap <staged> holds the OLD
 * bundle; the same call again swaps back (that is the way back).
 *
 * It checks everything itself and does nothing when a check fails: two
 * arguments (or the one mode below), no other options, no environment,
 * nothing printed but a short reason code. Both paths clean and absolute, the same parent folder, the second
 * name on the staging scheme, both existing directories and no links. The
 * parent folder must be its own real path (no link anywhere on the way);
 * it is opened ONCE, and everything after that - the kind of both entries
 * and the swap - happens relative to that open folder, so a link set into
 * the path afterwards changes nothing.
 *
 * Exit: 0 swapped · 2 misuse (arguments) · 3 not our names, not plain
 * directories, or a folder that is not its own real path · 4 the file system cannot swap (ENOTSUP, e.g. SMB, exFAT) ·
 * 5 not permitted (EPERM — e.g. the system's App Management; the app
 * remembers this as the reason for the next attempt) · 6 access denied
 * (EACCES — a folder permission, not remembered) ·
 * 1 the swap failed otherwise (nothing changed). The errno text goes to
 * stderr.
 *
 *   update-swap --check-running <pid> <target.app> <requirement> <not-before>
 *
 * Asked by the helper about the process that confirmed the start of the new
 * version (the pid comes from that confirmation, i.e. from the very process
 * that is to be checked). It holds only when ALL of these hold, each read
 * from the kernel, never from the process itself:
 *   - the file the process runs (proc_pidpath) IS the main program of
 *     <target.app> (its CFBundleExecutable), and the code the system sees
 *     for that pid has <target.app> as its bundle;
 *   - the process started at or after <not-before> (seconds since the epoch,
 *     taken by the helper right before it started the new version);
 *   - the RUNNING code satisfies <requirement> (dynamic check).
 * A process can rewrite its own argument area (that is what `ps` shows as
 * the command), but not the file the kernel started it from.
 * Exit: 0 holds · 2 misuse · 10 no such process (or not readable) · 11 not
 * the target's executable · 12 started before <not-before> · 13 the running
 * code fails the requirement · 14 the requirement cannot be read · 15 the
 * process belongs to another user.
 *
 * Plain C; libc, and on macOS CoreFoundation and Security (for
 * --check-running). CANONICAL: QTmux vendors src/update/. */
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#if defined(__APPLE__)
#include <AvailabilityMacros.h>
#include <CoreFoundation/CoreFoundation.h>
#include <Security/Security.h>
#include <libproc.h>
#include <sys/proc_info.h>
#endif

static int cleanAbsolute(const char* p)
{
    const size_t n = strlen(p);
    if (n < 2 || n >= PATH_MAX || p[0] != '/' || p[n - 1] == '/') return 0;
    if (strstr(p, "//") || strstr(p, "/./") || strstr(p, "/../")) return 0;
    if (n >= 2 && strcmp(p + n - 2, "/.") == 0) return 0;
    if (n >= 3 && strcmp(p + n - 3, "/..") == 0) return 0;
    for (size_t i = 0; i < n; ++i)
        if ((unsigned char)p[i] < 0x20 || p[i] == 0x7f) return 0;
    return 1;
}

static const char* baseName(const char* p) { return strrchr(p, '/') + 1; }

static int sameParent(const char* a, const char* b)
{
    const size_t la = (size_t)(baseName(a) - a), lb = (size_t)(baseName(b) - b);
    return la == lb && strncmp(a, b, la) == 0;
}

/* ".<Name>.update-staged-<1..16 lowercase hex>" for target name <Name>. */
static int onStagingScheme(const char* targetName, const char* stagedName)
{
    static const char kMid[] = ".update-staged-";
    const size_t tn = strlen(targetName), sn = strlen(stagedName), mn = sizeof kMid - 1;
    if (tn < 5 || strcmp(targetName + tn - 4, ".app") != 0) return 0;
    if (sn < 1 + tn + mn + 1 || sn > 1 + tn + mn + 16) return 0;
    if (stagedName[0] != '.' || strncmp(stagedName + 1, targetName, tn) != 0) return 0;
    if (strncmp(stagedName + 1 + tn, kMid, mn) != 0) return 0;
    for (const char* h = stagedName + 1 + tn + mn; *h; ++h)
        if (!((*h >= '0' && *h <= '9') || (*h >= 'a' && *h <= 'f'))) return 0;
    return 1;
}

static int plainDirectoryAt(int dirFd, const char* name)
{
    struct stat st;
    return fstatat(dirFd, name, &st, AT_SYMLINK_NOFOLLOW) == 0 && S_ISDIR(st.st_mode);
}

/* Opens the parent folder of `p` once: it must be its own real path, and the
 * opened folder must be the one that path names (device and inode). -1 if not. */
static int openParent(const char* p)
{
    char parent[PATH_MAX];
    const size_t n = (size_t)(baseName(p) - p) - 1; /* without the '/' before the name */
    if (n == 0 || n >= sizeof parent) return -1;
    memcpy(parent, p, n);
    parent[n] = '\0';
    char* real = realpath(parent, NULL);
    if (!real) return -1;
    const int same = strcmp(real, parent) == 0;
    free(real);
    if (!same) return -1;
    const int fd = open(parent, O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
    if (fd < 0) return -1;
    struct stat a, b;
    if (fstat(fd, &a) != 0 || lstat(parent, &b) != 0 || a.st_dev != b.st_dev || a.st_ino != b.st_ino) {
        close(fd);
        return -1;
    }
#if defined(__APPLE__)
    /* What was opened is still the folder under its real path (a link set
     * between realpath and open would show here). */
    char opened[PATH_MAX];
    if (fcntl(fd, F_GETPATH, opened) == -1 || strcmp(opened, parent) != 0) {
        close(fd);
        return -1;
    }
#endif
    return fd;
}

#if defined(__APPLE__)
static int digitsOnly(const char* s, long long* out)
{
    if (!*s || strlen(s) > 18) return 0;
    long long v = 0;
    for (const char* c = s; *c; ++c) {
        if (*c < '0' || *c > '9') return 0;
        v = v * 10 + (*c - '0');
    }
    *out = v;
    return 1;
}

static int checkRunning(const char* pidText, const char* target, const char* requirement, const char* notBefore)
{
    long long pidValue = 0, since = 0;
    if (!digitsOnly(pidText, &pidValue) || pidValue <= 1 || pidValue > 0x7fffffff || !digitsOnly(notBefore, &since) ||
        !cleanAbsolute(target) || strlen(target) < 5 || strcmp(target + strlen(target) - 4, ".app") != 0 ||
        !*requirement) {
        fputs("update-swap: usage\n", stderr);
        return 2;
    }
    const pid_t pid = (pid_t)pidValue;
    /* 1. The file the kernel started the process from. */
    char path[PROC_PIDPATHINFO_MAXSIZE];
    if (proc_pidpath(pid, path, sizeof path) <= 0) { fputs("update-swap: no such process\n", stderr); return 10; }
    char mainExe[PATH_MAX] = "";
    CFURLRef bundleUrl = CFURLCreateFromFileSystemRepresentation(kCFAllocatorDefault, (const UInt8*)target,
                                                                 (CFIndex)strlen(target), true);
    CFBundleRef bundleRef = bundleUrl ? CFBundleCreate(kCFAllocatorDefault, bundleUrl) : NULL;
    CFURLRef exeUrl = bundleRef ? CFBundleCopyExecutableURL(bundleRef) : NULL;
    if (exeUrl) {
        CFURLRef abs = CFURLCopyAbsoluteURL(exeUrl);
        if (abs) {
            CFURLGetFileSystemRepresentation(abs, true, (UInt8*)mainExe, sizeof mainExe);
            CFRelease(abs);
        }
        CFRelease(exeUrl);
    }
    if (bundleRef) CFRelease(bundleRef);
    if (bundleUrl) CFRelease(bundleUrl);
    if (!mainExe[0] || strcmp(path, mainExe) != 0) {
        fprintf(stderr, "update-swap: runs %s, not %s\n", path, mainExe);
        return 11;
    }
    /* 2. Started after the helper started the new version. */
    struct proc_bsdinfo bsd;
    if (proc_pidinfo(pid, PROC_PIDTBSDINFO, 0, &bsd, (int)sizeof bsd) != (int)sizeof bsd) {
        fputs("update-swap: no such process\n", stderr);
        return 10;
    }
    if (bsd.pbi_uid != getuid()) {
        fputs("update-swap: another user's process\n", stderr);
        return 15;
    }
    if ((long long)bsd.pbi_start_tvsec < since) {
        fprintf(stderr, "update-swap: started %llu, before %lld\n", (unsigned long long)bsd.pbi_start_tvsec, since);
        return 12;
    }
    /* 3. The running code: its bundle, and the requirement. */
    CFNumberRef num = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &pid);
    if (!num) { fputs("update-swap: no such process\n", stderr); return 10; }
    const void* keys[] = {kSecGuestAttributePid};
    const void* vals[] = {num};
    CFDictionaryRef attrs = CFDictionaryCreate(kCFAllocatorDefault, keys, vals, 1, &kCFTypeDictionaryKeyCallBacks,
                                               &kCFTypeDictionaryValueCallBacks);
    CFRelease(num);
    if (!attrs) { fputs("update-swap: no such process\n", stderr); return 10; }
    SecCodeRef code = NULL;
    const OSStatus got = SecCodeCopyGuestWithAttributes(NULL, attrs, kSecCSDefaultFlags, &code);
    CFRelease(attrs);
    if (got != errSecSuccess || !code) { fputs("update-swap: no such process\n", stderr); return 10; }
    int rc = 0;
    CFURLRef url = NULL;
    char bundle[PATH_MAX] = "";
    if (SecCodeCopyPath(code, kSecCSDefaultFlags, &url) == errSecSuccess && url) {
        CFURLGetFileSystemRepresentation(url, true, (UInt8*)bundle, sizeof bundle);
        CFRelease(url);
    }
    if (strcmp(bundle, target) != 0) {
        fprintf(stderr, "update-swap: code of %s\n", bundle);
        rc = 11;
    }
    CFStringRef reqText = rc ? NULL : CFStringCreateWithCString(kCFAllocatorDefault, requirement, kCFStringEncodingUTF8);
    SecRequirementRef req = NULL;
    if (!rc && (!reqText || SecRequirementCreateWithString(reqText, kSecCSDefaultFlags, &req) != errSecSuccess)) {
        fputs("update-swap: requirement\n", stderr);
        rc = 14;
    }
    if (reqText) CFRelease(reqText);
    if (!rc && SecCodeCheckValidity(code, kSecCSDefaultFlags, req) != errSecSuccess) {
        fputs("update-swap: the running code fails the requirement\n", stderr);
        rc = 13;
    }
    if (req) CFRelease(req);
    CFRelease(code);
    return rc;
}
#endif

/* The build id of the app this helper ships with ("<version>+<hash>",
 * "-dirty" for an uncommitted tree), the same string the app carries, so a
 * tool can tell which build a helper in a package came from. The app's build
 * hands it in: appupdate_embed_install_helpers(... BUILD_ID_HEADER <header>
 * BUILD_ID_MACRO <name>) includes that header first (a forced include, no
 * #include here) and defines APPUPDATE_HELPER_BUILD_ID as <name>. Without it:
 * "unknown". Kept in the binary on purpose (used, and read once in main);
 * nothing prints it - the helper takes no other options. */
#ifndef APPUPDATE_HELPER_BUILD_ID
#define APPUPDATE_HELPER_BUILD_ID "unknown"
#endif
__attribute__((used)) static const char kHelperBuildId[] = "update-swap build " APPUPDATE_HELPER_BUILD_ID;

int main(int argc, char** argv)
{
    (void)*(volatile const char*)kHelperBuildId;
#if defined(__APPLE__)
    if (argc == 6 && strcmp(argv[1], "--check-running") == 0) return checkRunning(argv[2], argv[3], argv[4], argv[5]);
#endif
    if (argc != 3) { fputs("update-swap: usage\n", stderr); return 2; }
    const char* target = argv[1];
    const char* staged = argv[2];
    if (!cleanAbsolute(target) || !cleanAbsolute(staged)) { fputs("update-swap: path\n", stderr); return 2; }
    if (!sameParent(target, staged) || !onStagingScheme(baseName(target), baseName(staged))) {
        fputs("update-swap: names\n", stderr);
        return 3;
    }
    const int dirFd = openParent(target);
    if (dirFd < 0) { fputs("update-swap: parent\n", stderr); return 3; }
    if (!plainDirectoryAt(dirFd, baseName(target)) || !plainDirectoryAt(dirFd, baseName(staged))) {
        close(dirFd);
        fputs("update-swap: kind\n", stderr);
        return 3;
    }
#if defined(__APPLE__)
    const int rc = renameatx_np(dirFd, baseName(staged), dirFd, baseName(target), RENAME_SWAP);
    const int e = errno;
    close(dirFd);
    if (rc == 0) return 0;
    fprintf(stderr, "update-swap: %s\n", strerror(e));
    if (e == ENOTSUP) return 4;
    if (e == EPERM) return 5;
    if (e == EACCES) return 6;
    return 1;
#else
    close(dirFd);
    fputs("update-swap: enotsup\n", stderr);
    return 4;
#endif
}
