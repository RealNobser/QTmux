// update-helper.exe — the Win32 shell around UpdateHelperCore (Windows only).
//
// GUI subsystem (no console window), no Qt, static runtime. Every decision
// lives in UpdateHelperCore and is tested there on every system; this file
// only touches the system: wait on the inherited app handle, hash the MSI
// with BCrypt, run msiexec, write the result file, start the app again.
//
// Started by the app from aboutToQuit (InstallBackend::launch), from a copy
// outside the install folder — a running helper inside it would be a file
// in use for the very MSI it starts.

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <bcrypt.h>
#include <shellapi.h>

#include "update/winhelper/UpdateHelperCore.hpp"

#include <string>
#include <vector>

using namespace appupdate::winhelper;

namespace {

std::wstring systemDirectory()
{
    wchar_t buf[MAX_PATH + 1] = {};
    const UINT n = GetSystemDirectoryW(buf, MAX_PATH);
    return (n > 0 && n < MAX_PATH) ? std::wstring(buf, n) : std::wstring(L"C:\\Windows\\System32");
}

// The result goes through a NEW file with a random name (CREATE_NEW never
// opens what already stands there; FILE_FLAG_OPEN_REPARSE_POINT never follows
// a link or junction), checked at the handle to be a plain file with one name
// only, then renamed over the result (a rename replaces a link, it does not
// write through it).
// The mark of this attempt, set once from the parsed call; every result
// carries it (the flow takes a result only with its own mark).
std::string gAttempt;

bool writeResult(const std::wstring& path, Outcome o)
{
    if (o.attempt.empty()) o.attempt = gAttempt;
    const std::string text = serializeOutcome(o);
    unsigned char rnd[8] = {};
    if (BCryptGenRandom(nullptr, rnd, sizeof rnd, BCRYPT_USE_SYSTEM_PREFERRED_RNG) != 0) return false;
    static const wchar_t* hexd = L"0123456789abcdef";
    std::wstring tmp = path + L".";
    for (unsigned char b : rnd) {
        tmp.push_back(hexd[b >> 4]);
        tmp.push_back(hexd[b & 0xF]);
    }
    tmp += L".tmp";
    // The folder of the result must not be a junction or link either: the
    // new file and the rename would otherwise land in its target.
    const size_t cut = path.find_last_of(L'\\');
    if (cut == std::wstring::npos) return false;
    const std::wstring parent = cut == 2 ? path.substr(0, 3) : path.substr(0, cut);
    HANDLE dir = CreateFileW(parent.c_str(), FILE_READ_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                             nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
    if (dir == INVALID_HANDLE_VALUE) return false;
    BY_HANDLE_FILE_INFORMATION dirInfo{};
    const bool plainDir = GetFileInformationByHandle(dir, &dirInfo) &&
                          (dirInfo.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0 &&
                          (dirInfo.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) == 0;
    CloseHandle(dir);
    if (!plainDir) return false;
    HANDLE h = CreateFileW(tmp.c_str(), GENERIC_WRITE | DELETE, 0, nullptr, CREATE_NEW,
                           FILE_ATTRIBUTE_NORMAL | FILE_FLAG_WRITE_THROUGH | FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;
    BY_HANDLE_FILE_INFORMATION info{};
    const bool plain = GetFileInformationByHandle(h, &info) && info.nNumberOfLinks == 1 &&
                       (info.dwFileAttributes & (FILE_ATTRIBUTE_REPARSE_POINT | FILE_ATTRIBUTE_DIRECTORY)) == 0;
    DWORD written = 0;
    const BOOL ok = plain && WriteFile(h, text.data(), static_cast<DWORD>(text.size()), &written, nullptr);
    CloseHandle(h);
    if (!ok || written != text.size()) {
        DeleteFileW(tmp.c_str());
        return false;
    }
    if (MoveFileExW(tmp.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) return true;
    DeleteFileW(tmp.c_str());
    return false;
}

// SHA-256 of the file, read once; the handle is closed again before msiexec
// starts (holding it without write sharing made msiexec /i fail with 1603).
bool sha256Hex(const std::wstring& path, unsigned long long expectedSize, std::wstring* hexOut)
{
    HANDLE f = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                           FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
    if (f == INVALID_HANDLE_VALUE) return false;
    LARGE_INTEGER size{};
    if (!GetFileSizeEx(f, &size) || static_cast<unsigned long long>(size.QuadPart) != expectedSize) {
        CloseHandle(f);
        return false;
    }
    BCRYPT_ALG_HANDLE alg = nullptr;
    BCRYPT_HASH_HANDLE hash = nullptr;
    bool ok = BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM, nullptr, 0) == 0 &&
              BCryptCreateHash(alg, &hash, nullptr, 0, nullptr, 0, 0) == 0;
    std::vector<unsigned char> buf(1 << 20);
    unsigned long long total = 0;
    while (ok) {
        DWORD got = 0;
        if (!ReadFile(f, buf.data(), static_cast<DWORD>(buf.size()), &got, nullptr)) { ok = false; break; }
        if (got == 0) break;
        total += got;
        if (total > expectedSize) { ok = false; break; }
        ok = BCryptHashData(hash, buf.data(), got, 0) == 0;
    }
    unsigned char digest[32] = {};
    ok = ok && total == expectedSize && BCryptFinishHash(hash, digest, sizeof digest, 0) == 0;
    if (hash) BCryptDestroyHash(hash);
    if (alg) BCryptCloseAlgorithmProvider(alg, 0);
    CloseHandle(f);
    if (!ok) return false;
    static const wchar_t* hexd = L"0123456789abcdef";
    hexOut->clear();
    for (unsigned char b : digest) {
        hexOut->push_back(hexd[b >> 4]);
        hexOut->push_back(hexd[b & 0xF]);
    }
    return true;
}

bool isElevated()
{
    HANDLE token = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) return false;
    TOKEN_ELEVATION e{};
    DWORD len = 0;
    const BOOL ok = GetTokenInformation(token, TokenElevation, &e, sizeof e, &len);
    CloseHandle(token);
    return ok && e.TokenIsElevated != 0;
}

// Starts the app again with the helper's own (unelevated) token, outside
// any job the helper may have been placed in.
bool startApp(const Args& a)
{
    std::wstring cmd = L"\"" + a.exe + L"\"";
    STARTUPINFOW si{};
    si.cb = sizeof si;
    PROCESS_INFORMATION pi{};
    if (!CreateProcessW(a.exe.c_str(), cmd.data(), nullptr, nullptr, FALSE,
                        CREATE_BREAKAWAY_FROM_JOB | CREATE_NEW_PROCESS_GROUP, nullptr,
                        a.installFolder.c_str(), &si, &pi)) {
        if (!CreateProcessW(a.exe.c_str(), cmd.data(), nullptr, nullptr, FALSE, CREATE_NEW_PROCESS_GROUP,
                            nullptr, a.installFolder.c_str(), &si, &pi))
            return false;
    }
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return true;
}

// Writes the outcome BEFORE the app starts again (the contract: the app reads
// it at its start), then starts it — never elevated (an elevated helper skips
// the start) — and rewrites the outcome if the start failed.
void reportAndRestart(const Args& a, Outcome o)
{
    o.restart = isElevated() ? "skipped-elevated" : "started";
    writeResult(a.result, o);
    if (o.restart == "started" && !startApp(a)) {
        o.restart = "failed";
        writeResult(a.result, o);
    }
}

}  // namespace

// The build id of the app this helper ships with, as in update_swap.c: handed
// in by appupdate_embed_install_helpers(... BUILD_ID_HEADER ... BUILD_ID_MACRO
// ...) as a forced include, "unknown" without. Read once below, so /OPT:REF
// keeps it; nothing prints it.
#ifndef APPUPDATE_HELPER_BUILD_ID
#define APPUPDATE_HELPER_BUILD_ID "unknown"
#endif
static const char kHelperBuildId[] = "update-helper build " APPUPDATE_HELPER_BUILD_ID;

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int)
{
    (void)*static_cast<volatile const char*>(kHelperBuildId);
    // Every later DLL load only from System32 (the loads before main are
    // bound by /DEPENDENTLOADFLAG:0x800 at link time): the helper runs from a
    // copy, and nothing next to it may be loaded.
    SetDefaultDllDirectories(LOAD_LIBRARY_SEARCH_SYSTEM32);
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (!argv) return 64;
    std::vector<std::wstring> args;
    for (int i = 1; i < argc; ++i) args.emplace_back(argv[i]);
    LocalFree(argv);

    // "--probe": the app checks that this location may run programs
    // (AppLocker/WDAC) before it quits. Changes nothing.
    if (args.size() == 1 && args[0] == L"--probe") return 0;

    std::wstring why;
    const auto parsed = parseArgs(args, &why);
    if (!parsed) return 64;  // no trustworthy result path to write to
    const Args& a = *parsed;
    gAttempt = toUtf8(a.attempt);
    const std::string version = toUtf8(a.version);

    // 1. Wait until the app is really gone (its own process handle — no pid
    //    that could be reused).
    HANDLE app = reinterpret_cast<HANDLE>(static_cast<ULONG_PTR>(a.appHandle));
    const DWORD w = WaitForSingleObject(app, a.waitMs);
    CloseHandle(app);
    if (w == WAIT_FAILED) {
        writeResult(a.result, Outcome{"failed", "wait-exit", "the handle to the app is unusable", "", "wait-failed", ""});
        return 1;
    }
    if (w != WAIT_OBJECT_0) {
        writeResult(a.result, Outcome{"failed", "wait-exit", "the app was still running", "", "app-running", ""});
        return 1;
    }

    // 2. Hash again, right before msiexec. Known remaining window: between
    //    this hash and the moment the Windows Installer
    //    service copies the package into its cache, a process of the SAME
    //    user could replace the file. Holding it open without write sharing
    //    is no remedy — measured: msiexec /i then fails itself (1603) — and a
    //    copy into a folder with a narrow ACL is none either: the folder
    //    belongs to the same user, who may change the ACL back. Only an
    //    Authenticode-signed MSI closes it (a separate decision). Do not add a
    //    lock here.
    std::wstring hex;
    if (!sha256Hex(a.msi, a.size, &hex) || hex != a.sha256) {
        reportAndRestart(a, Outcome{"failed", "verify", "the package does not match the manifest", "",
                                    "hash-mismatch", ""});
        return 1;
    }

    // 3. msiexec, unattended with progress; Windows asks for elevation itself.
    const std::wstring sys = systemDirectory();
    const std::wstring program = sys + L"\\msiexec.exe";
    std::wstring cmd = msiexecCommandLine(sys, a);
    STARTUPINFOW si{};
    si.cb = sizeof si;
    PROCESS_INFORMATION pi{};
    if (!CreateProcessW(program.c_str(), cmd.data(), nullptr, nullptr, FALSE, 0, nullptr, sys.c_str(), &si, &pi)) {
        reportAndRestart(a, Outcome{"failed", "install", "msiexec could not be started", "", "msiexec-start", ""});
        return 1;
    }
    CloseHandle(pi.hThread);
    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD exitCode = 1;
    GetExitCodeProcess(pi.hProcess, &exitCode);
    CloseHandle(pi.hProcess);

    Outcome o = outcomeForMsiexec(exitCode, version);

    // 4. Start the app again — the new version, or the old one that
    //    Windows Installer kept (failure rolls back) — never elevated.
    reportAndRestart(a, o);
    return o.result == "ok" ? 0 : (o.result == "cancelled" ? 2 : 1);
}
