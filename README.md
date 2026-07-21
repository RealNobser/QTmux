# QTmux

[![CI](https://github.com/RealNobser/QTmux/actions/workflows/ci.yml/badge.svg)](https://github.com/RealNobser/QTmux/actions/workflows/ci.yml)
![Version](https://img.shields.io/badge/version-1.4.0-blue)
![Plattformen](https://img.shields.io/badge/macOS%20%C2%B7%20Windows%20%C2%B7%20Linux-supported-success)

**🇩🇪 [Deutsch](#-deutsch)  ·  🇬🇧 [English](#-english)**

A cross-platform multi-AI-agent terminal manager built on Qt · Ein plattformübergreifender
Multi-KI-Agenten-Terminal-Manager auf Qt-Basis.

---

<a id="-deutsch"></a>
## 🇩🇪 Deutsch

Ein Terminal-Manager für alle, die **mehrere KI-Agenten gleichzeitig** laufen lassen —
mit einem Terminal-Kern, der auch als ganz normales Terminal taugt. Inspiriert von
[cmux](https://cmux.com/de) (Agenten-Handling, vertikale Tabs, Status-Ringe) und
[Tabby](https://tabby.sh/) (SSH/Serial/Telnet, Split-Panes, Plugins).

Läuft auf **macOS, Windows und Linux**.

### Was es kann

**Terminal-Kern** — `libvterm` als VT-Parser, eigenes GPU-Rendering über einen
Glyph-Atlas (Scene-Graph/RHI) mit QPainter-Fallback: Programmier-Ligaturen, True-Color,
Faint/Dim, Farb-Emojis, Scrollback mit Soft-Wrap-fähigem Kopieren, Maus- und
Scrollrad-Reporting an Anwendungen, stufenloser Zoom.

**Sessions & Layout** — datengetriebene Sidebar mit Status-Ringen, beliebig
verschachtelte H/V-Split-Panes, Drag-Reorder für Sidebar *und* Panes,
Command-Palette, Broadcast-Eingabe an alle Sessions, Quake-Modus (globaler Hotkey),
Sitzungen werden über Neustarts hinweg wiederhergestellt.

**KI-Agenten im Blick** — Status-Ringe aus OSC 133 (läuft / wartet / Fehler),
Benachrichtigungen (OSC 9 / 777), Fortschrittsbalken (OSC 9;4), Aufmerksamkeits-Puls bei
Bell. Agenten in verschiedenen Sessions können sich **gegenseitig benachrichtigen**
(„fertig", „Frage", „Fehler") — und ein Agent kann QTmux per **MCP fernsteuern**
(siehe [Agenten steuern](#agenten-steuern-mcp)).

**Verbindungen** — SSH und SFTP über die **System-Clients** im PTY (Auth, `known_hosts`
und Agent-Forwarding funktionieren dadurch einfach), serielle Schnittstellen über
QtSerialPort, Verbindungsprofile mit Login-Scripts und einem verschlüsselten
Secrets-Vault, konfigurierbare Tastenkürzel, importierbare Color-Schemes
(iTerm/Xresources/Ghostty).

**Erweiterbar** — Plugin-SDK mit `QPluginLoader`-Host: Ein Plugin kann ein eigenes
Terminal-Backend beisteuern. Mitgeliefert: **MacPCAN**, das einen CAN-Bus als Terminal
anbindet (gegen echte PCAN-USB-Hardware verifiziert).

**Ohne Fremdabhängigkeiten** — kein vcpkg, kein OpenSSL/libssh2. libvterm liegt im Repo,
alles andere kommt von Qt oder dem System.

### Status

**Funktionsreich und in aktiver Entwicklung.** Die Kernphasen (Terminal-Kern, Sessions &
Layout, Agent-Awareness, SSH/Serial, Plugin-System) sind abgeschlossen; aktuelle Version
**1.4.0** mit Early-Adopter-Installern für **macOS, Windows und Linux**. Siehe [Roadmap](#roadmap).

<a id="agenten-steuern-mcp"></a>
### Agenten steuern (MCP)

QTmux bringt einen eingebetteten **MCP-Server** mit (Model Context Protocol, HTTP/JSON-RPC
auf `127.0.0.1:7345`). Damit kann ein KI-Agent die Anwendung fernsteuern — genau das, was
aus einem Terminal-Manager eine **Arbeitsumgebung für mehrere Agenten** macht: Eine
Steuer-Sitzung verteilt Aufgaben an Worker-Sitzungen, liest deren Bildschirme mit und
schiebt mitten in der Arbeit Präzisierungen nach.

Die 22 Werkzeuge decken ab, was man sonst in der Oberfläche klickt: Sessions auflisten,
anlegen und schließen, Text senden, Bildschirm samt Historie lesen, Panes teilen und
belegen, gespeicherte Verbindungsprofile starten, Design umschalten.

```bash
U=http://127.0.0.1:7345/mcp

# Was läuft gerade?
curl -s -X POST $U -d '{"jsonrpc":"2.0","id":1,"method":"tools/call",
  "params":{"name":"list_sessions","arguments":{}}}'

# Einer Session eine Aufgabe geben
curl -s -X POST $U -d '{"jsonrpc":"2.0","id":2,"method":"tools/call",
  "params":{"name":"send_text","arguments":{"id":8,"text":"Bitte die Tests ausführen"}}}'
```

**Ereignisse statt Abfragen:** Wer nicht dauernd Bildschirme pollen will, abonniert mit
`subscribe_events` und wartet per Long-Poll auf `wait_for_events`. Wichtig zu wissen:
Es kommt nur an, was eine Sitzung **selbst meldet** — QTmux leitet nichts aus
Bildschirminhalt oder Prozesszustand ab. Ein Claude-Code-Worker wird ereignisfähig, indem
sein Stop-Hook [`shell-integration/qtmux-emit.sh`](shell-integration/qtmux-emit.sh)
aufruft. `subscribe_events` sagt einem direkt, ob die beobachteten Quellen überhaupt schon
je etwas gemeldet haben.

**Zweite Instanz zum Testen:** `QTMUX_MCP_PORT` wählt den Port, `QTMUX_PROFILE` trennt die
komplette Einstellungs-Domain — so probiert man etwas aus, ohne einer produktiv
arbeitenden Instanz in die Quere zu kommen:

```bash
QTMUX_PROFILE=test QTMUX_MCP_PORT=7346 ./build/macos/qtmux.app/Contents/MacOS/qtmux
```

**Sicherheitsgrenze:** Der Server lauscht ausschließlich auf `127.0.0.1`. Die Verwaltung
des Secrets-Vaults ist bewusst **nicht** über MCP erreichbar; `connect_profile` löst ein
hinterlegtes Passwort intern auf, ohne es je herauszugeben.

Vollständige Werkzeug-Referenz: **[`docs/MCP.md`](docs/MCP.md)**.

### Architektur

- **Terminal-Kern:** `libvterm` (VT-Parser, **mitgeliefert** unter `third_party/libvterm/`) +
  eigener, dependency-freier PTY-Layer (`forkpty` auf Unix, **ConPTY** auf Windows) + eigenes Rendering.
- **UI:** Qt Quick / QML (Qt 6).
- **Backend-Abstraktion `ITerminalBackend`:** lokale Shell, SSH, Serial und Plugin-Backends
  sind austauschbar — Sidebar, Layout und Rendering funktionieren für alle gleich.
- **Lizenz:** Open Source; Qt unter LGPLv3 (dynamisch gelinkt); libvterm BSD.

### Build

Voraussetzungen: CMake ≥ 3.24, Qt 6.6+ (inkl. **SerialPort**-Modul), ein C++20-Compiler, Ninja.
libvterm ist im Repo enthalten — **kein vcpkg/brew für libvterm nötig**.

```bash
# macOS (Qt + Ninja via Homebrew)
brew install qt ninja cmake

cmake --preset macos
cmake --build --preset macos
ctest --test-dir build/macos --output-on-failure
open build/macos/qtmux.app
```

Linux (GCC/Clang) nutzt denselben Preset-Mechanismus mit System-Qt.

#### Windows (MSVC)

Qt über den offiziellen Online-Installer; das Add-on **„Qt Serial Port"** muss mit
ausgewählt sein (der Installer macht es opt-in). Den Preset-Pfad ggf. an die eigene
Qt-Version anpassen (`CMakePresets.json` → `CMAKE_PREFIX_PATH`). Build in einer
Developer-Shell (`vcvars64`), damit MSVC/Ninja im `PATH` sind:

```bat
cmake --preset windows
cmake --build --preset windows
ctest --test-dir build\windows --output-on-failure
.\build\windows\qtmux.exe
```

`windeployqt` läuft automatisch als Post-Build-Schritt und legt die Qt-DLLs,
QML-Module und Plugins neben `qtmux.exe` — die App startet also ohne manuell
gesetzten `PATH`. (Die Test-Exes brauchen die Qt-`bin` im `PATH`.)

### Installer

Alle drei Installer sind hand-gerollt (bewusst kein CPack) und **unsigniert** —
Early-Adopter-Stand.

| Plattform | Befehl | Ergebnis |
|---|---|---|
| macOS | [`installer/build-dmg.sh`](installer/build-dmg.sh) `1.4.0` | `dist/QTmux-1.4.0-macos.dmg` |
| Windows | [`installer\build-msi.ps1`](installer/build-msi.ps1) `-Version 1.4.0` | `dist\QTmux-1.4.0-win64.msi` (+ portables ZIP) |
| Linux | [`installer/build-appimage.sh`](installer/build-appimage.sh) `1.4.0` | `dist/QTmux-1.4.0-x86_64.AppImage` |

**macOS (DMG):** Release-Build → `macdeployqt` → Ad-hoc-Signatur → `hdiutil`. Nicht
notarisiert, daher beim ersten Start Rechtsklick → „Öffnen" bzw.
`xattr -dr com.apple.quarantine /Applications/QTmux.app`. Läuft aus dem Zielverzeichnis
bereits eine Instanz, lenkt `QTMUX_BUILD_DIR` den Build um.

**Windows (MSI):** braucht die **freie** WiX-CLI als dotnet-Tool:

```powershell
dotnet tool install --global wix --version 5.0.2   # v6/v7 verlangen die OSMF-Fee
wix extension add -g WixToolset.UI.wixext/5.0.2     # optional (Assistent-UI)
```

WiX-Quelle: [`installer/QTmux.wxs`](installer/QTmux.wxs) (Installation nach
`Programme\QTmux` + Startmenü-Verknüpfung; Deinstallation über „Apps & Features").
Da unsigniert, warnt SmartScreen beim ersten Start („Weitere Informationen → Trotzdem ausführen").

**Linux (AppImage):** Standard-Qt-Toolchain `linuxdeploy` + `linuxdeploy-plugin-qt`.
Die CI baut das AppImage bei jedem Push und stellt es als Artefakt `QTmux-AppImage` bereit.

### Roadmap

Die Kernphasen (0–5) sind abgeschlossen; Phase 6 (Distribution) ist weit fortgeschritten.

- [x] **Phase 0 — Gerüst** — CMake/Presets, Qt-Quick-Shell; libvterm mitgeliefert
- [x] **Phase 1 — Terminal-Kern** — libvterm + **eigener, dependency-freier PTY-Layer**
  (`forkpty` / **ConPTY**) + **GPU-Glyph-Atlas** (Scene-Graph/RHI, mit QPainter-Fallback).
  Scrollback, Programmier-Ligaturen, True-Color/Faint, Zoom, Copy/Paste, Maus-/Scrollrad-Reporting
- [x] **Phase 2 — Sessions & Layout** — datengetriebene Sidebar, verschachtelte H/V-Split-Panes,
  Drag-Reorder (Sidebar + Panes), Command-Palette, Einstellungen, Broadcast-Input, Quake-Modus
- [x] **Phase 3 — Agent-Awareness** — OSC 133 / 9 / 9;4 (Status-Ringe, Notifications,
  Fortschrittsanzeige), Inter-Agenten-Benachrichtigung + eingebettete **MCP**-Steuerschnittstelle
- [x] **Phase 4 — SSH & Serial** — System-`ssh`/`sftp` im PTY, QtSerialPort, Connection-Manager
  (Profile), Login-Scripts, verschlüsselter Secrets-Vault, konfigurierbare Hotkeys, Color-Schemes
- [x] **Phase 5 — Plugin-System** — SDK + `QPluginLoader`-Host; **MacPCAN** als erstes echtes
  Plugin (CAN-Bus als Terminal-Backend, gegen echte PCAN-USB-Hardware verifiziert)
- [ ] **Phase 6 — Politur & Distribution** *(in Arbeit)* — Installer für **alle drei Plattformen**
  (DMG / MSI / AppImage) + CI-Matrix (macOS/Windows/Linux) ✅; **offen:** Signierung/Notarisierung

---

<a id="-english"></a>
## 🇬🇧 English

A terminal manager for anyone running **several AI agents at once** — with a terminal core
solid enough to be your everyday terminal. Inspired by [cmux](https://cmux.com/de) (agent
handling, vertical tabs, status rings) and [Tabby](https://tabby.sh/) (SSH/serial/telnet,
split panes, plugins).

Runs on **macOS, Windows and Linux**.

### What it does

**Terminal core** — `libvterm` as the VT parser plus custom GPU rendering via a glyph atlas
(scene graph / RHI) with a QPainter fallback: programming ligatures, true color, faint/dim,
color emoji, scrollback with soft-wrap-aware copying, mouse and wheel reporting to
applications, smooth zoom.

**Sessions & layout** — data-driven sidebar with status rings, arbitrarily nested H/V split
panes, drag-reorder for both sidebar *and* panes, command palette, broadcast input to all
sessions, Quake mode (global hotkey), and sessions restored across restarts.

**Agents at a glance** — status rings from OSC 133 (running / waiting / error),
notifications (OSC 9 / 777), progress bars (OSC 9;4), attention pulse on bell. Agents in
different sessions can **notify each other** ("done", "question", "error") — and an agent
can **drive QTmux over MCP** (see [Driving agents](#driving-agents-mcp)).

**Connections** — SSH and SFTP through the **system clients** inside a PTY (so auth,
`known_hosts` and agent forwarding simply work), serial ports via QtSerialPort, connection
profiles with login scripts and an encrypted secrets vault, configurable shortcuts, and
importable color schemes (iTerm/Xresources/Ghostty).

**Extensible** — a plugin SDK with a `QPluginLoader` host: a plugin can contribute its own
terminal backend. Shipped example: **MacPCAN**, which attaches a CAN bus as a terminal
(verified against real PCAN-USB hardware).

**No third-party dependencies** — no vcpkg, no OpenSSL/libssh2. libvterm lives in the repo,
everything else comes from Qt or the system.

### Status

**Feature-rich and under active development.** The core phases (terminal core, sessions &
layout, agent awareness, SSH/serial, plugin system) are complete; the current version is
**1.4.0** with early-adopter installers for **macOS, Windows and Linux**. See the [Roadmap](#roadmap-1).

<a id="driving-agents-mcp"></a>
### Driving agents (MCP)

QTmux embeds an **MCP server** (Model Context Protocol, HTTP/JSON-RPC on
`127.0.0.1:7345`), letting an AI agent drive the application. That is what turns a terminal
manager into a **workspace for multiple agents**: one controller session hands out tasks to
worker sessions, reads their screens, and refines instructions mid-flight.

Its 22 tools cover what you would otherwise click in the UI: list, create and close
sessions, send text, read the screen including history, split and populate panes, launch
saved connection profiles, switch the theme.

```bash
U=http://127.0.0.1:7345/mcp

# What is running?
curl -s -X POST $U -d '{"jsonrpc":"2.0","id":1,"method":"tools/call",
  "params":{"name":"list_sessions","arguments":{}}}'

# Hand a task to a session
curl -s -X POST $U -d '{"jsonrpc":"2.0","id":2,"method":"tools/call",
  "params":{"name":"send_text","arguments":{"id":8,"text":"Please run the tests"}}}'
```

**Events instead of polling:** rather than polling screens, subscribe via
`subscribe_events` and wait with the `wait_for_events` long poll. One thing to know: only
what a session **reports itself** ever arrives — QTmux infers nothing from screen content
or process state. A Claude Code worker becomes event-capable by pointing its Stop hook at
[`shell-integration/qtmux-emit.sh`](shell-integration/qtmux-emit.sh). `subscribe_events`
tells you upfront whether the sources you are watching have ever reported anything.

**A second instance for testing:** `QTMUX_MCP_PORT` picks the port and `QTMUX_PROFILE`
separates the entire settings domain, so you can experiment without disturbing an instance
someone is working in:

```bash
QTMUX_PROFILE=test QTMUX_MCP_PORT=7346 ./build/macos/qtmux.app/Contents/MacOS/qtmux
```

**Security boundary:** the server binds to `127.0.0.1` only. Managing the secrets vault is
deliberately **not** exposed over MCP; `connect_profile` resolves a stored password
internally and never hands it out.

Full tool reference: **[`docs/MCP.md`](docs/MCP.md)**.

### Architecture

- **Terminal core:** `libvterm` (VT parser, **vendored** under `third_party/libvterm/`) +
  a custom, dependency-free PTY layer (`forkpty` on Unix, **ConPTY** on Windows) + custom rendering.
- **UI:** Qt Quick / QML (Qt 6).
- **`ITerminalBackend` abstraction:** local shell, SSH, serial and plugin backends are
  interchangeable — sidebar, layout and rendering work the same for all of them.
- **License:** open source; Qt under LGPLv3 (dynamically linked); libvterm BSD.

### Building

Requirements: CMake ≥ 3.24, Qt 6.6+ (incl. the **SerialPort** module), a C++20 compiler, Ninja.
libvterm is bundled in the repo — **no vcpkg/brew needed for libvterm**.

```bash
# macOS (Qt + Ninja via Homebrew)
brew install qt ninja cmake

cmake --preset macos
cmake --build --preset macos
ctest --test-dir build/macos --output-on-failure
open build/macos/qtmux.app
```

Linux (GCC/Clang) uses the same preset mechanism with the system Qt.

#### Windows (MSVC)

Install Qt via the official online installer; the **"Qt Serial Port"** add-on must be
selected (the installer makes it opt-in). Adjust the preset path to your Qt version if
needed (`CMakePresets.json` → `CMAKE_PREFIX_PATH`). Build from a developer shell
(`vcvars64`) so MSVC/Ninja are on the `PATH`:

```bat
cmake --preset windows
cmake --build --preset windows
ctest --test-dir build\windows --output-on-failure
.\build\windows\qtmux.exe
```

`windeployqt` runs automatically as a post-build step and places the Qt DLLs, QML modules
and plugins next to `qtmux.exe` — so the app starts without a manually set `PATH`. (The test
executables do need Qt's `bin` on the `PATH`.)

### Installers

All three installers are hand-rolled (deliberately no CPack) and **unsigned** —
early-adopter state.

| Platform | Command | Output |
|---|---|---|
| macOS | [`installer/build-dmg.sh`](installer/build-dmg.sh) `1.4.0` | `dist/QTmux-1.4.0-macos.dmg` |
| Windows | [`installer\build-msi.ps1`](installer/build-msi.ps1) `-Version 1.4.0` | `dist\QTmux-1.4.0-win64.msi` (+ portable ZIP) |
| Linux | [`installer/build-appimage.sh`](installer/build-appimage.sh) `1.4.0` | `dist/QTmux-1.4.0-x86_64.AppImage` |

**macOS (DMG):** release build → `macdeployqt` → ad-hoc signing → `hdiutil`. Not notarized,
so on first launch use right-click → "Open", or
`xattr -dr com.apple.quarantine /Applications/QTmux.app`. If an instance is already running
from the target build directory, `QTMUX_BUILD_DIR` redirects the build.

**Windows (MSI):** requires the **free** WiX CLI as a dotnet tool:

```powershell
dotnet tool install --global wix --version 5.0.2   # v6/v7 require the OSMF fee
wix extension add -g WixToolset.UI.wixext/5.0.2     # optional (wizard UI)
```

WiX source: [`installer/QTmux.wxs`](installer/QTmux.wxs) (installs to `Program Files\QTmux`
+ Start-menu shortcut; uninstall via "Apps & Features"). Being unsigned, SmartScreen warns
on first launch ("More info → Run anyway").

**Linux (AppImage):** the standard Qt toolchain `linuxdeploy` + `linuxdeploy-plugin-qt`.
CI builds the AppImage on every push and publishes it as the `QTmux-AppImage` artifact.

### Roadmap

The core phases (0–5) are complete; phase 6 (distribution) is well advanced.

- [x] **Phase 0 — Scaffold** — CMake/presets, Qt Quick shell; libvterm vendored
- [x] **Phase 1 — Terminal core** — libvterm + **custom, dependency-free PTY layer**
  (`forkpty` / **ConPTY**) + **GPU glyph atlas** (scene graph / RHI, with a QPainter fallback).
  Scrollback, programming ligatures, true-color/faint, zoom, copy/paste, mouse/wheel reporting
- [x] **Phase 2 — Sessions & layout** — data-driven sidebar, nested H/V split panes,
  drag-reorder (sidebar + panes), command palette, settings, broadcast input, Quake mode
- [x] **Phase 3 — Agent awareness** — OSC 133 / 9 / 9;4 (status rings, notifications,
  progress display), inter-agent notification + an embedded **MCP** control interface
- [x] **Phase 4 — SSH & serial** — system `ssh`/`sftp` in a PTY, QtSerialPort, connection manager
  (profiles), login scripts, encrypted secrets vault, configurable hotkeys, color schemes
- [x] **Phase 5 — Plugin system** — SDK + `QPluginLoader` host; **MacPCAN** as the first real
  plugin (CAN bus as a terminal backend, verified against real PCAN-USB hardware)
- [ ] **Phase 6 — Polish & distribution** *(in progress)* — installers for **all three platforms**
  (DMG / MSI / AppImage) + CI matrix (macOS/Windows/Linux) ✅; **open:** signing/notarization
