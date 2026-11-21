#pragma once

// appupdate::winhelper — the logic of the Windows update helper
// (update-helper.exe), without Qt and without the Windows API, so it is
// built and tested on every system. The Win32 shell around it lives in
// UpdateHelperMain.cpp and is only built on Windows.
//
// What the helper does (contract V2 §6, Windows): wait until the app is
// really gone (an inherited handle to the app's process, never a pid),
// hash the MSI again right before msiexec (holding the MSI open without
// write sharing breaks msiexec /i itself — measured, exit 1603), run
//   msiexec /i "<msi>" /passive /norestart INSTALLFOLDER="<folder>" /l*v "<log>"
// with the folder the app runs from (an upgrade without it moves a
// custom-folder install to Program Files — measured), write the outcome
// in the result-file format of InstallBackend.hpp and start the app again
// without elevated rights. It never keeps a file of the install folder
// open: under /passive the Restart Manager ends processes that hold files
// there, and the upgrade then fails (measured, exit 1601).
//
// CANONICAL: QTmux vendors src/update/ byte-identically. Standard C++ only.

#include <optional>
#include <string>
#include <vector>

namespace appupdate::winhelper {

struct Args {
    std::wstring msi;            // the verified package (absolute, native)
    std::wstring sha256;         // 64 lowercase hex digits, from the signed manifest
    unsigned long long size = 0; // bytes, from the signed manifest
    std::wstring installFolder;  // folder the app runs from (absolute, no trailing '\')
    std::wstring exe;            // the app to start again, inside installFolder
    std::wstring version;        // the version being installed
    std::wstring result;         // result file (InstallBackend.hpp format)
    std::wstring log;            // msiexec log
    std::wstring attempt;        // the mark of this attempt (32 lowercase hex); goes into every result
    unsigned long long appHandle = 0;  // inherited handle to the app's process
    unsigned long waitMs = 120000;     // how long to wait for the app to end
};

// Command line, all options required except --wait-ms:
//   --msi <path> --sha256 <hex> --size <n> --install-folder <path>
//   --exe <path> --version <v> --result <path> --log <path>
//   --attempt <32 hex> --app-handle <n> [--wait-ms <n>]
// Every value is checked; anything unexpected (unknown option, missing
// value, a second occurrence, an unclean path) refuses the whole call.
// argv[0] (the program) is not part of `args`.
[[nodiscard]] std::optional<Args> parseArgs(const std::vector<std::wstring>& args,
                                            std::wstring* why = nullptr);

// "X:\…" with a drive letter, backslashes only, no '"', no control
// characters, no "\.\" or "\..\" segment, no trailing '\' (except "X:\"),
// not a UNC or device path ("\\…" would reach the network — the family
// rule since the UNC lesson of 2026-11-05), no reserved device name as a
// segment (CON, PRN, AUX, NUL, CONIN$, CONOUT$, COM1-9, LPT1-9, COM/LPT with
// a superscript 1-3, also with an extension).
// parseArgs() refuses a drive root as install folder on top.
[[nodiscard]] bool isCleanWindowsPath(const std::wstring& path);

// `exe` lies directly or deeper inside `folder` (case-insensitive, ASCII).
[[nodiscard]] bool isInsideFolder(const std::wstring& exe, const std::wstring& folder);

// The full command line for CreateProcessW, program included:
//   "<systemDir>\msiexec.exe" /i "<msi>" /passive /norestart
//   INSTALLFOLDER="<folder>" /l*v "<log>"
// Values are clean paths (no '"'), so quoting cannot be broken; a folder
// never ends in '\' (it would escape the closing quote).
[[nodiscard]] std::wstring msiexecCommandLine(const std::wstring& systemDir, const Args& args);

// Result-file fields (format of InstallBackend.hpp: key=value, UTF-8).
struct Outcome {
    std::string result;   // ok | failed | rolled-back | cancelled
    std::string step;     // wait-exit | verify | install | launch
    std::string reason;   // one line of English for "Details"
    std::string version;
    std::string code;     // machine word, may be empty
    // Whether the helper started the app again: started | failed |
    // skipped-elevated; empty = not attempted (key left out of the file).
    // Its own key, so `code` stays one word the flow can compare.
    std::string restart;
    // The mark of the attempt (Args::attempt); written as `attempt=` when set.
    std::string attempt;
};

// msiexec exit code → outcome. 0/3010/1641 ok (3010: restart-required);
// 1602 and 1223 cancelled (user cancelled, declined elevation — the code
// for a declined prompt is from the documentation, NOT measured); 1601
// failed (measured after the Restart Manager ended a holder); 1603 failed;
// 1618 another-install-running; everything else failed with msiexec-<n>.
[[nodiscard]] Outcome outcomeForMsiexec(unsigned long exitCode, const std::string& version);

// The text of the result file. Line breaks inside values become spaces.
[[nodiscard]] std::string serializeOutcome(const Outcome& outcome);

// UTF-8 of a wide string (UTF-16 on Windows, UTF-32 elsewhere).
[[nodiscard]] std::string toUtf8(const std::wstring& s);

}  // namespace appupdate::winhelper
