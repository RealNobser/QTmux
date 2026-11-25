// macOS bundle swap — see SelfInstall.hpp for the rules.

#include "update/SelfInstallPosix.hpp"

#if defined(Q_OS_MACOS)

#include <QCryptographicHash>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QLoggingCategory>
#include <QMetaObject>
#include <QCoreApplication>
#include <QDateTime>
#include <QTimeZone>
#include <QObject>
#include <QPointer>
#include <QProcess>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QSet>
#include <QTemporaryDir>
#include <QThread>

#include <CoreFoundation/CoreFoundation.h>
#include <Security/Security.h>

#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>

#include <libproc.h>
#include <sys/mount.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/xattr.h>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <cerrno>
#include <unistd.h>

namespace appupdate {

namespace {

Q_LOGGING_CATEGORY(lcSelfInstallMac, "appupdate.selfinstall")

// ---- small CF helpers ------------------------------------------------------

template <typename T>
struct CfRef {
    T ref = nullptr;
    CfRef() = default;
    explicit CfRef(T r) : ref(r) {}
    ~CfRef() { if (ref) CFRelease(ref); }
    CfRef(const CfRef&) = delete;
    CfRef& operator=(const CfRef&) = delete;
};

QString cfString(CFStringRef s)
{
    if (!s) return {};
    const CFIndex len = CFStringGetLength(s);
    const CFIndex max = CFStringGetMaximumSizeForEncoding(len, kCFStringEncodingUTF8) + 1;
    QByteArray buf(int(max), '\0');
    if (!CFStringGetCString(s, buf.data(), max, kCFStringEncodingUTF8)) return {};
    return QString::fromUtf8(buf.constData());
}

CFStringRef newCfString(const QString& s)
{
    const QByteArray u = s.toUtf8();
    return CFStringCreateWithBytes(kCFAllocatorDefault, reinterpret_cast<const UInt8*>(u.constData()),
                                   u.size(), kCFStringEncodingUTF8, false);
}

QString osStatusText(OSStatus st)
{
    CfRef<CFStringRef> msg(SecCopyErrorMessageString(st, nullptr));
    return QStringLiteral("OSStatus %1 (%2)").arg(st).arg(cfString(msg.ref));
}

// Info.plist keys, read from the file (not through CFBundle, which caches
// bundles by URL and would answer for a folder that was swapped meanwhile).
struct BundleKeys {
    QString identifier;
    QString version;
    bool ok = false;
};

BundleKeys readBundleKeys(const QString& bundle)
{
    BundleKeys k;
    const QString plist = bundle + QStringLiteral("/Contents/Info.plist");
    const QFileInfo fi(plist);
    if (!fi.isFile() || fi.size() > (1 << 20)) return k;  // a path is an input
    QFile f(plist);
    if (!f.open(QIODevice::ReadOnly)) return k;
    const QByteArray bytes = f.read((1 << 20) + 1);
    if (bytes.size() > (1 << 20)) return k;
    CfRef<CFDataRef> data(CFDataCreate(kCFAllocatorDefault,
                                       reinterpret_cast<const UInt8*>(bytes.constData()), bytes.size()));
    CfRef<CFPropertyListRef> pl(CFPropertyListCreateWithData(kCFAllocatorDefault, data.ref,
                                                             kCFPropertyListImmutable, nullptr, nullptr));
    if (!pl.ref || CFGetTypeID(pl.ref) != CFDictionaryGetTypeID()) return k;
    const auto dict = static_cast<CFDictionaryRef>(pl.ref);
    const auto str = [dict](CFStringRef key) {
        const void* v = CFDictionaryGetValue(dict, key);
        return (v && CFGetTypeID(v) == CFStringGetTypeID()) ? cfString(static_cast<CFStringRef>(v)) : QString();
    };
    k.identifier = str(kCFBundleIdentifierKey);
    k.version = str(kCFBundleVersionKey);
    k.ok = !k.identifier.isEmpty() && !k.version.isEmpty();
    return k;
}

// ---- signature ---------------------------------------------------------------

QString runningRequirement(const InstallContext& c)
{
    if (!c.tuning.runningRequirementForTests.isEmpty()) return c.tuning.runningRequirementForTests;
    SecCodeRef self = nullptr;
    if (SecCodeCopySelf(kSecCSDefaultFlags, &self) != errSecSuccess) return {};
    CfRef<SecCodeRef> selfRef(self);
    SecRequirementRef req = nullptr;
    if (SecCodeCopyDesignatedRequirement(reinterpret_cast<SecStaticCodeRef>(self), kSecCSDefaultFlags,
                                         &req) != errSecSuccess)
        return {};
    CfRef<SecRequirementRef> reqRef(req);
    CFStringRef txt = nullptr;
    if (SecRequirementCopyString(req, kSecCSDefaultFlags, &txt) != errSecSuccess) return {};
    CfRef<CFStringRef> txtRef(txt);
    return cfString(txt);
}

// Valid (strict, nested, all architectures) AND satisfies `requirement`.
// Measured on throwaway bundles: without the requirement, a foreign identity, ad hoc
// and a foreign identifier all pass.
bool satisfiesRequirement(const QString& bundle, const QString& requirement, QString* why)
{
    CfRef<CFStringRef> reqText(newCfString(requirement));
    SecRequirementRef req = nullptr;
    OSStatus st = SecRequirementCreateWithString(reqText.ref, kSecCSDefaultFlags, &req);
    if (st != errSecSuccess) {
        if (why) *why = QStringLiteral("requirement unreadable: %1").arg(osStatusText(st));
        return false;
    }
    CfRef<SecRequirementRef> reqRef(req);
    const QByteArray path = QFile::encodeName(bundle);
    const bool isDir = QFileInfo(bundle).isDir();
    CfRef<CFURLRef> url(CFURLCreateFromFileSystemRepresentation(
        kCFAllocatorDefault, reinterpret_cast<const UInt8*>(path.constData()), path.size(), isDir));
    SecStaticCodeRef code = nullptr;
    st = SecStaticCodeCreateWithPath(url.ref, kSecCSDefaultFlags, &code);
    if (st != errSecSuccess) {
        if (why) *why = QStringLiteral("not code: %1").arg(osStatusText(st));
        return false;
    }
    CfRef<SecStaticCodeRef> codeRef(code);
    const SecCSFlags flags = kSecCSCheckAllArchitectures | kSecCSStrictValidate | kSecCSCheckNestedCode;
    st = SecStaticCodeCheckValidity(code, flags, req);
    if (st != errSecSuccess) {
        if (why) *why = osStatusText(st);
        return false;
    }
    return true;
}

// ---- external tools ------------------------------------------------------------

struct ToolResult {
    bool ok = false;
    bool aborted = false;  // ended by us: deadline or cancel
    int exitCode = -1;
    QString output;
};

// `maxOutput`: what is kept of stdout+stderr. Messages are cut short for
// "Details"; a plist that is parsed needs its whole text (`hdiutil info`
// grows with every attached image — cut at 2000 characters, the disk-image
// check went blind as soon as a second image was attached; measured).
// `stdoutOnly`: stderr goes nowhere, the output is stdout alone - for a plist
// that is parsed (one stderr line in front of it, e.g. a deprecation note,
// and the parser sees no plist at all).
ToolResult runTool(const QString& program, const QStringList& args, int timeoutMs,
                   const std::atomic<bool>* cancel, int maxOutput = 2000, bool stdoutOnly = false)
{
    ToolResult r;
    QProcess p;
    p.setProgram(program);
    p.setArguments(args);
    p.setStandardInputFile(QProcess::nullDevice());
    if (stdoutOnly) {
        p.setProcessChannelMode(QProcess::SeparateChannels);
        p.setStandardErrorFile(QProcess::nullDevice());
    } else {
        p.setProcessChannelMode(QProcess::MergedChannels);
    }
    QProcessEnvironment env;
    env.insert(QStringLiteral("PATH"), QStringLiteral("/usr/bin:/bin:/usr/sbin:/sbin"));
    env.insert(QStringLiteral("LC_ALL"), QStringLiteral("C"));
    p.setProcessEnvironment(env);
    p.start();
    if (!p.waitForStarted(10000)) {
        r.output = QStringLiteral("cannot start %1").arg(program);
        return r;
    }
    QElapsedTimer t;
    t.start();
    while (p.state() != QProcess::NotRunning) {
        if (p.waitForFinished(100)) break;
        if ((cancel && cancel->load()) || t.elapsed() > timeoutMs) {
            p.kill();
            p.waitForFinished(2000);
            r.aborted = true;
            r.output = (cancel && cancel->load()) ? QStringLiteral("cancelled")
                                                  : QStringLiteral("%1 timed out").arg(program);
            return r;
        }
    }
    r.output = QString::fromLocal8Bit(stdoutOnly ? p.readAllStandardOutput() : p.readAll()).left(maxOutput);
    r.exitCode = p.exitCode();
    r.ok = p.exitStatus() == QProcess::NormalExit && p.exitCode() == 0;
    return r;
}

struct AttachedImage {
    QString imagePath;
    QString wholeDisk;    // shortest dev-entry, e.g. /dev/disk4
    QStringList mountPoints;
};

// All attached disk images (`hdiutil info -plist`). An image can be attached
// WITHOUT being mounted (measured: an attach that was ended half way), so the
// device is what detaching relies on, not the mount point.
QList<AttachedImage> attachedImages(bool* ok = nullptr)
{
    const ToolResult r = runTool(QStringLiteral("/usr/bin/hdiutil"),
                                 {QStringLiteral("info"), QStringLiteral("-plist")}, 15000, nullptr, 1 << 20,
                                 /*stdoutOnly=*/true);
    QList<AttachedImage> out;
    // "ok" only for a list that was really read: a run that failed, or an
    // output that is no plist with an "images" array, is NOT "nothing
    // attached" (until 0.8.0 a stderr line in front of the plist made every
    // detach and cleanup quietly do nothing).
    if (ok) *ok = false;
    if (!r.ok) return out;
    const QByteArray bytes = r.output.toUtf8();
    CfRef<CFDataRef> data(CFDataCreate(kCFAllocatorDefault,
                                       reinterpret_cast<const UInt8*>(bytes.constData()), bytes.size()));
    CfRef<CFPropertyListRef> pl(CFPropertyListCreateWithData(kCFAllocatorDefault, data.ref,
                                                             kCFPropertyListImmutable, nullptr, nullptr));
    if (!pl.ref || CFGetTypeID(pl.ref) != CFDictionaryGetTypeID()) return out;
    const auto images = static_cast<CFArrayRef>(
        CFDictionaryGetValue(static_cast<CFDictionaryRef>(pl.ref), CFSTR("images")));
    if (!images || CFGetTypeID(images) != CFArrayGetTypeID()) return out;
    if (ok) *ok = true;
    for (CFIndex i = 0; i < CFArrayGetCount(images); ++i) {
        const auto img = static_cast<CFDictionaryRef>(CFArrayGetValueAtIndex(images, i));
        if (!img || CFGetTypeID(img) != CFDictionaryGetTypeID()) continue;
        AttachedImage a;
        const void* ip = CFDictionaryGetValue(img, CFSTR("image-path"));
        if (ip && CFGetTypeID(ip) == CFStringGetTypeID())
            a.imagePath = QDir::cleanPath(cfString(static_cast<CFStringRef>(ip)));
        const auto ents = static_cast<CFArrayRef>(CFDictionaryGetValue(img, CFSTR("system-entities")));
        if (ents && CFGetTypeID(ents) == CFArrayGetTypeID()) {
            for (CFIndex j = 0; j < CFArrayGetCount(ents); ++j) {
                const auto e = static_cast<CFDictionaryRef>(CFArrayGetValueAtIndex(ents, j));
                if (!e || CFGetTypeID(e) != CFDictionaryGetTypeID()) continue;
                const void* mp = CFDictionaryGetValue(e, CFSTR("mount-point"));
                if (mp && CFGetTypeID(mp) == CFStringGetTypeID())
                    a.mountPoints << QDir::cleanPath(cfString(static_cast<CFStringRef>(mp)));
                const void* dev = CFDictionaryGetValue(e, CFSTR("dev-entry"));
                if (dev && CFGetTypeID(dev) == CFStringGetTypeID()) {
                    const QString d = cfString(static_cast<CFStringRef>(dev));
                    if (a.wholeDisk.isEmpty() || d.size() < a.wholeDisk.size()) a.wholeDisk = d;
                }
            }
        }
        out << a;
    }
    return out;
}

QStringList diskImageMountPoints()
{
    QStringList out;
    for (const AttachedImage& a : attachedImages()) out << a.mountPoints;
    return out;
}

// Detaches every attachment of the image FILE `image` (by device, forced).
// For our private copy only — it is never in use by anyone else.
void detachImageFile(const QString& image)
{
    const QString want = QDir::cleanPath(image);
    for (const AttachedImage& a : attachedImages()) {
        if (a.imagePath == want && a.wholeDisk.startsWith(QLatin1String("/dev/disk")))
            runTool(QStringLiteral("/usr/bin/hdiutil"),
                    {QStringLiteral("detach"), a.wholeDisk, QStringLiteral("-force"), QStringLiteral("-quiet")},
                    60000, nullptr);
    }
}

bool otherProcessRunsFrom(const QString& bundle)
{
    const QByteArray prefix = QFile::encodeName(QDir::cleanPath(bundle) + QStringLiteral("/Contents/MacOS/"));
    // A failed measurement is no "no": unknown counts as another instance.
    const int n = proc_listpids(PROC_ALL_PIDS, 0, nullptr, 0);
    if (n <= 0) return true;
    std::vector<pid_t> pids(size_t(n) / sizeof(pid_t) + 16);
    const int bytes = proc_listpids(PROC_ALL_PIDS, 0, pids.data(), int(pids.size() * sizeof(pid_t)));
    if (bytes <= 0) return true;
    const pid_t me = ::getpid();
    for (int i = 0; i < bytes / int(sizeof(pid_t)); ++i) {
        const pid_t pid = pids[size_t(i)];
        if (pid <= 0 || pid == me) continue;
        char path[PROC_PIDPATHINFO_MAXSIZE];
        if (proc_pidpath(pid, path, sizeof path) <= 0) continue;
        if (std::strncmp(path, prefix.constData(), size_t(prefix.size())) == 0) return true;
    }
    return false;
}

// The bundle as one resolved path: what is measured is what is swapped and
// what the helper is told (a link to the bundle is resolved once, here).
QString resolvedBundle(const QString& appLocation)
{
    if (appLocation.isEmpty()) return {};
    char* real = ::realpath(QFile::encodeName(appLocation).constData(), nullptr);
    if (!real) return QDir::cleanPath(appLocation);
    const QString r = QDir::cleanPath(QFile::decodeName(QByteArray(real)));
    std::free(real);
    return r;
}

InstallContext resolvedContext(const InstallContext& c)
{
    InstallContext r = c;
    r.appLocation = resolvedBundle(c.appLocation);
    return r;
}

// Contents/Helpers/update-swap is a plain file, and no step on the way is a link.
bool swapToolPresent(const QString& bundle)
{
    for (const QString& p : {bundle + QStringLiteral("/Contents"), bundle + QStringLiteral("/Contents/Helpers")}) {
        const QFileInfo fi(p);
        if (fi.isSymLink() || !fi.isDir()) return false;
    }
    const QFileInfo tool(macSwapPaths(bundle).swapTool);
    return !tool.isSymLink() && tool.isFile();
}

SelfInstallFacts measureFacts(const InstallContext& c)
{
    SelfInstallFacts f;
    const QString bundle = QDir::cleanPath(c.appLocation);
    const QFileInfo fi(bundle);
    f.locationExists = !bundle.isEmpty() && fi.isDir() && bundle.endsWith(QLatin1String(".app"));
    f.runningRequirement = runningRequirement(c);
    f.helperAvailable = ::access("/bin/sh", X_OK) == 0 && ::access("/usr/bin/codesign", X_OK) == 0 &&
                        ::access("/usr/bin/ditto", X_OK) == 0 && ::access("/usr/bin/hdiutil", X_OK) == 0;
    f.quarantine = quarantineState(c.verifiedFile);
    if (!f.locationExists) return f;
    const QByteArray path = QFile::encodeName(bundle);
    struct statfs sfs {};
    if (::statfs(path.constData(), &sfs) == 0) {
        f.onReadOnlyVolume = (sfs.f_flags & MNT_RDONLY) != 0;
        f.onNetworkVolume = (sfs.f_flags & MNT_LOCAL) == 0;
        const bool nullfs = std::strcmp(sfs.f_fstypename, "nullfs") == 0;
        f.translocated = nullfs || bundle.contains(QLatin1String("/AppTranslocation/"));
        if (f.onReadOnlyVolume && !f.translocated) {
            const QString mnt = QDir::cleanPath(QFile::decodeName(sfs.f_mntonname));
            f.onDiskImage = diskImageMountPoints().contains(mnt);
        }
    } else {
        f.onReadOnlyVolume = true;  // an unanswerable question is not a yes
    }
    f.translocated = f.translocated || bundle.contains(QLatin1String("/AppTranslocation/"));
    f.locationWritable = ::access(QFile::encodeName(fi.absolutePath()).constData(), W_OK) == 0;
    f.locationShared = posixhelper::parentIsShared(bundle, 80, &f.locationSharedRule);  // gid 80 = admin
    f.otherInstanceRunning = otherProcessRunsFrom(bundle);
    f.swapToolPresent = swapToolPresent(bundle);
    return f;
}

QString randomSuffix()
{
    return QString::number(QRandomGenerator::system()->generate64(), 16);
}

struct Staging {
    QString image;       // the private copy that gets attached
    QString privateDir;  // our own folder with the private copy of the image
    QString mountPoint;  // our own empty folder, removed after detach
    QString staged;      // the copied bundle next to the target
    QString copyDir;     // our 0700 folder next to the target that ditto writes into
    QString swapDir;     // our 0700 folder with the checked copy of update-swap
    bool attachEnded = false;  // `hdiutil attach` was ended by us (deadline, cancel)
};

void detachAndRemove(const QString& mountPoint)
{
    if (mountPoint.isEmpty()) return;
    ToolResult r = runTool(QStringLiteral("/usr/bin/hdiutil"),
                           {QStringLiteral("detach"), mountPoint, QStringLiteral("-quiet")}, 60000, nullptr);
    if (!r.ok)
        runTool(QStringLiteral("/usr/bin/hdiutil"),
                {QStringLiteral("detach"), mountPoint, QStringLiteral("-force"), QStringLiteral("-quiet")},
                60000, nullptr);
    QDir().rmdir(mountPoint);  // only removes an EMPTY folder — never the volume
}

// Exactly `<dir>/.<Name>.app.update-staged-<hex>` and not a link: the only
// thing discard() and launch() accept. A string check plus lstat — the
// removal itself never follows a link either (removeTreeNoFollow).
bool isOurStaging(const QString& staged, const QString& target, const InstallTuning& tuning)
{
    if (QDir::cleanPath(staged) != staged || !QDir::isAbsolutePath(staged)) return false;
    const QString name = QFileInfo(target).fileName();
    const QString dir = tuning.stagingDirForTests.isEmpty() ? QFileInfo(target).absolutePath()
                                                            : QDir::cleanPath(tuning.stagingDirForTests);
    const QString prefix = dir + QStringLiteral("/.") + name + QStringLiteral(".update-staged-");
    static const QRegularExpression hex(QStringLiteral("^[0-9a-f]{1,16}$"));
    if (!staged.startsWith(prefix) || !hex.match(staged.mid(prefix.size())).hasMatch()) return false;
    return !QFileInfo(staged).isSymLink();
}

void removeStaging(const QString& staged, const QString& target, const InstallTuning& tuning)
{
    if (isOurStaging(staged, target, tuning)) posixhelper::removeTreeNoFollow(staged);
}

// The swap program of the RUNNING bundle, copied through a chain of
// descriptors (no step may be a link) into a fresh 0700 folder of the user's
// temp directory, and the COPY checked: valid, and the identity of the running
// app with the identifier "<identifier>.update-swap". Only this copy ever runs.
bool copySwapTool(const QString& bundle, const QString& slug, const QString& appRequirement,
                  QString* dirOut, QString* toolOut, QString* why)
{
    const QString requirement = swapToolRequirement(appRequirement);
    if (requirement.isEmpty()) {
        *why = QStringLiteral("no requirement for the swap program follows from '%1'").arg(appRequirement);
        return false;
    }
    int fd = ::open(QFile::encodeName(bundle).constData(), O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
    for (const char* step : {"Contents", "Helpers"}) {
        if (fd < 0) break;
        const int next = ::openat(fd, step, O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
        ::close(fd);
        fd = next;
    }
    const int in = fd < 0 ? -1 : ::openat(fd, "update-swap", O_RDONLY | O_NOFOLLOW | O_NONBLOCK | O_CLOEXEC);
    if (fd >= 0) ::close(fd);
    struct stat st {};
    if (in < 0 || ::fstat(in, &st) != 0 || !S_ISREG(st.st_mode) || st.st_size <= 0 ||
        st.st_size > 16 * 1024 * 1024) {
        if (in >= 0) ::close(in);
        *why = QStringLiteral("the swap program of %1 is missing or not a plain file").arg(bundle);
        return false;
    }
    const int flags = ::fcntl(in, F_GETFL);
    if (flags >= 0) ::fcntl(in, F_SETFL, flags & ~O_NONBLOCK);
    const QString temp = posixhelper::userTempDir();
    QByteArray tmpl = QFile::encodeName(temp + QLatin1Char('/') + helperNamePart(slug) +
                                        QStringLiteral("-update-swap-XXXXXX"));
    if (temp.isEmpty() || !::mkdtemp(tmpl.data())) {  // 0700, new
        ::close(in);
        *why = QStringLiteral("cannot create a private folder for the swap program");
        return false;
    }
    *dirOut = QFile::decodeName(tmpl);
    const QString tool = *dirOut + QStringLiteral("/update-swap");
    QString err;
    const QString hash = posixhelper::copyAndHashFd(in, tool, 0700, st.st_size, nullptr, &err);
    ::close(in);
    if (hash.isEmpty()) {
        *why = QStringLiteral("cannot copy the swap program: %1").arg(err);
        return false;
    }
    if (!satisfiesRequirement(tool, requirement, &err)) {
        *why = QStringLiteral("the swap program fails its check against the running identity: %1").arg(err);
        return false;
    }
    *toolOut = tool;
    return true;
}

// <name>-update-swap-XXXXXX/update-swap in the user's temp folder, not a link.
bool isOurSwapTool(const QString& tool, const QString& slug)
{
    const QString temp = posixhelper::userTempDir();
    if (temp.isEmpty() || QDir::cleanPath(tool) != tool) return false;
    const QRegularExpression re(QStringLiteral("^") + QRegularExpression::escape(temp + QLatin1Char('/') +
                                                                                helperNamePart(slug)) +
                                QStringLiteral("-update-swap-[A-Za-z0-9]{6}/update-swap$"));
    const QFileInfo fi(tool);
    return re.match(tool).hasMatch() && !fi.isSymLink() && fi.isFile() &&
           !QFileInfo(fi.absolutePath()).isSymLink();
}

void removeSwapDir(const QString& tool, const QString& slug)
{
    if (isOurSwapTool(tool, slug)) posixhelper::removeTreeNoFollow(QFileInfo(tool).absolutePath());
}

// The whole of prepare(), off the GUI thread.
PrepareResult prepareWork(const InstallOrder& order, const SelfInstallFacts& facts,
                          const std::atomic<bool>& cancel, Staging* staging)
{
    PrepareResult res;
    const InstallCapability cap = selfInstallDecision(SelfInstallPlatform::MacOS, order.context, facts);
    if (cap.method != InstallMethod::Automatic) {
        res.manual = cap.reason;
        res.error = cap.detail;
        return res;
    }
    const QString target = QDir::cleanPath(order.context.appLocation);
    const QString bundleName = QFileInfo(target).fileName();
    const BundleKeys running = readBundleKeys(target);
    if (!running.ok) {
        res.error = QStringLiteral("cannot read the Info.plist of %1").arg(target);
        return res;
    }

    // The quarantine mark was measured on context.verifiedFile; it must be
    // the very file that is hashed and mounted.
    if (QDir::cleanPath(order.context.verifiedFile) != QDir::cleanPath(order.verifiedFile)) {
        res.error = QStringLiteral("the order names two different files");
        return res;
    }

    // 1. A private copy, hashed while it is written, against the signed manifest.
    const QString temp = posixhelper::userTempDir();
    if (temp.isEmpty()) {
        res.error = QStringLiteral("the user's temp folder is unknown");
        return res;
    }
    QTemporaryDir priv(temp + QLatin1Char('/') + helperNamePart(order.productSlug) +
                       QStringLiteral("-update-dmg-XXXXXX"));
    priv.setAutoRemove(false);
    if (!priv.isValid()) {
        res.error = QStringLiteral("cannot create a private folder");
        return res;
    }
    staging->privateDir = QDir::cleanPath(priv.path());
    const QString image = staging->privateDir + QStringLiteral("/update.dmg");
    staging->image = image;
    // Through ONE descriptor: no FIFO, no link, the size checked at the
    // descriptor; the quarantine mark is measured on that very descriptor
    // right before the copy (a mark that appears later would not reach the
    // copy, so it is asked here and not only in capability()).
    QString err;
    const int in = posixhelper::openRegularNoFollow(order.verifiedFile, order.expectedSize, &err);
    if (in < 0) {
        res.error = err;
        return res;
    }
    if (::fgetxattr(in, "com.apple.quarantine", nullptr, 0, 0, 0) >= 0 || errno != ENOATTR) {
        ::close(in);
        res.manual = ManualReason::Quarantined;
        res.error = QStringLiteral("the downloaded file carries (or may carry) the quarantine mark");
        return res;
    }
    // Against a process that may only write to the download folder (a
    // sandboxed app) the private copy closes the window between hash and
    // mount. Against any process of the SAME user it does not ($TMPDIR is
    // theirs too): there the signature check of the copied bundle against
    // the running app's requirement is the only lock.
    const QString got = posixhelper::copyAndHashFd(in, image, 0600, order.expectedSize, &cancel, &err);
    ::close(in);
    if (got.isEmpty() || got != order.expectedSha256.toLower()) {
        res.error = err.isEmpty() ? QStringLiteral("the downloaded file no longer matches the manifest hash") : err;
        return res;
    }

    // 2. Mount read-only, without a Finder window, at our own folder.
    QTemporaryDir mp(temp + QLatin1Char('/') + helperNamePart(order.productSlug) +
                     QStringLiteral("-update-mnt-XXXXXX"));
    mp.setAutoRemove(false);
    if (!mp.isValid()) {
        res.error = QStringLiteral("cannot create a mount point");
        return res;
    }
    staging->mountPoint = QDir::cleanPath(mp.path());
    const ToolResult attach = runTool(
        QStringLiteral("/usr/bin/hdiutil"),
        {QStringLiteral("attach"), QStringLiteral("-readonly"), QStringLiteral("-nobrowse"),
         QStringLiteral("-noautoopen"), QStringLiteral("-mountpoint"), staging->mountPoint, image},
        order.context.tuning.diskImageAttachTimeoutMs, &cancel);
    if (!attach.ok) {
        detachImageFile(image);  // an attach ended half way can leave the device attached
        QDir().rmdir(staging->mountPoint);
        staging->mountPoint.clear();
        res.error = QStringLiteral("cannot mount the disk image: %1").arg(attach.output.simplified());
        if (attach.aborted) {
            // Ending hdiutil ends only the client; the disk image service may
            // still finish attaching. The worker detaches once more later.
            staging->attachEnded = true;
            res.error += QStringLiteral(" (attach ended; the private copy is detached again in %1 s and "
                                        "on the next start)")
                             .arg(order.context.tuning.diskImageReDetachDelayMs / 1000);
            qCWarning(lcSelfInstallMac).noquote()
                << "hdiutil attach was ended (" << attach.output << ") for" << image
                << "- detached now, again in" << order.context.tuning.diskImageReDetachDelayMs << "ms";
        }
        return res;
    }

    // 3. The root: exactly one bundle with our name, plus the Applications link.
    QList<DiskImageEntry> entries;
    const QFileInfoList list = QDir(staging->mountPoint)
                                   .entryInfoList(QDir::AllEntries | QDir::Hidden | QDir::System |
                                                  QDir::NoDotAndDotDot);
    for (const QFileInfo& e : list) {
        DiskImageEntry d;
        d.name = e.fileName();
        d.isSymlink = e.isSymLink();
        d.isDir = !d.isSymlink && e.isDir();
        if (d.isSymlink) {
            char buf[PATH_MAX + 1];
            const ssize_t n = ::readlink(QFile::encodeName(e.filePath()).constData(), buf, PATH_MAX);
            if (n > 0) d.symlinkTarget = QFile::decodeName(QByteArray(buf, int(n)));
        }
        entries << d;
    }
    QString why;
    if (!validateDiskImageRoot(entries, bundleName, &why)) {
        res.error = why;
        return res;
    }

    // 4. Copy as a sibling of the target (same folder → same volume, renames only).
    const QString stagingDir = order.context.tuning.stagingDirForTests.isEmpty()
                                   ? QFileInfo(target).absolutePath()
                                   : QDir::cleanPath(order.context.tuning.stagingDirForTests);
    const QString staged = stagingDir + QStringLiteral("/.") + bundleName +
                           QStringLiteral(".update-staged-") + randomSuffix();
    if (QFileInfo::exists(staged)) {
        res.error = QStringLiteral("staging name collision");
        return res;
    }
    // ditto writes by path. It writes into a fresh folder of our own (0700,
    // mkdtemp) on the same volume, never straight under the staging name in
    // the app's folder: nobody else can set a link into the copy while it
    // is written. Only the finished copy is renamed to the staging name.
    QTemporaryDir copyDir(stagingDir + QStringLiteral("/.") + bundleName + QStringLiteral(".update-copy-XXXXXX"));
    copyDir.setAutoRemove(false);
    if (!copyDir.isValid()) {
        res.error = QStringLiteral("cannot create a private folder next to the app");
        return res;
    }
    staging->copyDir = QDir::cleanPath(copyDir.path());
    const QString copied = staging->copyDir + QLatin1Char('/') + bundleName;
    const ToolResult copy = runTool(QStringLiteral("/usr/bin/ditto"),
                                    {staging->mountPoint + QLatin1Char('/') + bundleName, copied},
                                    300000, &cancel);
    detachAndRemove(staging->mountPoint);
    detachImageFile(image);  // and nothing of it stays attached without a mount point
    staging->mountPoint.clear();
    if (!copy.ok) {
        res.error = QStringLiteral("cannot copy the new bundle: %1").arg(copy.output.simplified());
        return res;
    }
    if (::rename(QFile::encodeName(copied).constData(), QFile::encodeName(staged).constData()) != 0) {
        res.error = QStringLiteral("cannot move the copy to its staging name: %1").arg(QString::fromLocal8Bit(std::strerror(errno)));
        return res;
    }
    staging->staged = staged;
    QDir().rmdir(staging->copyDir);  // empty now
    staging->copyDir.clear();

    // 5. Judge the COPY (the disk image may have changed since it was read).
    const QString escaping = firstEscapingSymlink(staged);
    if (!escaping.isEmpty()) {
        res.error = QStringLiteral("a link inside the new bundle points outside of it (%1)")
                        .arg(escaping.mid(staged.size()));
        return res;
    }
    if (!satisfiesRequirement(staged, facts.runningRequirement, &why)) {
        res.error = QStringLiteral("the new bundle fails its signature check: %1").arg(why);
        return res;
    }
    const BundleKeys fresh = readBundleKeys(staged);
    if (!fresh.ok || fresh.identifier != running.identifier) {
        res.error = QStringLiteral("the new bundle has identifier '%1', expected '%2'")
                        .arg(fresh.identifier, running.identifier);
        return res;
    }
    if (fresh.version != order.toVersion) {
        res.error = QStringLiteral("the new bundle has version '%1', the manifest says '%2'")
                        .arg(fresh.version, order.toVersion);
        return res;
    }
    const QuarantineState q = quarantineState(staged);
    if (q != QuarantineState::Absent) {
        res.manual = ManualReason::Quarantined;
        res.error = QStringLiteral("the new bundle's quarantine mark is %1").arg(quarantineStateName(q));
        return res;
    }

    // 6. The swap program: a checked private copy, or Manual (no second way).
    QString tool;
    if (!copySwapTool(target, order.productSlug, facts.runningRequirement, &staging->swapDir, &tool, &why)) {
        res.manual = ManualReason::HelperUnavailable;
        res.error = why;
        return res;
    }

    PreparedInstall p;
    p.order = order;
    p.method = InstallMethod::Automatic;
    p.stagedFile = staged;
    p.backendState = facts.runningRequirement.toUtf8() + '\n' + tool.toUtf8();
    p.description = macHelperPlan(p, facts.runningRequirement, tool).description;
    res.prepared = p;
    staging->staged.clear();  // handed over: from now on discard() owns it
    staging->swapDir.clear();
    return res;
}

// Older than this, a temp leftover of ours (the private copy of an image, its
// mount point, the private copy of the swap program) belongs to no running
// attempt: prepare() ends within kPrepareDeadlineMs and removes its image
// itself; a helper needs its swap program at most helperWaitExitMs +
// startConfirmTimeoutMs (3 min by default) and removes it itself. A younger
// one may belong to another copy of the app that prepares right now - same
// product, same temp folder.
constexpr qint64 kTempLeftoverAgeSecs = 10 * 60;

// What prepare() and cleanupAfterStart() share across their worker threads.
// Owned through a shared_ptr by the backend AND every worker, so a worker
// never touches a destroyed backend.
struct MacBackendShared {
    // The work of a prepare() and a cleanup never run at the same time.
    std::timed_mutex work;
    // Staged bundles and swap folders of installs this backend has prepared
    // and not discarded: a cleanup leaves them alone.
    std::mutex heldMutex;
    QSet<QString> held;
    std::atomic<int> prepareCalls{0};
    // Set when the backend goes away: a running cleanup stops between two
    // items (a leftover it leaves is removed at the next start).
    std::atomic<bool> stopCleanup{false};

    static QStringList pathsOf(const PreparedInstall& p)
    {
        QStringList out{QDir::cleanPath(p.stagedFile)};
        const QList<QByteArray> state = p.backendState.split('\n');
        if (state.size() == 2) out << QDir::cleanPath(QFileInfo(QString::fromUtf8(state[1])).absolutePath());
        return out;
    }
    void hold(const PreparedInstall& p)
    {
        const std::lock_guard<std::mutex> l(heldMutex);
        for (const QString& s : pathsOf(p)) held.insert(s);
    }
    void release(const PreparedInstall& p)
    {
        const std::lock_guard<std::mutex> l(heldMutex);
        for (const QString& s : pathsOf(p)) held.remove(s);
    }
    bool isHeld(const QString& path)
    {
        const std::lock_guard<std::mutex> l(heldMutex);
        return held.contains(QDir::cleanPath(path));
    }
};

// Undo one prepared install: the staged bundle and the private swap program.
// Removed first, released second: a cleanup never sees them free while they
// are still being removed here.
void discardPrepared(const PreparedInstall& p, MacBackendShared* shared = nullptr)
{
    removeStaging(p.stagedFile, p.order.context.appLocation, p.order.context.tuning);
    const QList<QByteArray> state = p.backendState.split('\n');
    if (state.size() == 2) removeSwapDir(QString::fromUtf8(state[1]), p.order.productSlug);
    if (shared) shared->release(p);
}

// Which start asks: after a judged install (a pending marker), or any other
// start (cleanupLeftovers). The second may run while ANOTHER instance of the
// same app holds a prepared install - "held" knows only this process. So it
// leaves the two things such an install owns until it launches: the staged
// bundle next to the target and the private copy of the swap program (a Ready
// install may wait for hours). Images, mount points and copy folders live only
// during a prepare (deadline kPrepareDeadlineMs, far below the 10-min age), so
// both scopes clean them.
enum class CleanupScope { AfterInstall, Leftovers };

// The whole of cleanupAfterStart()/cleanupLeftovers(), on a worker thread
// (hdiutil and codesign may take seconds each; the GUI thread must not wait).
void cleanupWork(const InstallContext& requested, MacBackendShared& shared, CleanupScope scope)
{
    if (requested.appLocation.isEmpty()) return;
    const InstallContext context = resolvedContext(requested);
    const MacSwapPaths p = macSwapPaths(context.appLocation);
    const QString slug = helperNamePart(context.productSlug);
    const QString temp = posixhelper::userTempDir();
    const QDateTime limit = QDateTime::currentDateTimeUtc().addSecs(-kTempLeftoverAgeSecs);
    const auto isOld = [&limit](const QFileInfo& fi) { return fi.lastModified(QTimeZone::UTC) <= limit; };
    // Our own temp leftovers of an attempt that ended half way (a crash
    // during prepare), and only OLD ones: a young one may be another copy's
    // running prepare.
    if (!temp.isEmpty()) {
        const QString dmgPrefix = slug + QStringLiteral("-update-dmg-");
        // A private copy that is still attached although its folder is gone:
        // an attach that we ended and that the disk image service finished
        // afterwards (after the worker had removed the folder). No folder,
        // no prepare that still runs - detach by device. Found nowhere else:
        // the loops below start from folders.
        {
            bool listOk = false;
            const QList<AttachedImage> all = attachedImages(&listOk);
            if (!listOk)
                qCWarning(lcSelfInstallMac) << "cleanup: the list of attached disk images could not be read";
            for (const AttachedImage& a : all) {
                if (shared.stopCleanup.load()) return;
                const QFileInfo image(a.imagePath);
                const QFileInfo folder(image.absolutePath());
                const bool ours = image.fileName() == QLatin1String("update.dmg") &&
                                  folder.fileName().startsWith(dmgPrefix) &&
                                  QDir::cleanPath(folder.absolutePath()) == temp;
                if (!ours || folder.exists() || folder.isSymLink() ||
                    !a.wholeDisk.startsWith(QLatin1String("/dev/disk")))
                    continue;
                qCWarning(lcSelfInstallMac).noquote()
                    << "cleanup: detaching an orphaned private copy" << a.imagePath << "at" << a.wholeDisk;
                runTool(QStringLiteral("/usr/bin/hdiutil"),
                        {QStringLiteral("detach"), a.wholeDisk, QStringLiteral("-force"), QStringLiteral("-quiet")},
                        60000, nullptr);
            }
        }
        // The private copy of an image: detach it first (it may still be
        // attached), then the folder.
        for (const QFileInfo& fi : QDir(temp).entryInfoList({dmgPrefix + QLatin1Char('*')},
                                                            QDir::Dirs | QDir::NoDotAndDotDot)) {
            if (shared.stopCleanup.load()) return;
            if (fi.isSymLink() || !isOld(fi)) continue;
            const QFileInfo image(fi.absoluteFilePath() + QStringLiteral("/update.dmg"));
            if (image.exists() && !isOld(image)) continue;  // still being written
            detachImageFile(image.absoluteFilePath());
            posixhelper::removeTreeNoFollow(QDir::cleanPath(fi.absoluteFilePath()));
        }
        // Mount points: what is mounted there decides. Only an old private
        // copy of OURS is detached; a folder with nothing mounted goes by its
        // own age, and only by rmdir - never a recursive delete of something
        // that may still be a mount point.
        const QFileInfoList mountPoints = QDir(temp).entryInfoList({slug + QStringLiteral("-update-mnt-*")},
                                                                   QDir::Dirs | QDir::NoDotAndDotDot);
        const QList<AttachedImage> attached = mountPoints.isEmpty() ? QList<AttachedImage>() : attachedImages();
        for (const QFileInfo& fi : mountPoints) {
            if (shared.stopCleanup.load()) return;
            if (fi.isSymLink()) continue;
            const QString mp = QDir::cleanPath(fi.absoluteFilePath());
            const AttachedImage* at = nullptr;
            for (const AttachedImage& a : attached)
                if (a.mountPoints.contains(mp)) at = &a;
            if (at) {
                const QFileInfo image(at->imagePath);
                const QFileInfo folder(image.absolutePath());
                const bool ours = image.fileName() == QLatin1String("update.dmg") &&
                                  folder.fileName().startsWith(dmgPrefix) &&
                                  QDir::cleanPath(folder.absolutePath()) == temp;
                if (!ours || !isOld(image)) continue;
                runTool(QStringLiteral("/usr/bin/hdiutil"),
                        {QStringLiteral("detach"), mp, QStringLiteral("-force"), QStringLiteral("-quiet")}, 60000,
                        nullptr);
            } else if (!isOld(fi)) {
                continue;
            }
            QDir().rmdir(mp);
        }
        // Private copies of the swap program: only old ones and none this
        // backend still holds. A helper that is still waiting for the
        // confirmation may need its copy for the way back; it removes it
        // itself when it ends. Not on a start without a marker: another
        // instance's prepared install may own it (scope above).
        if (scope == CleanupScope::AfterInstall)
        for (const QFileInfo& fi : QDir(temp).entryInfoList({slug + QStringLiteral("-update-swap-*")},
                                                            QDir::Dirs | QDir::NoDotAndDotDot)) {
            const QString dir = QDir::cleanPath(fi.absoluteFilePath());
            if (fi.isSymLink() || !isOld(fi) || shared.isHeld(dir)) continue;
            posixhelper::removeTreeNoFollow(dir);
        }
    }
    // Copy folders next to the target that a prepare() left half way (a
    // crash during ditto): only old ones, never a link.
    {
        const QString copyDirs = context.tuning.stagingDirForTests.isEmpty()
                                     ? QFileInfo(p.target).absolutePath()
                                     : QDir::cleanPath(context.tuning.stagingDirForTests);
        for (const QFileInfo& fi :
             QDir(copyDirs).entryInfoList({QStringLiteral(".") + QFileInfo(p.target).fileName() +
                                           QStringLiteral(".update-copy-*")},
                                          QDir::Dirs | QDir::Hidden | QDir::NoDotAndDotDot)) {
            if (shared.stopCleanup.load()) return;
            if (fi.isSymLink() || !isOld(fi)) continue;
            posixhelper::removeTreeNoFollow(QDir::cleanPath(fi.absoluteFilePath()));
        }
    }
    // Not on a start without a marker: a staged bundle may be another
    // instance's prepared install (scope above).
    if (scope == CleanupScope::Leftovers) return;
    // After a swap the OLD bundle sits under a staging name until the new
    // one confirmed; staging names go only when what stands at the target
    // passes the check (signature + the running requirement) - a helper
    // that died in between leaves the way back intact. Asked once. A staged
    // bundle of an install this backend has prepared stays. The (expensive)
    // check of the target runs only when there is something to remove.
    const QString dir = context.tuning.stagingDirForTests.isEmpty()
                            ? QFileInfo(p.target).absolutePath()
                            : QDir::cleanPath(context.tuning.stagingDirForTests);
    const QString pattern = QStringLiteral(".") + QFileInfo(p.target).fileName() + QStringLiteral(".update-staged-*");
    QStringList candidates;
    for (const QFileInfo& fi : QDir(dir).entryInfoList({pattern}, QDir::Dirs | QDir::Hidden | QDir::NoDotAndDotDot)) {
        const QString staged = QDir::cleanPath(fi.absoluteFilePath());
        if (!shared.isHeld(staged)) candidates << staged;
    }
    if (candidates.isEmpty() || shared.stopCleanup.load()) return;
    const QString dr = runningRequirement(context);
    const QFileInfo target(p.target);
    const bool targetGenuine = target.isDir() && !target.isSymLink() && isCertificateAnchoredRequirement(dr) &&
                               satisfiesRequirement(p.target, dr, nullptr);
    if (!targetGenuine) return;
    // Only a bundle of THIS app goes under its staging name: same bundle
    // identifier as the target. A folder planted under the name stays.
    const QString id = readBundleKeys(p.target).identifier;
    if (id.isEmpty()) return;
    for (const QString& staged : candidates)
        if (!shared.isHeld(staged) && readBundleKeys(staged).identifier == id)
            removeStaging(staged, p.target, context.tuning);
}

class MacBundleSwapBackend final : public InstallBackend {
public:
    explicit MacBundleSwapBackend(SelfInstallHooks hooks) : hooks_(std::move(hooks)) {}

    // A quit during prepare() must not leave a disk image mounted: wait for
    // every worker (prepare and cleanup), which detaches and removes what it
    // owns.
    ~MacBundleSwapBackend() override
    {
        cancelPrepare();
        shared_->stopCleanup.store(true);
        for (const QPointer<QThread>& w : workers_)
            if (w && w->isRunning()) w->wait(120000);
    }

    InstallCapability capability(const InstallContext& context) const override
    {
        const InstallContext c = resolvedContext(context);
        return selfInstallDecision(SelfInstallPlatform::MacOS, c, facts(c));
    }

    void prepare(const InstallOrder& requested, PrepareDone done) override
    {
        cancelPrepare();
        InstallOrder order = requested;  // one resolved path for measuring, staging, helper and swap
        order.context = resolvedContext(requested.context);
        const int gen = ++generation_;
        auto cancel = std::make_shared<std::atomic<bool>>(false);
        cancel_ = cancel;
        const SelfInstallFacts f = facts(order.context);  // on the owner thread
        QPointer<QObject> ctx(&ctx_);
        auto shared = shared_;
        auto started = hooks_.prepareWorkStartedForTests;  // a copy: the worker never reads the backend
        QThread* worker = QThread::create([order, f, cancel, ctx, gen, this, done, shared, started]() {
            // One piece of disk work at a time (an earlier prepare that is
            // still winding down, a cleanup). A prepare cancelled while it
            // waits gives up without queueing.
            std::unique_lock<std::timed_mutex> lock(shared->work, std::defer_lock);
            while (!lock.try_lock_for(std::chrono::milliseconds(50)))
                if (cancel->load()) return;
            if (started) started(++shared->prepareCalls);
            Staging staging;
            PrepareResult res = prepareWork(order, f, *cancel, &staging);
            // Whatever is still owned here failed or was cancelled: remove it.
            if (!staging.mountPoint.isEmpty()) detachAndRemove(staging.mountPoint);
            if (!staging.image.isEmpty()) detachImageFile(staging.image);
            if (!staging.privateDir.isEmpty()) posixhelper::removeTreeNoFollow(staging.privateDir);
            if (!staging.staged.isEmpty())
                removeStaging(staging.staged, order.context.appLocation, order.context.tuning);
            if (!staging.swapDir.isEmpty()) posixhelper::removeTreeNoFollow(staging.swapDir);
            if (!staging.copyDir.isEmpty()) posixhelper::removeTreeNoFollow(staging.copyDir);
            if (cancel->load() && res.prepared) {
                discardPrepared(*res.prepared);
                res.prepared.reset();
            }
            // Held from here, while the lock is still ours: a cleanup that
            // starts next never sees the prepared install as a leftover.
            if (res.prepared) shared->hold(*res.prepared);
            lock.unlock();
            // Delivered on the owner (GUI) thread; whether the backend still
            // exists is asked THERE, where it would be destroyed.
            QMetaObject::invokeMethod(QCoreApplication::instance(), [this, ctx, gen, cancel, res, done, shared]() {
                if (!ctx || cancel->load() || gen != generation_.load()) {
                    if (res.prepared) discardPrepared(*res.prepared, shared.get());
                    return;
                }
                done(res);
            }, Qt::QueuedConnection);
            // An attach that we ended may still be finished by the disk image
            // service after the detach above (the private copy is gone by
            // now; hdiutil still names it by its path). Once more, later -
            // at once when the backend goes away, never not at all.
            if (staging.attachEnded && !staging.image.isEmpty()) {
                QElapsedTimer waited;
                waited.start();
                while (waited.elapsed() < order.context.tuning.diskImageReDetachDelayMs && !shared->stopCleanup.load())
                    QThread::msleep(50);
                bool listed = false;
                const QString want = QDir::cleanPath(staging.image);
                for (const AttachedImage& a : attachedImages())
                    listed = listed || a.imagePath == want;
                detachImageFile(staging.image);
                qCWarning(lcSelfInstallMac).noquote()
                    << "detached again after the ended attach:" << staging.image
                    << (listed ? "(it WAS attached again)" : "(nothing attached)");
            }
        });
        QObject::connect(worker, &QThread::finished, worker, &QObject::deleteLater);
        addWorker(worker);
        worker->start();
    }

    void cancelPrepare() override
    {
        if (auto c = cancel_.lock()) c->store(true);
        ++generation_;
    }

    LaunchResult launch(const PreparedInstall& prepared) override
    {
        LaunchResult r;
        if (launched_) {
            r.error = QStringLiteral("a helper was already started for this install");
            return r;
        }
        const QList<QByteArray> state = prepared.backendState.split('\n');
        const QString requirement = state.size() == 2 ? QString::fromUtf8(state[0]) : QString();
        const QString tool = state.size() == 2 ? QString::fromUtf8(state[1]) : QString();
        if (!validatePreparedForHelper(prepared, &r.error)) return r;
        // Both requirements go to the helper as plain argument values (never
        // evaluated by the shell): one line each, the tool's derived from the app's.
        const QString toolRequirement = swapToolRequirement(requirement);
        if (requirement.contains(QLatin1Char('\r')) || toolRequirement.isEmpty() ||
            toolRequirement.contains(QLatin1Char('\n')) || toolRequirement.contains(QLatin1Char('\r'))) {
            r.error = QStringLiteral("the requirements cannot be handed to the helper");
            return r;
        }
        if (!isCertificateAnchoredRequirement(requirement) ||
            !isOurStaging(prepared.stagedFile, prepared.order.context.appLocation, prepared.order.context.tuning) ||
            !isOurSwapTool(tool, prepared.order.productSlug)) {
            r.error = QStringLiteral("the prepared install is not one of ours");
            return r;
        }
        const HelperPlan plan = macHelperPlan(prepared, requirement, tool);
        r.started = posixhelper::spawnDetachedHelper(plan, prepared.order.resultFilePath + QStringLiteral(".log"),
                                                     hooks_.takeLifeline, &r.error);
        launched_ = r.started;
        return r;
    }

    void discard(const PreparedInstall& prepared) override
    {
        if (launched_) return;  // the helper owns staging and swap program now
        discardPrepared(prepared, shared_.get());
    }

    // A start without a pending install: the same work minus what another
    // instance's prepared install may own (CleanupScope).
    void cleanupLeftovers(const InstallContext& requested) override { startCleanup(requested, CleanupScope::Leftovers); }

    // Asynchronous: hdiutil and codesign may take seconds each. The worker
    // is waited for like a prepare() when the backend goes away.
    void cleanupAfterStart(const InstallContext& requested) override
    {
        startCleanup(requested, CleanupScope::AfterInstall);
    }

    void startCleanup(const InstallContext& requested, CleanupScope scope)
    {
        auto shared = shared_;
        auto finished = hooks_.cleanupFinishedForTests;
        QThread* worker = QThread::create([requested, shared, finished, scope]() {
            {
                const std::lock_guard<std::timed_mutex> lock(shared->work);
                cleanupWork(requested, *shared, scope);
            }
            if (finished) finished();
        });
        QObject::connect(worker, &QThread::finished, worker, &QObject::deleteLater);
        addWorker(worker);
        worker->start();
    }

private:
    SelfInstallFacts facts(const InstallContext& c) const
    {
        return hooks_.facts ? hooks_.facts(c) : measureFacts(c);
    }

    void addWorker(QThread* worker)
    {
        workers_.removeIf([](const QPointer<QThread>& w) { return w.isNull(); });
        workers_ << QPointer<QThread>(worker);  // every one is waited for, not only the last
    }

    SelfInstallHooks hooks_;
    QObject ctx_;
    std::atomic<int> generation_{0};
    std::weak_ptr<std::atomic<bool>> cancel_;
    QList<QPointer<QThread>> workers_;
    std::shared_ptr<MacBackendShared> shared_ = std::make_shared<MacBackendShared>();
    bool launched_ = false;
};

}  // namespace

QString posixhelper::userTempDir()
{
    char buf[PATH_MAX];
    const size_t n = ::confstr(_CS_DARWIN_USER_TEMP_DIR, buf, sizeof buf);
    if (n == 0 || n > sizeof buf) return {};
    char* real = ::realpath(buf, nullptr);
    if (!real) return {};
    const QString r = QDir::cleanPath(QFile::decodeName(QByteArray(real)));
    std::free(real);
    return r;
}

std::unique_ptr<InstallBackend> makeMacBundleSwapBackend(SelfInstallHooks hooks)
{
    return std::make_unique<MacBundleSwapBackend>(std::move(hooks));
}

}  // namespace appupdate

#endif  // Q_OS_MACOS
