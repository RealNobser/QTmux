# QTmux

**🇩🇪 [Deutsch](#-deutsch)  ·  🇬🇧 [English](#-english)**

A cross-platform multi-AI-agent terminal manager built on Qt · Ein plattformübergreifender
Multi-KI-Agenten-Terminal-Manager auf Qt-Basis.

---

<a id="-deutsch"></a>
## 🇩🇪 Deutsch

Ein plattformübergreifender Multi-KI-Agenten-Terminal-Manager auf Qt-Basis —
inspiriert von [cmux](https://cmux.com/de) (Agenten-Handling, vertikale Tabs, Status-Ringe)
und [Tabby](https://tabby.sh/) (SSH/Serial/Telnet, Split-Panes, Plugins).

Läuft auf **macOS, Windows und Linux**.

### Status

**Funktionsreich und in aktiver Entwicklung.** Die Kernphasen (Terminal-Kern, Sessions &
Layout, Agent-Awareness, SSH/Serial, Plugin-System) sind abgeschlossen; aktuelle Version
**1.4.0** mit Early-Adopter-Installern für **macOS, Windows und Linux**. Siehe [Roadmap](#roadmap).

### Architektur

- **Terminal-Kern:** `libvterm` (VT-Parser, **mitgeliefert** unter `third_party/libvterm/`) +
  eigener, dependency-freier PTY-Layer (`forkpty` auf Unix, **ConPTY** auf Windows) + eigenes Rendering.
- **UI:** Qt Quick / QML (Qt 6).
- **Backend-Abstraktion `ITerminalBackend`:** lokale Shell, SSH, Serial und Custom-Apps sind alle Backends.
- **Lizenz:** Open Source; Qt unter LGPLv3 (dynamisch gelinkt); libvterm BSD.

### Build

Voraussetzungen: CMake ≥ 3.24, Qt 6.6+ (inkl. **SerialPort**-Modul), ein C++20-Compiler, Ninja.
libvterm ist im Repo enthalten — **kein vcpkg/brew für libvterm nötig**.

```bash
# macOS (Qt + Ninja via Homebrew)
brew install qt ninja cmake

cmake --preset macos
cmake --build --preset macos
./build/macos/qtmux        # oder: open build/macos/qtmux.app
```

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

Linux (GCC/Clang) nutzt denselben Preset-Mechanismus mit System-Qt.

#### Windows-Installer (MSI)

Ein **unsignierter** MSI-Installer (für Early Adopters) entsteht reproduzierbar über
[`installer/build-msi.ps1`](installer/build-msi.ps1) (Release-Build → `windeployqt` →
WiX). Voraussetzung ist die **freie** WiX-CLI als dotnet-Tool:

```powershell
dotnet tool install --global wix --version 5.0.2   # v6/v7 verlangen die OSMF-Fee
wix extension add -g WixToolset.UI.wixext/5.0.2     # optional (Assistent-UI)

powershell -ExecutionPolicy Bypass -File installer\build-msi.ps1
# -> dist\QTmux-1.4.0-win64.msi (selbst-enthaltend, inkl. Qt + VC-Runtime)
```

WiX-Quelle: [`installer/QTmux.wxs`](installer/QTmux.wxs) (Installation nach
`Programme\QTmux` + Startmenü-Verknüpfung; Deinstallation über „Apps & Features").
Alternativ liegt ein **portables ZIP** der gleichen Laufzeit bei. Da unsigniert,
warnt SmartScreen beim ersten Start („Weitere Informationen → Trotzdem ausführen").

#### macOS-Installer (DMG)

Ein self-contained **DMG** entsteht über [`installer/build-dmg.sh`](installer/build-dmg.sh)
(Release-Build → `macdeployqt` → Ad-hoc-Signatur → `hdiutil`):

```bash
installer/build-dmg.sh 1.4.0          # -> dist/QTmux-1.4.0-macos.dmg
```

Nicht notarisiert (Early-Adopter) → beim ersten Start Rechtsklick → „Öffnen" bzw.
`xattr -dr com.apple.quarantine /Applications/QTmux.app`.

#### Linux-Installer (AppImage)

Ein **AppImage** entsteht über [`installer/build-appimage.sh`](installer/build-appimage.sh)
(Standard-Qt-Toolchain `linuxdeploy` + `linuxdeploy-plugin-qt`):

```bash
installer/build-appimage.sh 1.4.0     # -> dist/QTmux-1.4.0-x86_64.AppImage
chmod +x dist/QTmux-1.4.0-x86_64.AppImage && ./dist/QTmux-1.4.0-x86_64.AppImage
```

Die CI-Matrix baut das AppImage bei jedem Push und stellt es als Artefakt `QTmux-AppImage` bereit.

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

A cross-platform multi-AI-agent terminal manager built on Qt — inspired by
[cmux](https://cmux.com/de) (agent handling, vertical tabs, status rings) and
[Tabby](https://tabby.sh/) (SSH/serial/telnet, split panes, plugins).

Runs on **macOS, Windows and Linux**.

### Status

**Feature-rich and under active development.** The core phases (terminal core, sessions &
layout, agent awareness, SSH/serial, plugin system) are complete; the current version is
**1.4.0** with early-adopter installers for **macOS, Windows and Linux**. See the [Roadmap](#roadmap-1).

### Architecture

- **Terminal core:** `libvterm` (VT parser, **vendored** under `third_party/libvterm/`) +
  a custom, dependency-free PTY layer (`forkpty` on Unix, **ConPTY** on Windows) + custom rendering.
- **UI:** Qt Quick / QML (Qt 6).
- **`ITerminalBackend` abstraction:** local shell, SSH, serial and custom apps are all backends.
- **License:** open source; Qt under LGPLv3 (dynamically linked); libvterm BSD.

### Build

Requirements: CMake ≥ 3.24, Qt 6.6+ (incl. the **SerialPort** module), a C++20 compiler, Ninja.
libvterm is bundled in the repo — **no vcpkg/brew needed for libvterm**.

```bash
# macOS (Qt + Ninja via Homebrew)
brew install qt ninja cmake

cmake --preset macos
cmake --build --preset macos
./build/macos/qtmux        # or: open build/macos/qtmux.app
```

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

Linux (GCC/Clang) uses the same preset mechanism with the system Qt.

#### Windows installer (MSI)

An **unsigned** MSI installer (for early adopters) is produced reproducibly via
[`installer/build-msi.ps1`](installer/build-msi.ps1) (release build → `windeployqt` →
WiX). It requires the **free** WiX CLI as a dotnet tool:

```powershell
dotnet tool install --global wix --version 5.0.2   # v6/v7 require the OSMF fee
wix extension add -g WixToolset.UI.wixext/5.0.2     # optional (wizard UI)

powershell -ExecutionPolicy Bypass -File installer\build-msi.ps1
# -> dist\QTmux-1.4.0-win64.msi (self-contained, incl. Qt + VC runtime)
```

WiX source: [`installer/QTmux.wxs`](installer/QTmux.wxs) (installs to `Program Files\QTmux`
+ Start-menu shortcut; uninstall via "Apps & Features"). A **portable ZIP** of the same
runtime is provided as an alternative. Being unsigned, SmartScreen warns on first launch
("More info → Run anyway").

#### macOS installer (DMG)

A self-contained **DMG** is produced via [`installer/build-dmg.sh`](installer/build-dmg.sh)
(release build → `macdeployqt` → ad-hoc signing → `hdiutil`):

```bash
installer/build-dmg.sh 1.4.0          # -> dist/QTmux-1.4.0-macos.dmg
```

Not notarized (early adopter) → on first launch right-click → "Open", or
`xattr -dr com.apple.quarantine /Applications/QTmux.app`.

#### Linux installer (AppImage)

An **AppImage** is produced via [`installer/build-appimage.sh`](installer/build-appimage.sh)
(the standard Qt toolchain `linuxdeploy` + `linuxdeploy-plugin-qt`):

```bash
installer/build-appimage.sh 1.4.0     # -> dist/QTmux-1.4.0-x86_64.AppImage
chmod +x dist/QTmux-1.4.0-x86_64.AppImage && ./dist/QTmux-1.4.0-x86_64.AppImage
```

The CI matrix builds the AppImage on every push and publishes it as the `QTmux-AppImage` artifact.

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
