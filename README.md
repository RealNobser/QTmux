# QTmux

Ein plattformübergreifender Multi-KI-Agenten-Terminal-Manager auf Qt-Basis —
inspiriert von [cmux](https://cmux.com/de) (Agenten-Handling, vertikale Tabs, Status-Ringe)
und [Tabby](https://tabby.sh/) (SSH/Serial/Telnet, Split-Panes, Plugins).

Läuft auf **macOS, Windows und Linux**.

## Status

Frühe Entwicklung — siehe [Roadmap](#roadmap).

## Architektur

- **Terminal-Kern:** `libvterm` (VT-Parser) + `ptyqt` (ConPTY/WinPty/forkpty) + eigenes GPU-Rendering.
- **UI:** Qt Quick / QML (Qt 6).
- **Backend-Abstraktion `ITerminalBackend`:** lokale Shell, SSH, Serial und Custom-Apps sind alle Backends.
- **Lizenz:** Open Source; Qt unter LGPLv3 (dynamisch gelinkt).

## Build

Voraussetzungen: CMake ≥ 3.24, Qt 6.6+, ein C++20-Compiler, Ninja.

```bash
# macOS (Qt + Abhängigkeiten via Homebrew)
brew install qt libvterm libssh2 ninja cmake

cmake --preset macos
cmake --build --preset macos
./build/macos/qtmux        # oder: open build/macos/qtmux.app
```

Windows (VSCode + CMake + MSVC) und Linux (GCC/Clang, AppImage) nutzen dieselben CMake-Presets;
Abhängigkeiten dort über vcpkg (`vcpkg.json`).

## Roadmap

- [x] Phase 0 — Gerüst (CMake/Qt-Quick-Shell)
- [ ] Phase 1 — Stabiler Terminal-Kern (libvterm + ptyqt + GPU-Rendering)
- [ ] Phase 2 — Mehrere Sessions, Sidebar, Split-Panes
- [ ] Phase 3 — Agent-Awareness (OSC 133/9, Status-Ringe)
- [ ] Phase 4 — SSH & Serial
- [ ] Phase 5 — Plugin-System (MacPCAN-Integration)
- [ ] Phase 6 — Politur & Distribution (CPack, CI)
