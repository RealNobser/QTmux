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
 * arguments, no options, no environment, nothing printed but a short reason
 * code. Both paths clean and absolute, the same parent folder, the second
 * name on the staging scheme, both existing directories and no links.
 *
 * Exit: 0 swapped · 2 misuse (arguments) · 3 not our names or not plain
 * directories · 4 the file system cannot swap (ENOTSUP, e.g. SMB, exFAT) ·
 * 5 not permitted (EPERM — e.g. the system's App Management; the app
 * remembers this as the reason for the next attempt) · 6 access denied
 * (EACCES — a folder permission, not remembered) ·
 * 1 the swap failed otherwise (nothing changed). The errno text goes to
 * stderr.
 *
 * Plain C, libc only. CANONICAL: QTmux vendors src/update/. */
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#if defined(__APPLE__)
#include <AvailabilityMacros.h>
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

static int plainDirectory(const char* p)
{
    struct stat st;
    return lstat(p, &st) == 0 && S_ISDIR(st.st_mode);
}

int main(int argc, char** argv)
{
    if (argc != 3) { fputs("update-swap: usage\n", stderr); return 2; }
    const char* target = argv[1];
    const char* staged = argv[2];
    if (!cleanAbsolute(target) || !cleanAbsolute(staged)) { fputs("update-swap: path\n", stderr); return 2; }
    if (!sameParent(target, staged) || !onStagingScheme(baseName(target), baseName(staged))) {
        fputs("update-swap: names\n", stderr);
        return 3;
    }
    if (!plainDirectory(target) || !plainDirectory(staged)) { fputs("update-swap: kind\n", stderr); return 3; }
#if defined(__APPLE__)
    if (renamex_np(staged, target, RENAME_SWAP) == 0) return 0;
    const int e = errno;
    fprintf(stderr, "update-swap: %s\n", strerror(e));
    if (e == ENOTSUP) return 4;
    if (e == EPERM) return 5;
    if (e == EACCES) return 6;
    return 1;
#else
    fputs("update-swap: enotsup\n", stderr);
    return 4;
#endif
}
