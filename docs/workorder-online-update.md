# Workorder: Online-Update-Epic — Anteil QTmux

> Auftraggeber: Orchestrator-Session (QTmux #23, `_ClaudeWorkspace`), 2026-08-02.
> Masterplan: `/Users/nobser/Projects/_ClaudeWorkspace/online-update-masterplan.md` (maßgeblich bei Widersprüchen).
> Jira: **QTMUX-125** existiert (aktualisieren, nicht neu anlegen), dual. Autarke Auftragsgrundlage.

## Rolle im Epic

QTmux ist reiner Desktop-Update-Konsument. Es teilt KEINEN Code über Submodule mit MacPCAN/RAFTNG → der Update-Core wird **byte-identisch vendiert** (libvterm-Muster). Kein CAN, keine Firmware.

## Arbeitspakete (E im Masterplan)

1. **Vendoring**: `third_party/updater/` = byte-identische Kopie von `../MacPCAN/src/update/` (inkl. `ed25519/`); neues CMake-Target `qtmux_updater` (STATIC, Qt6::Core+Network) — **NICHT** in `qtmux_core` (bleibt Qt6::Core-only, CLAUDE.md-Regel). `tools/check-updater-sync.sh` (SHA-256-Abgleich gegen MacPCAN) + `third_party/updater/UPSTREAM.md` (gepinnter MacPCAN-Commit).
2. **ViewModel**: `src/viewmodels/UpdateViewModel.{h,cpp}` (Q_PROPERTYs busy/updateAvailable/remoteVersion/notes[DE/EN nach `ui/language`]/downloadProgress/state/lastError; Q_INVOKABLEs checkNow/download/skipVersion/launchInstaller). Registrierung als **Context-Property** in `src/app/main.cpp` (CLAUDE.md-Regel: kein `qmlRegisterSingletonInstance` in URI „QTmux"). Startup-Hook dort (Throttle via `UpdatePolicy`).
3. **QML**: `qml/dialogs/UpdateDialog.qml` (Notes, Progress, Buttons, SmartScreen/Gatekeeper-Hinweis); Hilfe-Menü-Action vor `actAbout` (`qml/Main.qml` ~2952) + Command-Palette-Eintrag; Checkbox in `qml/prefs/CatAllgemein.qml`.
4. **Settings**: Keys `update/*` in `src/viewmodels/SettingsIo.cpp` (~Zeile 96) + `tests/tst_settingsio.cpp` erweitern; i18n DE-Quellsprache + `qtmux_en.ts` + `tst_i18n` grün; neuer `tests/tst_updater.cpp` gegen file://-Fixtures.
5. **Opportunistisch**: generiertes `qtmux_version.h` aus `PROJECT_VERSION` (`cmake/Version.h.in`) → ersetzt Hardcodes in `src/app/main.cpp:256` + `src/server/McpServer.cpp:413`; übrige Bump-Stellen bleiben (CLAUDE.md:337).

## Abhängigkeiten (WICHTIG)

| Richtung | Partner | Gegenstand |
|---|---|---|
| ⬅ EINGEHEND (BLOCKER) | **MacPCAN** (`../MacPCAN`, Session #12) | Der zu vendierende `src/update/`-Core muss existieren + gepusht sein. **E erst starten, wenn MacPCANUpdater fertig ist.** Danach byte-identisch kopieren; bei künftigen Core-Änderungen re-vendieren (Sync-Skript) |
| ⬅ EINGEHEND | **Owner** | Ed25519-Produktions-Public-Key (kommt über MacPCANs `UpdateKeys.hpp`) — bis dahin Test-Key |
| ➡ AUSGEHEND | — | Einbahnstraße: `third_party/updater/` NIE lokal editieren (sonst Drift); QTmux-spezifische Logik gehört ins ViewModel/QML |

## Infrastruktur-Fakten

- Update-Basis-URL: **`https://nobser.de/updates/qtmux/manifest.json`** (statisch, T-Online). Assets liegen zusätzlich auf GitHub Releases (`RealNobser/QTmux`) — Manifest kann auf beides zeigen (Owner-Entscheidung bei Umsetzung; Default T-Online-Webspace).
- Owner-Entscheidungen: Download + geführte Installation (kein Silent-Self-Replace); Check beim Start max. 1×/Tag + manuell, abschaltbar; Downgrade zulassen mit Warnung.
- QTmux-Installer existieren alle (`installer/build-dmg.sh`, `build-msi.ps1`, `build-appimage.sh`) → alle drei OS-Keys von Anfang an belegbar.
- Diese Session (QTmux-Worker) ODER die Orchestrator-Session setzt E um — vor Beginn `list_sessions`/Arbeitsverzeichnis prüfen, um Doppelarbeit zu vermeiden.
