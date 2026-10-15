#pragma once

// appupdate::quarantineState — does a file carry macOS' download mark?
//
// CANONICAL: RAFTNG consumes this via submodule, QTmux vendors src/update/
// byte-identically. It is built once, here, and depends on nothing outside
// this directory.
//
// Why an update dialog has to ask: Gatekeeper's first-open check ("Apple
// could not verify …") hangs on the extended attribute com.apple.quarantine.
// A browser writes it; the hub downloader does not (measured 2026-10-14,
// docs/HISTORY.md „Update-Dialog: Quarantäne gemessen statt vermutet“ — a
// plain QFile from a process without LSFileQuarantineEnabled, over file://
// AND over https). Until 2026-10-15 that held only for a FRESH name: the
// downloader wrote INTO a file already lying there (WriteOnly|Truncate),
// which kept that file's inode and with it a browser's mark, and it wrote
// through a symlink. Since then it replaces the name with a new file
// (UpdateChecker.cpp, replaceName) — no inherited mark — but the file can
// still be marked by others afterwards, so the dialog keeps asking THIS
// function instead of assuming. A dialog that promises a Gatekeeper block
// on every update is wrong, and since macOS 15 its old remedy („right-click
// › Open") unblocks nothing anyway. The rule (docs/SHARED.md): name the remedy
// („Open Anyway" under System Settings › Privacy & Security) ONLY for a file
// this function measured as Present. The WORDING stays with each product —
// the hub delivers the mechanics, never the sentence.
//
// Four states, not a bool, because three different answers would otherwise
// collapse into one "no":
//   Absent        macOS, the attribute is not there (ENOATTR) — measured no.
//   Present       macOS, the attribute is there.
//   Unreadable    macOS, the question could NOT be answered (missing file,
//                 no permission, a file system that refuses …). Never read
//                 this as Absent: a failed measurement is not a negative one.
//   NotApplicable every other OS — the attribute does not exist there.
//                 (Windows' Mark of the Web is a different mechanism and NOT
//                 measured here.)

#include <QString>

#include <cstdint>
#include <functional>

namespace appupdate {

enum class QuarantineState : std::uint8_t {
    Absent,
    Present,
    Unreadable,
    NotApplicable,
};

// Asks the FILE at `path` (symlinks are followed: `open` follows them too, so
// the target's attribute is the one Gatekeeper sees). Reads only the
// attribute's existence, never its value and never the file's content — no
// size limit is needed, and a FIFO or device does not block. `errorOut`
// (optional) gets a plain-text reason for Unreadable and is cleared
// otherwise.
[[nodiscard]] QuarantineState quarantineState(const QString& path,
                                              QString* errorOut = nullptr);

// Stable lower-case name ("absent" | "present" | "unreadable" |
// "not-applicable") for logs and test output. Not a UI text.
[[nodiscard]] QString quarantineStateName(QuarantineState state);

// For product tests: answer quarantineState() with `probe` instead of the
// file's attributes, so a dialog's binding to the measurement is testable on
// every runner (Present cannot be produced on Windows or Linux). An empty
// function restores the real check. Process-wide, not thread-safe — set it
// from the test's main thread only.
void setQuarantineProbeForTests(
    std::function<QuarantineState(const QString&)> probe);

}  // namespace appupdate
