#include "update/winhelper/UpdateHelperCore.hpp"

#include <map>

namespace appupdate::winhelper {

namespace {

bool isHex64(const std::wstring& s)
{
    if (s.size() != 64) return false;
    for (wchar_t c : s)
        if (!((c >= L'0' && c <= L'9') || (c >= L'a' && c <= L'f'))) return false;
    return true;
}

bool isHex32(const std::wstring& s)
{
    if (s.size() != 32) return false;
    for (wchar_t c : s)
        if (!((c >= L'0' && c <= L'9') || (c >= L'a' && c <= L'f'))) return false;
    return true;
}

bool parseNumber(const std::wstring& s, unsigned long long* out)
{
    if (s.empty() || s.size() > 19) return false;
    unsigned long long v = 0;
    for (wchar_t c : s) {
        if (c < L'0' || c > L'9') return false;
        v = v * 10 + static_cast<unsigned long long>(c - L'0');
    }
    *out = v;
    return true;
}

bool isCleanVersion(const std::wstring& v)
{
    if (v.empty() || v.size() > 64) return false;
    const auto ok = [](wchar_t c) {
        return (c >= L'0' && c <= L'9') || (c >= L'a' && c <= L'z') || (c >= L'A' && c <= L'Z') ||
               c == L'.' || c == L'+' || c == L'_' || c == L'-';
    };
    if (!((v[0] >= L'0' && v[0] <= L'9') || (v[0] >= L'a' && v[0] <= L'z') || (v[0] >= L'A' && v[0] <= L'Z')))
        return false;
    for (wchar_t c : v)
        if (!ok(c)) return false;
    return true;
}

wchar_t lowerAscii(wchar_t c) { return (c >= L'A' && c <= L'Z') ? static_cast<wchar_t>(c - L'A' + L'a') : c; }

// CON, PRN, AUX, NUL, CONIN$, CONOUT$, COM1-9, LPT1-9 and COM/LPT with a
// superscript 1-3 — also with an extension ("nul.txt" names the device too):
// a path through one of them reaches a device.
bool isReservedDeviceName(const std::wstring& seg)
{
    std::wstring base;
    for (wchar_t c : seg) {
        if (c == L'.') break;
        base.push_back(lowerAscii(c));
    }
    while (!base.empty() && base.back() == L' ') base.pop_back();
    if (base == L"con" || base == L"prn" || base == L"aux" || base == L"nul" || base == L"conin$" ||
        base == L"conout$")
        return true;
    if (base.size() != 4 || (base.compare(0, 3, L"com") != 0 && base.compare(0, 3, L"lpt") != 0)) return false;
    const wchar_t d = base[3];
    // 1-9, and the superscript digits that Windows also maps to a port.
    return (d >= L'1' && d <= L'9') || d == L'\u00B9' || d == L'\u00B2' || d == L'\u00B3';
}

std::string oneLine(std::string s)
{
    for (char& c : s)
        if (c == '\n' || c == '\r') c = ' ';
    return s;
}

}  // namespace

bool isCleanWindowsPath(const std::wstring& p)
{
    if (p.size() < 3 || p.size() > 1024) return false;
    const wchar_t d = lowerAscii(p[0]);
    if (d < L'a' || d > L'z' || p[1] != L':' || p[2] != L'\\') return false;
    for (wchar_t c : p) {
        if (c < 0x20 || c == 0x7f || c == L'"' || c == L'/' || c == L'*' || c == L'?' || c == L'<' ||
            c == L'>' || c == L'|')
            return false;
    }
    if (p.find(L':', 2) != std::wstring::npos) return false;  // alternate data streams
    if (p.size() > 3 && p.back() == L'\\') return false;
    if (p.find(L"\\\\") != std::wstring::npos) return false;
    // No "." or ".." segment.
    size_t start = 3;
    while (start <= p.size()) {
        const size_t end = p.find(L'\\', start);
        const std::wstring seg = p.substr(start, end == std::wstring::npos ? std::wstring::npos : end - start);
        if (seg == L"." || seg == L"..") return false;
        if (isReservedDeviceName(seg)) return false;
        if (!seg.empty() && (seg.back() == L'.' || seg.back() == L' ')) return false;  // Windows trims them
        if (end == std::wstring::npos) break;
        start = end + 1;
    }
    return true;
}

bool isInsideFolder(const std::wstring& exe, const std::wstring& folder)
{
    if (folder.empty() || exe.size() <= folder.size() + 1) return false;
    for (size_t i = 0; i < folder.size(); ++i)
        if (lowerAscii(exe[i]) != lowerAscii(folder[i])) return false;
    return folder.back() == L'\\' ? true : exe[folder.size()] == L'\\';
}

std::optional<Args> parseArgs(const std::vector<std::wstring>& args, std::wstring* why)
{
    const auto fail = [why](const std::wstring& w) -> std::optional<Args> {
        if (why) *why = w;
        return std::nullopt;
    };
    if (why) why->clear();
    std::map<std::wstring, std::wstring> v;
    static const wchar_t* const kKnown[] = {L"--msi", L"--sha256", L"--size", L"--install-folder", L"--exe",
                                            L"--version", L"--result", L"--log", L"--attempt", L"--app-handle",
                                            L"--wait-ms"};
    for (size_t i = 0; i < args.size(); i += 2) {
        const std::wstring& k = args[i];
        bool known = false;
        for (const wchar_t* n : kKnown) known = known || k == n;
        if (!known) return fail(L"unknown option " + k);
        if (i + 1 >= args.size()) return fail(L"missing value for " + k);
        if (v.count(k)) return fail(L"option given twice: " + k);
        v[k] = args[i + 1];
    }
    for (const wchar_t* n : kKnown)
        if (std::wstring(n) != L"--wait-ms" && !v.count(n)) return fail(std::wstring(L"missing option ") + n);

    Args a;
    a.msi = v[L"--msi"];
    a.sha256 = v[L"--sha256"];
    a.installFolder = v[L"--install-folder"];
    a.exe = v[L"--exe"];
    a.version = v[L"--version"];
    a.attempt = v[L"--attempt"];
    a.result = v[L"--result"];
    a.log = v[L"--log"];
    for (const std::wstring* p : {&a.msi, &a.installFolder, &a.exe, &a.result, &a.log})
        if (!isCleanWindowsPath(*p)) return fail(L"not a clean absolute path: " + *p);
    if (!isHex64(a.sha256)) return fail(L"sha256 must be 64 lowercase hex digits");
    if (!parseNumber(v[L"--size"], &a.size) || a.size == 0) return fail(L"bad size");
    if (!isCleanVersion(a.version)) return fail(L"bad version");
    if (!isHex32(a.attempt)) return fail(L"attempt must be 32 lowercase hex digits");
    if (a.installFolder.size() <= 3) return fail(L"a drive root is no install folder");
    if (!isInsideFolder(a.exe, a.installFolder)) return fail(L"the app is not inside the install folder");
    if (!parseNumber(v[L"--app-handle"], &a.appHandle) || a.appHandle == 0) return fail(L"bad app handle");
    if (v.count(L"--wait-ms")) {
        unsigned long long w = 0;
        if (!parseNumber(v[L"--wait-ms"], &w) || w == 0 || w > 3600000) return fail(L"bad wait-ms");
        a.waitMs = static_cast<unsigned long>(w);
    }
    return a;
}

std::wstring msiexecCommandLine(const std::wstring& systemDir, const Args& a)
{
    std::wstring folder = a.installFolder;
    while (folder.size() > 3 && folder.back() == L'\\') folder.pop_back();
    return L"\"" + systemDir + L"\\msiexec.exe\" /i \"" + a.msi + L"\" /passive /norestart INSTALLFOLDER=\"" +
           folder + L"\" /l*v \"" + a.log + L"\"";
}

Outcome outcomeForMsiexec(unsigned long exitCode, const std::string& version)
{
    Outcome o;
    o.step = "install";
    o.version = version;
    switch (exitCode) {
    case 0: o.result = "ok"; break;
    case 3010: o.result = "ok"; o.code = "restart-required"; o.reason = "Windows asks for a restart"; break;
    case 1641: o.result = "ok"; o.code = "restart-initiated"; o.reason = "the installer started a restart"; break;
    case 1602:
    case 1223:
        o.result = "cancelled"; o.code = "user-cancelled";
        o.reason = "the installation was cancelled or the elevation was declined";
        break;
    case 1618: o.result = "failed"; o.code = "another-install-running"; o.reason = "another installation is running"; break;
    case 1601:
        o.result = "failed"; o.code = "msiexec-1601";
        o.reason = "the Windows Installer service could not be accessed (another program may have held files)";
        break;
    default:
        o.result = "failed"; o.code = "msiexec-" + std::to_string(exitCode);
        o.reason = "msiexec ended with " + std::to_string(exitCode);
        break;
    }
    return o;
}

std::string serializeOutcome(const Outcome& o)
{
    std::string text = "result=" + oneLine(o.result) + "\nstep=" + oneLine(o.step) + "\nreason=" +
                       oneLine(o.reason) + "\nversion=" + oneLine(o.version) + "\ncode=" + oneLine(o.code) + "\n";
    if (!o.restart.empty()) text += "restart=" + oneLine(o.restart) + "\n";
    if (!o.attempt.empty()) text += "attempt=" + oneLine(o.attempt) + "\n";
    return text;
}

std::string toUtf8(const std::wstring& s)
{
    std::string out;
    for (size_t i = 0; i < s.size(); ++i) {
        unsigned long c = static_cast<unsigned long>(s[i]);
        if (sizeof(wchar_t) == 2 && c >= 0xD800 && c <= 0xDBFF && i + 1 < s.size()) {
            const unsigned long lo = static_cast<unsigned long>(s[i + 1]);
            if (lo >= 0xDC00 && lo <= 0xDFFF) {
                c = 0x10000 + ((c - 0xD800) << 10) + (lo - 0xDC00);
                ++i;
            }
        }
        if (c < 0x80) {
            out += static_cast<char>(c);
        } else if (c < 0x800) {
            out += static_cast<char>(0xC0 | (c >> 6));
            out += static_cast<char>(0x80 | (c & 0x3F));
        } else if (c < 0x10000) {
            out += static_cast<char>(0xE0 | (c >> 12));
            out += static_cast<char>(0x80 | ((c >> 6) & 0x3F));
            out += static_cast<char>(0x80 | (c & 0x3F));
        } else {
            out += static_cast<char>(0xF0 | (c >> 18));
            out += static_cast<char>(0x80 | ((c >> 12) & 0x3F));
            out += static_cast<char>(0x80 | ((c >> 6) & 0x3F));
            out += static_cast<char>(0x80 | (c & 0x3F));
        }
    }
    return out;
}

}  // namespace appupdate::winhelper
