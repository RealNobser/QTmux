# QTmux — Projektkontext für Claude

> Diese Datei wird zu Beginn jeder Session geladen. Sie fasst Ziel, Architektur,
> Build und Status zusammen, damit der Kontext einen Compact übersteht.

## Was ist QTmux?

Ein **plattformübergreifender Multi-KI-Agenten-Terminal-Manager** auf **Qt 6 / C++20**.
Inspiriert von [cmux](https://cmux.com/de) (Agenten-Handling, vertikale Tabs, Status-Ringe)
und [Tabby](https://tabby.sh/) (SSH/Serial/Telnet, Split-Panes, Plugins).
Zielplattformen: **macOS, Windows, Linux**. **Prio 1: stabile Terminal-Integration.**

Detaillierter Plan: `~/.claude/plans/neue-projekt-idee-eine-qt-version-radiant-wind.md`.

## Festgelegte Architektur-Entscheidungen

- **Terminal-Kern:** `libvterm` (VT-Parser, BSD) + **eigener PTY-Layer** + eigenes Rendering.
- **UI:** Qt Quick / QML (Qt 6), GPU-beschleunigt.
- **Lizenz:** Open Source, Qt unter LGPLv3 (dynamisch gelinkt) — Komponenten BSD/MIT.
- **Build/IDE:** VSCode + CMake + Presets überall; MSVC (Win), Clang (mac), GCC/Clang (Linux).

### Zwei bewusste Abweichungen vom Ursprungsplan
1. **Kein `ptyqt`.** Es nutzt `QProcess::setupChildProcess()`, das in **Qt6 entfernt** wurde →
   baut nicht mit Qt 6.11. Stattdessen **eigener, dependency-freier PTY-Layer**
   (`forkpty` auf Unix, ConPTY auf Windows). Erfüllt das Plan-Ziel robuster.
2. **Rendering via `QQuickPaintedItem`** (GPU-getextured, robust) statt direkt QSGRenderNode.
   GPU-Glyph-Atlas ist die spätere Performance-Stufe.

## Architektur-Schichten

```
QML UI (qml/Main.qml)  — Sidebar, vertikale Tabs, Status-Ringe
   │
TerminalItem (src/terminal/)  — QQuickItem: Rendering + Tastatur/Resize
   │
VtScreen (src/core/)  — libvterm-Wrapper: Screen-State, Farben, Scrollback, Cursor
   │
ITerminalBackend (src/core/)  — Abstraktion: alles, was Bytes liest/schreibt
   ├─ PtyBackend   (lokale Shell/Agenten)  [FERTIG]
   ├─ SshBackend   (libssh2)               [Phase 4]
   ├─ SerialBackend(QtSerialPort)          [Phase 4]
   └─ AppBackend   (Custom-Apps/MacPCAN)   [Phase 5]
   │
Pty (src/core/)  — Pty.h + UnixPty.cpp (forkpty) / WindowsPty.cpp (ConPTY)
```

**Kernidee:** Sidebar, Layout und Rendering funktionieren für *alle* Backend-Typen identisch,
weil alles über `ITerminalBackend` läuft.

## Wichtige Dateien

| Datei | Rolle |
|---|---|
| `src/core/ITerminalBackend.h` | Zentrale Backend-Abstraktion + `BackendState` (für Status-Ringe) |
| `src/core/Pty.h` | Plattformneutrale PTY-Schnittstelle |
| `src/core/UnixPty.cpp` | `forkpty`-Implementierung (macOS/Linux) — FERTIG |
| `src/core/WindowsPty.cpp` | ConPTY-Skeleton (Windows) — **noch zu vervollständigen** |
| `src/core/PtyBackend.{h,cpp}` | Lokale Shell/Agenten über PTY |
| `src/core/VtScreen.{h,cpp}` | libvterm-Wrapper; `Cell` = Qt-freundliche Zelle |
| `src/core/Session.{h,cpp}` | Bündelt Backend + VtScreen; UI spricht nur mit Session |
| `src/viewmodels/SessionModel.{h,cpp}` | QAbstractListModel für die Sidebar; `sessionAt()` bindet an `TerminalItem.session` |
| `src/viewmodels/Theme.{h,cpp}` | QML-Singleton `Theme.*`: Dark/Light-Palette, via QSettings persistiert |
| `src/viewmodels/AppController.{h,cpp}` | QML-Singleton `App.*`: UI-Sprache; Translator-Wechsel in `main.cpp` |
| `src/core/AgentRegistry.{h,cpp}` | Bekannte Agenten-CLIs (claude, codex, gemini, **agy**=AntiGravity, …); `detect()` |
| `i18n/qtmux_{de,en}.ts` | Übersetzungen; via `qt_add_translations` zu `:/i18n/*.qm` kompiliert/eingebettet |
| `src/terminal/TerminalItem.{h,cpp}` | QML-`TerminalItem`; rendert zugewiesene `Session` (besitzt sie nicht) |
| `qml/Main.qml` | App-Shell: datengetriebene Sidebar + Terminal der aktuellen Session |
| `tests/` | QtTest: `tst_pty`, `tst_vtscreen`, `tst_session` (E2E) |

## Build & Test (macOS)

Abhängigkeiten: `brew install qt libvterm libssh2 ninja cmake`

```bash
cmake --preset macos
cmake --build --preset macos
ctest --test-dir build/macos --output-on-failure
open ./build/macos/qtmux.app          # sichtbar starten
QT_QPA_PLATFORM=offscreen ./build/macos/qtmux.app/Contents/MacOS/qtmux   # headless smoketest
```

Konventionen: deutsche Kommentare/Kommunikation; `qtmux_core` bleibt **Gui-frei**
(nur Qt6::Core) → Farben als `quint32` 0xRRGGBB, nicht `QRgb`. Code-Referenzen als
Markdown-Links. Commit-Trailer: `Co-Authored-By: Claude …`.

## Status (Stand: 2026-06-04)

- ✅ **Phase 0** — Gerüst (CMake/Presets/vcpkg, Qt-Quick-Shell, .vscode)
- ✅ **Phase 1** — Terminal-Kern: PTY + libvterm + TerminalItem; 3 Tests grün; läuft auf macOS
- ⬜ **Phase 1 (Windows)** — ConPTY in `WindowsPty.cpp` implementieren & testen
- 🟡 **Phase 2** — Session + SessionModel + datengetriebene Sidebar + Session-Wechsel: FERTIG.
  Offen: **Split-Panes** (SplitView mit mehreren TerminalItems), Reorder
- ✅ **UI-Basis** — Theme `System/Hell/Dunkel` (`Theme.mode`, System folgt OS via
  `QStyleHints::colorScheme`, Ctrl+D), Terminal-Farben folgen dem Theme
  (`TerminalItem.background/foregroundColor` ← `Theme.terminalBg/Fg`), MenuBar mit allen
  Befehlen, i18n DE/EN (`App`-Singleton + `qt_add_translations`, Laufzeit-Umschaltung),
  Agent-Erkennung (`AgentRegistry.detect()`; `agy`→AntiGravity). 5 Tests grün.
  Sessions werden bei Shell-Ende automatisch entfernt (SessionModel) + „×" pro Sidebar-Zeile.
- ✅ **Phase 3 (Agent-Awareness)** — vollständige OSC-Erkennung:
  - `VtScreen` fängt unbekannte OSC via `vterm_screen_set_unrecognised_fallbacks` ab →
    Signale `notify()` (OSC 9 / 777) und `promptMarker(kind,exit)` (OSC 133).
  - `Session`: `Activity`-Zustand (Running/Error/Closed, Sidebar-Ring) aus OSC 133;
    `lastNotification` aus OSC 9/777; `needsAttention` (blau, pulsierend) aus Bell,
    Notification oder Befehls-Ende (OSC 133;D) einer inaktiven Session.
  - `SessionModel`: `attentionRaised(row)` → `window.alert()` (Dock/Taskbar) wenn QTmux
    nicht im Vordergrund. Sidebar zeigt Notification-Text.
  - Shell-Integration: `shell-integration/qtmux.{bash,zsh}` (OSC 133 Marker + `qtmux-notify`).
  - **Wichtige Lektion (Bugfix):** Backend wird NUR vom `unique_ptr` besessen (kein
    `setParent`); stateChanged-Handler nimmt den `BackendState` aus dem Signal-Argument
    (nicht `m_backend->state()`), da das Signal während der Backend-Zerstörung feuert.
  - 9 Tests grün (test_pty/vtscreen/session/agent).
- ⬜ **Phase 3** — Agent-Awareness (OSC 133/9, Status-Ringe, Notifications)
- ⬜ **Phase 4** — SSH (libssh2) + Serial (QtSerialPort) + Connection-Manager
- ⬜ **Phase 5** — Plugin-System (QPluginLoader), MacPCAN-Integration
- ⬜ **Phase 6** — Politur & Distribution (CPack: DMG/MSI/AppImage, CI-Matrix)

## Offene technische Notizen

- Rendering ist aktuell zellweise (ausreichend für Phase 1). Optimierung: Runs gleicher
  Attribute zusammenfassen, dann GPU-Glyph-Atlas.
- Scrollback wird in `VtScreen` gespeichert (`sb_pushline`), aber noch nicht gerendert/gescrollt.
- Font: `QFontDatabase::systemFont(FixedFont)`; offscreen warnt über fehlende Family „Monospace" (harmlos).
