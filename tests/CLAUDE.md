# tests/ — Tests

> Schicht-Datei: Claude Code lädt sie, wenn in `tests/` gearbeitet wird. Die Tabellenzeile stammt
> wörtlich aus „Wichtige Dateien“ der Wurzel-`CLAUDE.md` (ausgelagert 2026-11-19). Die Teststände
> und Zählregeln stehen dort im Abschnitt „Arbeitsstand“, CI-Flakes und blinde Negativprüfungen
> in [.github/CLAUDE.md](../.github/CLAUDE.md), PTY-Tests unter Windows in
> [src/core/CLAUDE.md](../src/core/CLAUDE.md).

| Datei | Rolle |
|---|---|
| `tests/` | **54** ctest-Tests: 47 QtTest-Binaries (**ownerdraft**, **inputpriority**, **agentinbox**, **mcpmessage** — QTMUX-157, pty, vtscreen, linkdetector, **screenlinks**, **terminalselection**, **terminalitem** — das echte Item mit Maus-Ereignissen, ohne Fenster, **filemanagerreveal**, **executabletarget**, **execopendialog** — der echte Dialog in einem Attrappen-Modul, session, sessiongroups, windowmodel, agent, profiles, hotkeys, vault, sftp, plugins, agenteventhub, macpcan, keyencoding, terminalsearch, terminalgrid, settingsio, i18n, shellintegration, gitinfo, projectcommands, promptqueue, updater, updateviewmodel, **updatehardening** — Vertrag V1, QTMUX-159, mcpaccess, proxycredentials, safefileread, restorehistory, pastewrite, mcpfocus, stalldetector, icons, statuscolors, combowidth, **conptyreplay** — echte ConPTY-Bytes, s. Feature-Referenz) + **5 × `I18n.*`** (Anrede-Wächter [tools/i18n/check_address.py](tools/i18n/check_address.py) + vier Positivproben, QTMUX-145 — brauchen Python 3, ohne Python entsteht ein roter ERROR-Test statt eines stillen Fehlens) + `test_doc_duplicates` (reines CMake-Skript) + `test_check_updater_sync` (bash-Skripttest des Vendoring-Wächters, QTMUX-144 — **nicht auf Windows**). `test_i18n` entsteht nur, wenn `qtbase_*.qm` in der Qt-Installation liegt — sonst 53. Zahl per `ctest -N` gegenprüfen, nicht schätzen |

## Grenzen der CI als Beleg

Aus den Teststand-Regeln der Wurzel-`CLAUDE.md`:

🔑 **„CI grün auf allen drei Plattformen" ist KEIN Vollständigkeitsbeleg** (Lektion aus
QTMUX-124): Die CI baut **Release**, ebenso Homebrew-Qt und der Linux-Container —
**Debug-only-Asserts in Qts vorgebauter Bibliothek sieht sie prinzipiell nicht**; der
Windows-**Debug**-Build ist dafür das einzige taugliche Messmittel (`-DQT_FORCE_ASSERTS`
trägt nicht, es wirkt nur in den *Headern*).
