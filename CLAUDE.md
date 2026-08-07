# QTmux — Projektkontext für Claude

> Diese Datei wird zu Beginn jeder Session geladen — jede Zeile kostet Kontext in **jeder**
> Session. **Pflegeregeln:** (1) nur dauerhaft Gültiges — Architektur, Build/CI, Konventionen,
> teuer erkaufte Lektionen; (2) **kein Status-Changelog** — Verlauf steht in der Git-Historie
> dieser Datei und in Confluence („CLAUDE.md-Archiv" unter der Entwicklerdoku, IDs in
> `CLAUDE.local.md`); (3) **compact-fest**: Was hier nicht steht, ist nach einer
> Kontext-Kompaktierung verloren — offene Fäden gehören darum in **„Arbeitsstand"** unten,
> nicht nur ins Gespräch; (4) je Sachverhalt EINE Stelle, sonst driften die Fassungen
> auseinander (schon passiert: Feature-Referenz beschrieb das Split-Modell, das QTMUX-83
> längst ersetzt hatte).

## Was ist QTmux?

Ein **plattformübergreifender Multi-KI-Agenten-Terminal-Manager** auf **Qt 6 / C++20**.
Inspiriert von [cmux](https://cmux.com/de) (Agenten-Handling, vertikale Tabs, Status-Ringe)
und [Tabby](https://tabby.sh/) (SSH/Serial/Telnet, Split-Panes, Plugins).
Zielplattformen: **macOS, Windows, Linux**. Prio 1: stabile Terminal-Integration.

## Architektur

**Entscheidungen:** Terminal-Kern = `libvterm` (vendored, BSD) + **eigener PTY-Layer**
(`forkpty` Unix / ConPTY Windows — kein `ptyqt`, das ist Qt6-inkompatibel) + eigenes
Rendering (Scene-Graph/GPU-Glyph-Atlas mit `QPainter`-Fallback `gpuRendering=false`).
UI: Qt Quick/QML. Lizenz: **Apache-2.0** (LICENSE/NOTICE), Qt LGPLv3 dynamisch. Build: CMake + Presets,
VSCode; MSVC (Win), Clang (mac), GCC/Clang (Linux). **Keine externen Abhängigkeiten**
(kein vcpkg, kein OpenSSL/libssh2 — System-`ssh`/`sftp`, Pure-Qt-Krypto im Vault).

```
QML UI (qml/Main.qml)  — Sidebar, vertikale Tabs, Status-Ringe, Split-Layout-Baum
   │
TerminalItem (src/terminal/)  — QQuickItem: Rendering + Tastatur/Maus/Resize
   │
VtScreen (src/core/)  — libvterm-Wrapper: Screen-State, Farben, Scrollback, Cursor, OSC
   │
ITerminalBackend (src/core/)  — Abstraktion: alles, was Bytes liest/schreibt
   ├─ PtyBackend   (lokale Shell/Agenten)
   ├─ SshBackend   (System-ssh im PTY; SFTP via System-sftp)
   ├─ SerialBackend(QtSerialPort)
   └─ Plugin-Backends (z. B. MacPCAN-CAN-Bus, via Plugin-SDK)
   │
Pty (src/core/)  — Pty.h + UnixPty.cpp (forkpty) / WindowsPty.cpp (ConPTY)
```

**Kernidee:** Sidebar, Layout und Rendering funktionieren für *alle* Backend-Typen
identisch, weil alles über `ITerminalBackend` läuft.

### Wichtige Dateien

| Datei | Rolle |
|---|---|
| `src/core/ITerminalBackend.h` | Backend-Abstraktion + `BackendState` (Status-Ringe) |
| `src/core/Pty.h` / `UnixPty.cpp` / `WindowsPty.cpp` | PTY-Layer (forkpty/ConPTY) |
| `src/core/PtyBackend.{h,cpp}` | Lokale Shell/Agenten; zerlegt `program` via `splitCommand` |
| `src/core/VtScreen.{h,cpp}` | libvterm-Wrapper; `Cell` Qt-freundlich; OSC-Parser (133/9/777/Maus) |
| `src/core/LinkDetector.{h,cpp}` | Gui-freie Link-Heuristik (URLs + existierende Dateipfade, Scheme-Whitelist) für klickbare Links (QTMUX-39) |
| `src/core/Session.{h,cpp}` | Backend + VtScreen; Activity/Attention/Progress; Login-Script; SSH-Passwort-Auto-Fill |
| `src/viewmodels/SessionModel.{h,cpp}` | QAbstractListModel Sidebar; Persistenz/Restore; CWD-Vererbung |
| `src/viewmodels/Theme.{h,cpp}` | QML-Singleton `Theme.*`; leitet ALLE Chrome-Farben aus dem aktiven Color-Scheme ab |
| `src/viewmodels/AppController.{h,cpp}` | QML-Singleton `App.*`: Sprache, `shortcutText`, `keyChord`, Clipboard |
| `src/viewmodels/SftpClient.{h,cpp}` | SFTP-Browser (treibt System-`sftp` interaktiv im PTY) |
| `src/core/{AgentRegistry,ShellRegistry,ColorScheme,HotkeyRegistry,ConnectionProfile,SecretsVault,AgentEventHub,GlobalHotkey,ProcessInfo,KeyEncoding}.{h,cpp}` | Gui-freie Registries/Helfer (Details: Feature-Referenz) |
| `src/plugins/QTmuxPlugin.h` / `PluginHost.{h,cpp}` | Plugin-SDK (IID `com.qtmux.PluginInterface/1.0`) + Loader |
| `src/server/McpServer.{h,cpp}` + `McpAccess.{h,cpp}` | Eingebetteter MCP-Server (39 Tools; Doku `docs/MCP.md`) + dessen Gui-freie **Zugriffsregeln** (Bind-Adresse, Token-Pflicht, QTMUX-127) |
| `third_party/updater/update/` + `src/viewmodels/UpdateViewModel.{h,cpp}` + `qml/dialogs/UpdateDialog.qml` | Online-Update (QTMUX-125): byte-identisch aus MacPCAN vendierter Kern (`appupdate`, Target `qtmux_updater`) + QTmux-Persistenz/Dialog; Abgleich `tools/check-updater-sync.sh`, Herkunft `third_party/updater/UPSTREAM.md` |
| `src/terminal/TerminalItem.{h,cpp}` / `GlyphAtlas.{h,cpp}` | Rendering (GPU-Atlas + Fallback), Selektion, Copy/Paste, Maus-Reporting |
| `qml/Main.qml` / `qml/SplitNode.qml` | App-Shell + rekursiver Split-Layout-Baum |
| `plugins/echo/`, `plugins/macpcan/` | Demo-Plugin (Kopiervorlage) + CAN-Bus-Plugin |
| `installer/build-{dmg.sh,msi.ps1,appimage.sh}` | Installer aller 3 Plattformen (hand-gerollt, bewusst kein CPack) |
| `tools/vsdev-build.cmd` | Windows-Build in der **VS-2022**-Umgebung (vswhere-begrenzt); von der VSCode-Task genutzt, s. Build-Abschnitt (QTMUX-79) |
| `.qmllint.ini` + `.vscode/settings.json` (+ generierte `.qmlls.ini`) | Editor-Diagnosen für QML: abgeschaltete Kategorien mit Begründung, Ausschluss von `build/`, Importpfade für qmlls (s. QML-Lektionen) |
| `shell-integration/qtmux.{bash,zsh,ps1}`, `qtmux-event.cmd`, `qtmux-emit.{sh,ps1,cmd}`, `qtmux-wait.{sh,ps1,cmd}` | OSC-133-Marker, `qtmux-notify`/`qtmux-event`, Hook-Helfer zum **Senden** (HTTP, QTMUX-30) und zum **Warten** (Hintergrund-Wächter, QTMUX-37). Stecken seit QTMUX-38 als **Ressource im Binary** — `src/core/ShellIntegration.*` schreibt sie per `qtmux --install-shell-integration` heraus |
| `src/core/{GitInfo,ProjectCommands,PromptQueue}.{h,cpp}` | Gui-freie Kerne (QTMUX-58/96/90): Branch aus `.git/HEAD` ohne git-Prozess · Scanner für `.claude/commands`, `.claude/skills`, `.gemini/commands`, `.junie/commands`, `.agents/skills` (+ `filterForAgent`) · FIFO-Warteschlange + `mayDispatchNext`. Alle drei sind angebunden (Kachel, Palette, Session/MCP) |
| `tests/` | **30** ctest-Tests: 29 QtTest-Binaries (pty, vtscreen, linkdetector, session, sessiongroups, windowmodel, agent, profiles, hotkeys, vault, sftp, plugins, agenteventhub, macpcan, keyencoding, terminalsearch, terminalgrid, settingsio, i18n, shellintegration, gitinfo, projectcommands, promptqueue, updater, updateviewmodel, mcpaccess, proxycredentials, safefileread, **restorehistory**) + `test_doc_duplicates` (reines CMake-Skript). `test_i18n` entsteht nur, wenn `qtbase_*.qm` in der Qt-Installation liegt — sonst 29. Zahl per `ctest -N` gegenprüfen, nicht schätzen |

## Build & Test (macOS)

Abhängigkeiten: `brew install qt ninja cmake` (libvterm vendored).

```bash
cmake --preset macos && cmake --build --preset macos
ctest --test-dir build/macos --output-on-failure
open ./build/macos/qtmux.app
QT_QPA_PLATFORM=offscreen ./build/macos/qtmux.app/Contents/MacOS/qtmux   # headless
```

> **⚠️ Läuft eine produktive Instanz aus `build/macos`, dort NICHT hineinbauen** — das
> Überschreiben des Binaries reißt den laufenden Prozess mit (und alle Terminal-Sessions).
> Vorher `lsof -nP -iTCP:7345 -sTCP:LISTEN` prüfen, sonst in ein eigenes Verzeichnis bauen
> (`-B build/macos-test`). **Isolierte Testinstanz** (der Standardweg für jede Verifikation,
> alle Plattformen): `QTMUX_PROFILE=test QTMUX_MCP_PORT=7346` — `QTMUX_PROFILE` trennt die
> ganze QSettings-Domain (sonst überschreibt die Testinstanz beim Beenden die Session-Liste
> der produktiven), `QTMUX_MCP_PORT` den Port (vor der Einstellung `mcp/port`, sonst 7345).

**DMG:** `installer/build-dmg.sh [version]` — baut `macos-release` (oder `QTMUX_BUILD_DIR`,
wenn aus `macos-release` gerade eine Instanz läuft), `macdeployqt -qmldir=qml`
(self-contained inkl. Plugins/PCBUSB), dann **ad-hoc-Re-Signatur** (`codesign --force --deep
--sign -` — macdeployqt schreibt rpaths NACH seiner Signatur um → ungültig; Apple Silicon
startet nur signiert), `hdiutil`-DMG → `dist/QTmux-<ver>-macos.dmg`. Nicht notarisiert
(Early-Adopter): Rechtsklick→Öffnen bzw. `xattr -dr com.apple.quarantine`. macdeployqt-
`ERROR` zu QtVirtualKeyboard/Multimedia/Pdf ist harmlos.

## Build & Test (Linux)

Lokal wie macOS: `cmake --preset linux` + `--build` + `QT_QPA_PLATFORM=offscreen ctest`, sofern
Qt im System liegt. Der Build-Server **rtzsvr02** baut seit 2026-07-31 **im Docker-Container**
(#14s `buildenv.sh`, Qt im `qtcache`-Volume) statt auf dem Host — Aufruf, sudo-Bedarf und die
qtserialport/qtshadertools-Ergänzung stehen in `CLAUDE.local.md`.

**AppImage:** `installer/build-appimage.sh [version]` — `linux-release`-Preset (oder
`QTMUX_BUILD_DIR` von der CI), AppDir (Binary + Plugins nach `usr/bin/plugins`),
`linuxdeploy --plugin qt` (`QML_SOURCES_PATHS`) → `dist/QTmux-<ver>-x86_64.AppImage`.
**CI-Fallen:** Runner ohne FUSE → `APPIMAGE_EXTRACT_AND_RUN=1`; `ARCH=x86_64`;
`qmake` via `QMAKE`/`QT_ROOT_DIR`. Nutzt `installer/qtmux.desktop` + `resources/appicon/`.
AppImage ist **kein CPack-Generator** — bewusst hand-gerollt wie DMG/MSI.

## Build & Test (Windows, MSVC)

VS 2022 (MSVC+CMake+Ninja), Qt `msvc2022_64` **inkl. Add-on „Qt Serial Port"** (sonst
CMake-Fehler). `CMakePresets.json`-`CMAKE_PREFIX_PATH` ggf. anpassen. In `vcvars64`-Shell:

```bat
cmake --preset windows && cmake --build --preset windows
ctest --test-dir build\windows --output-on-failure   :: Qt-bin muss im PATH sein!
```

> **⚠️ Eine laufende Instanz blockiert den Linker — auch auf Windows** (2026-08-03, zweimal
> hineingelaufen): `LNK1168: "plugins\qtmux_echo_plugin.dll" kann nicht zum Schreiben geöffnet
> werden`. Zerstört wird nichts (anders als auf macOS), aber die Meldung nennt die Datei statt
> des Schuldigen. Diagnose und die Regel „danach die **mtime** des Testbinaries prüfen, nicht
> die ctest-Zusammenfassung": [[laufende-instanz-blockiert-linker-windows]].
> 🔑 **Hier konkret:** Die produktive Instanz des Anwenders läuft aus `build\windows` (Debug!) —
> also in ein eigenes Verzeichnis bauen, `build\windows-qt6103` (Qt 6.10.3 = CI-Version) taugt
> dafür. Gilt genauso für die **eigene** isolierte Testinstanz: vor dem Build beenden.
>
> **⚠️ Zwei Visual Studios auf einer Maschine (QTMUX-79).** Neben VS 2022 liegt **VS 18**;
> die CMake-Tools-Erweiterung injiziert immer die **neueste** Dev-Umgebung → `error STL1001`.
> Ursachen, Diagnose-Merkmale und der Wrapper-Weg stehen vollständig in
> [[vs18-neben-vs2022-stl1001]] — hier nur das Projektspezifische:
> - **VS 2022 bleibt Standard** (Qt ist `msvc2022_64`, CI und `build-msi.ps1` ebenso); die
>   Umgebung kommt deterministisch aus **`tools/vsdev-build.cmd`** (vswhere auf `[17.0,18.0)`).
> - **Strg+Umschalt+B / F5** → Task → jenes Skript. Die Task setzt das **aktive** Preset ein
>   (`${command:cmake.activeBuildPresetName}`), sonst baut sie bei gewähltem Release stumm
>   Debug; im Skript **gequotet**, damit ein Anzeigename („Windows (MSVC)") sauber als
>   `No such preset` scheitert statt die Batch-Zeile zu zerlegen.
> - **Build-Knopf/Palette** ruft cmake selbst → braucht den Wrapper
>   [tools/cmake-vsdev.cmd](tools/cmake-vsdev.cmd), eingetragen als `"cmake.cmakePath"` in den
>   **Benutzer**-Einstellungen (nicht ins Repo: `.vscode/settings.json` gilt auch für
>   macOS/Linux). `configureOnOpen`/`configureOnEdit`/`automaticReconfigure` bleiben trotzdem
>   **aus** — `automaticReconfigure` feuert beim **Preset-Wechsel**.
> 🔑 **Qt-Pfad:** Das `windows`-Preset nimmt `$env{QTMUX_QT_PREFIX}` und fällt sonst auf
> **beide** bekannten Installationen zurück — `C:/Qt/6.10.3/msvc2022_64;C:/Qt/6.11.1/msvc2022_64`.
> `CMAKE_PREFIX_PATH` ist eine **Liste**: CMake nimmt den ersten Treffer und überspringt
> nicht existierende Pfade. Damit stimmt derselbe Preset auf beiden Windows-Maschinen — ein
> **einzelner** Wert war zweimal falsch und wurde hin- und hergedreht (Entwicklungsrechner
> `30516935D11` hatte nur 6.11.1, Build-Rechner RTZBLD01 nur 6.10.3, `QTMUX_QT_PREFIX`
> nirgends gesetzt). **6.10.3 steht bewusst vorn**: das ist die Version der CI
> (`env.QT_VERSION`), von RTZBLD01 und damit der ausgelieferten Installer — lokal gegen ein
> *neueres* Qt zu bauen ist die klassische Quelle für „geht hier, bricht in der CI".
> Seit 2026-07-30 ist 6.10.3 auch auf dem Entwicklungsrechner installiert (per `aqtinstall`,
> s. u.); wer gegen 6.11.1 bauen will, setzt `QTMUX_QT_PREFIX`.
> ⚠️ Ein **bestehendes** Build-Dir merkt eine Preset-Änderung nie (Qt liegt im Cache) — die
> Reihenfolge wirkt nur auf **frische** Verzeichnisse. `build/windows` und `build/windows-release`
> stehen deshalb weiter auf 6.11.1, bis sie neu angelegt werden.
> 🔑 **Qt nachinstallieren ohne Qt-Account:** `aqtinstall` mit den CI-Modulen
> (`-m qtserialport qtshadertools`) — Rezept, Proxy-Verhalten und der Pflicht-Vortest
> `aqt list-qt … --arch <ver>` (daran hängt der 6.11-Blocker der CI) stehen in
> [[qt-nachinstallieren-aqtinstall]]. Die CI selbst ist davon unabhängig (eigener
> `cmake`-Aufruf mit `QT_ROOT_DIR`).
> 🔑 Batch-Fallen der `.cmd`-Skripte hier (CRLF-Pflicht, rein ASCII, `%ProgramFiles(x86)%`
> nie in einer `for`-/`if`-Klammer): [[cmd-skript-fallen-windows]]. Deshalb schreibt vswhere
> in eine temporäre Datei.

`windeployqt` läuft als Post-Build. **MSI/ZIP:** `installer/build-msi.ps1 -Version <ver>`
(WiX v5 als dotnet-Tool; nutzt dasselbe `windows-release`-Preset → nur 2 Build-Dirs:
`build/windows` Debug, `build/windows-release` Release). Unsigniert (Early-Adopter).
**One-Click-Release** auf der Windows-Maschine (Zugang: `CLAUDE.local.md`):
`installer/build-release.ps1` (Desktop-Verknüpfung „Build QTmux Installer") holt den
aktuellen Stand und baut MSI + portables ZIP; `-NoFetch` überspringt den Pull.
- ⚠️ **Fehler, die wie ein normaler Lauf aussehen** — beide Male eine falsche Version
  gebaut: `_build.cmd` nutzt `-NoFetch` (baut klaglos einen **alten** Checkout, fiel erst am
  Dateinamen auf), und die Qt-Kit-Auswahl sortierte Versionsordner als **Zeichenkette**
  (`6.8.3` > `6.10.3`, weil `"8" > "1"`) → `Sort-Object { [version]$_.Name }` mit
  `0.0.0`-Fallback. Für solche Fälle ist der **Gegentest Pflicht**: die alte Logik muss
  nachweislich das falsche, die neue das richtige Ergebnis liefern. Und gegenprüfen, dass
  die neue Version wirklich im Paket steckt (nicht nur im Dateinamen): `strings qtmux.exe`
  auf Version **und** ein neues Merkmal.
- ⚠️ **Fernsteuern per SSH:** Windows-OpenSSH beendet Kindprozesse beim Sitzungsende — Build
  in einer *offenen* Sitzung laufen lassen, nicht per `Start-Process` detachen (endet sonst
  stumm mit 0-Byte-Logs). PowerShell dort: `&` ist kein Trenner, `-Filter` nimmt nur EINE
  Zeichenkette, Pfade mit Klammern (`… (x86)…`) **nicht** inline quoten (PowerShell wertet
  `(x86)` als Befehl) → `.cmd` lokal erzeugen und per `scp` übertragen. cmake/ninja/ctest
  liegen unter `Common7\IDE\CommonExtensions\Microsoft\CMake\`.

Vor Windows-Releases: `tests/release-visual-check.ps1` (screenshottet alle Menüs in
beiden Themes + MCP-Smoke — Theming-Regressionen sind unit-test-unsichtbar).

**ConPTY-Lektionen (teuer erkauft):**
- qtmux MUSS **`WIN32_EXECUTABLE`** (GUI-Subsystem) sein — eine Konsolen-App vererbt ihre
  Konsole an die ConPTY-Kindshells → Terminal stumm.
- conhost gibt **kein Pipe-EOF** beim Shell-Selbst-Exit → **Waiter-Thread**
  (`WaitForSingleObject` auf das Prozess-Handle) erkennt das Prozessende.
- terminate-Reihenfolge: `ClosePseudoConsole` **vor** `TerminateProcess` (+ `CancelSynchronousIo`).
- PTY-Tests sind `WIN32_EXECUTABLE` und laufen via `tests/run_detached.ps1` losgelöst
  (sonst erben sie die ctest-Konsole); ohne Qt-bin im PATH → Exit `0xc0000135`.
- VS-Code-Debugger: `launch.json` braucht **`"console": "externalTerminal"`**
  (internalConsole leitet Std-Handles um → ConPTY-Kindshells hängen).
- **PS-5.1-Umlaut-Mojibake** („für"→„fÃ¼r"): conhost re-kodiert Kind-Ausgabe von CP1252 nach
  UTF-8, **bevor** die Bytes QTmux erreichen (per rohen UTF-8-Bytes bewiesen). Keine
  Shell-Einstellung hilft (chcp/OutputEncoding alles getestet); Abhilfe nur PowerShell 7 oder
  die System-Option „UTF-8 weltweit". Kein QTmux-Bug → bewusst **kein** Dekodier-Hack.
- `Pty::currentWorkingDirectory()` Windows via PEB (`NtQueryInformationProcess` +
  `ReadProcessMemory`) — **Funktionstest bestanden** (2026-07-30, QTMUX-2): in `cmd.exe`
  folgt der Wert dem `cd /d` über Laufwerksgrenzen (`…\src\core` → `H:\…\OfficeCompagnion`
  → `C:\Windows\System32`, zweimal mit dem Prompt gegengeprüft).
  🔑 **Aber: PowerShell lässt sich so prinzipiell nicht verfolgen.** `Set-Location` ist ein
  PowerShell-*Provider*-Begriff und ruft **kein** `SetCurrentDirectory` — das Win32-Arbeits-
  verzeichnis des Prozesses bleibt, wo die Shell gestartet ist. Bewiesen in einer Zeile:
  `(Get-Location).Path` = `C:\Windows\System32`, gleichzeitig
  `[System.IO.Directory]::GetCurrentDirectory()` = `D:\…\src\core`. QTmux liest also richtig;
  jedes Werkzeug, das das Prozess-CWD liest, sieht dasselbe. Der einzige saubere Ausweg ist
  **OSC 7** (die Shell meldet ihr Verzeichnis selbst) — **seit QTMUX-108 umgesetzt**, Mechanik
  in der Feature-Referenz. Auf rtzbld01 am echten PowerShell 5.1 belegt: **ohne** die
  Shell-Integration bleibt die Anzeige nach `Set-Location` auf `C:\` stehen, **mit** ihr folgt
  sie (`…\tests` → `…\src\core` → `C:\Windows`). ⚠️ Voraussetzung ist also die gesourcte
  `qtmux.ps1` — ohne sie gilt der Absatz oben unverändert weiter.
  🔑 Testfalle dabei: `cd H:\…` **ohne `/d`** wechselt in `cmd` das Laufwerk nicht — der
  Prompt bleibt stehen, und QTmux meldet korrekt *keine* Änderung. Sah zunächst wie ein
  Fehler aus; der Bildschirminhalt hat es aufgeklärt.
- Debug-Qt asserted „QSGGeometryNode is missing geometry": jedem GeometryNode beim Anlegen
  sofort eine (leere) `QSGGeometry` setzen (Release-Qt prüft das nicht).

## libvterm (vendored + lokaler Patch)

`third_party/libvterm/` (0.3.3, BSD, neovim-Mirror), mitgebaut — `project(... LANGUAGES
C CXX)` ist Pflicht (ohne `C` → leere `vterm.lib` → Linkfehler).

> **⚠️ Lokaler Patch — bei einem libvterm-Update NICHT verlieren** (alle Stellen mit
> `QTMUX:`-Kommentar markiert, `grep -rn "QTMUX:" third_party/libvterm`): **Faint/Dim
> (SGR 2)** wurde additiv ergänzt (Bit `faint` in `VTermScreenCellAttrs` + interne Pens,
> `VTERM_ATTR_FAINT` ans Enum-**Ende** (ABI), SGR 2 + erweitertes SGR 22 in pen.c,
> Durchreichung bis `get_cell`/`attrs_differ`). `VtScreen` → `Cell.faint`;
> `TerminalItem::effectiveFg()` dimmt 45 % Richtung bg. Tests `trueColorRgb`/`faintAttribute`.
>
> **Patch 2 — Emoji-Presentation (Unicode TR#51), `state.c` (QTMUX-97):** Basiszeichen mit
> Text-Default + **VS-16 (U+FE0F)** → Breite **2** (VS-15/U+FE0E umgekehrt → 1). libvterm 0.3.3
> führt U+FE00–FE0F in seiner `combining`-Tabelle (Breite 0) und kennt die Emoji-Form nicht;
> `fullwidth.inc` deckt nur Zeichen mit Emoji-**Default** ab (✅ ❌ 😀 sind drin, **U+26A0 nicht**).
> Betroffen ist also genau die VS-16-Klasse: ⚠️ ©️ ®️ ❤️ ‼️ ✔️ … Test
> `tst_vtscreen::emojiPresentationWidth`.
>
> **Patch 3 — DECSET 1007 (Alternate Scroll), `state.c`/`vterm.h`/`vterm.c` (QTMUX-128):**
> Modus 1007 lief in den `default:`-Zweig von `set_dec_mode` (nur `DEBUG_LOG`) und war damit
> unsichtbar; der **CSI-Fallback sieht ihn nie**, weil `CSI ? Ps h` als Sequenz erkannt ist —
> ein Abfangen im Frontend ist also prinzipiell unmöglich. Neu: `case 1007` →
> `settermprop_bool(VTERM_PROP_ALTSCROLL)`, Prop am **Enum-Ende** (ABI, wie
> `VTERM_ATTR_FAINT`), Typ in `vterm_get_prop_type`, in `vterm_state_set_termprop` nur
> **durchgereicht** (wie TITLE) — der Modus wird nicht im State gehalten, allein das
> Frontend entscheidet. `VtScreen::altScroll()`; Tests `tst_vtscreen::altScrollModeTracked`
> + `altScrollRule` (Gegentest ohne den Patch: genau die zwei parse-abhängigen Fälle fallen,
> die reine Regel bleibt grün).

## CI (GitHub Actions)

`.github/workflows/ci.yml`: Build + headless-Tests (`QT_QPA_PLATFORM=offscreen`) auf
macOS/Windows/Linux; Qt via `jurplel/install-qt-action` (Module **qtserialport** +
**qtshadertools**). Linux-Job baut zusätzlich das AppImage (Artefakt `QTmux-AppImage`).
**Windows-Tests sind seit `db52b41` blockierend** — vorher lief der Schritt mit
`continue-on-error`, weil `test_pty` dort umgebungsbedingt fällt; damit waren aber auch alle
übrigen Tests wirkungslos, eine echte Regression hätte den Job nie rot gemacht. Jetzt wird nur
`test_pty` per `-E` ausgenommen. Node-20-Abkündigung ist abgearbeitet (checkout v5,
upload-artifact v6, `setup-msvc-dev` statt `ilammy`, Ninja kommt vom Image); die beiden
Dritt-Actions (`setup-msvc-dev`, `install-qt-action`) sind auf **Commit-SHAs gepinnt** —
Tags/Branches sind verschiebbar; das Anhebe-Rezept steht als Kommentar in der `ci.yml`.
> ⚠️ **Zweiter beobachteter Flake (2026-08-02): macOS-Build bricht bei
> `qmltyperegistrar` mit `qt6qtmux_metatypes.json:: Failed to parse JSON: 7 illegal
> number`.** Die Datei ist **generiert** — derselbe Commit-Inhalt baut im Folgelauf
> sauber, lokal ist die JSON jederzeit parsebar. Also dieselbe Regel wie unten: nicht
> als Regression lesen, **erst wiederholen**. (Wäre es echt, zeigte es sich lokal
> reproduzierbar an einer moc-JSON.)

> ⚠️ **Sporadisch: `test_doc_duplicates` fällt auf Windows mit `0xc0000142`**
> (STATUS_DLL_INIT_FAILED). Das ist **kein** Prüfbefund — der Prozess (`cmake -P`) startet gar
> nicht erst, die Doku wird nie gelesen. Einmal gesehen (Lauf `30635817273`), im nächsten Lauf
> **derselben** Datei grün. Fällt erst seit der Blockier-Umstellung überhaupt auf. Also: nicht
> als Regression lesen und nicht in der CheckDocDuplicates.cmake suchen — **erst wiederholen**.
> Bleibt es reproduzierbar, ist die Spur Ressourcenerschöpfung durch die davor laufenden
> `run_detached.ps1`-Tests, nicht der Doku-Wächter selbst.

> 🔑 **Wie man einen CI-Flake BEWEIST, statt ihn zu vermuten** (an `3b63f00` durchgespielt,
> dritter Windows-Fehlschlag dieser Art): Ein roter Lauf und ein grüner Folgelauf sind erst
> dann ein Beleg, wenn der Folgecommit **denselben Code** trägt —
> `git diff --name-only <rot>..<gruen>` muss ausschließlich Doku nennen. Bei `3b63f00`→`923e8fc`
> war der Unterschied genau `CLAUDE.md`, und `923e8fc` ist auf allen drei Plattformen grün:
> damit ist der rote Lauf erledigt, ohne ihn zu wiederholen.
> ⚠️ **Auf das Log darf man dabei nicht bauen** — schon einen Tag später antwortete
> `gh run view --job <id> --log` mit `log not found`, während `gh run view <lauf> --json jobs`
> Status und Fehlschritt weiter lieferte. Wer erst das Log sucht, steht ohne Diagnose da; der
> Commit-Vergleich ist unabhängig davon. Und: `gh` braucht das Repo-Verzeichnis oder
> `-R RealNobser/QTmux`, sonst „failed to determine base repo".

> **⚠️ `env.QT_VERSION` (6.10.3) ist bewusst gewählt — nicht blind hochziehen.**
> **Nicht 6.8.x:** dessen CMake-Config verlinkt das aus dem macOS-SDK entfernte
> **AGL-Framework** → `ld: framework 'AGL' not found` (lokal unsichtbar, Homebrew-Qt ist
> AGL-frei); Ausweichen auf `macos-13`-Runner geht nicht (werden nicht mehr zugeteilt).
> **Nicht 6.11/6.12:** aqtinstall kann die **Windows**-Arch-Metadaten nicht abrufen
> (macOS/Linux lösen sie sauber auf — der Blocker ist allein Windows). Der offizielle
> Installer bräuchte **Qt-Account-Secrets**; das Repo ist öffentlich, Fork-PRs bekommen
> keine → der Windows-Job bräche für jeden externen Beitrag.
> 🔑 Die Fehlermeldung wechselt (zuletzt `Failed to download checksum for … Updates.xml`)
> und *sieht* nach Mirror-Flattern aus. Nur der **Gegentest** entlarvt es: 3/3
> Wiederholungen scheitern, während 6.10.3 im selben Lauf durchläuft. Vor jedem Bump
> lokal prüfen: `aqt list-qt <windows|mac|linux> desktop --arch <ver>` (aqtinstall 3.3.x)
> — Fehler = Version unbrauchbar.

## Projekt-Doku: Confluence (DUAL: on-prem + Cloud)

Bei jeder Doku-Änderung **beide** aktualisieren (identischer Storage-Inhalt). Token nur
einlesen, **nie ausgeben/committen**. **Hosts, Space-Keys, Seiten-IDs und Board-ID stehen
nur in `CLAUDE.local.md`** (git-ignoriert, damit das Repo öffentlich sein kann; auf der
Windows-Maschine existiert die Datei nicht → dort sind Doku-/Jira-Pflege nicht möglich).

- **On-prem** Confluence Server, `Credential-Confluence.txt` (**Bearer**, `verify_ssl=false`
  → `curl -k`); **Cloud** Atlassian, `Credential-Atlassian.txt` (**Basic** `email:api_token`),
  Pfade `/wiki/rest/api/content/<id>`. Seitenbaum beidseitig: Home → Benutzerdoku ·
  Entwicklerdoku → Unterseiten.
- **Update:** `GET …?expand=version` → `PUT` mit `version.number+1`, `body.storage` = XHTML.
  **Neu:** `POST` mit `space.key` + `ancestors:[{id:<parent>}]`. Mermaid-Makro heißt on-prem
  `mermaid-macro`, in der Cloud `mermaid-cloud`.
- **⚠️ Entity-Falle:** Cloud-Storage kodiert Umlaute als HTML-Entities (`n&ouml;tig`), on-prem
  als UTF-8 — String-Anker mit Umlauten in der Cloud zusätzlich als Entity-Variante probieren;
  eingefügtes UTF-8 akzeptieren beide.

## Jira (DUAL: on-prem + Cloud)

Beide Projekte Key **QTMUX**, identischer Issue-Satz (Abgleich per Summary), Typ **Task**;
idempotent anlegen (Summaries vorher holen). On-prem `Credential-Jira.txt` (**Bearer**-PAT,
`verify_ssl=false`), API `/rest/api/2/`, description = Klartext, Suche `GET /search?jql=…`.
Cloud **Basic**, API `/rest/api/3/`, **description braucht ADF**
(`{type:doc,version:1,…}`), Suche `POST /search/jql`. Kein Atlassian-MCP (nur Cloud +
interaktives OAuth) — einheitlicher REST-Weg.
**Kanban-Konvention (Anwender-Vorgabe):** bei jedem Fortschritt **dual** weiterschieben —
Arbeitsbeginn → „In Progress" (on-prem 31) / „In Arbeit" (Cloud 21); fertig + verifiziert →
„Done"/„Erledigt" (41) mit Kurzkommentar. Transition-IDs je Issue via
`GET /issue/<key>/transitions` prüfen.

## Konventionen

- **Deutsche** Kommentare/Kommunikation; Code-Referenzen als Markdown-Links;
  Commit-Trailer `Co-Authored-By: Claude …`. Committen/pushen nur auf Auftrag.
- `qtmux_core` bleibt **Gui-frei** (nur Qt6::Core) → Farben als `quint32` 0xRRGGBB.
  Alles, was Qt6::Network braucht, bekommt ein **eigenes** Target (so entstand
  `qtmux_updater`, QTMUX-125).
- **Vendierter Fremdcode wird nie lokal editiert** (`third_party/libvterm` mit
  `QTMUX:`-Markern, `third_party/updater` mit `tools/check-updater-sync.sh`). Beim
  Updater ist die kanonische Quelle MacPCAN: Fehler dort beheben, pushen,
  `--update` nachziehen, Pin in `UPSTREAM.md` setzen.
  Gui-freie Singletons als **Context-Properties** in `main.cpp` registrieren (KEIN
  `qmlRegisterSingletonInstance` in die URI „QTmux" — kollidiert mit der Modul-
  Typregistrierung; Symptom „TerminalItem is not a type").
- **Bei jedem Build-Zyklus auch Release bauen** (Presets `*-release`; Standard-Presets
  sind Debug) — Release-only-Probleme (Optimierung, Asserts, RHI/Shader) fallen sonst
  nicht auf.
- **Versions-Bump-Stellen** (alle zusammen, am 2026-08-05 nachgemessen):
  `CMakeLists.txt` (project VERSION) · `installer/build-dmg.sh` ·
  `installer/build-appimage.sh` · **`installer/build-msi.ps1`** (`$Version`-Vorgabe) ·
  `.github/workflows/ci.yml` (AppImage-Schritt) · `README.md` (**8** Vorkommen, DE **und** EN).
  🔑 `src/app/main.cpp`, `src/server/McpServer.cpp` und `src/viewmodels/UpdateViewModel.cpp`
  stehen seit QTMUX-125 **nicht mehr** in der Liste: alle drei lesen `QTMUX_VERSION_STRING`
  aus dem generierten `qtmux_version.h` (`cmake/Version.h.in` ← `PROJECT_VERSION`). Die
  übrigen laufen ohne CMake-Konfiguration und bleiben darum manuell.
  🔑 **`installer/QTmux.wxs` ist KEINE Bump-Stelle** (stand hier früher fälschlich drin): Die
  Zahl darin steht nur in einem Kommentar-Beispiel, der echte Wert kommt über `-d Version=`.
  ✅ **`build-msi.ps1` hatte eine Vorgabe, die auf `1.2.0` stehengeblieben war** —
  parameterlos gebaut ergäbe das ein MSI mit falscher `ProductVersion`, ohne dass irgendetwas
  rot wird. Seit 2026-08-06 ist `-Version` **Pflichtparameter** (`Mandatory` +
  `ValidatePattern`): Ein Pflichtparameter kann nicht veralten. Kein Aufrufer bricht daran —
  `build-release.ps1` liest die Version aus `CMakeLists.txt` und reicht sie durch.
  📋 Vollständige Analyse als **Vorlage für die anderen fünf Repos**:
  [docs/versionsquellen.md](docs/versionsquellen.md) — dort auch die Kernfalle für die
  workspace-weite Build-ID `<version>+<hash>[-dirty]`: `configure_file` friert den Hash zur
  **Configure**-Zeit ein (ein Commit löst kein Re-Configure aus), er muss zur **Build**-Zeit
  entstehen. Und: Die Build-ID ist eine **Anzeige**, nie eine Vergleichsgröße — in
  `UpdateViewModel::currentVersion` darf nur `1.8.0` ankommen, sonst bricht der
  Manifest-Vergleich.
- 🔑 **Der Vendoring-Kontrakt umfasst NUR `MacPCAN/src/update/`** — und
  [tools/check-updater-sync.sh](tools/check-updater-sync.sh) wacht seit 2026-08-06 über
  **beides**: den Datei-Abgleich (Liste aus **beiden** Bäumen, neue und gelöschte Dateien
  fallen auf) **und** den Kontrakt selbst. Der zweite Wächter meldet jeden `#include`, der
  aus `update/` herausführt.
  ⚠️ **Warum das nötig wurde:** Der Datei-Abgleich allein bemerkt eine neue Abhängigkeit
  nach außen **nicht** — er meldet nur „ABWEICHUNG", man zieht nach, und der Compiler sagt
  dann „file not found", ohne dass jemand den Kontrakt als Ursache erkennt.
  **Zum Hub-Paket AP8** (Profillader `DbcLoader`/`JsonLoader` wandern in den Hub):
  **QTmux ist nicht betroffen** — MacPCANs `DbcDecoder` liegt in `src/specs/`, RAFTNGs Lader
  in `src/io/`, beide außerhalb des Kontrakts; QTmux liest keine DBC-Profile. Nachziehen
  wäre nur nötig, wenn der Update-Kern selbst eine Abhängigkeit darauf bekäme — genau das
  meldet der Wächter. Reihenfolge bleibt MacPCAN → RAFTNG → QTmux.
- ⚠️ **Fremde Pfade NIE mit `readAll()` lesen** (Sicherheitsbefund aus RAFTNG, 2026-08-06).
  Dort ließ `{"path":"/dev/zero"}` den Prozess in zwei Sekunden von 17 MB auf **30,4 GB**
  wachsen, blockierte die Event-Loop und beendete ihn **ohne Crashreport**.
  Gemeinsame Einheit: [src/core/SafeFileRead.h](src/core/SafeFileRead.h)
  (`safefile::read(pfad, grenze)`), Test `test_safefileread`.
  🔑 **DREI Riegel, und die ersten beiden allein genügen nicht:** (1) `QFileInfo::isFile()` —
  wirft Geräte, FIFOs, Sockets, Verzeichnisse raus (`:/`-Ressourcen gelten als Datei und
  bleiben erlaubt); (2) gemeldete Größe gegen eine Obergrenze; (3) **höchstens `grenze+1`
  Bytes lesen** statt `readAll()`.
  🔑 **Gemessen, nicht angenommen:** `/dev/zero`, `/dev/urandom` und ein FIFO melden alle
  **`size() == 0`** bei `isFile() == false`. Eine reine Größenprüfung ließe sie also **durch**.
  Umgekehrt fängt das gedeckelte Lesen `/dev/zero` auch ohne `isFile()` ab — beim **FIFO**
  aber nicht, das blockiert für immer, **ohne** Speicher zu fressen, und sieht damit wie ein
  Hänger aus, nicht wie ein Leck. Genau deshalb alle drei.
  ⚠️ Ein Token schützt nicht davor: Wer sich anmeldet, kann trotzdem einen bösen Pfad
  schicken — und ein **versehentlich** falscher Pfad tut dasselbe wie ein böser.
  🔑 **Der MCP-Server ist frei von dieser Bauart** (2026-08-06 geprüft): Kein Werkzeug nimmt
  einen Pfad zum **Lesen**. `cwd` wird nur als Arbeitsverzeichnis gesetzt, `program`
  ausgeführt, `identity` unverändert an `ssh -i` durchgereicht. Gehärtet sind die vier
  Stellen, die einen fremden Pfad wirklich öffnen: `SettingsIo`-Import, Farbschema-Import,
  die beiden Scrollback-Dumps und der Vergleich in `ShellIntegration`.
  📋 Nachprüfen mit [tools/fuzz_mcp.py](tools/fuzz_mcp.py) — 51 Fälle, prüft **nach jeder
  Anfrage**, ob der Prozess noch lebt (sonst meldet so ein Lauf nur „es hat nicht geknallt",
  statt zu sagen, welcher Aufruf getötet hätte). ⚠️ Nur gegen eine **eigene** Instanz auf
  eigenem Port — QTmux hält Terminal-Sessions.
  ⚠️ **Nebenbefund für Diagnosen:** Trotz 30 GB gab es **keinen** jetsam-Eintrag im
  macOS-Log. Wer einen Prozesstod mit „kein OOM laut Log" ausschließt, schließt zu früh aus.
- **Build-ID `<version>+<git-short-hash>[-dirty]`** (workspace-weite Owner-Vorgabe): steht im
  **Fenstertitel** (`qml/Main.qml`, `App.buildId`) und in MCP `get_server_info`
  (`buildId`/`buildDirty`). Erzeugt von [cmake/BuildId.cmake](cmake/BuildId.cmake) +
  `BuildId.h.in` über das Target `qtmux_buildid`.
  ⚠️ **VORLÄUFIG** — der kanonische Baustein entsteht in MacPCAN und wird vendiert; getauscht
  wird dann nur die **Quelle**, nicht die Anzeige.
  🔑 **Warum ein eigenes Target und kein zweites `configure_file`:** Letzteres liefe zur
  **Configure**-Zeit, und ein Commit ändert keine CMake-Datei — der Hash bliebe stehen und
  die App behauptete einen fremden Stand. Das ist schlimmer als gar keine Angabe.
  🔑 **`-dirty` nur aus verfolgten Dateien** (`--untracked-files=no`), sonst macht ein
  herumliegendes `build/` jeden Build „dirty". Header nur bei **Inhaltsänderung** schreiben
  (`copy_if_different`), sonst Rebuild bei jedem Bauen — beides gemessen.
  ⚠️ **Anzeige, NIE Vergleichsgröße:** `Updates.currentVersion` bekommt weiterhin nur
  `1.8.0`. Mit angehängtem Hash schlüge der Manifest-Vergleich fehl oder böte dauerhaft ein
  „Update" an. Vollständige Analyse: [docs/versionsquellen.md](docs/versionsquellen.md).
- **i18n:** Quellsprache Deutsch; QML `qsTr`, C++ `QCoreApplication::translate("<Kontext>",…)`.
  `cmake --build … --target update_translations`; gescannt werden **genau `qtmux` und
  `qtmux_core`** (`SOURCE_TARGETS` an `qt_add_translations`);
  `cmake/FinishSourceLanguageTs.cmake` finalisiert die DE-Datei
  automatisch — **nur `i18n/qtmux_en.ts` braucht echte Übersetzungspflege**. Eigennamen
  (PowerShell, Bash, …) bleiben unübersetzt. Neben den eigenen `.qm` wird **`qtbase_<lang>`**
  eingebettet und geladen (QTMUX-117) — sonst bleiben Qts eigene Texte englisch, allen voran
  die Standardknöpfe; Mechanik in den QML-/Theming-Lektionen.
  🔑 **`SOURCE_TARGETS` ist Pflicht, nicht Kosmetik (2026-07-31).** Ohne die Angabe scannt
  lupdate **alle** Targets — auch `tests/`. `tst_i18n.cpp` fragt für QTMUX-117 gezielt
  Qt-eigene Texte ab (Kontext **`QPlatformTheme`**: „Cancel"/„OK"/„Save"/„Close"), und
  lupdate trug sie in unsere `.ts` ein; weil Deutsch Quellsprache ist, finalisierte der Hook
  sie prompt als `Cancel` → **„Cancel"**. Damit lag in `qtmux_de` ein **konkurrierender**
  Eintrag zu dem aus `qtbase_de` (dort korrekt „Abbrechen") — also eine latente Regression
  von QTMUX-117. Unsichtbar blieb sie nur, weil Qt die Translator in **umgekehrter
  Installationsreihenfolge** befragt und `qtbase` zuletzt installiert wird: Ein Vertauschen
  der zwei `swapTranslator`-Zeilen in [main.cpp](src/app/main.cpp) hätte QTMUX-117 still
  zurückgedreht. **Merke:** Ein Test, der Fremdtexte abfragt, wird sonst zur
  Übersetzungsquelle. Kontrollgröße nach der Eingrenzung: **kein `QPlatformTheme`-Kontext**
  in den `.ts` (eine absolute Quelltext-Zahl veraltet mit jedem Feature und taugt nicht als
  Anker).
- README.md ist **zweisprachig** (DE/EN, Anker `#-deutsch`/`#-english`) — beide Hälften
  pflegen.

## Reifegrad (dauerhaft — der Zahlenstand steht NUR im „Arbeitsstand")

**Phasen 0–6 komplett** (Terminal-Kern, Sessions/Sidebar, Agent-Awareness, SSH/Seriell/SFTP,
Plugins + MacPCAN, Installer), dazu **Online-Update** (QTMUX-125) und die **GUI-Auffrischung
Design 1a/2a** (einklappbare Seitenleiste + Flyout, Statusleiste, sechs Menüs, neugestaltetes
Einstellungsfenster, Reset/Import/Export — Details im Abschnitt „Design 1a/2a" unten).
**39 MCP-Tools** (GUI-MCP-Parität für den geplanten AI-Companion), i18n finalisiert,
Installer für alle drei Plattformen (DMG/MSI+ZIP/AppImage).

**Window-Modell (QTMUX-83, seit v1.7.0):** Kein globales Split-Layout mehr, sondern das
tmux-Modell — Sidebar = **Windows** (Tabs), jedes Window hat sein eigenes Split-Layout,
Splits = Panes **im** Window, `focus_window` schaltet das ganze Layout um. Gruppen sind
seither **Window**-Gruppen. Details/Verifikation:
`docs/design/per-window-layouts/Umsetzung.md`; Mechanik unten in der Feature-Referenz.
🔑 Aus dem Anwendertest (QTMUX-87): Das **letzte** Fenster zu schließen **beendet QTmux**
(`requestQuit` + normale Rückfrage) — vorher entstand sofort ein neues, leeres Fenster mit
höherer ID, was wie ein durchlaufender Zähler aussah. Über **MCP** beendet `close_window`
die App bewusst **nicht** (ein aufräumender Agent würde sich sonst selbst abschalten).
Vorarbeit QTMUX-80/81/82, dabei **stiller Selbst-Screenshot** `--screenshot <png>` — der
Standardweg für visuelle Abnahmen. ⚠️ Er setzt `QTMUX_NO_GPU=1` und fotografiert damit den
**QPainter-Fallback**: Fehler im Glyph-Atlas (QTMUX-97) sind darauf **prinzipiell unsichtbar**.
Plattform-Eigenheiten und die teuer erkauften Fallen dazu stehen in den E2E-Fallen, nicht hier.

## Arbeitsstand & Wiedereinstieg (2026-08-06)

> Die EINE Stelle für den aktuellen Stand (Pflegeregeln 2–4 oben). Verlauf steht in
> Git/Jira/Confluence; Feature-Mechanik in der Feature-Referenz; Abnahme-Rezepte in
> [docs/owner-abnahmen.md](docs/owner-abnahmen.md).

**Ausgeliefert: v1.8.0** — Tag auf `4f10eb8`, alle 4 Installer, Manifest live unter
`https://nobser.de/updates/qtmux/`; der volle Update-Zyklus ist am lebenden Objekt
verifiziert (Details im Abschnitt „Online-Update"). Jira dual synchron bis **QTMUX-129**.

**Teststände:** **30** Tests (s. Dateitabelle). macOS Debug/Release je **30/30** (dort läuft
`test_pty` mit und besteht). Linux (rtzsvr02-Container) und Windows nehmen `test_pty` per
`-E` aus (umgebungsbedingt: nicht-interaktive Shell/ConPTY; unter Windows braucht `ctest`
zusätzlich Qt-`bin` im PATH, sonst `0xc0000135`) — dort sind also **29** zu erwarten;
zuletzt gemessen wurde dort 28/28, **vor** `test_restorehistory`. Größte Binaries:
`tst_session` 24 Fälle, `tst_vtscreen` 24, `tst_agent` 20.
🔑 Der **CI**-Linux-Job ist nicht der rtzsvr02-Container: dort läuft `test_pty` mit und
besteht. Eine kleinere Zahl aus dem Container ist kein Widerspruch, sondern die
Ausnahme per `-E`. **Zahl immer per `ctest -N` gegenprüfen, nie schätzen.**
🔑 **„CI grün auf allen drei Plattformen" ist KEIN Vollständigkeitsbeleg** (Lektion aus
QTMUX-124): Die CI baut **Release**, ebenso Homebrew-Qt und der Linux-Container —
**Debug-only-Asserts in Qts vorgebauter Bibliothek sieht sie prinzipiell nicht**; der
Windows-**Debug**-Build ist dafür das einzige taugliche Messmittel (`-DQT_FORCE_ASSERTS`
trägt nicht, es wirkt nur in den *Headern*).

### Nächster Schritt (Wiedereinstieg nach /compact)

Stand **2026-08-07 abends** · `origin/main` = **`20ab250`**, darauf **1 ungepushter Commit**
(das Doku-Aufräumen). Ein Arbeitsbaum, nur Branch `main` (Worktree und Feature-Branches
abgebaut). Teststand macOS Debug **30/30** und Release **30/30** (`ctest -N` = 30).
🔑 **Der eigene Commit-Hash steht hier bewusst NICHT** — ein `--amend` am Aufräum-Commit
ändert ihn, und der Anker wäre im selben Moment falsch (2026-08-07 genau so passiert).
Belastbar ist die Beziehung zu `origin/main`; die Lage prüft man mit
`git log --oneline origin/main..HEAD`. Beim Wiedereinstieg zusätzlich `git log --oneline -3`
gegenlesen — die Windows-Session pusht ebenfalls.

⚠️ **Working Tree ist NICHT sauber — eine Änderung wartet auf Owner-Freigabe:**
- `qml/Ui/AppComboBox.qml` — **Bugfix für einen gemeldeten Anwenderfehler**: Popup-Einträge
  jeder ComboBox mit `textRole` blieben **leer** (Sprach- und Shell-Auswahl). Ursache
  gemessen: `Array.isArray(cb.model)` liefert **false**, auch wenn das Model ein JS-Array
  ist — die `model`-Property reicht es als QVariant durch. Der Delegate lief deshalb immer
  in den `model[textRole]`-Zweig, den es bei Array-Models nicht gibt → `undefined`.
  Gegentest belegt (alt `undefined`, neu `"Deutsch"`); 30/30 grün. **Committen wurde
  zweimal angeboten, nie beantwortet.** Regel und Messung stehen in der Feature-Referenz
  (QML-Lektionen, `Array.isArray`).

✅ **In `main` seit heute:** QTMUX-130 (Verlaufs-Umbruch, `47d313e`, CI grün auf allen drei
Plattformen — Lauf `31165779520`) · Vendoring auf MacPCAN `58df9e4` (`c9fee38`) samt
Richtigstellung (`4eb04b0`) · korrigierter Sprach-Hinweis in den Prefs (`20ab250`).
⚠️ **QTMUX-130 wirkt nicht rückwirkend:** Vorhandene Dumps tragen die eingefrorenen
80er-Umbrüche als Inhalt — erst der **zweite** Neustart nach dem Update ist sauber. Wer das
übersieht, hält den Fix für wirkungslos.
⚠️ Die **Build-ID-Quelle ist vorläufig** ([cmake/BuildId.cmake](cmake/BuildId.cmake)) — der
kanonische Baustein kommt aus MacPCAN; beim Tausch ändert sich nur die Quelle.

**Nächster Punkt: QTMUX-94** — Terminal-Ausgabe als Agenten-Kontext.
- Einstieg: `VtScreen::screenText()`/Scrollback liegen fertig vor; es fehlt allein der Weg
  für den Menschen — Auswahl bzw. Bildschirm einer Session an eine **andere** Session geben.
- Vorgehen: (1) Gui-freie Hälfte zuerst (Formatierung/Begrenzung der Übergabe, eigener
  Header in `core`, Test in **neuer** Datei); (2) Anbindung über Palette + Kontextmenü;
  (3) MCP-Tool nur, wenn die Palette es kann (Regel QTMUX-46).
- Bauen/Testen: `tools\vsdev-build.cmd windows all` **und** `windows-release all`, danach
  `ctest --test-dir build\windows-release -E "^test_pty$"` (Qt-`bin` in den PATH).
  ⚠️ Nicht in `build\windows` bauen, solange die produktive Instanz daraus läuft.
- Beachten: `qtmux_core` bleibt Gui-frei; jede neue Zeichenkette in `qsTr` + **beide** `.ts`;
  neue QML-Datei ohne `QML_FILES`-Eintrag existiert zur Laufzeit nicht.

**Danach:** Windows-**Debug**-Build von QTMUX-130 auf rtzbld01 (die CI baut Release und sieht
Qts Debug-Asserts prinzipiell nicht, Lektion QTMUX-124) · Owner-Durchklick der fertigen
Tickets ([docs/owner-abnahmen.md](docs/owner-abnahmen.md)) · Windows-/Linux-Zweig des
Update-Wegs am lebenden Objekt belegen · QTMUX-127-Rest (Prefs-Sichtprüfung, pf-Installation).

### Offene Owner-Entscheide (blockieren nichts, aber warten)

1. **Den AppComboBox-Bugfix committen?** (s. Working Tree oben) — zweimal angeboten,
   unbeantwortet. Bis dahin hat **keine** gebaute Instanz den Fix.
2. **System-Modus für die Sprache von RAFTNG übernehmen?** RAFTNG bietet seinen
   `Mode::System` an (`QLocale::system()`, EN/DE/SV); QTmux kann nur fest Deutsch/Englisch.
   RAFTNG nennt es ausdrücklich eine **Produktentscheidung**, keine technische, und liefert
   auf Zuruf. Nicht eigenmächtig übernommen.
3. **Produktivinstanz neu bauen?** Sie läuft aus `build/macos` mit `1.8.0+34449de` und hat
   damit keinen der heutigen Stände. Ein Neubau reißt alle Terminal-Sessions mit.

#### Zustand, der nicht aus Code/Git hervorgeht

- ⚠️ **Die Produktivinstanz läuft aus `build/macos`** (Port 7345, `1.8.0+34449de`, gebaut
  2026-08-07 09:24) — dort **nicht** hineinbauen, das reißt alle Terminal-Sessions mit.
  🔑 Sie ist damit **älter als jeder heutige Stand**. Vor jeder Diagnose an ihr die Build-ID
  gegen `git log` halten; PID über `lsof -nP -iTCP:7345 -sTCP:LISTEN` holen, **nie** eine
  notierte PID verwenden (die hier eingetragene war zweimal veraltet).
- Der Workflow-Eintrag **`Trigger-Test`** (ID `328898398`) steht bleibend in
  `gh workflow list --all`, obwohl sein Branch gelöscht ist — GitHub führt ihn, solange sein
  Lauf existiert. Bewusst nicht entfernt: Lauf `#31128515520` ist der Beleg, dass der
  `push`-Trigger funktioniert. Wirkungslos, verschwindet von selbst.

### Zuletzt abgeschlossen (Mechanik in der Feature-Referenz, Verlauf in Git)

QTMUX-124 (Windows-Absturz) · **125** (Online-Update) · **127** (MCP im LAN) · **128**
(Mausrad in Codex) · **129** (Proxy) · QML-Editor-Diagnosen von >2000 auf **2** (beide echt).
Alle selbst verifiziert und in `main`; die dauerhaften Lektionen stehen jeweils im
zuständigen Fachabschnitt, nicht hier.

### Offene Fäden (dauerhaft relevant)

- ⚠️ **QTMUX-127-Rest:** Sichtprüfung der Einstellungsseite (auf macOS ist das Prefs-Fenster
  mit `--screenshot` prinzipiell nicht greifbar — eigenes `Window`; Rezept in den E2E-Fallen)
  und die **pf-Installation** auf dem Zielrechner (`sudo` verlangt hier ein Passwort; Skript
  fertig und trocken geprüft, `pfctl -n -f` sauber, aber nicht geladen). Application Firewall
  dort aus.

- ⚠️ **Proxy (QTMUX-129) — offen ist nur die Abnahme am echten Firmen-Proxy**, hier steht
  keiner. Umgesetzt und selbst verifiziert ist alles übrige (Mechanik im Abschnitt
  „Online-Update"); belegt sind die Übersetzung der Einstellungen, die Ein-Versuch-Regel,
  das Schweigen des Start-Checks und dass das Passwort in keiner Einstellungsdatei landet.
- ⚠️ **Am Update-Weg noch nicht am lebenden Objekt belegt:** Nur der **macOS**-Zweig wurde
  real ausgelöst (DMG gemountet). `msiexec /i` (Windows) und der **AppImage-Selbsttausch**
  (Linux) sind bisher nur als *Start-Plan* geprüft — die Zeichenkette stimmt, ausgeführt hat
  sie niemand. Nachholen: auf rtzbld01 bzw. im Linux-Container eine Instanz mit älterer
  Version gegen die Produktions-URL fahren (Rezept im Abschnitt „Online-Update").
- **Offener Code-Faden „Modul B":** `WindowModel` aggregiert noch nicht über die Panes (TODO
  in [WindowModel.cpp](src/viewmodels/WindowModel.cpp) + neutrale Stubs in
  `tst_windowmodel.cpp`); die Aggregat-Zähler liegen deshalb bewusst in `SessionModel`.
- ⚠️ **Nebenbefund QTMUX-100, ungeprüft:** Kachel auf Zeile 0 schieben, während die Liste
  **gescrollt** ist → `contentY` driftet. Anderer Pfad als der behobene, im Ticket notiert.
- **Fortsetzungs-Vorlagen (QTMUX-91):** verifiziert ist **codex** (alle drei Modi); offen
  bleiben gemini/aider/cursor/qwen und die 13 Nachtrags-Einträge — dort sind die Vorlagen
  bewusst leer, weil die CLIs hier nicht installiert sind (ein ungeprüftes Flag sähe für den
  Anwender wie ein QTmux-Fehler aus).
- **Architektur-Landkarte** (Vollanalyse 2026-08-01, nicht beauftragt): C++ sauber, die
  Schulden sitzen in `qml/Main.qml` (~4.700 LOC). Befund und Abbaupfad stehen in
  [docs/architektur-landkarte.md](docs/architektur-landkarte.md) — **vor** jedem
  strukturellen Umbau dort lesen.

### Maschinen-Eigenheiten (Build-Verzeichnisse)

- ⚠️ **`build/macos`: die Produktivinstanz läuft daraus** (altes Binary). Verifizierte
  Endstände sind `build/macos-test` und `build/macos-release`; Neubau von `build/macos` nur
  mit Freigabe (vorher `lsof -nP -iTCP:7345 -sTCP:LISTEN`), er reißt sonst alle
  Terminal-Sessions mit. 🔑 **Dauerhafte Konsequenz:** Solange die Produktivinstanz aus einem
  Build-Verzeichnis läuft, ist „steht im Repo" **nie** gleich „ist in der App" — jede
  Owner-Abnahme braucht eine frische Testinstanz (`QTMUX_PROFILE=test QTMUX_MCP_PORT=7346`)
  oder den Neubau.
- **Windows, drei Build-Verzeichnisse:** `build/windows` (Debug, **hier läuft die produktive
  Instanz**) und `build/windows-release` sind die Standardpaare (Qt **6.11.1** im Cache);
  `build/windows-qt6103` ist der Nachweis gegen die CI-Version (Qt **6.10.3** via
  `QTMUX_QT_PREFIX`) und der Ausweichplatz, solange die produktive Instanz läuft — jederzeit
  löschbar. Eine Preset-Änderung erreicht ein bestehendes Build-Verzeichnis nie.
- 🔑 **Parallelarbeit mit Worker-Sessions: Worktrees, nicht ein gemeinsamer Baum** — fast
  jede Aufgabe endet in `qml/Main.qml` und den beiden `CMakeLists.txt`. Bewährt: je Worker
  ein `git worktree` mit eigenem Branch **und eigenem Build-Verzeichnis** (`build/w<N>`),
  Auftrag strikt auf die **Gui-freie** Hälfte plus Tests in **neuen** Dateien begrenzt,
  QML-Anbindung danach seriell durch eine Instanz.

**Offene Jira (geführt wird in Jira, hier nur Zeiger):**
✅ **122/123 sind umgesetzt und rebast** (`162f079`, s. Owner-Entscheid 1) — **122** = OSC 52
(Zwischenablage aus dem Terminal füllen; Anwenderbefund — aus einem Claude Code über `ssh`
ließ sich nichts kopieren: die Anwendung hält Maus-Tracking + Alt-Screen, QTmux reicht die
Maus durch (QTMUX-104), markiert wird **in der Anwendung**, QTmux hat gar keine Auswahl),
**123** = sichtbarer Hinweis, wenn eine Vollbild-Anwendung die Maus hält (ohne ihn findet man
die Shift-Geste nicht — dieselbe Erfahrung wie bei den Links in QTMUX-39). Offen ist nur der
Merge (Klassifikator-Freigabe) und danach Jira dual. ·
**40** (OSC-8-Hyperlinks — deferred; bräuchte Cursor-Span-Tracking + neues `Cell`-Feld,
teuer, da `VtScreen` den Sichtbereich lazy aus libvterm bildet) ·
**13** (native macOS-Menü-Icons — deferred; Qt reicht `icon.source`/`icon.name` in nativen
Menüs nicht durch, einziger Weg wäre ein QMenuBar-Umbau; [[qtmux-native-menu-icons]]) ·
**126** (Marken-Badge „Q" in Menüleiste + App-Icon; Farbe **`#B239EA`** (Violett), festgelegt 2026-08-05 nachdem `#0284C7` sich als die Farbe des **Deskstarters** herausstellte. 🔑 Eine weitere **Blaustufe war rechnerisch nicht möglich**: Der Bereich H 184–275 ist von MacPCAN, Deskstarter und AstroCAN dreifach belegt, jede Blaustufe scheitert am Abstand zu MacPCAN oder geht im **Dunkel**-Design unter. Belege — ΔE2000 und WCAG-Kontrast gegen **beide** Designs samt Kopfleiste — stehen in der Spec; Backlog; Spec
`_ClaudeWorkspace/brand-badge-spec.md`).
**Aus der Air-Evaluation (air.dev):** offen **91** (Agenten-Startprofile, gehört zu
QTMUX-85) · **92** (Container-Backend) · **93** (Spike ACP, berührt 55/73/75) · **94**
(s. „Nächster Schritt") · **95** (Auslöser Zeitplan/Webhook am MCP-Server); schon abgedeckt
und darum NICHT neu angelegt: 55/69/72/73/75/76. 🔑 **Bewusst nicht übernommen:** alles
Editor-artige (Symbols, Go-to-Definition, Datei-Baum, projektweite Suche, Commit-Erzeugung,
Diff-Kommentare) und die Cloud-Hälfte — QTmux ist ein Terminal-Manager, kein IDE-Ersatz;
diese Linie beim nächsten Feature-Vergleich wiederverwenden.

**Backlog (nicht beauftragt):** SFTP-MCP-Tools (Companion-Prio 2) ·
Signierung/Notarisierung (macOS Developer-ID, Windows Authenticode) · MacPCAN-Feinschliff
(CAN-FD, ID-Filter, Konfig-Dialog, DBC-Decoding) · optional CPack-Distro-Pakete
(.deb/.rpm) · **LGPL-Beilagen** fürs gebündelte Qt (Lizenztext + Quellen-Hinweis).

### Design 1a/2a (GUI-Umbau) — abgeschlossen

Siebenstufiger Umbau (Original im Claude-Design-Projekt
`ab66e9b5-053b-4e81-9e4a-c45752fd42d1`, über `DesignSync` `get_file`); Stufen und Commits
stehen in der Git-Historie, die Mechanik in der Feature-Referenz.
**Randbedingungen (dauerhaft):** Qt Quick Controls **Basic**, keine neuen Effekte,
Chrome-Farben nur über `Theme.*` (Ausnahme: Statusfarben), jede neue Zeichenkette in `qsTr`
+ beiden `.ts`.
**Bewusste Abweichungen von der Anweisung** (Begründungen in der Feature-Referenz): keine
macOS-Menü-Rollen (QtQuick.Controls kennt sie nicht) · Umbenennen/Gruppe wirken aufs
**Window** (QTMUX-83) · Export ist eine **Allowlist**, nicht „alles außer dem Vault" ·
listenartige Prefs-Seiten bleiben strukturell wie sie waren.

**Offen (bewusst nicht behauptet):** der **Owner-Durchklick** der GUI — nativen macOS-Menüs,
Flyout, Statusleiste, Einstellungsfenster, `App.reduceMotion` in beiden Designs, dazu
Export/Import durch die **echten** Dateidialoge (auf keiner Maschine automatisierbar).
Rezepte: [docs/owner-abnahmen.md](docs/owner-abnahmen.md).

### Zusammenarbeit Windows ↔ macOS

Zwei Claude-Sessions teilen die Arbeitskopie (Windows-Maschine + Mac). **Arbeitsteilung
(2026-07-30):** Windows schreibt Code und baut/testet **lokal auf der Entwicklungsmaschine**
`30516935D11` (Debug **und** Release); der Mac verifiziert dieselbe Änderung auf den zwei
Plattformen, die Windows nicht sieht — macOS lokal, Linux auf rtzsvr02 (Container) — macht die
GUI-Abnahmen und pflegt **Jira/Confluence** (geht nur dort, `CLAUDE.local.md` existiert nur auf
dem Mac). ⚠️ **rtzbld01 ist NICHT die Testmaschine**, sondern der Release-Build-Rechner für die
Installer (Zugang in `CLAUDE.local.md`) — und hat als einzige Windows-Maschine Qt 6.10.3.
**Nach jeder großen Anpassung auf allen drei Plattformen bauen + testen**
([[qtmux-build-alle-plattformen]]). Koordination: die drei Regeln aus
[[zwei-sessions-eine-arbeitskopie]] — gezielt stagen (**nie** `git add -A`), vor dem Push
`git fetch` und bei Divergenz erst committen, dann mergen, und nach jedem Merge, der die
CLAUDE.md anfasst, `test_doc_duplicates` laufen lassen. Dass beide Sessions denselben Fehler
(`#include <limits>`) unabhängig fixten, war der harmlose Fall — der Merge blieb sauber, weil
die Zeile identisch war.

### Jira-Statuskonvention & Lektionen (dauerhaft)

Der aktuelle Zahlenstand steht NUR im Abschnitt „Arbeitsstand & Wiedereinstieg" oben.
🔑 **Statuskonvention:** „Fertig + verifiziert → Done" meint **selbst verifiziert** (Tests,
E2E, in `main`), **nicht** „vom Owner abgenommen". Die offene Owner-Abnahme wird bewusst
NICHT über den Ticket-Status geführt, sondern über [docs/owner-abnahmen.md](docs/owner-abnahmen.md) (je Punkt ein
Rezept); ein Befund bei der Abnahme öffnet das Ticket wieder oder erzeugt ein Folgeticket.
**In Arbeit ist nur, woran gerade jemand sitzt** — alles andere ist Backlog oder Done.
(Anders gelesen zeigte das Board 24 fertige Tickets als „in Arbeit" — ein falsches Bild.)
🔑 Vor jedem neuen Ticket in **beiden** Systemen die höchste Nummer holen
(`ORDER BY key DESC`, maxResults 1) — die Windows-Session legt ebenfalls an; nur solange
beide gleich stehen, bleiben die Keys deckungsgleich. Und prüfen, ob die Doku bereits höhere
Nummern *vergeben* hat; Behauptungen wie „X ist nicht angelegt" vor dem Handeln gegenprüfen
(die alte „QTMUX-46/-79 nicht angelegt"-Notiz war schlicht falsch).
🔑 **Werkzeug-Falle Cloud:** Python-`urllib` schlägt hier mit `CERTIFICATE_VERIFY_FAILED`
fehl (kein Issuer im Store) — ADF-Rumpf mit Python **bauen**, aber mit
`curl --data @datei` **senden**. On-prem braucht ohnehin `curl -k`.
**Bewusst NICHT als Ticket angelegt:** Qt 6.10.3 lokal + Preset-Reihenfolge — steht
vollständig im Windows-Preset-Kasten oben; ein sofort geschlossenes Ticket ohne eigenen
Inhalt wäre nur Rauschen im Board.
Confluence-Entwicklerdoku-Unterseiten: „GUI-Auffrischung Design 1a/2a",
„Agenten-Wiederherstellung", „MCP im Netzwerk: Bind-Adresse, Token, pf (QTMUX-127)"
und **„Online-Update (QTMUX-125)"** (Seiten-IDs in `CLAUDE.local.md`). Die
**Benutzerdoku** trägt seit 2026-08-03 zusätzlich den Abschnitt „Aktuell bleiben
(Online-Update)" — beide Systeme gepflegt.

### Owner-Abnahmen offen — **28 Tickets**, je umgesetzt + selbst verifiziert

**Die Liste samt Abnahme-Rezept je Ticket steht in
[docs/owner-abnahmen.md](docs/owner-abnahmen.md)** — sie ist Arbeitsvorrat und gehört
nicht in jede Session. Hier bleiben nur die zwei Regeln, die man beim Planen kennen muss:

- Eine Abnahme braucht eine **frisch gebaute** Instanz, keine laufende Produktivinstanz.
  Der Standardweg ist die isolierte Instanz (`QTMUX_PROFILE=<name> QTMUX_MCP_PORT=<frei>`).
- **Selbst verifiziert ist nicht abgenommen** (Statuskonvention oben) — die offene Abnahme
  wird über jene Datei geführt, nicht über den Jira-Status.

## Repository, Release, Zusammenarbeit

- **Öffentlich:** `github.com/RealNobser/QTmux` (Apache-2.0). Archiv: `QTmux-private`
  (privat + archiviert = read-only; enthält als einziges noch die **unbereinigte**
  Historie). Beide Namen sind belegt — die alte Weiterleitung ist dadurch tot.
- **`main` ist geschützt**, aber `enforce_admins` ist bewusst **aus**: Die PR-Pflicht gilt
  **Collaborators**, nicht der eigenen Arbeit. Anwender und Claude pushen als Admins
  **direkt auf `main`** — ausdrückliche Vorgabe, kein Notfallweg. GitHub protokolliert das
  als „Bypassed rule violations"; das ist erwartet und kein Warnsignal.
  (Nur ein **Force**-Push braucht das kurzzeitige Lockern der Regel, s. Git-Lektionen.)
- **Release:** `gh release create v<ver> --target <voller SHA>` — ein **Kurz-SHA wird
  abgelehnt** (HTTP 422). Assets: DMG + MSI + portables ZIP + AppImage.
  Das AppImage stammt aus dem CI-Lauf desselben Commits
  (`gh run download <id> -n QTmux-AppImage`), nicht aus einem Extra-Build.
- **Interne Zugänge** (Confluence-/Jira-Hosts, Space-Keys, Seiten-IDs, Build-Maschinen)
  stehen **nur** in `CLAUDE.local.md` — git-ignoriert, bewusst nicht im öffentlichen Repo.

## Git-/GitHub-Lektionen (teuer erkauft)

- ⚠️ **Merge-Konflikte zwischen zwei Features liegen meist NEBENeinander, nicht gegeneinander.**
  Beim Rebase von QTMUX-122/123 (`162f079`) waren alle sieben Konflikte von dieser Sorte:
  128 (`altScrollMode`) und 122 (`appClipboardWrite`) landeten in denselben Listen
  (Settings-Allowlist, Alias-Block, `restoreDefault`-`switch`, Prefs-Suchindex). Überall
  gehören **beide** hinein. Wer reflexhaft „ours" nimmt, verliert still ein fremdes Feature —
  **ohne Compilerfehler**, weil eine fehlende Zeile in einer Allowlist niemandem auffällt.
- ⚠️ **Ein ausstehender Fast-Forward verträgt keinen Commit auf `main`** — sonst wird daraus
  ein echter Merge mit einem zweiten Konfliktdurchgang in denselben Dateien. Während der
  Merge an einer Freigabe hängt, gehört auch die Doku auf den **Feature-Branch**.
- ⚠️ **Vor jedem Worktree-/Branch-Abbau gegenprüfen**, besonders nach einem fremden
  Hand-Eingriff: `git log --oneline main..<branch>` muss **leer** sein, und Branches mit
  **`-d`** löschen statt `-D` (`-d` verweigert bei Ungemergtem, `-D` fragt nicht). Am
  2026-08-07 traf ein Owner-Handmerge nur zwei von drei Commits — ein ungeprüftes Aufräumen
  hätte den dritten spurlos vernichtet.
- 🔑 **Ein roter Job ist nicht automatisch ein Fehlschlag.** Erst `gh api …/jobs` lesen:
  **leere Schrittliste (`steps: []`) = nie einem Runner zugeteilt**, also kein Befund. Kam
  während der Actions-Störung mehrfach vor und färbte ganze Läufe rot.
- 🔑 **Trigger-Diagnosen beginnen außerhalb des Repos:**
  `curl -s https://www.githubstatus.com/api/v2/summary.json` **vor** jeder Suche. Am
  2026-08-06 erzeugten vier Pushes null Läufe — Ursache war eine GitHub-Störung
  (Webhooks auf ~15 % gedrosselt), nichts am Repo; vier Repos zeigten dasselbe Muster,
  auch eines mit self-hosted Runner. ⚠️ **Durchgekommene Läufe sind Stichproben, kein
  Störungsende** — aus „was durchkam" lässt sich kein Ausfallfenster ableiten; den Zeitraum
  liefert allein die Statuspage. Solange ein Incident offen ist: nach jedem Push prüfen, ob
  ein Lauf entstand, sonst `gh workflow run CI --ref main`.

- **Historien-Rewrites betreffen vier Ebenen:** `git filter-repo --replace-text` fasst nur
  **Dateiinhalte** an — Commit-Nachrichten brauchen `--replace-message`, Autor-Adressen
  `--mailmap`, Tag-Nachrichten eigene Behandlung. `git grep` durchsucht **keine**
  Commit-Nachrichten, eine Verifikation nur damit übersieht sie komplett.
- **Force-Push löscht nichts:** Alte Commits bleiben per SHA abrufbar, solange etwas sie
  referenziert — und **jeder Actions-Lauf referenziert seinen Commit**. Ein frisches Repo war
  der einzige verlässliche Weg. Jeder Rewrite zieht eine Kette nach sich: Force-Push → Tag neu
  → Release-Target umhängen → **alle Klone neu klonen** (auch die Build-Maschine). Deshalb
  Textkorrekturen sammeln. Force-Push ist freigegeben (2026-07-21), aber vorher
  `git bundle create ~/QTmux-backup-<datum>.bundle --all`; blockiert der Sicherheitsfilter
  des Harness den Vorgang, nicht umgehen, sondern melden.
- Persönliche Repos kennen **keine** abgestuften Collaborator-Rollen (API meldet
  `204 No Content` und ändert nichts); `gh api …/protection/allow_force_pushes` existiert
  nicht (404) — Protection nur über den kompletten Payload setzen.

## 📖 Ausgelagertes Detailwissen — zwei Pflichtlektüren je nach Aufgabe

Beide Dateien standen bis **2026-08-06** hier drin und machten zusammen zwei Drittel dieser
`CLAUDE.md` aus. Sie sind **nicht** entwertet, sondern **umgezogen** — jede Zeile darin gilt
unverändert weiter. Der Grund: Diese Datei wird in *jeder* Session vollständig geladen, jenes
Nachschlagewerk braucht man aber nur beim Anfassen des jeweiligen Themas.

| Datei | Wann sie zu lesen ist |
|---|---|
| **[docs/feature-referenz.md](docs/feature-referenz.md)** | **Bevor du an einem Feature arbeitest.** Mechanik und Begründungen zu: Rendering/Glyph-Atlas · Terminal-Verhalten (Maus, Tasten, Scrollback, Links, OSC) · PTY-Layer · Sessions & UI (Persistenz, Gruppen, Palette, Prefs, Agenten-Wiederherstellung, Warteschlange) · Online-Update inkl. Proxy · QML-/Theming-Lektionen · macOS-Spezifika · Vault/Profile · Plugins · MCP-Server. |
| **[docs/e2e-fallen.md](docs/e2e-fallen.md)** | **Bevor du irgendetwas misst**, das über `ctest` hinausgeht — MCP-E2E, `--screenshot`, GUI-Automatisierung, Messung an einer laufenden Instanz. Enthält die Fälle, in denen das **Messmittel** log (falsch-positive Gegentests, Detektor-Blindheit, Screenshot-Eigenheiten je Plattform). |

⚠️ **Die Auslagerung ist keine Erlaubnis, sie zu überspringen.** Wer eine Änderung an einem
Feature plant oder eine Verifikation aufsetzt, ohne den zuständigen Abschnitt gelesen zu
haben, läuft mit hoher Wahrscheinlichkeit in eine dort bereits dokumentierte Falle — mehrere
davon erzeugen ein **grünes, aber wertloses** Ergebnis. Beide Dateien hängen im Doku-Wächter
`test_doc_duplicates`, werden also wie diese Datei auf Kompaktierungs-Duplikate geprüft.
