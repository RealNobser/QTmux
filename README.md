# QTmux

Ein plattformübergreifender Multi-KI-Agenten-Terminal-Manager auf Qt-Basis —
inspiriert von [cmux](https://cmux.com/de) (Agenten-Handling, vertikale Tabs, Status-Ringe)
und [Tabby](https://tabby.sh/) (SSH/Serial/Telnet, Split-Panes, Plugins).

Läuft auf **macOS, Windows und Linux**.

## Status

Frühe Entwicklung — siehe [Roadmap](#roadmap).

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

## Roadmap

- [x] Phase 0 — Gerüst (CMake/Qt-Quick-Shell)
- [ ] Phase 1 — Stabiler Terminal-Kern (libvterm + ptyqt + GPU-Rendering)
- [ ] Phase 2 — Mehrere Sessions, Sidebar, Split-Panes
- [ ] Phase 3 — Agent-Awareness (OSC 133/9, Status-Ringe)
- [ ] Phase 4 — SSH & Serial
- [ ] Phase 5 — Plugin-System (MacPCAN-Integration)
- [ ] Phase 6 — Politur & Distribution (CPack, CI)
