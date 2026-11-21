// Windows: the helper around msiexec — see SelfInstall.hpp for the rules and
// winhelper/UpdateHelperCore.hpp for what the helper program itself does.
//
// The app side, in order:
//   capability()  the exe belongs to the product registered under the app's
//                 UpgradeCode (stored install location, or the default folder
//                 for older installations without a stored location), no other
//                 process runs from the install folder, update-helper.exe
//                 lies next to the exe.
//   prepare()     size and SHA-256 of the MSI against the signed manifest;
//                 copy the helper out of the install folder (the MSI replaces
//                 it there) into a fresh folder under the app's local data,
//                 hold the copy open so nobody may write, rename or delete it
//                 (reading and running stay possible), check the copy through
//                 that handle against the original, and start it once with
//                 --probe: a place where programs may not run (AppLocker,
//                 WDAC) means Manual BEFORE the app quits.
//   launch()      start the copy with an inheritable handle to the app's own
//                 process (only that handle is inherited) — the helper waits
//                 on it, never on a pid.

#include "update/SelfInstall.hpp"

#if defined(Q_OS_WIN)

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <knownfolders.h>
#include <msi.h>
#include <shlobj.h>
#include <tlhelp32.h>

#include "update/winhelper/UpdateHelperCore.hpp"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QList>
#include <QMetaObject>
#include <QObject>
#include <QPointer>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QThread>
#include <QTimeZone>

#include <atomic>
#include <map>
#include <memory>
#include <vector>

namespace appupdate {

namespace {

constexpr qint64 kHelperMaxBytes = 32 * 1024 * 1024;  // the helper is ~200 KB; a bound, not a guess of its size
constexpr DWORD kProbeTimeoutMs = 15'000;
// Younger than this, a helper folder may belong to a prepare() that runs
// right now (this process during the first seconds, or another copy of the
// app with the same local data): cleanupAfterStart leaves it.
constexpr qint64 kHelperLeftoverAgeSecs = 10 * 60;

QString native(const QString& p) { return QDir::toNativeSeparators(p); }

std::wstring wide(const QString& s) { return s.toStdWString(); }

QString windowsErrorText(DWORD e)
{
    wchar_t* buf = nullptr;
    const DWORD n = FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
                                       FORMAT_MESSAGE_IGNORE_INSERTS,
                                   nullptr, e, MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US),
                                   reinterpret_cast<LPWSTR>(&buf), 0, nullptr);
    QString text = n ? QString::fromWCharArray(buf, int(n)).trimmed() : QString();
    if (buf) LocalFree(buf);
    return QStringLiteral("error %1%2").arg(e).arg(text.isEmpty() ? QString() : QStringLiteral(" (") + text + QLatin1Char(')'));
}

// "{XXXXXXXX-…}" as MsiEnumRelatedProducts wants it; the .wxs writes it
// without braces, so both spellings are accepted from the app's code.
QString bracedUpgradeCode(const QString& code)
{
    const QString c = code.trimmed().toUpper();
    return c.startsWith(QLatin1Char('{')) ? c : QLatin1Char('{') + c + QLatin1Char('}');
}

WindowsInstallRecord installRecord(const QString& upgradeCode)
{
    WindowsInstallRecord r;
    const std::wstring code = wide(bracedUpgradeCode(upgradeCode));
    for (DWORD i = 0; i < 16; ++i) {
        wchar_t product[39] = {};
        if (MsiEnumRelatedProductsW(code.c_str(), 0, i, product) != ERROR_SUCCESS) break;
        ++r.relatedProducts;
        if (r.relatedProducts != 1) continue;
        std::vector<wchar_t> loc(1024);
        DWORD n = DWORD(loc.size());
        if (MsiGetProductInfoW(product, INSTALLPROPERTY_INSTALLLOCATION, loc.data(), &n) == ERROR_SUCCESS)
            r.installLocation = QString::fromWCharArray(loc.data(), int(n));
    }
    return r;
}

QString programFilesFolder()
{
    PWSTR p = nullptr;
    QString r;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_ProgramFiles, 0, nullptr, &p))) r = QString::fromWCharArray(p);
    CoTaskMemFree(p);
    return r;
}

// Another process runs from the install folder. A process this user may not
// open (another user, a protected process) counts when its exe has the app's
// file name — what cannot be measured counts against the automatic way.
bool otherInstanceRuns(const QString& exe, const QString& folder)
{
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return true;
    const DWORD me = GetCurrentProcessId();
    const QString exeName = QFileInfo(exe).fileName();
    const std::wstring nativeFolder = wide(native(folder));
    PROCESSENTRY32W pe{};
    pe.dwSize = sizeof pe;
    bool found = false;
    std::vector<wchar_t> path(32768);
    for (BOOL ok = Process32FirstW(snap, &pe); ok && !found; ok = Process32NextW(snap, &pe)) {
        if (pe.th32ProcessID == me || pe.th32ProcessID == 0) continue;
        HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pe.th32ProcessID);
        if (!h) {
            if (QString::fromWCharArray(pe.szExeFile).compare(exeName, Qt::CaseInsensitive) == 0) found = true;
            continue;
        }
        DWORD n = DWORD(path.size());
        if (QueryFullProcessImageNameW(h, 0, path.data(), &n) &&
            winhelper::isInsideFolder(std::wstring(path.data(), n), nativeFolder))
            found = true;
        CloseHandle(h);
    }
    CloseHandle(snap);
    return found;
}

SelfInstallFacts measureFacts(const InstallContext& c, const SelfInstallHooks& hooks)
{
    if (hooks.facts) return hooks.facts(c);
    SelfInstallFacts f;
    const QFileInfo exe(c.appLocation);
    f.locationExists = exe.isFile() && !exe.isSymLink() && !exe.isJunction();
    f.locationWritable = true;  // msiexec writes the install folder, not the app
    f.platformMeasured = kWindowsSelfInstallMeasured || hooks.assumePlatformMeasuredForTests;
    if (!f.locationExists) return f;
    if (!c.windowsUpgradeCode.isEmpty())
        f.installedPackage = windowsRunsFromInstalledPackage(native(exe.absoluteFilePath()),
                                                             installRecord(c.windowsUpgradeCode),
                                                             programFilesFolder(), c.productSlug,
                                                             &f.installedPackageWhy);
    f.otherInstanceRunning = otherInstanceRuns(exe.absoluteFilePath(), exe.absolutePath());
    const QFileInfo helper(exe.absolutePath() + QLatin1Char('/') + QLatin1String(kWindowsHelperFileName));
    f.helperAvailable = helper.isFile() && !helper.isSymLink() && !helper.isJunction();
    return f;
}

// Where helper copies live: <app local data>/update-helper/<slug>-<16 hex>/.
QString helperBaseDir(const InstallTuning& t)
{
    if (!t.stagingDirForTests.isEmpty()) return QDir::cleanPath(t.stagingDirForTests);
    const QString data = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    return data.isEmpty() ? QString() : QDir::cleanPath(data + QStringLiteral("/update-helper"));
}

bool isOurHelperDirName(const QString& name, const QString& slug)
{
    const QRegularExpression re(QStringLiteral("^") + QRegularExpression::escape(helperNamePart(slug)) +
                                QStringLiteral("-[0-9a-f]{16}$"));
    return re.match(name).hasMatch();
}

// Removes the files directly in one of our helper folders, then the folder.
// Never follows a link or junction; a file still in use simply stays.
void removeHelperDir(const QString& dir)
{
    const QFileInfo d(dir);
    if (dir.isEmpty() || !d.isDir() || d.isSymLink() || d.isJunction()) return;
    for (const QFileInfo& fi : QDir(dir).entryInfoList(QDir::Files | QDir::Dirs | QDir::Hidden | QDir::System |
                                                       QDir::NoDotAndDotDot)) {
        if (fi.isSymLink() || fi.isJunction() || fi.isDir()) continue;
        QFile::remove(fi.absoluteFilePath());
    }
    QDir().rmdir(dir);
}

bool readGuarded(const QString& path, qint64 limit, QByteArray* out, QString* why)
{
    const QFileInfo fi(path);
    if (!fi.isFile() || fi.isSymLink() || fi.isJunction()) {
        *why = QStringLiteral("%1 is not a plain file").arg(path);
        return false;
    }
    if (fi.size() > limit) {
        *why = QStringLiteral("%1 is larger than %2 bytes").arg(path).arg(limit);
        return false;
    }
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        *why = QStringLiteral("cannot read %1: %2").arg(path, f.errorString());
        return false;
    }
    *out = f.read(limit + 1);
    if (out->size() > limit) {
        *why = QStringLiteral("%1 grew beyond %2 bytes").arg(path).arg(limit);
        return false;
    }
    return true;
}

bool fileMatches(const QString& path, qint64 size, const QString& sha256, const std::atomic<bool>& cancel,
                 QString* why)
{
    QFile f(path);
    if (!QFileInfo(path).isFile() || !f.open(QIODevice::ReadOnly)) {
        *why = QStringLiteral("cannot read %1").arg(path);
        return false;
    }
    QCryptographicHash h(QCryptographicHash::Sha256);
    qint64 total = 0;
    while (!f.atEnd()) {
        if (cancel.load()) {
            *why = QStringLiteral("cancelled");
            return false;
        }
        const QByteArray chunk = f.read(1 << 20);
        if (chunk.isEmpty()) break;
        total += chunk.size();
        if (total > size) break;
        h.addData(chunk);
    }
    if (total != size || QString::fromLatin1(h.result().toHex()) != sha256) {
        *why = QStringLiteral("%1 does not match the manifest (size or SHA-256)").arg(path);
        return false;
    }
    return true;
}

// SHA-256 through an open handle, from the start.
bool hashHandle(HANDLE h, QByteArray* out)
{
    LARGE_INTEGER zero{};
    if (!SetFilePointerEx(h, zero, nullptr, FILE_BEGIN)) return false;
    QCryptographicHash hash(QCryptographicHash::Sha256);
    std::vector<char> buf(1 << 16);
    qint64 total = 0;
    for (;;) {
        DWORD got = 0;
        if (!ReadFile(h, buf.data(), DWORD(buf.size()), &got, nullptr)) return false;
        if (got == 0) break;
        total += got;
        if (total > kHelperMaxBytes) return false;
        hash.addData(QByteArrayView(buf.data(), qsizetype(got)));
    }
    *out = hash.result();
    return true;
}

// Runs the copy once with --probe and waits for exit code 0.
bool probeHelper(const QString& copy, const QString& dir, QString* why)
{
    const std::wstring program = wide(native(copy));
    std::wstring cmd = L"\"" + program + L"\" --probe";
    STARTUPINFOW si{};
    si.cb = sizeof si;
    PROCESS_INFORMATION pi{};
    if (!CreateProcessW(program.c_str(), cmd.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr,
                        wide(native(dir)).c_str(), &si, &pi)) {
        *why = QStringLiteral("the update helper cannot be started from %1: %2").arg(dir, windowsErrorText(GetLastError()));
        return false;
    }
    CloseHandle(pi.hThread);
    const DWORD w = WaitForSingleObject(pi.hProcess, kProbeTimeoutMs);
    DWORD code = 1;
    if (w == WAIT_OBJECT_0) GetExitCodeProcess(pi.hProcess, &code);
    else TerminateProcess(pi.hProcess, 1);
    CloseHandle(pi.hProcess);
    if (w != WAIT_OBJECT_0 || code != 0) {
        *why = QStringLiteral("the update helper's probe in %1 answered %2").arg(dir).arg(w == WAIT_OBJECT_0 ? QString::number(code) : QStringLiteral("nothing"));
        return false;
    }
    return true;
}

struct WinStaging {
    QString dir;
    QString copy;
    HANDLE hold = INVALID_HANDLE_VALUE;
};

void releaseStaging(WinStaging* s)
{
    if (s->hold != INVALID_HANDLE_VALUE) CloseHandle(s->hold);
    s->hold = INVALID_HANDLE_VALUE;
    removeHelperDir(s->dir);
    s->dir.clear();
    s->copy.clear();
}

// The whole of prepare(), off the GUI thread.
PrepareResult prepareWork(const InstallOrder& order, const SelfInstallFacts& facts,
                          const std::atomic<bool>& cancel, WinStaging* st)
{
    PrepareResult res;
    const InstallCapability cap = selfInstallDecision(SelfInstallPlatform::Windows, order.context, facts);
    if (cap.method != InstallMethod::Automatic) {
        res.manual = cap.reason;
        res.error = cap.detail;
        return res;
    }
    if (QDir::cleanPath(order.context.verifiedFile) != QDir::cleanPath(order.verifiedFile)) {
        res.error = QStringLiteral("the order names two different files");
        return res;
    }
    if (!fileMatches(order.verifiedFile, order.expectedSize, order.expectedSha256, cancel, &res.error)) return res;

    // The helper, out of the install folder that the MSI is about to replace.
    const QFileInfo exe(order.context.appLocation);
    const QString source = exe.absolutePath() + QLatin1Char('/') + QLatin1String(kWindowsHelperFileName);
    QByteArray bytes;
    if (!readGuarded(source, kHelperMaxBytes, &bytes, &res.error)) {
        res.manual = ManualReason::HelperUnavailable;
        return res;
    }
    const QByteArray sourceHash = QCryptographicHash::hash(bytes, QCryptographicHash::Sha256);

    const QString base = helperBaseDir(order.context.tuning);
    if (base.isEmpty() || !QDir().mkpath(base)) {
        res.manual = ManualReason::HelperUnavailable;
        res.error = QStringLiteral("no folder for the update helper (%1)").arg(base);
        return res;
    }
    const QString name = helperNamePart(order.productSlug) + QLatin1Char('-') +
                         QString::number(QRandomGenerator::system()->generate64(), 16).rightJustified(16, QLatin1Char('0'));
    const QString dir = base + QLatin1Char('/') + name;
    if (!QDir().mkdir(dir)) {  // must be new: an existing name is not ours
        res.error = QStringLiteral("cannot create %1").arg(dir);
        return res;
    }
    st->dir = dir;
    st->copy = dir + QLatin1Char('/') + QLatin1String(kWindowsHelperFileName);
    {
        QFile out(st->copy);
        if (!out.open(QIODevice::WriteOnly | QIODevice::NewOnly) || out.write(bytes) != bytes.size() ||
            !out.flush()) {
            res.error = QStringLiteral("cannot write %1: %2").arg(st->copy, out.errorString());
            return res;
        }
    }
    // Held from here until the app ends: nobody may write, rename or delete
    // the copy; reading and running it stay possible.
    st->hold = CreateFileW(wide(native(st->copy)).c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                           FILE_ATTRIBUTE_NORMAL, nullptr);
    if (st->hold == INVALID_HANDLE_VALUE) {
        res.error = QStringLiteral("cannot hold %1: %2").arg(st->copy, windowsErrorText(GetLastError()));
        return res;
    }
    QByteArray copyHash;
    if (!hashHandle(st->hold, &copyHash) || copyHash != sourceHash) {
        res.error = QStringLiteral("the copy of the update helper differs from %1").arg(source);
        return res;
    }
    if (cancel.load()) {
        res.error = QStringLiteral("cancelled");
        return res;
    }
    if (!probeHelper(st->copy, dir, &res.error)) {
        res.manual = ManualReason::HelperUnavailable;
        return res;
    }

    PreparedInstall p;
    p.order = order;
    p.method = InstallMethod::Automatic;
    p.stagedFile = order.verifiedFile;
    p.backendState = st->copy.toUtf8();
    p.description = QStringLiteral("helper %1: wait for the app, check sha256 of %2, msiexec /i /passive "
                                   "INSTALLFOLDER=%3, start %4")
                        .arg(native(st->copy), native(order.verifiedFile), native(exe.absolutePath()),
                             native(exe.absoluteFilePath()));
    res.prepared = p;
    return res;
}

class WindowsMsiHelperBackend final : public InstallBackend {
public:
    explicit WindowsMsiHelperBackend(SelfInstallHooks hooks) : hooks_(std::move(hooks)) {}

    ~WindowsMsiHelperBackend() override
    {
        cancelPrepare();
        for (const QPointer<QThread>& w : workers_)
            if (w && w->isRunning()) w->wait(120000);
        // Not launched: the copies go. Launched: the running helper holds its
        // own image; its folder goes with cleanupAfterStart of the next start.
        for (auto& [copy, h] : held_) {
            CloseHandle(h);
            if (!launched_) removeHelperDir(QFileInfo(copy).absolutePath());
        }
    }

    InstallCapability capability(const InstallContext& context) const override
    {
        return selfInstallDecision(SelfInstallPlatform::Windows, context, measureFacts(context, hooks_));
    }

    void prepare(const InstallOrder& order, PrepareDone done) override
    {
        cancelPrepare();
        const int gen = ++generation_;
        auto cancel = std::make_shared<std::atomic<bool>>(false);
        cancel_ = cancel;
        const SelfInstallFacts f = measureFacts(order.context, hooks_);  // on the owner thread
        QPointer<QObject> ctx(&ctx_);
        QThread* worker = QThread::create([order, f, cancel, ctx, gen, this, done]() {
            WinStaging st;
            PrepareResult res = prepareWork(order, f, *cancel, &st);
            if (!res.prepared || cancel->load()) {
                releaseStaging(&st);
                res.prepared.reset();
            }
            const quintptr hold = reinterpret_cast<quintptr>(st.hold);
            const QString dir = st.dir;
            // Delivered on the owner (GUI) thread; whether the backend still
            // exists is asked THERE, where it would be destroyed.
            QMetaObject::invokeMethod(QCoreApplication::instance(), [this, ctx, gen, cancel, res, done, hold, dir]() {
                if (!ctx || cancel->load() || gen != generation_.load()) {
                    if (res.prepared) {
                        WinStaging s{dir, {}, reinterpret_cast<HANDLE>(hold)};
                        releaseStaging(&s);
                    }
                    return;
                }
                if (res.prepared) held_[QString::fromUtf8(res.prepared->backendState)] = reinterpret_cast<HANDLE>(hold);
                done(res);
            }, Qt::QueuedConnection);
        });
        QObject::connect(worker, &QThread::finished, worker, &QObject::deleteLater);
        workers_.removeIf([](const QPointer<QThread>& w) { return w.isNull(); });
        workers_ << QPointer<QThread>(worker);
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
        const QString copy = QString::fromUtf8(prepared.backendState);
        if (held_.find(copy) == held_.end()) {
            r.error = QStringLiteral("the prepared install is not one of ours");
            return r;
        }
        const QString dir = QFileInfo(copy).absolutePath();

        HANDLE self = hooks_.appProcessForTests ? static_cast<HANDLE>(hooks_.appProcessForTests())
                                                : GetCurrentProcess();
        HANDLE inherit = nullptr;
        if (!DuplicateHandle(GetCurrentProcess(), self, GetCurrentProcess(), &inherit, SYNCHRONIZE, TRUE, 0)) {
            r.error = QStringLiteral("cannot pass the app's handle: %1").arg(windowsErrorText(GetLastError()));
            return r;
        }
        const auto args = windowsHelperArguments(prepared, dir + QStringLiteral("/msiexec.log"),
                                                 quint64(reinterpret_cast<quintptr>(inherit)), &r.error);
        const auto line = args ? windowsCommandLine(native(copy), *args) : std::nullopt;
        if (!args || !line) {
            if (r.error.isEmpty()) r.error = QStringLiteral("the helper's command line cannot be quoted safely");
            CloseHandle(inherit);
            return r;
        }

        // Only this one handle is inherited, nothing else the app has open.
        SIZE_T size = 0;
        InitializeProcThreadAttributeList(nullptr, 1, 0, &size);
        std::vector<char> buf(size);
        auto* list = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(buf.data());
        if (!InitializeProcThreadAttributeList(list, 1, 0, &size) ||
            !UpdateProcThreadAttribute(list, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST, &inherit, sizeof inherit,
                                       nullptr, nullptr)) {
            r.error = QStringLiteral("cannot restrict the inherited handles: %1").arg(windowsErrorText(GetLastError()));
            CloseHandle(inherit);
            return r;
        }
        STARTUPINFOEXW si{};
        si.StartupInfo.cb = sizeof si;
        si.lpAttributeList = list;
        PROCESS_INFORMATION pi{};
        const std::wstring program = wide(native(copy));
        const std::wstring workDir = wide(native(dir));
        // Outside the app's job where allowed (a job that ends with the app
        // would take the helper along); without breakaway otherwise.
        DWORD flags = EXTENDED_STARTUPINFO_PRESENT | CREATE_NO_WINDOW | CREATE_NEW_PROCESS_GROUP |
                      CREATE_BREAKAWAY_FROM_JOB;
        std::wstring cmd = wide(*line);
        BOOL ok = CreateProcessW(program.c_str(), cmd.data(), nullptr, nullptr, TRUE, flags, nullptr,
                                 workDir.c_str(), &si.StartupInfo, &pi);
        DWORD err = ok ? 0 : GetLastError();
        if (!ok && err == ERROR_ACCESS_DENIED) {
            flags &= ~DWORD(CREATE_BREAKAWAY_FROM_JOB);
            cmd = wide(*line);
            ok = CreateProcessW(program.c_str(), cmd.data(), nullptr, nullptr, TRUE, flags, nullptr,
                                workDir.c_str(), &si.StartupInfo, &pi);
            err = ok ? 0 : GetLastError();
        }
        DeleteProcThreadAttributeList(list);
        CloseHandle(inherit);
        if (!ok) {
            r.error = QStringLiteral("cannot start the update helper: %1").arg(windowsErrorText(err));
            return r;
        }
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
        launched_ = true;
        r.started = true;
        return r;
    }

    void discard(const PreparedInstall& prepared) override
    {
        if (launched_) return;  // the helper runs from that copy
        const QString copy = QString::fromUtf8(prepared.backendState);
        const auto it = held_.find(copy);
        if (it == held_.end()) return;
        CloseHandle(it->second);
        held_.erase(it);
        removeHelperDir(QFileInfo(copy).absolutePath());
    }

    void cleanupAfterStart(const InstallContext& context) override
    {
        const QString base = helperBaseDir(context.tuning);
        if (base.isEmpty()) return;
        const QDateTime limit = QDateTime::currentDateTimeUtc().addSecs(-kHelperLeftoverAgeSecs);
        for (const QFileInfo& fi : QDir(base).entryInfoList(QDir::Dirs | QDir::Hidden | QDir::NoDotAndDotDot)) {
            if (fi.isSymLink() || fi.isJunction() || !isOurHelperDirName(fi.fileName(), context.productSlug))
                continue;
            if (fi.lastModified(QTimeZone::UTC) > limit) continue;
            bool ours = false;
            for (const auto& [copy, h] : held_)
                ours = ours || QFileInfo(copy).absolutePath() == fi.absoluteFilePath();
            if (!ours) removeHelperDir(fi.absoluteFilePath());  // a helper still running keeps its exe
        }
    }

private:
    SelfInstallHooks hooks_;
    QObject ctx_;
    QList<QPointer<QThread>> workers_;
    std::atomic<int> generation_{0};
    std::weak_ptr<std::atomic<bool>> cancel_;
    std::map<QString, HANDLE> held_;  // helper copy -> the handle that holds it (GUI thread only)
    bool launched_ = false;
};

}  // namespace

std::unique_ptr<InstallBackend> makeWindowsMsiHelperBackend(SelfInstallHooks hooks)
{
    return std::make_unique<WindowsMsiHelperBackend>(std::move(hooks));
}

}  // namespace appupdate

#endif  // Q_OS_WIN
