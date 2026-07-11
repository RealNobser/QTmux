# QTmux

Ein plattformübergreifender Multi-KI-Agenten-Terminal-Manager auf Qt-Basis —
inspiriert von [cmux](https://cmux.com/de) (Agenten-Handling, vertikale Tabs, Status-Ringe)
und [Tabby](https://tabby.sh/) (SSH/Serial/Telnet, Split-Panes, Plugins).

Läuft auf **macOS, Windows und Linux**.

## Status

**Funktionsreich und in aktiver Entwicklung.** Die Kernphasen (Terminal-Kern, Sessions &
Layout, Agent-Awareness, SSH/Serial, Plugin-System) sind abgeschlossen; aktuelle Version
**1.3.0** mit Early-Adopter-Installern für **macOS, Windows und Linux**. Siehe [Roadmap](#roadmap).

## Architektur

- **Terminal-Kern:** `libvterm` (VT-Parser, **mitgeliefert** unter `third_party/libvterm/`) +
  eigener, dependency-freier PTY-Layer (`forkpty` auf Unix, **ConPTY** auf Windows) + eigenes Rendering.
- **UI:** Qt Quick / QML (Qt 6).
- **Backend-Abstraktion `ITerminalBackend`:** lokale Shell, SSH, Serial und Custom-Apps sind alle Backends.
- **Lizenz:** Open Source; Qt unter LGPLv3 (dynamisch gelinkt); libvterm BSD.

## Build

Voraussetzungen: CMake ≥ 3.24, Qt 6.6+ (inkl. **SerialPort**-Modul), ein C++20-Compiler, Ninja.
libvterm ist im Repo enthalten — **kein vcpkg/brew für libvterm nötig**.

```bash
# macOS (Qt + Ninja via Homebrew)
brew install qt ninja cmake

cmake --preset macos
cmake --build --preset macos
./build/macos/qtmux        # oder: open build/macos/qtmux.app
```

### Windows (MSVC)

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

### Windows-Installer (MSI)

Ein **unsignierter** MSI-Installer (für Early Adopters) entsteht reproduzierbar über
[`installer/build-msi.ps1`](installer/build-msi.ps1) (Release-Build → `windeployqt` →
WiX). Voraussetzung ist die **freie** WiX-CLI als dotnet-Tool:

```powershell
dotnet tool install --global wix --version 5.0.2   # v6/v7 verlangen die OSMF-Fee
wix extension add -g WixToolset.UI.wixext/5.0.2     # optional (Assistent-UI)

powershell -ExecutionPolicy Bypass -File installer\build-msi.ps1
# -> dist\QTmux-1.3.0-win64.msi (selbst-enthaltend, inkl. Qt + VC-Runtime)
```

WiX-Quelle: [`installer/QTmux.wxs`](installer/QTmux.wxs) (Installation nach
`Programme\QTmux` + Startmenü-Verknüpfung; Deinstallation über „Apps & Features").
Alternativ liegt ein **portables ZIP** der gleichen Laufzeit bei. Da unsigniert,
warnt SmartScreen beim ersten Start („Weitere Informationen → Trotzdem ausführen").

### macOS-Installer (DMG)

Ein self-contained **DMG** entsteht über [`installer/build-dmg.sh`](installer/build-dmg.sh)
(Release-Build → `macdeployqt` → Ad-hoc-Signatur → `hdiutil`):

```bash
installer/build-dmg.sh 1.3.0          # -> dist/QTmux-1.3.0-macos.dmg
```

Nicht notarisiert (Early-Adopter) → beim ersten Start Rechtsklick → „Öffnen" bzw.
`xattr -dr com.apple.quarantine /Applications/QTmux.app`.

### Linux-Installer (AppImage)

Ein **AppImage** entsteht über [`installer/build-appimage.sh`](installer/build-appimage.sh)
(Standard-Qt-Toolchain `linuxdeploy` + `linuxdeploy-plugin-qt`):

```bash
installer/build-appimage.sh 1.3.0     # -> dist/QTmux-1.3.0-x86_64.AppImage
chmod +x dist/QTmux-1.3.0-x86_64.AppImage && ./dist/QTmux-1.3.0-x86_64.AppImage
```

Die CI-Matrix baut das AppImage bei jedem Push und stellt es als Artefakt `QTmux-AppImage` bereit.

## Roadmap

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
