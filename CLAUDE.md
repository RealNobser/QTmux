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
| `tests/` | **27** ctest-Tests: 26 QtTest-Binaries (pty, vtscreen, linkdetector, session, sessiongroups, windowmodel, agent, profiles, hotkeys, vault, sftp, plugins, agenteventhub, macpcan, keyencoding, terminalsearch, terminalgrid, settingsio, i18n, shellintegration, gitinfo, projectcommands, promptqueue, updater, updateviewmodel, **mcpaccess**) + `test_doc_duplicates` (reines CMake-Skript). `test_i18n` entsteht nur, wenn `qtbase_*.qm` in der Qt-Installation liegt — sonst 26. Zahl per `ctest -N` gegenprüfen, nicht schätzen |

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
- **Versions-Bump-Stellen** (alle zusammen): `CMakeLists.txt` (project VERSION),
  `installer/build-dmg.sh`, `installer/build-appimage.sh`, `.github/workflows/ci.yml`
  (AppImage-Schritt), `installer/QTmux.wxs`, `README.md` (DE **und** EN).
  🔑 `src/app/main.cpp` und `src/server/McpServer.cpp` stehen seit QTMUX-125 **nicht mehr**
  in der Liste: beide lesen `QTMUX_VERSION_STRING` aus dem generierten `qtmux_version.h`
  (`cmake/Version.h.in` ← `PROJECT_VERSION`). Die übrigen Stellen laufen ohne
  CMake-Konfiguration und bleiben darum manuell.
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

## Arbeitsstand & Wiedereinstieg (2026-08-04)

> Die EINE Stelle für den aktuellen Stand (Pflegeregeln 2–4 oben). Verlauf steht in
> Git/Jira/Confluence; Feature-Mechanik in der Feature-Referenz; Abnahme-Rezepte in
> [docs/owner-abnahmen.md](docs/owner-abnahmen.md).

**Ausgeliefert: v1.8.0** — Tag auf `4f10eb8`, alle 4 Installer, Manifest live unter
`https://nobser.de/updates/qtmux/`; der volle Update-Zyklus ist am lebenden Objekt
verifiziert (Details im Abschnitt „Online-Update"). Jira dual synchron bis **QTMUX-128**.

**Teststände:** **27** Tests (s. Dateitabelle). macOS Debug/Release je **27/27** (dort läuft
`test_pty` mit und besteht); Linux (rtzsvr02-Container) und Windows je **26/26** —
`test_pty` per `-E` ausgenommen (umgebungsbedingt: nicht-interaktive Shell/ConPTY; unter
Windows braucht `ctest` zusätzlich Qt-`bin` im PATH, sonst `0xc0000135`). Größte Binaries:
`tst_session` 24 Fälle, `tst_vtscreen` 24, `tst_agent` 20.
🔑 Der **CI**-Linux-Job ist nicht der rtzsvr02-Container: dort läuft `test_pty` mit und
besteht (**27/27**, am Lauf zu `3b63f00` abgelesen). Ein „26 statt 27" aus dem Container ist
also kein Widerspruch, sondern die Ausnahme per `-E`.
🔑 **„CI grün auf allen drei Plattformen" ist KEIN Vollständigkeitsbeleg** (Lektion aus
QTMUX-124): Die CI baut **Release**, ebenso Homebrew-Qt und der Linux-Container —
**Debug-only-Asserts in Qts vorgebauter Bibliothek sieht sie prinzipiell nicht**; der
Windows-**Debug**-Build ist dafür das einzige taugliche Messmittel (`-DQT_FORCE_ASSERTS`
trägt nicht, es wirkt nur in den *Headern*).

### Nächster Schritt (Wiedereinstieg nach /compact)

Stand **2026-08-05** · Branch `main`, alles committet **und gepusht**: QTMUX-128 +
QML-Editor-Tooling liegen in `3b63f00`, der Wiedereinstiegs-Anker in `923e8fc`.
Bei Wiedereinstieg `git log --oneline -3` gegenprüfen — die Windows-Session pusht ebenfalls.
✅ **Die Mac-Nachlese zu QTMUX-128 ist vollständig abgearbeitet** (2026-08-05): CI geprüft,
macOS + Linux gebaut und getestet, Jira dual angelegt und auf Done, Prefs-Zeile in beiden
Designs bildlich abgenommen, Confluence-Benutzerdoku dual ergänzt. Einzelheiten unten.

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

**Danach:** Owner-Durchklick der **28 fertigen Tickets** ([docs/owner-abnahmen.md](docs/owner-abnahmen.md))
· QTMUX-122/123 (OSC 52 + Hinweis bei Maus-Grab) · Proxy-Unterstützung erst, wenn MacPCAN
sie in `appupdate` geliefert hat (s. u.).

✅ **Mac-Nachlese QTMUX-128 — erledigt am 2026-08-05** (die Liste stand hier als „Was nur der
Mac tun kann"; sie ist abgearbeitet, die dauerhaft nützlichen Ergebnisse bleiben):
1. **Jira dual auf QTMUX-128** — beide Systeme standen frisch geholt auf 127, der Key ist
   also deckungsgleich; beide auf Done/Erledigt mit Kurzkommentar.
2. **macOS Debug + Release je 27/27, Linux (rtzsvr02-Container) 26/26.**
3. **Prefs-Zeile in beiden Designs abgenommen** — Bild sauber, keine QML-Warnung beim Start.
   🔑 Der Weg dorthin ist dauerhaft nützlich: Das Prefs-Fenster ist ein eigenes `Window` und
   mit `--screenshot` auf macOS prinzipiell nicht greifbar. Was trägt, ist ein **temporärer**
   QML-Hook in [qml/Main.qml](qml/Main.qml) — Timer → `prefs.open("<kategorie>")`, zweiter
   Timer → `prefs.contentItem.children[0].grabToImage(…)` → `Qt.quit()`; der Pfad kommt aus
   `Theme.dark`, also liefern zwei Läufe mit `defaults write com.qtmux.QTmux-<profil>
   ui.themeMode -int 1|2` beide Designs. Danach Hook entfernen und neu bauen.
   ⚠️ `timeout` gibt es auf macOS nicht (Exit 127) — solche Läufe in den Hintergrund geben.
4. **Confluence-Benutzerdoku dual ergänzt** (neuer Abschnitt „Mausrad in Agenten-Oberflächen"
   hinter „Verlauf (Scrollback)"; on-prem v19, Cloud v18, Umlaute beidseitig gegengelesen).

### Zuletzt abgeschlossen (Details in Git/Feature-Referenz)

- ✅ **QTMUX-128 (Mausrad in Codex tot)** — umgesetzt und E2E verifiziert (2026-08-03,
  Anwenderbefund, auf der Windows-Maschine gebaut). Mechanik in der Feature-Referenz,
  libvterm-Patch 3 im Kasten oben, Regel in [AltScroll.h](src/core/AltScroll.h),
  Scroll-Tasten je Agent in der `AgentRegistry`, Einstellung `window/altScrollMode`
  (Vorgabe „nur auf Anforderung", umschaltbar auf „immer" — Owner-Entscheidung).
  Messstand: Windows Debug **und** Release je 26/26, drei neue Fälle namentlich als PASS
  belegt; Gegentest ohne den libvterm-Patch lässt genau die zwei parse-abhängigen Fälle
  fallen. E2E gegen eine isolierte Instanz mit echtem **Codex 0.146.0**: eine Rad-Rastung
  scrollt, eine zurück führt exakt zum Ausgangsbild (auf nachweislich ruhigem Bildschirm),
  Gegenkontrolle mit unerkanntem Agenten wirkungslos. Das Messgerüst (synthetische
  `QWheelEvent` wie QTMUX-100, dateigesteuert) ist wieder entfernt.
- ✅ **QML-Editor-Diagnosen entrümpelt** (2026-08-04, Anwenderbefund „über 2000 Probleme"):
  jetzt **2**, beide echt. Ursachen und Abhilfen in den QML-Lektionen; die Begründungen je
  Kategorie stehen in [.qmllint.ini](.qmllint.ini). Dabei **im Code** behoben: 4 echte
  Layout-Fehler (`width`/`height` an Layout-Kindern → `implicitWidth`/`implicitHeight`, am
  Screenshot gegengeprüft) und 17 blockbereichs-`var` in `Main.qml` → `let`.
- ✅ **QTMUX-127 (MCP im LAN)** — Bind-Adresse konfigurierbar, Token-Pflicht außerhalb von
  Loopback, Startverweigerung ohne Token, Request-Deckel, `get_server_info`. Mechanik in der
  Feature-Referenz, Netzebene in [tools/pf/](tools/pf/).
  🔑 Offen: Sichtprüfung der Einstellungsseite (auf macOS ist das Prefs-Fenster mit
  `--screenshot` prinzipiell nicht greifbar — eigenes `Window`) und die **pf-Installation**
  auf dem Zielrechner (`sudo` verlangt hier ein Passwort; Skript ist fertig und trocken
  geprüft, `pfctl -n -f` sauber, aber nicht geladen). Application Firewall dort aus.
- ✅ **QTMUX-124 (Windows-Absturz)** und **QTMUX-125 (Online-Update)** erledigt und
  gegengeprüft; die dauerhaften Lektionen daraus stehen im Kasten oben bzw. im Abschnitt
  „Online-Update". `.github/workflows/ci.yml` hat beide Dritt-Actions auf Commit-SHAs
  gepinnt.

### Offene Fäden (dauerhaft relevant)

- 📋 **Owner-Anforderung Proxy fürs Firmenumfeld** (2026-08-03) — in
  [docs/workorder-online-update.md](docs/workorder-online-update.md), **nicht begonnen**.
  ⛔ **Keine eigene Implementierung:** Der Mechanismus entsteht kanonisch in der Shared-Lib
  `appupdate` (MacPCAN ist der Hub), QTmux **erbt ihn über das Vendoring** — nachvendieren,
  `tools/check-updater-sync.sh` muss danach grün sein, `UPSTREAM.md` nachziehen. QTmux-Anteil
  ist nur die App-Hälfte (Settings-Keys `update/proxy*`, Prefs-Abschnitt, Auth-Abfrage im
  Dialog, ViewModel-Properties, i18n), weil die Lib GUI- und QSettings-frei bleibt.
  🔑 **Die Zuarbeit an MacPCAN ist geliefert** (Abschnitt „Zuarbeit an MacPCAN" in derselben
  Datei) — Settings-Keys, der zweistufige Auth-Weg (`proxyAuthenticationRequired` ist
  **synchron** und damit als QML-Rückfrage untauglich → Anmelde-Lieferant + typisierter
  Fehler + **genau ein** Versuch wegen AD-Kontosperre), Anmeldedaten im Sitzungsspeicher
  (Keychain abgelehnt, Vault startet gesperrt). Nicht erneut erarbeiten.
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
- **Architektur-Landkarte (Vollanalyse 2026-08-01, nicht beauftragt):** Die C++-Seite ist
  sauber (Gui-freier Core bestätigt, keine Include-Zyklen, keine Lifetime-Probleme); die
  Schulden sitzen in **`qml/Main.qml`** (~4.700 LOC, 136 Funktionen: Split-Baum,
  Layout-Persistenz, Aggregationslogik als ungetestetes JS, zwei divergierende
  Layout-Serialisierer). Abbaupfad: (1) Sidebar (~735 LOC) + Inline-Dialoge (~945 LOC) in
  eigene QML-Dateien, (2) Layout-Baum + Persistenz als testbare C++-Klasse in `core`, (3)
  damit entfällt die QML-Brücke — **13 der 39 MCP-Tools brauchen heute die geladene UI**,
  weshalb der McpServer keinen einzigen Test hat (25 Tools wären schon jetzt testbar).
  Kleinere Punkte: `Session` ist ein God-Object (~40 Member; Extraktionskandidaten
  AgentDetection/LoginAutomation/CwdTracker) · 22 attached `ToolTip` über
  [IconToolButton.qml](qml/Ui/IconToolButton.qml) statt `AppToolTip` · Statusfarben-Literale
  ~10× dupliziert (Kandidat StatusColors-Singleton) · `SessionModel::sessionById` fehlt
  `Q_INVOKABLE`. Umgesetzt daraus: `Theme.accentText` statt hartem Weiß (6 Stellen).

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
**122** (**OSC 52**: Zwischenablage aus dem Terminal füllen. Anwenderbefund — aus einem
Claude Code über `ssh` ließ sich nichts kopieren: die Anwendung hält Maus-Tracking +
Alt-Screen, QTmux reicht die Maus durch (QTMUX-104), markiert wird **in der Anwendung**,
QTmux hat gar keine Auswahl) ·
**123** (sichtbarer **Hinweis**, wenn eine Vollbild-Anwendung die Maus hält — ohne ihn
findet man die Shift-Geste nicht; dieselbe Erfahrung wie bei den Links in QTMUX-39) ·
**40** (OSC-8-Hyperlinks — deferred; bräuchte Cursor-Span-Tracking + neues `Cell`-Feld,
teuer, da `VtScreen` den Sichtbereich lazy aus libvterm bildet) ·
**13** (native macOS-Menü-Icons — deferred; Qt reicht `icon.source`/`icon.name` in nativen
Menüs nicht durch, einziger Weg wäre ein QMenuBar-Umbau; [[qtmux-native-menu-icons]]) ·
**126** (Marken-Badge „Q"/Violett in Menüleiste + App-Icon, Backlog; Spec
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

Siebenstufiger Umbau nach der Design-Anweisung „Menü-Struktur und Design-Auffrischung"
(Original im Claude-Design-Projekt `ab66e9b5-053b-4e81-9e4a-c45752fd42d1`, über `DesignSync`
`get_file`). **Komplett und auf allen drei Plattformen grün** (Teststände oben). Die sieben
Stufen (1/2 einklappbare Seitenleiste + Flyout · 3 Statusleiste · 4 sechs Menüs · 5
Einstellungsfenster mit `PrefRow`/`SegmentedControl`/`AppSwitch` · 6 `SettingsIo`
Reset/Import/Export · 7 i18n + README-Screenshots) stehen samt Commits in der Git-Historie.
**Randbedingungen (dauerhaft):** Qt Quick Controls **Basic**, keine neuen Effekte, Chrome-
Farben nur über `Theme.*` (Ausnahme: Statusfarben), jede neue Zeichenkette in `qsTr` + beiden
`.ts`. Die temporär eingecheckte `Arbeitsanweisung-1a-2a.md` ist nach Stufe 7 wieder
entfernt; bei Bedarf gilt das Original im Design-Projekt.

**Bewusste Abweichungen von der Anweisung** (Mechanik jeweils in der Feature-Referenz):
Stufe 4 — keine **macOS-Menü-Rollen** (QtQuick.Controls kennt sie nicht → „Einstellungen …"
bleibt überall im Datei-Menü) · Umbenennen/Gruppe wirken aufs **Window** (QTMUX-83, die
Anweisung beschrieb das Split-Modell) · „Alle nach vorne" fehlt (ein Fenster je Prozess).
Stufe 5 — `PrefGroup` und `AppSwitch` sind **zusätzliche** Dateien (Rahmen und Schalter
brauchen einen Ort) · listenartige Seiten (Tastenkürzel, Verbindungen, Vault, Erweiterungen)
bleiben strukturell wie sie sind · Abo-Matrix behält Toggle-Kacheln (QTMUX-47) · die zwei
Kopfzeilen-Knöpfe folgen mit Stufe 6.
Stufe 6 — **Export ist eine Allowlist, nicht „alles außer dem Vault"** (Begründung in der
Feature-Referenz: kein Leck durch Wachstum, und ein Reset darf das Fenster-/Session-Layout
nicht mitnehmen) · Rückmeldung nach Reset/Import erscheint als Zeile am Fuß der Rail, nicht
als weiterer Dialog · beim Import werden unbekannte Schlüssel **angezeigt und übersprungen**
(die Anweisung sagt dazu nichts).
Stufe 7 — README-Bilder über ein **sichtbares cocoa-Fenster + `grabWindow()`** (der
Offscreen-Grab lässt das custom `TerminalItem` leer; die native macOS-Menüleiste ist global und
nicht im Fenster-Grab); das separate Einstellungsfenster via QML-`grabToImage` des PrefsWindow
(`prefs.contentItem.children[0]` — der C++-`contentItem` selbst hat keine QML-Engine).

Die **Rastergröße** ist seit QTMUX-120 nachgeliefert (`Session::cols/rows` +
`window.windowGridText`, s. Feature-Referenz).

**Offen (bewusst nicht behauptet):** der **Owner-Durchklick** der GUI — die nativen
macOS-Menüs, Flyout, Statusleiste, Einstellungsfenster und `App.reduceMotion` in beiden
Designs, dazu Export/Import durch die **echten** Dateidialoge (auf keiner Maschine
automatisierbar). Vollständige Liste mit Rezept:
[docs/owner-abnahmen.md](docs/owner-abnahmen.md).

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

## Feature-Referenz (kompakt, mit Lektionen)

### Rendering (GPU-Glyph-Atlas, QTMUX-6)
`TerminalItem` = `QQuickItem` mit eigenem `QSGMaterial` + RHI-Shadern
(`src/terminal/shaders/glyph.{vert,frag}`, via `qt_add_shaders`); `GlyphAtlas` rastert
zellweise Alpha-Masken (Shelf-Packer, wächst in der Höhe) + **Glyph-Index-Atlas** für
Ligaturen (`glyphByIndex`, `QTextLayout`-Run-Shaping — Atlas durch Glyph-Zahl des Fonts
begrenzt). Farb-Emojis: `tileHasColor()`-Erkennung, Vertex-Alpha als Mono/Farb-Selektor
im Shader. **Damage-Gating:** teurer Inhalt nur bei `m_geomDirty`, Overlay
(Selektion/Cursor/Scrollbalken) billig bei jedem Update.
- 🔑 Custom-Material: Textur in `updateSampledImage` per **`commitTextureOperations`**
  hochladen — sonst Glyphen unsichtbar (`QSGSimpleTextureNode` macht es intern, wir nicht).
- 🔑 **Renderpfad-Tests müssen beweisen, dass der Pfad aktiv ist** (Fallback absichtlich
  brechen oder loggen) — der GPU-Ligatur-Code war einmal toter Code (`useGpu()`-Bedingung
  nicht geändert) und die „Verifikation" lief unbemerkt über den korrekten Fallback.
- Fallback: `gpuRendering=false` / Env `QTMUX_NO_GPU=1` → `QPainter`-Pfad (Run-basiert).
- 🔑 **Keine Glyphe darf über ihre Kachel hinausmalen (QTMUX-97).** `glyph()` zeichnet mit
  hartem `setClipRect(rect)`; passt die **Tinte** trotz korrekter Zellzahl nicht, wird
  proportional eingepasst (gemessen am `tightBoundingRect`, **nicht** am Advance — Emoji-
  Bitmaps haben Seitenränder, ein Advance-Vergleich verkleinerte sonst jedes Doppelzellen-
  Emoji grundlos). Vorher blutete ein Farb-Emoji (**2,15 Zellen breit**) in die
  **Nachbarkachel** des Shelf-Packers; der Überhang blieb dort stehen (spätere Glyphen
  werden nur *darüber*gemalt) und — perfider — `tileHasColor()` stempelte den verunreinigten
  Nachbarn als **Farb-Glyphe** ab, die der Shader dann nicht mehr einfärbt. Symptome daher
  zweierlei: fremde Emoji-Bruchstücke auf beliebigen Zeichen **und einzelne Buchstaben in
  Emoji-Farbe**. Wen es traf, entschied allein die Einfüge-Reihenfolge in den Atlas → wirkte
  zufällig. Die Wurzel lag aber in libvterm (VS-16-Breite, s. o.): Der Atlas-Clip ist das
  Sicherheitsnetz, die Zellbreite der Fix. Messung: Überhang **21 px → 0 px**; live per MCP
  `read_screen` gegengeprüft (114 Spalten: **57** ⚠️ pro Zeile statt 100 in einer).

### Terminal-Verhalten
- **Scrollback** (Cap 10000) in `VtScreen`; Selektion in **absoluten** Inhalts-Zeilen
  (scroll-fest); **Soft-Wrap-Copy** via `sb_pushline4`-Continuation-Flags (eine logische
  Zeile ohne `\n` am weichen Umbruch).
- **Maus-Reporting:** `VtScreen` trackt `VTERM_PROP_MOUSE` (DECSET 1000/1002/1003);
  `TerminalItem` leitet Rad/Klick/Drag an libvterm (X10/SGR-Sequenzen), sonst lokaler
  Scrollback/Selektion; **Shift+Drag** selektiert immer lokal. macOS: Cmd=ControlModifier,
  physisches Ctrl=Meta. libvterm **entprellt** (Tests brauchen press→release-Paare).
  Hover-only-Tracking (1003 ohne Taste) nicht gemeldet.
  🔑 **Nur im Alternate Screen weiterleiten (QTMUX-104).** Bedingung ist
  `appMouseActive() = mouseTracking() != 0 && altScreen()`. Grund: Endet ein Maus-TUI
  **unsauber** (Crash, `kill`, SSH-Abbruch), kommt kein `DECRST` und das Tracking-Flag bleibt
  hängen — die zurückkehrende Shell füllte sich sonst bei jeder Mausbewegung mit SGR-Codes
  (`35;63;49M…`). Vollbild-TUIs (Agenten, vim, htop, less) laufen im Alt-Screen, ihre Maus
  bleibt also intakt; die Shell am Prompt ist im Primary Screen → dort nie weiterleiten.
  🔑 **Voraussetzung: `vterm_screen_enable_altscreen(m_screen, 1)`** im Konstruktor (VOR
  `reset`). Ohne das meldet libvterm **kein** `VTERM_PROP_ALTSCREEN`, und `altScreen()` bliebe
  immer false (Gegenprobe: Test `altScreenTracked` FAIL). Nebeneffekt ist zugleich korrektes
  Terminal-Verhalten: Nach einem TUI kehrt der vorherige Shell-Inhalt zurück (Alt-Screen-Puffer);
  am Scrollback-Dump (QTMUX-81, `serializeAnsiRoundTrip`) ändert sich nichts.
  ⚠️ **Falle beim Testen des Alt-Screen-Inhalts über MCP:** `1049h` blendet um, ein echtes TUI
  **löscht** den Alt-Screen selbst — ein Testskript ohne `\033[2J` zeigt darum Reste des
  Primary, was wie ein fehlender Wechsel aussieht. Mit `clear`/sleep im Alt-Screen sauber
  messbar (dann nur der Alt-Inhalt sichtbar).
  🔑 **Manueller Notausgang (QTMUX-104):** `VtScreen::resetInputModes()` speist DECRST-Sequenzen
  (1000/1002/1003/1006 Maus, 2004 Bracketed Paste, SGR-Reset, Cursor sichtbar) in den EIGENEN
  Parser — `m_mouseTracking` geht über den regulären `cbSetMouse`-Pfad auf 0, **ohne** Bildschirm
  oder Alt-Screen anzutasten. In der GUI „Terminal-Eingabe zurücksetzen" (Ctrl/Cmd+Shift+I,
  Menü, Palette). Für die Fälle, in denen auch der Alt-Screen hängt. Tests
  `altScreenTracked`, `resetInputModesClearsMouse`.
  🔑 **1007 wird dabei bewusst NICHT gelöscht** (QTMUX-128): Ein hängendes Alternate-Scroll-Flag
  richtet keinen Schaden an (das Rad wird nur im Alt-Screen zur Taste), aber es zu löschen würde
  einem *laufenden* Codex das Rad abschalten — der sendet 1007h nie erneut.
- **Mausrad, wenn die App ihren Verlauf selbst zeichnet (QTMUX-128):** QTmux kann nur scrollen,
  was in **seinem** Scrollback liegt, und dorthin gelangen ausschließlich Zeilen, die oben aus
  dem **Primary** Screen herausgeschoben wurden. Zwei Fälle füllen ihn nie: ein Vollbild-TUI im
  Alt-Screen (libvterm schiebt Alt-Screen-Zeilen bewusst nicht hinein) — **und** eine Anwendung,
  die im Primary Screen einen festen Sichtbereich an Ort und Stelle neu zeichnet. Dann ist
  `scrollByLines` ein No-op und das Rad wirkt „festgenagelt" (genau der Anwenderbefund). Regel
  Gui-frei in [src/core/AltScroll.h](src/core/AltScroll.h) (`wheelGoesToApp`, Enum
  `AltScrollMode`), Einstellung `window/altScrollMode` (Eingabe & Zwischenablage, Palette,
  Suchindex), Tastenfolge je Agent in der `AgentRegistry` (`scrollKeysFor`).
  🔑 **Der teuerste Irrtum dieses Tickets — zwei falsche Schlüsse in Folge, beide von einer
  Messung widerlegt:** (1) „Live-Bildschirm == Scrollback ⇒ Alt-Screen" ist **kein** gültiger
  Schluss — ein leerer Scrollback heißt nur, dass nie etwas oben herausgeschoben wurde. Codex
  läuft nachweislich im **Primary** Screen (`altScreen()` = false, im `wheelEvent` protokolliert).
  (2) Codex **bringt `ESC[?1007h` im Binary mit, sendet es aber nicht** — und reagiert auch nicht
  auf das, was 1007 verspricht: einfache Cursor-Tasten, SS3-Form, PageUp/PageDown und End
  bewirken bei ihm **nichts**, es scrollt allein auf **Shift+Pfeil** (`ESC[1;2A`/`B`; am
  laufenden Codex 0.146.0 gemessen, Rundlauf exakt umkehrbar). Eine Umsetzung nach reiner
  Sequenz-Archäologie hätte also fehlerfrei gebaut, getestet und beim Anwender **nichts**
  bewirkt.
  🔑 **Zwei Klauseln tragen die Sicherheit:** Im Primary Screen wird nur delegiert, wenn (a) für
  den erkannten Agenten eine **gemessene** Taste vorliegt und (b) QTmux selbst **keinen**
  Scrollback hat. (a) verhindert Tasten ins Blaue — an einem Shell-Prompt blättern Cursor-Tasten
  die Befehls-Historie durch; (b) gibt dem eigenen Verlauf des Anwenders Vorrang und heilt
  zugleich „Agent beendet, Shell wieder da". Im Alt-Screen wird (b) bewusst **nicht** geprüft:
  Was dort liegt, stammt vom Primary Screen und wäre der falsche Inhalt.
  ⚠️ **Bekannte Grenze:** Der Weg hängt an der Agenten-**Erkennung**, und die prüft nur den
  ersten Token (`cd X; codex` erkennt nichts, s. QTMUX-88) — dann bleibt das Rad tot. Das war
  zugleich die Gegenkontrolle: identische App, unerkannter Agent → keine Wirkung.
- **Kopieren nimmt die Auswahl, nicht den Fokus (QTMUX-105):** Cmd+C/Kopieren läuft über
  `window.copyActiveSelection()` — bevorzugt das aktive Pane, fällt aber auf das erste Pane
  mit `hasSelection` zurück. `activeHasSelection` (treibt `actCopy.enabled` und damit den
  Cmd+C-Shortcut) berücksichtigt **jedes** Pane, nicht nur das aktive; dazu meldet jedes Pane
  in [SplitNode.qml](qml/SplitNode.qml) sein `selectionChanged` ans Fenster. 🔑 **Warum:**
  Der sporadische „Cmd+C kopiert nichts"-Bug entstand, weil der Fokus nach dem Selektieren
  wegwandern kann (anderes Pane, Statusleiste, Flyout, Suchfeld — mit Design 1a/2a mehr
  Fokus-Fänger); `activeTerminal.copy()` fand dann keine Auswahl. `TerminalItem::copy()`
  protokolliert den **Problemfall** (leerer `selectedText()`) leise nach Console.app —
  fängt eine etwaige andere Ursache (reine Whitespace-Auswahl wird zu `""` getrimmt), ohne
  Rauschen im Normalbetrieb. Ohne Bedienungshilfen-Recht nicht per synthetischem Cmd+C
  reproduzierbar → Code-Review + Diagnose.
- **Bildschirm leeren, Verlauf behalten (QTMUX-61):** `VtScreen::clearViewportKeepScrollback()`
  schiebt alles **oberhalb der Cursorzeile** in den Scrollback; die Prompt-Zeile rückt nach
  oben. Umgesetzt als **CSI `<n>` S** (Scroll Up) in den **eigenen** Parser (`inputWrite`) —
  nicht ans PTY: Ein getipptes `clear` verwirft je nach Agent/TUI den Verlauf und landete im
  Eingabefeld eines laufenden Agenten. Danach muss der Cursor per CSI H **selbst** gesetzt
  werden, CSI S bewegt ihn nicht mit (Gegenprobe ohne diese Zeile: Test FAIL).
  Bei Cursor in Zeile 0 passiert nichts — CSI **0** S würde als „rolle um 1" gelesen und die
  Prompt-Zeile schlucken. Kürzel `Ctrl/Cmd+Shift+K` (nicht Ctrl+K — das gehört der Shell),
  Ansicht-Menü, Palette. Test `tst_vtscreen::clearViewportKeepsScrollback`.
  🔑 `screenText()` schneidet **rechte Leerzeichen** ab — ein `startsWith("$ ")` im Test
  scheitert also, obwohl die Zeile korrekt steht; direkt an `cell(0,0)` prüfen.
- **Tasten:** Übersetzungslogik Gui-frei in `src/core/KeyEncoding.cpp` (`encodeKeyBytes`,
  Test `test_keyencoding`); `TerminalItem::encodeKey` delegiert nur. F1–F12 als
  xterm/VT220-Sequenzen (F-Tasten gehören der Shell — keine globalen F-Tasten-Shortcuts);
  **Shift/Alt+Enter → ESC CR** (QTMUX-43: Umbruch einfügen statt absenden in Agenten-TUIs
  wie Claude Code — dieselbe Sequenz, die `/terminal-setup` anderswo auf Shift+Enter legt;
  klassische Shells binden ESC CR nicht, unter ConPTY kann das ESC die Eingabe verwerfen —
  bewusst in Kauf genommen, unmodifiziertes Enter bleibt CR). Copy/Paste macOS Cmd+C/V,
  sonst Ctrl+Shift+C/V; Smart Ctrl+C (Auswahl→Copy, sonst SIGINT). Bracketed Paste +
  Multiline-Warnung; Copy-on-Select + Rechtsklick-Paste optional.
- **Meta-Kodierung Alt+&lt;Zeichen&gt; → `ESC`+Zeichen (QTMUX-84):** `encodeMetaSequence`
  (eigene Funktion, damit sie **plattformunabhängig** testbar bleibt) + Gate
  `metaPrefixEnabled()` = **Windows/Linux, auf macOS aus** (dort erzeugt Option
  Sonderzeichen und physisches Ctrl ist bereits Meta). Damit kommen Claude Codes **Alt+V**
  (Bild aus der Zwischenablage — unter Windows, weil Ctrl+V dort Text-Paste ist) und die
  readline-Kürzel Alt+B/F/D beim Agenten an. Kodiert wird im `default`-Zweig, Enter (QTMUX-43),
  Backspace und die Steuertasten bleiben also unberührt.
  🔑 **Teuerste Falle: AltGr meldet Windows als Ctrl+Alt.** Ohne die Ausnahme
  `if (mods & Qt::ControlModifier) return {}` würden auf deutschen Tastaturen `@` (AltGr+q),
  `€`, `\ ~ | [ ] { }` zerstört — der Fix wäre schlimmer als der Fehler. Am echten Layout
  gegengeprüft (`keybd_event` mit VK_RMENU): `altgr:@` bleibt `@`.
  🔑 Zwei Quellen für das Zeichen: ist `text()` gefüllt (Linux), wird es layout-treu
  übernommen; ist es leer (Windows bei Alt+Buchstabe), wird das Zeichen aus dem **Key-Code**
  gebildet (`Qt::Key_V == 'V'`, ohne Shift kleingeschrieben). Nicht-ASCII-Keycodes ohne
  `text()` werden abgelehnt, statt ein Zeichen zu erfinden.
- **Klickbare Links (QTMUX-39):** `LinkDetector` (Gui-frei) findet **URLs**
  (Scheme-Whitelist http/https/ftp/mailto/file — KI-Output darf keinen beliebigen Handler
  starten) und **existierende Dateipfade** (gegen Session-CWD; die `QFileInfo::exists`-Prüfung
  IST der Fehlalarm-Filter). Unterstreichung + Hand-Cursor schon beim **Hover**, Pane-Pille
  „⌘/Strg-Klick zum Öffnen: <ziel>" (`hoverLinkTarget`); das **Öffnen** bleibt an
  Cmd/Ctrl-Klick (`QDesktopServices::openUrl`) — bewusste Geste gegen versehentliches Öffnen.
  🔑 Erkennung lief zuerst nur bei gehaltenem Modifier (Syscall-Sparen) — ohne sichtbaren
  Hinweis fand der Anwender die Geste nicht. Jetzt Hover, aber **je Zeile gecacht**
  (`m_hoverDetectRow`), nicht je Pixel. Klick läuft **vor** der App-Maus-Weiterleitung.
  Zeilentext aus `absLineText(absRow)` (Spalten↔Zeichen 1:1, solange kein Emoji davor).
  Tests: `tst_linkdetector` + `tst_vtscreen::linkDetectionOnScreenLine`.
  **OSC 8 bewusst NICHT** — s. offene Jira (QTMUX-40).

### PTY-Layer
- `UnixPty`: forkpty, O_NONBLOCK-Master. **⚠️ `write()` ist gepuffert** (`pending` +
  `pendingPos` + Write-`QSocketNotifier`, nur aktiv solange etwas wartet) — der Kernel
  nimmt nur ~1 KB pro `::write()`; ohne Pufferung ging alles darüber **still verloren**
  (QTMUX-28; Regressionstest `tst_pty::largeWriteIsNotTruncated`).
- `terminate()`: Prozessbaum-Kill (SIGHUP→SIGKILL via `descendantPids`); das Abernten
  läuft im **detached Thread** (blockierendes `waitpid` auf schwere node-Bäume fror sonst
  die GUI sekundenlang ein); App-Quit-Sonderpfad `Pty::s_quitting` = synchron ohne
  `waitpid` (damit `nohup`-Nachfahren vor Prozessende sterben).
- **Login-Shell:** `argv[0] = "-zsh"` (optionaler `argv0`-Parameter in `Pty::start`) —
  nur für echte Shells ohne eigene Args; sonst fehlen `~/.zprofile`/Homebrew-PATH.
- `currentWorkingDirectory()`: macOS libproc, Linux `/proc`, Windows PEB — Funktionstest
  bestanden; Details und die prinzipielle PowerShell-Grenze im ConPTY-Abschnitt oben.

### Sessions & UI
- **Persistenz:** Session-Liste (Typ, Serial/SSH-Parameter, CWD) via QSettings;
  `m_shuttingDown`-Guard (sonst leert `shutdownAll` den gespeicherten Zustand),
  `m_restoring`-Guard (Restore erbt kein fremdes CWD, führt keine Login-Scripts aus).
  Neue Shell **erbt das Live-CWD** der aktiven Session (nur Shell-Quellen, explizites
  Verzeichnis hat Vorrang).
- **Gruppen in der Sidebar (QTMUX-42/45, seit QTMUX-83 **Window**-Gruppen):** Frei benannte,
  einklappbare Gruppen mit Kopfzeile + Anzahl; Farbe aus dem Namen gehasht. Zuordnung per
  Rechtsklick, Palette oder MCP (`set_window_group`, `set_session_group` wirkt aufs Window
  der Session). 🔑 Angezeigt über **`ListView.section`** → verlangt **zusammenhängende
  Blöcke**, also sortiert das **Model** um, nicht die View. Drei Fallen (Model-Teil durch
  `tst_sessiongroups` abgesichert): Umgruppieren darf **nicht** über `moveSession` laufen
  (Drag übernimmt bewusst die Gruppe der Nachbarschaft und überschriebe die neue);
  **gruppenlose** Einträge sind KEIN schützenswerter Block (unsichtbare Section — sonst
  springt die erste Zuordnung ans Listenende); `groups()`/`groupSize()` sind Funktionen ohne
  Property → QML braucht den Anker `groupsChanged`/`groupsRevision`, sonst frieren Kopfzeile
  und Kontextmenü ein.
  🔑 **Einzug statt nur Farbe:** Gruppierte Kacheln sind 12 px eingerückt (Form erkennbar,
  nicht nur Farbe), die Farbmarke sitzt in der Einzugsspalte, der rote MCP-Controller-Tab am
  Rand der Kachel — vorher teilten sich beide den Kachelrand und die Marke war per
  `!mcpController` abgeschaltet, also genau an der interessantesten Kachel unsichtbar.
  Eingerückt wird der **Inhalt über Margins**, NICHT die Delegate-Wurzel: ein `x`-Binding
  dort ist wirkungslos (die ListView setzt die Querachse selbst) und schob die Marke aus dem
  `clip:true`-Viewport. Ein inneres `card`-Rechteck trägt Auswahl/Hover.
- **Befehlspalette (Strg/Cmd+K):** Das Such-/Befehlsfeld in der Toolbar ist die zentrale
  Sammelstelle **aller** Funktionen — feste Befehle plus dynamisch je Plugin-Backend, je
  Verbindungsprofil, je Sitzungsgruppe und je offener Session („Wechseln zu: …"). Sie wird
  bei **jedem Öffnen** neu gebaut (`buildCommands()` in `openFor()`), dynamische Einträge
  sind also immer aktuell; Kürzel-Anzeige kommt live aus `Hotkeys.bindings`. 🔑 Neue
  Funktionen gehören HIER hinein, sonst entsteht die Schieflage aus QTMUX-46: Ein Schalter
  lag nur im Einstellungsdialog (`confirmQuit`), Gruppen nur im Rechtsklick — beides in der
  Palette unauffindbar. Faustregel: Was per MCP steuerbar ist, muss auch die Palette können
  (Ausnahme Vault — bewusste Sicherheitsgrenze).
- **Session-Größe wird entprellt (QTMUX-86):** `recomputeGrid()` reicht das Raster **nicht
  sofort** an die Session, sondern über einen 60-ms-Einmal-Timer (`m_resizeTimer` →
  `applyPendingResize`). Grund: Beim Auf-/Abbauen des Pane-Baums (Window-Wechsel, Teilen,
  Zoom) durchläuft ein Pane binnen Millisekunden Zwischenhöhen — **gemessen 418×56 = 2 Zeilen**,
  Millisekunden später 418×278 = 14. Bei 2 Zeilen schiebt libvterm den **ganzen sichtbaren
  Bildschirm in den Scrollback**. Ein TUI holt sich das per SIGWINCH zurück, eine einfache
  Shell (`cmd`/PowerShell) zeichnet **nicht** neu → das Pane bleibt leer, obwohl die Session
  lebt und korrekt gezeichnet wird. Genau daher die Nicht-Determinismus-Erfahrung: es hängt am
  Inhalt, nicht am Zufall. Belegt per A/B am instrumentierten `Session::resize` (identischer
  Detektor): **ohne** Fix 34 angekommene Resizes, davon 2 auf ≤ 5 Zeilen — **mit** Fix 7, davon 0.
  Zusätzlich Gui-frei abgesichert: `gridFor()` ([src/core/TerminalGrid.h](src/core/TerminalGrid.h),
  Test `test_terminalgrid`) liefert bei nicht positiver Größe `valid=false` → ohne belastbare
  Größe wird gar nichts abgeleitet (früher wurde auf 1×1 geklemmt; ein 1-Spalten-Reflow kürzt
  jede Zeile auf ihr erstes Zeichen — im Scrollback als einzelne `H` aus `H:\…>` sichtbar).
  🔑 **Merke:** Nicht die Anzeige war schuld, sondern eine **transiente Layout-Größe, die bis
  ins PTY durchgereicht wurde**. Wer hier etwas ändert, prüft nicht Pixel, sondern die
  **angekommenen** Größen (`Session::resize` protokollieren).
- **Session-ID in der Kachel (QTMUX-44):** Jede Sidebar-Kachel zeigt neben dem Titel klein
  und monospaced `#<id>` — die **stabile** `Session::id()`, also genau die Nummer, mit der
  man die Session per MCP anspricht (`send_text`, `set_session_group` …). Model-Rolle
  `IdRole`/`"sessionId"`; im Delegate `required property int sessionId`. Bewusst NICHT der
  Zeilenindex (der wandert beim Umsortieren/Gruppieren).
- **Fenster darf nicht neben dem Bildschirm starten (2026-07-31):** `window.ensureWindowOnScreen(win)`
  in [qml/Main.qml](qml/Main.qml), gerufen als **erstes** in `Component.onCompleted` und aus
  `PrefsWindow.open()` (über `host.app`, **vor** `show()`). Prüft, ob das Fensterrechteck
  mindestens 120 × 120 px mit *irgendeinem* Bildschirm überlappt; sonst wird die Größe in den
  Bildschirm eingepasst und das Fenster **zentriert**.
  🔑 **Warum** (Diagnose, Messfalle `GetWindowRect` vs. `window.x/y` und das A/B stehen in
  [[app-startet-nicht-fenster-ausserhalb]]): Die Geometrie wird persistiert
  (`window/x|y|width|height`, für den Dialog `ui/prefsX|Y`), der Monitor aber nicht — fehlt er
  beim nächsten Start, läuft QTmux korrekt und ist trotzdem **unsichtbar**. Ein paar Pixel
  Überlappung reichen als Kriterium NICHT (ein Fenster, das mit 5 px am Rand klebt, ist
  genauso unbedienbar) — daher die 120-px-Schwelle.
- **Beenden mit Rückfrage (QTMUX-41):** Dialog listet die offenen Sitzungen auf, bevor
  alles geschlossen wird; abschaltbar (`window/confirmQuit`, **Vorgabe an**; Einstellungen →
  **Allgemein**, Abschnitt „Fenster" — dazu Datei-Menü und Palette, QTMUX-46).
  🔑 Zentraler Wächter ist **`Window.onClosing`** (`close.accepted = false`), NICHT die
  Beenden-Aktion: Seit **Qt 6.5** läuft auch ein Anwendungs-Quit (natives macOS-App-Menü,
  Cmd+Q, `Qt.quit()`) über das Schließen aller Fenster und bricht ab, wenn ein Fenster
  ablehnt — dadurch greift dieselbe Rückfrage auch für Schließkreuz und Alt+F4.
  `quitConfirmed` schaltet die Frage für den bestätigten Durchlauf ab (sonst fragt der
  Wächter beim `close()` aus `onAccepted` erneut).
- **Einstellungsfenster (QTMUX-47):** Nicht-modales `qml/PrefsWindow.qml` (Rail + View) mit
  neun Kategorie-Seiten `qml/prefs/Cat*.qml` auf einem `CatPage`-Gerüst. 🔑 **Brücken-Muster:**
  ein eigenes `Window` sieht die IDs aus `Main.qml` NICHT → `app`/`sessions`/`mcp` und die noch
  modalen Editier-Dialoge werden als `property var` hineingereicht (`host.*`); globale
  Registries (Theme/App/ColorSchemes/Profiles/Hotkeys/Vault/AgentEvents/Plugins) sind
  Context-Properties und überall direkt. Kürzel-Aufnahme inline; `prefs.capturing` deaktiviert
  währenddessen ALLE App-Shortcuts. **Abo-Matrix**: Toggle-Kacheln (TapHandler), KEINE
  CheckBoxen — deren `checked`-Bindung bräche beim Klick und die Kreuzeffekte (leere Liste =
  „alle") ließen Stände veralten. **Suche**: `PrefAnchor` je Sektion (nicht je Grid-Zelle, das
  bräche die GridLayouts) + `host.pendingSetting` blendet ~1,2 s auf.
  🔑 **Fallen:** `MultiEffect` braucht `import QtQuick.Effects` **je Datei**; typografisches
  Schluss-Anführungszeichen in `qsTr` (gerades `"` bricht den String); verschachtelte
  Repeater-Delegates brauchen `pragma ComponentBehavior: Bound` + qualifizierte IDs.
  Headless-Verifikation: das Fenster referenziert alle 9 Cat-Typen → ein defekter Typ bricht
  den App-Start; Seiten einzeln über vorgeseedetes `ui.prefsCategory` instanziierbar.
  🔑 **Vollständigkeit prüfen** (Audit 2026-07-29, Stand: lückenlos): Quelle echter
  Nutzer-Einstellungen ist der `Settings`-Block **`window/*`** in
  [qml/Main.qml](qml/Main.qml) (plus `ui/language`, `ui/themeMode`, `mcp/port` aus C++).
  Seit 2026-07-30 gibt es dort einen **zweiten** Block **`ui/*`** — der hält bewusst nur
  **Ansichtszustand** (Seitenleiste ein-/ausgeklappt, ihre Breite, Statusleiste sichtbar)
  und gehört wie `windows/*` **nicht** in den Dialog. Wer auditiert, liest beide Blöcke und sortiert
  Zustand aus. Jeden Alias von dort gegen
  [qml/prefs/](qml/prefs/) greppen — was fehlt, ist entweder eine Lücke oder bewusst
  **Laufzeitzustand** (`newSessionType`, `collapsedGroups`; die gehören NICHT in den Dialog).
  Alles unter `windows/*` in [WindowModel.cpp](src/viewmodels/WindowModel.cpp) ist ebenfalls
  Zustand, keine Einstellung.
  🔑 **Zeilenformat seit Design 1a, Stufe 5:** eine Einstellung = eine `PrefRow` (Titel +
  Beschreibung links, Control rechts) in einer gerahmten `PrefGroup`; ≤ 3 Optionen als
  `SegmentedControl`, Booleans als `AppSwitch`. Alle vier liegen in [qml/Ui/](qml/Ui/) und
  sind in `CMakeLists.txt` (`QML_FILES`) eingetragen — **eine neue QML-Datei ohne diesen
  Eintrag existiert zur Laufzeit nicht**. Die freistehenden Erklärtexte unter den früheren
  CheckBoxen sind damit weg; **listenartige** Seiten (Hotkeys/Verbindungen/Vault/
  Erweiterungen) bleiben bewusst außen vor.
  🔑 **`SegmentedControl` schreibt `currentIndex` NIE selbst** — es meldet nur
  `activated(index)`, wie `AppComboBox.onActivated`. Ein internes Setzen zerrisse die Bindung
  der Aufrufstelle (`currentIndex: Theme.mode`) beim ersten Klick; danach zeigte der
  Umschalter seinen eigenen Zustand statt den der Einstellung — dieselbe Falle wie bei der
  Abo-Matrix.
  🔑 **`font.pixelSize` ist ein `int`.** Die Anweisung nennt 11,5 px; eine Gleitkommazahl
  scheitert erst zur **Laufzeit** („Invalid property assignment: int expected") und reißt dann
  den GANZEN App-Start mit, weil `PrefsWindow` alle neun Kategorien referenziert. Nach jeder
  QML-Änderung darum einmal starten (`QT_FORCE_STDERR_LOGGING=1`), nicht nur bauen — der Build
  ist dafür blind.
  🔑 **Schrift auf Akzentflächen: `Theme.accentText`** (neu) statt eines weißen Literals — sie
  entscheidet über die **Luminanz des Akzents**, weil ein Schema mit hellem ANSI-Blau sonst
  weiß auf hell zeichnete. Chrome-Farben bleiben damit vollständig schema-abgeleitet.
  🔑 **Rail-Badges dürfen den Kategorienamen nicht verdrängen:** „QTmux Dunkel" ließ in der
  236-px-Rail nur „Erscheinu…" übrig. Das Badge zeigt darum den Schemanamen **ohne den
  eigenen Präfix** („Dunkel") und ist zusätzlich auf 84 px gedeckelt.
- **Zurücksetzen / Export / Import (Design 1a, Stufe 6):** `SettingsIo`
  ([src/viewmodels/SettingsIo.h](src/viewmodels/SettingsIo.h), Context-Property `SettingsIo`)
  plus die zwei Textknöpfe in der Kopfzeile. Export als JSON mit Kopf
  (`format: "qtmux-settings"`), Import erst als **Vorschau** der zu ändernden Schlüssel.
  🔑 **Zentraler Entwurf: eine ALLOWLIST, und Export und Reset arbeiten auf derselben Menge**
  (`patternsFor(<kategorie>)`, Präfix wenn das Muster auf `/` endet). Die Anweisung sagt „alle
  Schlüssel der Domain außer dem Vault" — dagegen sprechen zwei Dinge: (1) eine Blocklist
  müsste bei **jedem** neuen Schlüssel gepflegt werden, und ein vergessener landet in einer
  Datei, die der Anwender weitergibt; (2) unter `windows/*` und `sessions/*` liegt das
  Fenster-/Pane-Layout samt Session-Liste — ein „alles zurücksetzen" darf die Arbeit des
  Anwenders nicht mitnehmen. Der Vault ist doppelt außen vor: er steht ohnehin nicht in
  QSettings, sondern verschlüsselt in `vault.json`. Wächter dagegen: `tst_settingsio`
  (8 Fälle; Gegentest mit `windows/` in der Allowlist → **4 Fehlschläge**, u. a.
  `resetAllKeepsWindowLayout`).
  🔑 **Ein QML-`Settings`-Alias liest seinen Schlüssel NUR beim Aufbau.** Entfernt oder
  überschreibt `SettingsIo` einen Schlüssel, zeigt die laufende App weiter den alten Wert.
  Deshalb `window.applySettingValue(key)` in [qml/Main.qml](qml/Main.qml), von
  `SettingsIo.changed` je Schlüssel gerufen; die **Standardwerte stehen dort, wo die Property
  deklariert ist** (nicht doppelt in C++). Drei Schlüssel kennen ihren Standard nur in C++
  (System-Sprache, System-Design, `QTMUX_MCP_PORT`) → `App.reloadLanguage()`, `Theme.reload()`,
  `mcp.reloadPort()`, alle drei **ohne** erneutes Persistieren (sonst schriebe ein Reset den
  Standard sofort wieder in die Einstellungen). `colorSchemes/*`, `hotkeys/*`, `profiles/*`
  halten C++-Registries → dort neues `reload()` (bei den Schemata mit vollständigem Neuaufbau,
  weil `loadPersisted` importierte Schemata nur **anhängt**).
  ⚠️ **Bewusst nicht bekämpft:** Setzt `applySettingValue` eine Property auf ihren Standard,
  merkt QML-`Settings` die Änderung und schreibt den Standardwert zurück — der Wert ist danach
  korrekt der Standard, der Schlüssel kann aber wieder auftauchen. Das zu verhindern hieße, die
  Settings-Bindung zu umgehen.
  🔑 `FileDialog.selectedFile` ist **kein Namensvorschlag**: ein nicht existierender Pfad
  erzeugt „Cannot set … because it doesn't exist". Stattdessen `defaultSuffix` + `currentFolder`.
- **Agent-Awareness:** OSC 133 (Prompt-Marker → Activity-Ring), OSC 9/777 (Notify),
  OSC 9;4 (Progress-Balken), Bell → Attention-Pulse (blau); MCP-Controller-Tab rot.
  🔑 **Reduzierte Bewegung (QTMUX-47):** `App.reduceMotion` (AppController, beim Start
  ermittelt — macOS CoreFoundation `com.apple.universalaccess/reduceMotion`, Windows
  `SPI_GETCLIENTAREAANIMATION`, sonst false; Env-Override `QTMUX_REDUCE_MOTION` für Tests)
  schaltet die drei Sidebar-Puls-Animationen ab (`running: … && !App.reduceMotion`) → Ring
  in Akzentfarbe und Rahmen statisch statt pulsierend.
- **Umfang der Wiederherstellung ist eine WAHL (QTMUX-99):** `window/restoreSessionMode` =
  `qtmux::RestoreMode` — 0 gar nicht · 1 ohne Verlauf · 2 alles (**Vorgabe**, bisheriges
  Verhalten). Regeln Gui-frei in [src/core/RestoreMode.h](src/core/RestoreMode.h), QML fragt
  ausschließlich über `windows.restoresLayout/restoresHistory/persistsOnQuit` — so wird ein
  defekter Wert an EINER Stelle normalisiert statt an dreien. Erreichbar in Einstellungen →
  **Allgemein** (Abschnitt „Fenster", direkt beim Beenden-Schalter — das eine steuert das
  Ende, das andere den nächsten Start), Datei-Menü, Palette, Suchindex.
  🔑 **Der Kern des Tickets ist NICHT das Nicht-Laden, sondern das Nicht-Speichern.** Bei
  Modus 0 kehrt `persistWindows()` sofort zurück: Sonst schriebe das erste Beenden die eine
  frisch geöffnete Session über den gesamten gespeicherten Stand — ein einmaliges Umstellen
  wäre unwiderruflich. Aus demselben Grund unterbleibt dort auch `pruneHistoryExcept`, sonst
  räumt es die `.ans`-Dumps des eingefrorenen Stands als „verwaist" weg. E2E-belegt: mit
  Wächter ist der gespeicherte Stand nach einem Modus-0-Durchlauf **bitidentisch**
  (gleicher shasum), ohne Wächter bleibt von zwei Windows **eines** übrig und die alten
  Arbeitsverzeichnisse sind weg.
  🔑 **Unbekannte Werte → `Full`, nie `None`** (`restoreModeFromInt`): `None` unterdrückt ja
  zusätzlich das Speichern; ein defekter oder aus einer neueren Version stammender Wert würde
  sonst still den letzten Stand einfrieren und sähe für den Anwender wie Totalverlust aus.
  Tests `tst_windowmodel::restoreModeGatesLayoutHistoryAndPersistence` und
  `unknownRestoreModeFallsBackToFull` (Gegenprobe mit umgedrehter Fallback-Richtung: FAIL).
  🔑 Modus 1 lässt nur `loadHistoryFor` weg — die Dumps bleiben liegen, ein späteres „Alles"
  findet sie wieder vor.
- **Ruhezustand verhindern (QTMUX-89):** `SleepInhibitor` ([src/core/SleepInhibitor.h](src/core/SleepInhibitor.h),
  plattform-gekapselt wie `GlobalHotkey`) — macOS `IOPMAssertionCreateWithName`
  (`PreventUserIdleSystemSleep`, IOKit-Framework), Windows `SetThreadExecutionState`
  (⚠️ **pro Thread**: Setzen und Aufheben aus demselben Thread), **Linux noch Stub**
  (login1/DBus wäre hier nicht lauffähig prüfbar — eine hängende Sperre ist schlimmer als
  keine). Nur **System**schlaf, nie das Display. Schalter `window/preventSleep`,
  **Vorgabe AUS** (Anwender-Vorgabe: ungefragt den Ruhezustand aushebeln ist ein Ärgernis);
  Einstellungen → Allgemein → „Energie" mit Live-Anzeige, ob gerade gesperrt ist, dazu Palette
  und Suchindex. Regel Gui-frei in `shouldPreventSleep` (Test `tst_session::sleepInhibitRule`).
  🔑 **`Activity` allein ist als Auslöser UNBRAUCHBAR** — der Startwert ist `Running`, damit
  der Sidebar-Ring sofort grün ist. Eine Shell **ohne** Shell-Integration meldet nie etwas und
  bliebe für immer „beschäftigt": Der Rechner schliefe nie wieder ein. Deshalb zählt nur, was
  eine Session **selbst gemeldet** hat (`Session::activityReported()`, gesetzt von OSC 133 und
  MCP `set_activity`) — ungemeldet heißt **unbekannt**, nicht „arbeitet". Dieselbe Linie wie
  QTMUX-30/37: QTmux leitet nichts ab. Empirisch aufgefallen: `pmset -g assertions` zeigte die
  Sperre direkt nach dem Start, ohne dass irgendwer arbeitete.
  🔑 **Die erste Meldung ist oft `busy` und ändert den Startwert `Running` gar nicht** →
  `setActivity` feuert kein `activityChanged`, und die Neuberechnung liefe nie an. Deshalb
  `markActivityReported()`, das beim Übergang „ungemeldet → gemeldet" **einmal**
  `activityChanged` auslöst. Symptom vorher: Die Sperre kam erst beim **zweiten** `busy`
  (nach einem Umweg über `waiting`) — sah aus wie ein Wettlauf, war aber ein fehlendes Signal.
  🔑 `Waiting` zählt bewusst **nicht** als Arbeiten: Da wartet der Agent auf einen Menschen,
  und dann darf der Rechner schlafen (Gegenprobe im Test: FAIL, wenn man es mitzählt).
  Abnahme mit `pmset -g assertions` über alle Zustände, inkl. Freigabe beim Beenden.
- **AgentRegistry: Aliase, Kommandonamen, Unterkommando-Vorlagen (QTMUX-88):** Die Liste der
  bekannten CLIs ist **einziger Pflegeort** (nirgends im QML oder README gedoppelt — geprüft).
  `AgentInfo` hat neben `command` jetzt `aliases`, und `AgentInfo::matches()` ist die EINE Stelle,
  die Namen vergleicht (`detect` ruft nur noch sie). Einträge stehen als **designierte
  Initialisierer** (C++20) da — ein neues Feld verschiebt damit keine Werte mehr lautlos.
  🔑 **Der Fehler, um den es ging:** eingetragen war `cursor` — das ist der **Editor**-Starter (wie
  `code` bei VS Code). Der Agent hieß `cursor-agent` und wird laut Dokumentation inzwischen als
  `agent` installiert; beide sind eingetragen, `cursor` **keiner** von beiden (Test hält das
  ausdrücklich fest). Wirkung vorher: `cursor .` machte eine Editor-Session zur „Cursor"-Agenten-
  Session, und der echte Agent wurde nie erkannt.
  ⚠️ Der Alias **`agent` ist generisch** — ein eigenes Skript dieses Namens erbt das Cursor-Etikett;
  bewusst in Kauf genommen, Begründung am Eintrag.
  🔑 **Codex fortsetzt über ein UNTERKOMMANDO** (`codex resume [--last] [SESSION_ID]`, am `--help`
  belegt 2026-07-31) — damit kann Codex als **zweiter** Agent alle drei Modi aus QTMUX-98, inkl.
  Picker. Dafür hat `resumeCommand` einen Wächter: Trägt die Zeile schon ein eigenes Unterkommando,
  darf eine Unterkommando-Vorlage nicht davor rutschen (aus `codex exec "…"` würde sonst
  `codex resume --last exec "…"`). Die Prüfung hängt an der **Form der Vorlage** (erstes Zeichen
  kein `-`), nicht am Agenten — gilt also für jeden künftigen Eintrag dieser Art.
  Jetzt **22 Einträge** (Vorlagen bei allen Nachträgen leer, weil die CLIs hier nicht installiert
  sind — ein ungeprüftes Flag sieht wie ein QTmux-Fehler aus).
  🔑 **Ein Paketname ist kein Kommandoname — die teuerste Lektion der zweiten Runde.** Die
  Ticket-Recherche forderte die Aliase `gemini-cli`, `qwen-code`, `iflow-cli`; alle drei sind
  **npm-Paket- bzw. Homebrew-Formelnamen** und existieren als Kommando nirgends. Beweis in einer
  Zeile: die Formel macht `bin.install_symlink libexec.glob("bin/*")`, verlinkt also exakt das
  npm-`bin` — und das ist `{"gemini"}` bzw. `{"qwen"}`. Aus derselben Verwechslung stammen drei
  falsche **Kommandonamen** im Ticket: `augment` → **`auggie`**, `kiro` → **`kiro-cli`** (Kiro ist
  die IDE — derselbe Fall wie cursor/cursor-agent), `kimi-code-cli` → **`kimi`**. Umgekehrt fand
  die Registry-Abfrage einen Alias, den die Recherche nicht kannte: **`kilocode`** ist ein zweites
  `bin` von `@kilocode/cli`. **Prüfweg für jeden künftigen Eintrag:**
  `curl -s https://registry.npmjs.org/<paket>/latest | python3 -c "…d['bin']"` — er nennt das
  tatsächlich installierte Kommando und schlägt damit jede Doku-Seite.
  Tests: `tst_agent` 17 Fälle (u. a. `registryNamesAreUniqueAndDetectable` — kein Name doppelt,
  jeder eingetragene Name auch erkennbar, `{id}`-Platzhalter vorhanden; dazu
  `packageNamesAreNotCommandNames`, das die drei Paketnamen und die bewussten Ausschlüsse
  `air`/`warp`/`cline` als **nicht** erkennbar festschreibt). Gegentest mit dem alten
  `cursor`-Eintrag und ohne den Unterkommando-Wächter: **3 Fälle FAIL**; Gegentest mit dem
  Ticket-Alias `gemini-cli` und `kiro` statt `kiro-cli`: **2 Fälle FAIL**. Dazu E2E über MCP gegen
  eine isolierte Instanz (6 Fälle, u. a. `cursor .` → **nicht** erkannt, `agent`/`goose` → erkannt),
  und in der zweiten Runde vier weitere per **Stub-Agent** unter absolutem Pfad: `droid` →
  „Droid", `kiro-cli` → „Kiro", dagegen `kiro` und `augment` → agentId **leer**, Titel bleibt
  „Zsh" (die Positivkontrolle zur Namenskorrektur — ohne sie hätte man den Fehler auch dadurch
  „behoben", dass gar nichts mehr erkannt wird).
- **Agenten überleben den Neustart (QTMUX-85):** Ein Agent läuft **nicht** als `program` —
  er wird in eine Shell **getippt** und in `Session::observeInput` über
  `AgentRegistry::detect` erkannt. Deshalb speichert die Session die erkannte Zeile in
  `m_agentCommand` (vor dem `m_inputLine.clear()`, dort ging sie bisher verloren);
  `sessionConfig()` legt sie als `agentCommand`/`agentId` ins Blatt-`cfg`. Beim Restore baut
  `_createSessionFromCfg` daraus ein **Login-Script** (QTMUX-23) — nicht `program`: Letzteres
  wird direkt exec't und bei argumentloser Angabe als Login-Shell markiert (`argv0 = "-claude"`),
  der Agent liefe ohne Shell-Umgebung und sein `exit` schlösse das Pane.
  Schalter `window/restoreAgents`, **Vorgabe AUS**.
- **Unterhaltung fortsetzen ist eine WAHL, kein Schalter (QTMUX-98):** `window/resumeAgentMode`
  = `qtmux::ResumeMode` — 0 gar nicht (Vorgabe) · 1 **jüngste** im Verzeichnis · 2 **Auswahl**
  beim Start · 3 die vom Agenten **gemeldete** Sitzung. Je Modus eine Argument-Vorlage in
  `AgentInfo` (`resumeLastArgs`/`resumePickArgs`/`resumeIdArgs`, Letztere mit Platzhalter
  `{id}`); leer = der Agent kann das nicht → er startet frisch, es wird **nie** auf einen
  anderen Weg ausgewichen. Am `--help` verifiziert (2026-07-29): `--continue` für claude/agy/
  opencode/hermes, per ID `--resume {id}` (claude, hermes), `--conversation {id}` (agy),
  `--session {id}` (opencode); **einen Picker hat nur Claude Code** (`--resume` ohne Wert).
  🔑 **Warum eine Wahl:** `--continue` heißt wörtlich „jüngste Unterhaltung **im Verzeichnis**".
  Wer einen Agenten je Verzeichnis fährt, ist damit exakt bedient; wer mehrere im selben Ordner
  laufen lässt (hier der Normalfall — 5 Panes in RAFTNG, 3 in QTmux), bekäme in **allen**
  dieselbe. E2E-belegt: Modus 1 → beide Panes `--continue`; Modus 3 → `--session
  unterhaltung-EINS` bzw. `-ZWEI`.
  🔑 **Die ID kann QTmux nicht selbst ermitteln — vier Wege gemessen, alle tot:** MCP-Server
  ruft keinen Client (und ein beschäftigter Agent pollt nicht, QTMUX-37); in die PTY tippen
  landet im Eingabefeld der TUI und die Antwort wäre Scraping (gegen QTMUX-30); `lsof` findet
  nichts, weil Claude Code die `.jsonl` nicht offen hält; `ps eww` zeigt nur die **Start**-
  Umgebung, `CLAUDE_CODE_SESSION_ID` wird erst zur Laufzeit gesetzt (nur an Kindprozesse
  vererbt). Deshalb **meldet der Agent** per MCP `set_agent_session` — und muss das nach
  `/resume`/`/clear` **erneut** tun, weil sich die Kennung dabei ändert.
  🔑 **Drei Fallen, jede einzeln erlebt:** (1) Die Startzeile MUSS als `loginScript`-Argument
  von `create*Session` mitgehen — **vor** dem Start. Nachträglich gesetzt kann der Prompt
  schon durch sein, und `armLoginScript` wird erst beim **nächsten** Output scharf; eine
  wartende Shell liefert keinen mehr, der Agent startete nie. (2) `runLoginScript` schreibt
  **direkt ans Backend** und läuft an `observeInput` vorbei → Kennung und Titel muss
  `Session::setRestoredAgent` selbst setzen, sonst steht in der Sidebar weiter „zsh".
  (3) `sessionConfig()` braucht den **Rückfall** auf den vorgemerkten `cfg`-Wert
  (`seedAgentConfig`): Ein einziger Start mit **abgeschaltetem** Schalter schrieb sonst den
  leeren Laufzeitwert zurück und **löschte** den gespeicherten Befehl — ein späteres
  Einschalten fand nichts mehr vor. `resumeCommand` setzt die Argumente **hinter dem
  Kommando-Token** ein (nicht am Ende, sonst bräche eine Subkommando-Form) und ist
  idempotent, sonst sammelt sich `--continue --continue …` über Neustarts an.
  Tests: `tst_agent` (Einfügen/Idempotenz/unbekannt), `tst_session`
  (`agentCommandLineIsRemembered`, `restoredAgentSetsIdentityAndRunsCommand`).
- **Shell-Helfer stecken im Binary (QTMUX-38):** `src/core/ShellIntegration.{h,cpp}` (Gui-frei)
  plus `qt_add_resources(qtmux_core "shell_integration" …)`; `qtmux --install-shell-integration
  [ZIEL]` schreibt sie heraus, nennt den Pfad **und die fertige Hook-Zeile**. Standardziel
  `<GenericDataLocation>/QTmux/shell-integration`.
  🔑 **Warum nicht mitpaketieren** (der Grund, warum es jahrelang liegen blieb): Für DMG und MSI
  ginge das — das **AppImage hat aber gar keinen stabilen Pfad**, es wird bei jedem Start unter
  einem anderen `/tmp/.mount_XXXXXX` gemountet. Ein Hook-Eintrag in einer `settings.json` muss
  Neustarts überleben; Mitpaketieren löst den Linux-Fall also grundsätzlich nicht. Aus dem
  Programm geschrieben gilt **ein** Weg für alle drei Plattformen, und die Dateien passen
  zwangsläufig zur laufenden Version (keine Drift gegenüber einzeln von GitHub Gezogenem).
  🔑 **`GenericDataLocation`, NICHT `AppDataLocation`:** Letzteres trägt den `applicationName`,
  und der bekommt bei `--profile`/`QTMUX_PROFILE` ein Suffix — das Ziel wanderte je Instanz,
  obwohl ein Hook-Eintrag für alle Profile gilt. Test `defaultTargetIsProfileIndependent`.
  🔑 **Windows: GUI-App ohne stdout — und zwei Fallen, beide gemessen.** `qtmux` MUSS
  `WIN32_EXECUTABLE` sein (ConPTY, s. o.) und hat damit keine eigene Ausgabe.
  `runInstallShellIntegration` hängt sich per `AttachConsole(ATTACH_PARENT_PROCESS)` +
  `freopen_s("CONOUT$")` an — **aber nur, wenn `GetStdHandle(STD_OUTPUT_HANDLE)` kein gültiges
  Handle liefert.** Ohne diese Bedingung schreibt `freopen` an einer bestehenden Umleitung
  (`> datei`, PowerShell-Pipe) **vorbei** ins Konsolenfenster: erste Messung am portablen ZIP
  = Dateien korrekt geschrieben, Exit 0, Ausgabe spurlos weg — exakt das „wirkt kaputt", das
  das Ticket vorhergesagt hatte. `SetConsoleOutputCP(CP_UTF8)` nur, wenn es eine Konsole gibt
  (sonst Mojibake bei Umlauten).
  ⚠️ **Bleibt bewusst ungelöst:** Die Shell **wartet nicht** auf ein GUI-Programm. Der Prompt
  ist sofort zurück, die Ausgabe erscheint gleich danach (die Dateien werden vollständig
  geschrieben — nach 3 s gemessen: 11/11). PowerShells `>` schließt seine Zieldatei dabei zu
  früh und bleibt **0 Bytes**; `cmd /c "… > datei"` trägt (763 Bytes gemessen), weil cmd das
  Handle vererbt statt selbst zu lesen. Ein Konsolen-Subsystem ist keine Option, ein zweites
  `qtmux-cli.exe` wäre genau das Extra-Artefakt in allen Paketen, das dieses Ticket vermeiden
  wollte. Steht so in der Doku.
  🔑 Der Befehl läuft **vor** der `QGuiApplication` mit eigener `QCoreApplication` und beendet
  den Prozess selbst — sonst blitzt auf macOS ein Dock-Icon auf und Qt fährt eine GUI-Umgebung
  hoch, die niemand braucht. Eigene Argument-Schleife, weil das Ziel **optional** ist (die
  bestehende Schleife sieht nur `--x <wert>`-Paare).
  Tests: `tst_shellintegration` (8 Fälle) — darunter der Wächter, dass die Ressource aus der
  **statischen** `qtmux_core` überhaupt im Programm landet (ein Linker darf Objektdateien
  verwerfen, auf die niemand verweist), Ausführbar-Bit nur für `.sh`, Idempotenz, und dass eine
  veränderte Datei wieder auf die mitgelieferte Fassung zurückgesetzt wird.
  **Am Artefakt abgenommen, alle drei Pakete** (2026-07-31): DMG gemountet und die App daraus
  gestartet · portables ZIP auf rtzbld01 entpackt · AppImage aus dem CI-Lauf `30641780941` auf
  rtzsvr02 — je 11/11 Dateien. Dazu E2E: das **installierte** `qtmux-emit.sh` stellte einer
  isolierten Instanz ein Ereignis zu (`seq 1`, `sourceSessionId 2`).
  🔑 Das AppImage lässt sich auf rtzsvr02 **nur im Container** starten (`libOpenGL.so.0` fehlt
  auf dem aufgeräumten Host) — die Meldung sieht nach einem kaputten Paket aus, ist aber die
  Umgebung. Mit `sudo /opt/docker/buildenv/buildenv.sh` + `APPIMAGE_EXTRACT_AND_RUN=1` läuft es.
- **AgentEventHub** (Gui-frei, Ringpuffer 256, monotone `seq`): Inter-Agenten-Ereignisse
  `done|question|error|info` via OSC `777;qtmux-event` oder MCP `post_event`; Zustellung
  über MCP-Long-Poll `wait_for_events`. **⚠️ Hook-stdout wird vom Agenten gekapselt** —
  aus KI-Hooks immer `post_event` (HTTP) statt OSC nutzen.
- **Split-Layout je Window (QTMUX-83):** rekursiver JS-Baum **pro Window** — Blatt
  `{paneId, sessionId}` (stabile `Session::id()`, **kein** Row-Index mehr → kein Remap beim
  Umsortieren), Split `{orientation, children}`; QML-Rekursion via Loader —
  **`setSource(url,{props})` VOR dem Laden** (sonst evaluieren Bindungen mit
  `win===undefined` und brechen dauerhaft). `pruneLeaves(pred)` entfernt Blätter gelöschter
  Sessions (sonst teilen sich Panes eine Session und kämpfen um `resize()` → Verzerrung).
  Pane-Reorder: `DragHandler(target:null)` + manueller Szenen-Hit-Test (Qt-`Drag`/`DropArea`
  war fragil). Extern (MCP) erzeugte Sessions werden per `_wrapPending` in ein Window verpackt.
- 🔑 **Niemals ein ListView-Delegat als `DragHandler.target` (QTMUX-100).** Ein ListView
  vergibt die `y` seiner Delegates **selbst** und leitet daraus die Ausdehnung des Inhalts ab.
  Zieht man die **letzte** Kachel nach oben, schrumpft diese Ausdehnung, Flickable korrigiert
  `contentY` ins Negative — und schiebt damit **alle übrigen** Kacheln nach unten. Die
  Korrektur verschiebt die gezogene Kachel erneut gegenüber dem Zeiger → nächste Korrektur:
  eine **Rückkopplung**, die erst endet, wenn nichts mehr im Bild ist. Gemessen (3 Kacheln,
  ohne Gruppen): letzte Kachel `contentY` 0 → −6 → −52 → −100 → −163 → −213 …, erste und
  mittlere Kachel dagegen konstant 0. Daher auch der Anwender-Befund „nur ohne Gruppen": ein
  Section-Header hält die Ausdehnung unten fest. **Richtig ist `target: null` + rein optischer
  Versatz per `transform: Translate { y: … }`** aus `activeTranslation`; die Zielzeile beim
  Loslassen aus Layout-`y` **plus** Versatz. Gilt für Kachel- **und** Gruppen-Header-Drag.
  🔑 **Positivkontrolle ist hier Pflicht**, sonst „behebt" man den Fehler, indem man den Drag
  abschaltet: Der Versatz muss dem Zeiger 1:1 folgen (gemessen: Maus −372 px → `dy −372`) und
  das Loslassen muss umsortieren (`#1 #2 #3` → `#3 #1 #2`).
  Der Versatz ist zusätzlich auf den Inhaltsbereich geklemmt (QTMUX-102, `[-tile.y,
  contentHeight-tile.height-tile.y]`) — sonst zieht man die Kachel aus dem Bild und sieht
  nicht mehr, was man gerade bewegt.
- **ToolTips (QTMUX-101):** [qml/Ui/AppToolTip.qml](qml/Ui/AppToolTip.qml), Verzögerung 600 ms.
  Wie bei `ThemedMenu`/`AppPopupBg` gilt: Popups erben die Window-`palette` **nicht** → Farben
  explizit aus `Theme`, sonst dunkle Schrift auf dunklem Grund. In beiden Designs per
  `--screenshot` abgenommen. Die Sidebar-Kachel zeigt darin vollen Titel, `#Session-ID` und
  Arbeitsverzeichnis — die Kachel elidiert, und bei mehreren Agenten im selben Projekt sind
  die Titel vorne identisch.
  🔑 **`qsTr`-Plurale brauchen Handpflege:** `FinishSourceLanguageTs.cmake` füllt die
  `numerusform` der **Quellsprache** nicht automatisch — die deutschen Pluralformen (z. B.
  `%n Einträge`) sind inzwischen von Hand nachgetragen (beide `.ts` stehen auf
  0 unfinished). Bei neuen Plural-Strings also entweder die Zahl erst ab 2 anzeigen und
  eine feste Form nehmen, oder die deutschen Pluralformen direkt mitpflegen.
- **Einklappbare Seitenleiste + Statusleiste (Design 1a/2a, 2026-07-30):** Zustand in einem
  **zweiten** `Settings`-Block `ui/*` (`sidebarWidth`, `sidebarCollapsed`, `statusBarVisible`).
  Breite [180, 420]; **unter 140 px rastet sie ein** (52 px), Zwischenwerte werden nicht
  gehalten — die gespeicherte Breite bleibt immer im Bereich, sonst endet ein Aufklappen in
  einer unbrauchbar schmalen Liste. Der Splitter ist **neu** (die Leiste war ein festes
  240-px-`Rectangle`; die `SplitView`s gehören den Panes), `DragHandler` mit `target: null`
  wie bei QTMUX-100. Eingeklappt zeigt **dieselbe Kachel** einen zweiten Inhalt (Icon +
  Nummer + Statuspunkt) statt eines zweiten Delegates — sonst müsste die
  Rückkopplungsfalle aus QTMUX-100 zweimal richtig vermieden werden.
  🔑 **Der Chevron ist in BEIDEN Zuständen sichtbar** (2026-07-31): Er war eingeklappt
  ausgeblendet — Begründung damals „kein Platz, Splitter/Kürzel/Ansicht-Menü bleiben als
  Wege". Das trug nicht (Anwender-Befund): Ohne sichtbaren Knopf findet man den Rückweg
  nicht, Kürzel und Menü muss man erst kennen, und der Splitter ist eine unbeschriftete
  Kante. Eingeklappt weicht daher der **Schriftzug** („QTmux" → unsichtbar) und der Chevron
  rückt mittig; die Spitze zeigt, was der Klick tut (`rotation: 90` = links = einklappen,
  `-90` = rechts = ausklappen). Statusleiste als
  `footer` mit Inline-Komponente `StatusField`; die Aggregat-Zähler liegen in
  `SessionModel` (`waitingCount`/`errorCount` + `countersChanged`, Test
  `tst_sessiongroups::statusBarCounters`) — **nicht** in `WindowModel`, das kennt keine
  Sessions.
  🔑 **Rastergröße „80×24" (QTMUX-120) kommt aus der SESSION, nie aus dem `TerminalItem`.**
  `Session` hielt `m_cols`/`m_rows` längst; es fehlte nur die Veröffentlichung als
  `Q_PROPERTY cols/rows` (NOTIFY `sizeChanged`), die Leiste bindet über
  `window.windowGridText()`. Der Grund für die Quelle ist QTMUX-86: Ans Item gebunden zeigt
  die Anzeige die **transienten** Layout-Zwischengrößen (beim Window-Wechsel und beim Teilen
  gemessen 80×2), denn dorthin gelangt die Größe erst nach der 60-ms-Entprellung
  (`applyPendingResize`). Was in `Session` steht, ist genau das, was auch im PTY ankommt.
  Ohne Session blendet sich das Feld aus — eine Größe ohne Terminal wäre eine Erfindung.
  Test `tst_session::gridSizeIsPublishedAndSignalled` (Signal nur bei echter Änderung).
  🔑 **Kein Text ohne `elide` in eine Leiste (QTMUX-121).** Das `Text` im `StatusField` hatte
  weder `elide` noch Breitengrenze; die `Layout.maximumWidth` an der Aufrufstelle begrenzt nur
  das **Item**, der Text darin malte einfach weiter — bei einem langen Arbeitsverzeichnis über
  „x Sessions", Rastergröße und „UTF-8" hinweg. Mit `~` als CWD bleibt das unsichtbar, deshalb
  fiel es erst bei der Bildabnahme auf.
  ⚠️ **Der naheliegende Fix ist eine Bindungsschleife** (selbst hineingelaufen): `width` aus
  `sf.width` zu rechnen schließt einen Kreis, denn die Feldbreite kommt ja aus
  `sfRow.implicitWidth`, also aus der Textbreite. QML löst das mit **0** auf → die ganze
  Statusleiste war leer. Richtig ist eine **eigene** Property (`maxLabelWidth`, 0 = unbegrenzt),
  die nur das eine lange Feld setzt.
  🔑 **Drei QML-Fallen, jede einzeln erlebt:** (1) `Behavior on Layout.preferredWidth` ist
  **ungültig** — auf einer *attached property* erlaubt QML kein `Behavior`; es braucht eine
  eigene Property (hier `animWidth`). (2) Der **Inhalt eines `Popup` entsteht mit dem
  Delegate**, nicht beim Öffnen, und `Date.now()` ist nicht reaktiv → eine gerechnete Dauer
  steht sonst für immer auf „seit 0 s" (am Bild aufgefallen); Abhilfe ist ein `tick`-Anker,
  den ein Timer nur bei offenem Popup hochzählt. (3) Im eingeklappten Zustand muss der
  `AppToolTip` **abgeschaltet** werden, sonst stehen ToolTip und Flyout übereinander.
  🔑 **`Ctrl+B` gehört der Shell** (tmux-Präfix, readline `backward-char`): Der Umschalter
  liegt auf **macOS `Ctrl+B`** (= Cmd+B) und **Windows/Linux `Ctrl+Shift+L`** — dieselbe Linie
  wie `actFind`/`actClearScreen`. Und: `actVault` und `actClearScreen` lagen **beide** auf
  `Ctrl+Shift+K` (in Qt „ambiguous", also feuerte keiner zuverlässig) — „Bildschirm leeren"
  behält die Taste, der Vault hat keine Vorgabe mehr. Wächter dagegen:
  `tst_hotkeys::defaultsAreConflictFree`.
- **Verzeichnis auf der Kachel (2026-07-30):** Zweite, gedimmte Zeile unter dem Titel
  (`tile.dispDir` → `window.prettyDir`, `ElideLeft`, ausgeblendet wenn leer). 🔑 **Warum
  nötig:** Der Kacheltitel kommt **ausschließlich** aus dem OSC-0/2-Titel der Shell — Claude
  Code schreibt dort inzwischen das **Gesprächsthema** (`✳ …`), `cmd.exe` setzt **gar nichts**
  (bleibt ewig „Eingabeaufforderung"). Der Ort war damit nirgends direkt ablesbar; der ToolTip
  aus QTMUX-101 zeigt ihn nur beim Hover und bleibt der Fallback für den **vollen** Pfad.
  `prettyDir` kürzt Home zu `~` und **vergleicht** auf einer normalisierten Kopie
  (`QDir::homePath()` liefert `/`, die Shell unter Windows `\`), **zeigt** aber den
  Originalpfad — sonst stünde dort `C:/Windows/System32`. Der Home-Vergleich geht gegen
  Gleichheit bzw. `home + "/"`, sonst würde `/Users/nrx` als Home `/Users/nr` gelesen.
  ⚠️ Bei **PowerShell**-Sessions **ohne** gesourcte Shell-Integration steht hier dauerhaft das
  Startverzeichnis — nicht die Anzeige ist schuld, sondern `Set-Location` (Begründung im
  ConPTY-Abschnitt). Mit der Integration meldet die Shell ihr Verzeichnis per OSC 7 (QTMUX-108)
  und die Zeile folgt.
- **Git-Branch auf der Kachel (QTMUX-58):** `Session::refreshGitBranch()` führt ihn im
  1500-ms-Takt des `SessionModel` nach (Rollen `gitBranch`/`gitDetached`), die Kachel zeigt ihn
  **vor** dem Verzeichnis (`⎇ main …/projekt`; bei detached `➟ <shortSha>`), der ToolTip
  ungekürzt. 🔑 **Eigene Methode, nicht Teil von `refreshWorkingDirectory()`:** Letzteres
  steigt bei OSC-7-Sessions sofort aus (die Shell meldet ja selbst) — der Branch würde dort nie
  aktualisiert. Und `git checkout` wechselt den Branch, **ohne** dass sich das Verzeichnis
  ändert; die Prüfung darf also nicht am Verzeichnis-Vergleich hängen.
  🔑 **Bei einem Verzeichnis auf einem FREMDEN Rechner bleibt der Branch leer** (QTMUX-108):
  Existiert der gemeldete Pfad hier zufällig auch, läse man den Branch eines **anderen**
  Repositories und hängte ihn an diese Kachel. Test `gitBranchStaysEmptyForRemoteDirectory`
  (Gegentest ohne die Sperre: FAIL).
- **Prompt-Warteschlange (QTMUX-90):** `Session` hält eine `PromptQueue`, stempelt bei jeder
  Backend-Ausgabe eine `QElapsedTimer` (das ist `msSinceLastOutput`) und versucht die Abgabe
  über einen Timer, der **nur läuft, solange etwas ansteht**; abgegeben wird über
  `writeWithEnter` (QTMUX-31 — ein Eintrag landet typischerweise in genau der TUI, die einen
  Block mit `\r` als Einfügen wertet). Erreichbar über Palette („In die Warteschlange
  einreihen …"), `sessions.queueText()` und MCP **`queue_text`**; Zähler als Abzeichen auf der
  Kachel. Ein `static_assert` in [Session.cpp](src/core/Session.cpp) nagelt die
  Zahlen-Spiegelung `ActivityCode` ↔ `Session::Activity` fest — ein Verstoß bricht den
  **Build** (gegengetestet: `Waiting`/`Error` vertauscht → Compile-Fehler).
  ⚠️ **Bekannte Grenze, am laufenden Programm gemessen:** Meldet die Session ihren Zustand
  nicht, entscheidet die **Ruhe im Ausgabestrom** (500 ms) — ein *stiller* Langläufer wie
  `sleep 6` oder ein Build ohne Ausgabe gilt damit als „frei", und der Eintrag geht zu früh
  raus. Das ist bewusst so belassen: Der naheliegende Zusatz „hat die Shell einen
  Kindprozess?" (`ProcessInfo::descendantPids`) würde bei einem laufenden **Agenten-TUI** die
  Warteschlange dauerhaft blockieren — und Agenten sind der Hauptanwendungsfall. Für sie
  greift ohnehin der gemeldete Zweig (OSC 133 / `set_activity`), und sie geben während der
  Arbeit Ausgabe aus. In einer Shell ist der Schaden gering, weil sie die Eingabe puffert.
- **Arbeitsverzeichnis per OSC 7 (QTMUX-108):** `VtScreen` wertet `ESC ] 7 ; file://host/pfad`
  aus (`case 7` im OSC-Fallback — libvterm behandelt nur 0/1/2/52 selbst), dekodiert die
  Prozent-Kodierung und trennt den Host ab; `Session` nimmt die Meldung an, sie hat **Vorrang**
  vor dem gepollten `Pty::currentWorkingDirectory()`, und sobald gemeldet wird, **schweigt das
  Polling** (sonst überschriebe es die genauere Angabe im 1500-ms-Takt).
  🔑 **Ein Pfad von einem FREMDEN Host wird angezeigt, aber nicht als lokales CWD
  weitergereicht.** An `currentWorkingDirectory()` hängen Persistenz und die CWD-Vererbung an
  neue Shells — ein Verzeichnis der Gegenstelle existiert hier womöglich gar nicht. Genau daran
  hängt auch das Gate in `refreshWorkingDirectory()`: Beim entfernten Fall fällt
  `currentWorkingDirectory()` bewusst auf den gepollten Wert zurück, der nicht in die Anzeige
  gespeichert werden darf. **Nur dieser Fall prüft das Gate** — bei einer *lokalen* Meldung
  greift die Vorrangregel ohnehin, der erste Gegentest bestand deshalb fälschlich.
  🔑 **Der Fehler, den kein Unit-Test fand:** `printf '%d' "'$str"` liefert in bash für Bytes
  ≥ 0x80 eine **negative** Zahl (signed char) — das erste Skript schrieb
  `%FFFFFFFFFFFFFFC3` statt `%C3`, also war **jeder Umlaut im Pfad** kaputt. Sichtbar erst im
  realen Skriptlauf; Abhilfe `$((byte & 0xFF))` in `qtmux.bash`/`.zsh`.
  Tests `tst_vtscreen` (3 Fälle) + `tst_session::osc7*` (2, volle Kette PTY→libvterm→Session).
  **Offen:** `pwsh` (nur PS 5.1 belegt), Linux-Lauf, `cmd.exe` (hat keinen Prompt-Hook), und
  die GUI kennzeichnet einen **entfernten** Pfad nicht als solchen.
- **Projekt-Befehle in der Befehlspalette (QTMUX-96):** `ProjectCommands::scan(dir)` (Gui-frei)
  liest `.claude/commands`, `.claude/skills`, `.gemini/commands` (TOML), `.junie/commands`,
  `.agents/skills`; `filterForAgent()` blendet auf den erkannten Agenten ein — **kein** erkannter
  Agent heißt **alles zeigen** (dieselbe Linie wie QTMUX-30: nichts ableiten; der Anwender tippt
  den Agenten oft erst noch, und ein verborgener Befehl sieht aus wie ein fehlender). QML-Brücke
  `App.projectCommands(dir, agentId)`, Absenden über `sessions.sendText(row, text, submit)` →
  `Session::writeWithEnter`. 🔑 **Die Enter-Verzögerung ist hier kein Detail:** Ein Palette-Befehl
  landet typischerweise in einem Agenten-TUI, und dort wird ein `\r` im selben Block als
  Einfügen gewertet (QTMUX-31).
  🔑 **Bei Skills bestimmt das VERZEICHNIS den Befehl**, `name:` im Frontmatter ist nur
  Anzeige-Label (an den echten Bäumen unter `~/.hermes/skills`, `~/.cursor/skills-cursor`
  gegengeprüft). Und die Ticket-Annahme `/db:reset` für verschachtelte `.claude/commands` ist in
  der aktuellen Doku **nicht belegt** (dort nur „File name without extension"); dokumentiert ist
  die Namespace-Regel allein bei Gemini CLI. Verkettet wird trotzdem einheitlich mit `:` — eine
  begründete Entscheidung, keine Messung.
  🔑 **Die neue Funktion legte einen alten Layout-Fehler frei — Befund der Bildabnahme.** Im
  Palette-Delegate hat der **Titel** `Layout.fillWidth: true` (Minimum also 0), die
  Beschreibung (`sub`) hatte **weder `elide` noch Deckel** und bekam ihre volle
  `implicitWidth`: Sie quetschte den **Befehlsnamen auf null**, ausgerechnet bei den ausführlich
  beschriebenen Einträgen. Jahrelang unauffällig, weil `sub` bis dahin nur kurze Tastenkürzel
  trug — die Projekt-Befehle sind die ersten Einträge mit langen Beschreibungen. Fix:
  `elide` + `Layout.maximumWidth: cmdRow.width * 0.55` an der Beschreibung.
  **Merke:** Ein neuer Datentyp in einer bestehenden Liste ist ein Layout-Risiko; kein Test
  findet so etwas, nur der Blick aufs Bild.
- **Umbenennen erhält den Aktivitäts-Indikator (QTMUX-116):** `windowTitle(w)` gab bei gesetztem
  custom `w.name` bisher sofort den Namen zurück und verwarf damit den Session-Titel samt
  führendem Aktivitäts-Indikator (Agenten wie Claude Code setzen `✳` U+2733 im Ruhezustand, einen
  Braille-Spinner U+2800–28FF beim Arbeiten an den Anfang des OSC-Titels). Diese Zeichen kann der
  Anwender im Umbenennen-Dialog nicht tippen. `windowTitle` stellt den Indikator jetzt dem Namen
  **dynamisch** voran — aus dem aktuellen Session-Titel gelesen (`dispTitle` hängt an
  `sessionsRevision`), NICHT statisch in `w.name` gespeichert, damit er dem Wechsel `✳↔Spinner`
  folgt; Hilfsfunktion `activityIndicator()`, Doppelungs-Schutz, wenn der Name schon einen trägt.
  🔑 **Der MCP-`rename_window`-Parameter heißt `name`, nicht `title`** — ein Aufruf mit `title`
  setzt still den leeren Namen (also gar keinen) und sieht dann wie „Rename wirkt nicht" aus.
- **Arbeitsverzeichnis (QTMUX-103):** `windowWorkingDir(w)` liefert das CWD des **aktiven**
  Panes, leer bei seriellen/Plugin-Sessions — daran hängen „Arbeitsverzeichnis öffnen" und
  „Pfad kopieren" ihr `enabled`. Geöffnet wird über `App.openLocalPath` (C++,
  `QUrl::fromLocalFile` + `QDesktopServices`) statt per `"file://" + pfad` in QML: nur so
  werden Leerzeichen kodiert und aus `C:\Pfad` ein gültiges `file:///C:/Pfad`.
  ⚠️ `Session::workingDirectory()` ist ein **Cache**, den `SessionModel` alle **1500 ms**
  auffrischt — direkt nach dem Start ist er noch leer. Wer ihn in einem Test ausliest, misst
  sonst „" und hält die Funktion für kaputt (genau so passiert).
- 🔑 `TerminalItem::setSession` ruft `recomputeGrid` **bedingungslos** — die Regel „ohne
  belastbare Größe nichts ableiten" liegt seit QTMUX-86 in `gridFor()` (s. o.), also an
  EINER Stelle. Vorher stand sie nur in `setSession`, und `geometryChange` hatte sie nicht
  (der Kommentar in [TerminalItem.cpp](src/terminal/TerminalItem.cpp) direkt über dem
  `recomputeGrid()`-Aufruf in `setSession` hält das fest — Zeilennummern veralten, der
  Anker­satz nicht).
- **Backend-Ownership:** Backend gehört NUR dem `unique_ptr` (kein `setParent`);
  stateChanged-Handler nimmt den State aus dem **Signal-Argument** (feuert während der
  Backend-Zerstörung).

### Online-Update (QTMUX-125)

Kern **byte-identisch aus MacPCAN vendiert** (`third_party/updater/update/`, Namespace
`appupdate`, Target **`qtmux_updater`** = STATIC + Qt6::Core/Network). Bewusst **nicht** in
`qtmux_core` — der bleibt Qt6::Core-only. Abgleich `tools/check-updater-sync.sh`
(`--update` zieht nach), Herkunft/Pin in
[third_party/updater/UPSTREAM.md](third_party/updater/UPSTREAM.md).
🔑 **Die Kopie liegt in einem Verzeichnis namens `update`**, weil sich der Kern selbst mit
dem Präfix `update/` inkludiert; flach vendiert müsste man jede `#include`-Zeile ändern und
gäbe die Byte-Identität — und damit den Sync-Wächter — auf.
⚠️ **Einbahnstraße:** nie lokal editieren. Erlebt und bewährt: Der Windows-Build fand einen
Fehler IM KERN (s. u.); der Fix ging nach MacPCAN, wurde dort gepusht und kam per
`--update` zurück.

**App-Seite:** [`UpdateViewModel`](src/viewmodels/UpdateViewModel.h) (Context-Property
`Updates`, Zustandsautomat Idle/Checking/UpToDate/Available/Downloading/Ready/Failed,
QSettings `update/autoCheck|lastCheck|skippedVersion|baseUrl`) +
[`qml/dialogs/UpdateDialog.qml`](qml/dialogs/UpdateDialog.qml) + Hilfe-Menü + zwei
Palette-Einträge + Einstellungen → Allgemein → „Aktualisierung". Start-Hook in `main.cpp`
(3 s nach dem Start; im Screenshot-Modus nie). Basis-URL `https://nobser.de/updates`,
Produkt `qtmux` → `…/qtmux/manifest.json`.

**Owner-Regeln, die im Code hängen** (jede hat einen Grund, keine ist Geschmack):
- Start-Check **höchstens 1×/Tag, abschaltbar, Fehler bleiben STILL** — ein Rechner ohne
  Netz darf nicht jeden Morgen mit einem Fehlerdialog begrüßen.
- Der Zeitstempel wird **auch nach einem Fehlschlag** geschrieben; sonst wird aus
  „1×/Tag" bei unerreichbarem Server „bei jedem Start".
- **„Version überspringen" bindet nur den stillen Check.** Von Hand sieht man sie weiter —
  sonst wäre ein Fehlklick unwiderruflich.
- **Downgrade** erlaubt (Owner), aber nur auf ausdrückliche Anforderung und mit Warnung;
  der Start-Check bietet ihn nie an, sonst fragte er jeden Entwickler-Build täglich.
- **Kein Silent-Self-Replace:** QTmux startet den Installer und bleibt stehen.

🔑 **Linux ohne `$APPIMAGE` hat keinen Start-Plan** — die AppImage-„Installation" IST die
Selbstersetzung von `$APPIMAGE`; läuft QTmux aus einem Distributionspaket oder einem
Entwickler-Build, gibt es nichts zu ersetzen. Der Dialog blendet „Installieren …" dann aus
und nennt stattdessen den Pfad der geprüften Datei (`canLaunchInstaller()`). Aufgefallen ist
das erst am **Linux-Build** — auf macOS/Windows gibt es den Fall nicht.

🔑 **Signierte Fixtures brauchen `-text` in `.gitattributes`.** Auf der Windows-Maschine
(`core.autocrlf=true`) machte git aus 935 Byte `manifest.json` 966 Byte — 31 eingefügte CR.
Die Ed25519-Signatur steht über die **exakten** Bytes, also fiel `test_updateviewmodel` mit
6 von 11 Fällen und sah dabei nach einem Fehler im Update-Code aus. Gilt für jedes signierte
Artefakt im Ökosystem.

🔑 **`busy()` darf im Abschluss-Callback nicht mehr wahr sein.** Der Kern hielt seine
`QPointer` auf die Reply bis zur nächsten Event-Loop-Runde (nur `deleteLater()`), also
meldete `busy()` „ja", während der Aufrufer schon „fertig" hörte. Wer daraus die nächste
Anfrage startet — Check → Download, also genau die GUI, weil erst dieser Callback den
Dialog aufgehen lässt — bekam `a request is already running`: **Der Dialog ging auf und
sein erster Knopf tat nichts.** Fix in MacPCAN `59a9e35` (`finishActive()`).
🔑 **Die Lehre daneben ist wertvoller als der Fix:** Das sah zwei Läufe lang wie ein
**Flake** aus (einmal rtzbld01, einmal CI-Windows) und verschwand beim Wiederholen. Es
trat nur über **HTTP** auf, weil `file://` anders verschränkt. **Ein sporadischer
Fehlschlag, der nur auf EINEM Transportweg auftritt, ist ein Timing-Fehler, kein
Rauschen.** Sichtbar wurde er erst, nachdem die Windows-Testbinaries auf das
Konsolen-Subsystem umgestellt waren — vorher meldete die CI nur „***Failed" ohne Fall.

🔑 **Auf Windows muss ein Datei-Handle VOR dem Löschen zu sein.** Bei SHA-Mismatch löscht der
Kern den Download — mit noch offenem Read-Back-Handle ist das dort ein stilles No-op: Der
Aufrufer bekam „file deleted", der beschädigte Installer blieb in Downloads liegen. Unter
POSIX unsichtbar (offene Dateien lassen sich entlinken). Fix in MacPCAN `d0ed07b`.

🔑 **`file://` prüft den echten Transportweg NICHT.** Zwei Dinge gehen daran vorbei: die
Cache-Bust-Abfrage `?ts=<epoch>` (der Kern hängt sie nur an http(s) an — an einem
Datei-URL zerstörte sie die Pfadauflösung) und ein Download, der in Häppchen ankommt und
darum überhaupt Fortschritt meldet. Deshalb hat `tst_updateviewmodel` einen eigenen
In-Process-HTTP-Server; genau dort ist der `busy()`-Fehler oben aufgeschlagen.

**Nächste Version veröffentlichen — das Rezept** (einmal komplett gefahren für 1.8.0):
1. Bump an den Stellen aus den Konventionen; `qtmux_version.h` danach **gegenlesen**.
2. Alle drei Plattformen bauen + testen, committen, pushen, CI abwarten.
3. Installer: `installer/build-dmg.sh <ver>` lokal · auf rtzbld01
   `C:\Tools\qtmux-build\build_msi.cmd <ver>` (Version ist **Argument** — s. Falle im
   Arbeitsstand) · AppImage aus dem **CI-Lauf desselben Commits**
   (`gh run download <id> -n QTmux-AppImage`).
4. **Am Artefakt gegenprüfen, nicht am Build-Log:** je Binary Treffer auf die neue
   Nummer **und 0 Reste der alten**. DMG mounten, ZIP entpacken, AppImage mit
   `--appimage-extract` auspacken (squashfs — ein `grep` aufs AppImage selbst findet nie
   etwas und sähe wie ein Fehler aus).
5. Tag + `gh release create` (voller SHA!), dann
   `UPDATES_SFTP_HOST=… python3 ../MacPCAN/tools/updates/publish.py --product qtmux
   --version <ver> --notes-de … --notes-en … --artifact <key>=<datei>,kind=<art> …
   --upload --verify`. Schlüssel und Zielpfad kennt `publish.py` als Vorgabe
   (`~/.ssh/updates_publish_ed25519`, `public_html/updates`).
6. Gegenprobe: `curl` + `openssl pkeyutl -verify` auf die Live-Bytes und ein manueller
   Check aus der App.

🔑 **Der Zyklus-Nachweis braucht eine ÄLTERE Instanz MIT Feature** — dafür ein
`git worktree` auf den Vor-Bump-Commit, dort ein temporäres Gerüst in `main.cpp`, das
`checkForUpdates` → `download()` → `launchInstaller()` durchruft und mitloggt. Der
Hauptbaum bleibt sauber, das Gerüst wird danach mit dem Worktree entfernt.
🔑 **Beleg für „kein stiller Selbsttausch":** SHA-256 **und** mtime des laufenden
Binaries vor und nach `launchInstaller()` vergleichen — beides unverändert.

**Tests:** `test_updater` (13 Fälle, Kern; Fixtures zur Laufzeit erzeugt und mit dem
mitkompilierten Monocypher signiert, dazu ein **RFC-8032-Vektor** als Gegenprobe gegen eine
nur zu sich selbst passende Krypto) und `test_updateviewmodel` (11 Fälle, App-Seite gegen
committete, mit dem **Produktionsschlüssel** signierte Fixtures unter
`tests/fixtures/update/`). 🔑 Letzteres ist zugleich der Interop-Nachweis der
ausgelieferten App (openssl signiert, das vendierte Monocypher verifiziert) — ein
Schlüsselwechsel macht den Test rot, und genau das ist der gewollte Alarm.

### QML-/Theming-Lektionen
- **Editor-Diagnosen (VSCode/qmlls), 2026-08-04:** Ausgangslage waren **über 2000**
  „Probleme" in den QML-Dateien, geblieben sind **2** (beide echt, s. u.). Drei Ursachen,
  jede mit eigener Abhilfe — die Begründungen stehen in den Dateien selbst
  ([.qmllint.ini](.qmllint.ini), [.vscode/settings.json](.vscode/settings.json)), hier nur
  das, was man ohne Messung nicht wiederfindet:
  1. **Ohne Importpfade kennt qmlls weder `QtQuick` noch das eigene Modul `QTmux`** — jede
     Typreferenz wird zur Fehlermeldung (1092 über 28 Dateien, davon 372 echt). Abhilfe ist
     Qts Schalter `QT_QML_GENERATE_QMLLS_INI`; die erzeugte `.qmlls.ini` enthält absolute
     Pfade und ist **git-ignoriert**. ⚠️ Sie entsteht **erst beim Build** (eigenes Target
     `qtmux_generate_qmlls_ini_file`, weil der Schreiber `$<TARGET_FILE:qtpaths>` braucht) —
     nach einem reinen `--preset`-Lauf fehlt sie und man sucht am falschen Ende. Dazu
     `…_NO_CMAKE_CALLS`: qmlls würde sonst **selbst CMake rufen** — ohne die VS-2022-Umgebung
     (QTMUX-79) und in das Verzeichnis, aus dem die produktive Instanz läuft (LNK1168).
  2. 🔑 **`qmllint` von der Kommandozeile ist NICHT das Messmittel.** Die Erweiterung
     `theqtcompany.qt-qml` lädt sich eine **eigene, neuere** `qmlls.exe` nach
     `%APPDATA%\Code\User\globalStorage\theqtcompany.qt-qml\qmlls\files\` und meldet damit
     Kategorien, die das Qt des Projekts gar nicht kennt (`id-shadows-member`,
     `confusing-expression-statement`, var-Hoisting). Wer nur die CLI prüft, hält die Arbeit
     für fertig, während die IDE weiter rot ist. Gegenprobe **gegen jene Datei**, per
     LSP-Handshake skriptbar (`initialize` → `didOpen` → `publishDiagnostics` zählen).
     ⚠️ Die erste Analyse braucht **30–45 s**; nach 18 s kamen 0 Diagnosen — das sah wie
     „alles behoben" aus.
  3. **`build/` muss aus dem Arbeitsbereich heraus** (`files.exclude`/`search.exclude`):
     dort liegen **1501** `.qml` — je Build-Verzeichnis eine Kopie unserer 28 **plus** Qts
     komplette `QtQuick.Controls.Basic`-Quellen, die `windeployqt` neben die EXE legt.
  🔑 **Nicht jede Meldung ist über eine Kategorie abschaltbar.** Die var-Hoisting-Warnung trägt
  keinen `[kategorie]`-Suffix und ließ sich weder über `VarUsedBeforeDeclaration` noch über
  `CompilerWarnings` stummschalten (beides gemessen) — sie ist deshalb **im Code** behoben
  (17× `var` → `let`). Kontrolle, dass keine Variable ihren Block verlässt: die Zahl der
  unqualifizierten Zugriffe in `Main.qml` ist vorher und nachher **0**.
  **Abgeschaltet** sind nur Kategorien, die hier Idiome treffen (Begründung je Eintrag in der
  Datei): `UnqualifiedAccess` — die Gui-freien Registries sind **Context-Properties**
  (s. Konventionen), ein Werkzeug kann sie prinzipiell nicht kennen —, `Comma` und
  `ConfusingExpressionStatement` (Revisions-Anker `(window.xRevision, …)`) sowie
  `IdShadowsMember`. Wer den Abbau von `Main.qml` angeht, schaltet `UnqualifiedAccess` für
  diese Arbeit wieder ein. **Die 2 verbliebenen sind echt** und bleiben sichtbar:
  `Member "title"/"needsAttention" not found on type "QObject"` in
  [SplitNode.qml](qml/SplitNode.qml) — `TerminalItem::session` ist `QObject*` und `Session`
  bewusst **kein** registrierter QML-Typ; ein Fix wäre ein Signaturwechsel, keine
  Konfigurationszeile.
- Popups/Menüs erben die Window-`palette` NICHT → `ThemedMenu`/`AppPopupBg` mit eigener
  Palette; Menübreite explizit setzen (`window.sizeMenu` → `contentWidth`); Basic-Style-
  Highlight im Hell-Modus braucht eigenen Hintergrund.
- Modale Dialoge: **Enter=OK** braucht In-Dialog-`Shortcut` (fensterweite feuern über
  Modals nicht) UND `TextField.onAccepted` (fokussierte Felder kapern Return via
  ShortcutOverride); Qt-Quick-`Button` im Fokus reagiert nur auf Leertaste. ESC via
  `closePolicy`.
- `header: ToolBar` braucht feste `height` (sonst Kollaps auf 0); verschachtelte
  `RowLayout`-`fillHeight`-Kinder brauchen `maximumHeight`/`fillHeight:false`.
- Icon-Tinting in Delegates: explizite `MultiEffect`-Form (`layer.effect` greift dort
  nicht zuverlässig). Icons: Phosphor-SVGs `qrc:/icons/`, via `icon.source`+`icon.color`.
  🔑 **`brightness: 1.0` VOR `colorization: 1.0` ist Pflicht** — colorize wichtet mit der
  Quell-Luminanz, und die Phosphor-SVGs sind schwarz (≈ 0), also bleibt das Icon sonst dunkel.
  Genau daran hing der Sidebar-Chevron im Dunkel-Design (er benutzte `layer.effect` **ohne**
  brightness). Und: **positive `rotation` dreht im Uhrzeigersinn** (y zeigt nach unten) — aus
  `caret-down` wird „links" bei **+90**, nicht bei −90.
- **Menüs (Design 1a, Stufe 4):** ein Menü enthält **Befehle, keine Zustände**; checkbar sind
  nur die drei Ansichtsumschalter (Seitenleiste, Statusleiste, Broadcast). Was ein Menü
  verlässt, muss in **Einstellungen UND Palette** landen (QTMUX-46) — die Palette bekommt
  dafür je Einstellungs-Kategorie und je Shell einen Eintrag.
  🔑 **`visible: false` an einem `Menu` blendet dessen MenuBar-Eintrag NICHT aus** (am Bild
  belegt: „Fenster" stand auf Windows trotzdem da). Nur ein nicht enthaltenes Menü
  verschwindet → `appMenuBar.removeMenu(macWindowMenu)` in `Component.onCompleted`.
  🔑 **QtQuick.Controls kennt keine Menü-Rollen.** `MenuItem.PreferencesRole` gibt es nur im
  veralteten `Qt.labs.platform`; `QQuickNativeMenuItem` ruft `setRole` nie auf (im Qt-Header
  geprüft). Auf macOS wandert „Einstellungen …" also **nicht** ins App-Menü — Ausblenden im
  Datei-Menü würde es dort schlicht entfernen. Deshalb überall sichtbar.
  🔑 **Ein deaktivierter Menüeintrag sah aus wie ein aktiver** (aufgefallen an „Diese Seite
  zurücksetzen", Stufe 6, gilt aber für JEDEN `enabled: false`-Eintrag der App): das
  contentItem des Basic-Styles färbt den Text **unbedingt** mit `palette.windowText`
  (Qt-Quelltext `Basic/MenuItem.qml`, Z. 48/59) — es gibt keinen Disabled-Zustand zu themen,
  und `palette.disabled.*` an `ThemedMenu` bleibt darum wirkungslos (ausprobiert, am Bild
  widerlegt, wieder entfernt). Lösung: `opacity: enabled ? 1.0 : 0.45` in
  [qml/Ui/ShortcutMenuItem.qml](qml/Ui/ShortcutMenuItem.qml) — dimmt die ganze Zeile inklusive
  Kürzel-Label. A/B am Screenshot belegt.
  🔑 **Standardknöpfe brauchen einen ZWEITEN Translator (QTMUX-117, behoben 2026-07-31).**
  Die Beschriftungen von `standardButtons` kommen nicht aus unseren `.ts`, sondern aus Qts
  eigener Übersetzung im Kontext **`QPlatformTheme`** — vorher hieß der Knopf jedes
  `AppDialog` auch auf Deutsch „Cancel" (per UIA belegt: `Buttons: … | OK | Cancel`).
  `applyLanguage` in [main.cpp](src/app/main.cpp) tauscht deshalb über `swapTranslator`
  **zwei** Translator: `qtmux_<lang>` und `qtbase_<lang>`.
  🔑 **Die `.qm` wird EINGEBETTET, nicht mitgeliefert** (CMakeLists, `qt_add_resources` mit
  `BASE ${QT_TRANSLATIONS_DIR}` aus `qmake -query`). Der naheliegende Weg — jedes der drei
  Deployment-Werkzeuge die Datei kopieren lassen — wären drei Konfigurationen für dieselbe
  Datei (macdeployqt kopiert `translations` **nicht**, windeployqt schon, linuxdeploy je nach
  Plugin), und der Fehler träfe **nur die gepackte App**, nie den Entwickler-Build. Als
  Ressource liegt sie überall unter `:/i18n/`. Fehlt sie in einer Qt-Installation, gibt es
  eine CMake-**Warnung** und `test_i18n` wird gar nicht erst angelegt — ein roter Test wäre
  hier ein Umgebungsproblem, keine Regression. Gegengeprüft: die aqt-Installationen auf
  rtzbld01 (Windows) und im `qtcache` (Linux) führen `qtbase_de.qm` beide.
  🔑 Messfalle: In einer `.qm` stehen die Texte **UTF-16BE**, ein ASCII-`grep -a` auf das
  Binary findet sie also prinzipiell nicht. Ressourcen*namen* legt Qt dagegen unkomprimiert
  als UTF-16BE ab — `"qtbase_de".encode("utf-16-be")` im Binary zählen ist damit der
  belastbare A/B-Beleg für die Einbettung (vorher 0, nachher 1).
- App-Icon: `resources/appicon/` (SVG → icns/ico/png via `generate.sh` + Qt-`svgrender`-
  Mini-Tool, da kein rsvg/inkscape auf den Maschinen).

### macOS-Spezifika
- **Sprache:** Translator + `singletonInstance(App)` VOR `loadFromModule` installieren;
  native App-Menü-Standarditems folgen **AppleLanguages** → in `main.cpp` vor
  `QGuiApplication` per `CFPreferencesSetAppValue` aus `ui/language` setzen (argv-Injektion
  wirkt NICHT); Laufzeit-Wechsel greift fürs native App-Menü erst nach Neustart.
- Native Menüs rendern keine QML-Icons (QTMUX-13, deferred).
- Quake-Modus: Carbon `RegisterEventHotKey` (Ctrl+`), ohne Bedienungshilfen-Rechte;
  Windows/Linux Stub. Session-Nav macOS: `Meta+Tab` (Ctrl+Tab = Cmd+Tab gehört dem OS).
- Einstellungen-Shortcut bewusst String „Ctrl+," statt `StandardKey.Preferences`
  (macOS verschöbe ihn ins App-Menü).

### Verbindungen, Vault, Profile
- **SSH/SFTP über System-Clients im PTY** (Auth/known_hosts „funktionieren einfach";
  SFTP: interaktives `sftp` bis zum `sftp> `-Prompt getrieben, TOFU `accept-new`).
- **SecretsVault:** Pure-Qt-Krypto (PBKDF2-HMAC-SHA512, 210k Iterationen; HMAC-SHA256-
  CTR-Keystream, Encrypt-then-MAC) — bewusste dependency-free-Abwägung, kein AES.
  **Vault-Verwaltung ist NIE über MCP exponiert** (Sicherheitsgrenze); Profile speichern
  nur den **Geheimnis-Namen** (`passwordSecret`), Auflösung intern.
- SSH-Passwort-Auto-Fill: Prompt-Scan auf `password:`, **genau einmal** senden (kein
  Lockout); Login-Scripts einmal am ersten OSC-133-Prompt bzw. Fallback-Timer 800 ms —
  beides NICHT beim Restore.
- Profile: `ConnectionProfileRegistry` (QSettings, Upsert über Name); Registry kennt
  keine Sessions — Starten macht QML (`window.connectProfile`).
- Hotkeys: `HotkeyRegistry` (Gui-frei, nur Overrides persistiert, Multi-Chord);
  während des Aufnahme-Dialogs alle App-Shortcuts deaktivieren.
- Color-Schemes: je ein Schema für Hell und Dunkel; `Theme` leitet ALLE Chrome-Farben
  aus dem aktiven Schema ab; Import iTerm/Xresources/Ghostty.

### Plugin-System (QTMUX-8/9)
- SDK `QTmuxPlugin.h` (IID `com.qtmux.PluginInterface/1.0` — bei inkompatiblen Änderungen
  hochzählen); Plugin linkt `qtmux_core` statisch. `PluginHost`-Suchpfade:
  `QTMUX_PLUGIN_DIR` → `<App>/plugins` → macOS `Contents/PlugIns` → `<AppData>/plugins`.
  Restore überspringt fehlende Plugins still. `qt_add_plugin` ohne Namespace-`CLASS_NAME`.
- **MacPCAN** (`plugins/macpcan/`, nur APPLE): CAN-Bus als Terminal-Backend, zwei Typen
  (`pcan` = Hardware, `pcan-mock` = Demo, bewusst ohne stillen Fallback). Aufbau, PCBUSB-
  Einbindung über `QTMUX_PCBUSB_DIR`, Terminal-UX, Fallen und v1-Backlog stehen in
  [plugins/macpcan/README.md](plugins/macpcan/README.md) — dort pflegen, nicht hier.

### Shells (Windows)
- `ShellRegistry`: cmd/PowerShell/pwsh + **„Eingabeaufforderung (Clink)"** wenn Clink
  installiert (GPL — bewusst nicht gebündelt, nur erkannt; `program` = komplette
  Kommandozeile, `PtyBackend` zerlegt via `splitCommand`). AutoRun-Dedup: ist Clink per
  cmd-AutoRun aktiv, wird der redundante Eintrag ausgeblendet.

### MCP-Server (39 Tools)
`src/server/McpServer.{h,cpp}`, HTTP/JSON-RPC, **Vorgabe** `127.0.0.1:7345`; Tool-Referenz in
`docs/MCP.md`. Kernpunkte:
- **Netzzugang ist eine WAHL, und sie kostet ein Token (QTMUX-127).** Bind-Adresse:
  `QTMUX_MCP_BIND` > Einstellung `mcp/bindAddress` > `127.0.0.1`; Regeln Gui-frei in
  [src/server/McpAccess.h](src/server/McpAccess.h) (Test `test_mcpaccess`, 13 Fälle),
  bedient über Einstellungen → Agenten & MCP (Schalter „Im Netzwerk erreichbar",
  Adressfeld, Token anzeigen/kopieren/neu erzeugen) und die Palette.
  🔑 **Ungültige Adresse fällt auf Loopback zurück, nie auf `Any`** — ein Tippfehler in
  der Einstellung darf den Server nicht ins Netz stellen; kein DNS (blockierte den Start
  und ein Name kann auf eine fremde Adresse zeigen). Gegentest: mit `Any` als Fallback
  fällt `invalidAddressFallsBackToLoopbackWithReason`.
  🔑 **Nicht-Loopback ⇒ Token-Pflicht für ALLE Anfragen**, auch die lokalen: `send_text`
  ist Befehlsausführung unter unserer UID. Ohne Token **startet der Server nicht**
  (`mcp.lastError` + qWarning) statt „unsicher, aber es läuft". Loopback bleibt
  tokenfrei — bestehende lokale Clients laufen unverändert.
  🔑 **Auto-Erzeugung nur, wenn die Öffnung aus der EINSTELLUNG kommt**: dann gibt es
  eine Oberfläche, die das Token anzeigt. Kommt sie aus `QTMUX_MCP_BIND` (Skript, CI),
  bekäme es niemand zu sehen → Startverweigerung ist die ehrlichere Antwort.
  🔑 Vergleich zeitkonstant, leeres erwartetes Token passt **nie**; 401 antwortet **vor**
  dem Ansehen des Rumpfes, dazu ein Deckel von 4 MiB je Request (vorher wuchs der Puffer
  unbegrenzt). `mcp/token` steht bewusst **nicht** in der Export-Allowlist (`SettingsIo`),
  `mcp/bindAddress` schon. Zweite Schicht auf Netzebene: [tools/pf/](tools/pf/) (macOS).
- **Controller-Auto-Erkennung** beim `initialize`: TCP-Port → PID → **Prozess-Vorfahren-
  kette** bis zur Session-Shell-PID (macOS gibt Environments fremder Prozesse nicht mehr
  heraus — daher Hierarchie statt `QTMUX_SESSION_ID`-Lesen); Fallback `attach_controller`.
  🔑 **Nur bei Loopback-Peer** (QTMUX-127): Die Heuristik sucht einen Prozess auf DIESER
  Maschine; bei einer Verbindung aus dem Netz gibt es ihn nicht, und ein lokaler Prozess
  mit zufällig gleichem Quellport würde fälschlich zur Controller-Session erklärt. Aus
  dem Netz gilt darum „unbekannt" (-1) → solche Clients müssen `sessionId` mitgeben.
  Positivkontrolle beim Prüfen ist Pflicht (sonst „behebt" man es durch Abschalten):
  LAN-Aufruf → `mcpController:false`, Aufruf **aus** einer Session → `true`.
- **Die Zugriffseinstellungen sind über MCP bewusst NICHT änderbar** — nur lesbar über
  `get_server_info` (ohne Token-Wert). Ein Fernsteuerungs-Endpunkt, der seine eigene
  Zugriffskontrolle umkonfigurieren kann, hat keine; dieselbe Linie wie beim Vault.
- **Long-Poll `wait_for_events`**: vor `callTool` abgezweigt, Socket bleibt offen
  (`PendingPoll` + QTimer, Default 25 s); `disconnected`-Handler räumt Polls ab.
- **Layout/Profile-Tools (QTMUX-29):** Layout und Windows leben in QML → diese Tools laufen
  über `*Requested`-Signale, deren QML-Handler **synchron** (Direct-Connection) läuft und über
  die **`provideResult`-Brücke** (`bridgedCall`) antwortet; ohne UI → „UI nicht verbunden".
  `list_profiles` liefert nur Flags, `connect_profile` löst Vault-Passwörter **intern**.
- **QTMUX-31 (`send_text`):** Das Enter geht **zeitlich abgesetzt** raus
  (`Session::writeWithEnter`, Vorgabe 60 ms, Tool-Parameter `enterDelayMs`). TUI-Apps
  (belegt mit Claude Code) werten einen in EINEM Rutsch ankommenden Block als
  Einfügevorgang → das `\r` wurde zum Zeilenumbruch im Eingabefeld statt zum Absenden,
  und der Aufruf meldete trotzdem `ok`. Regressionstest bricht bei `enterDelayMs: 0`.
- **QTMUX-30/37 (Ereignis-Kanal — die Quelle ist das Problem, nicht der Kanal):** QTmux
  leitet **nichts** aus Bildschirm/Prozesszustand ab; ein Claude-Code-Worker meldet von sich
  aus nichts (auch keine Bell). Deshalb Ehrlichkeit statt erzwungener Ereignisse:
  `subscribe_events` meldet je Quelle `hasPostedEvents`, `wait_for_events` bricht ohne Abo
  **sofort** ab (statt 25 s Stille) und legt bei Leerlauf einen `hinweis` bei. Worker
  ereignisfähig machen: Stop-Hook auf `shell-integration/qtmux-emit.{sh,ps1}` — **als Skript,
  nicht als curl-Einzeiler** (die dreifache Escape-Verschachtelung scheitert still und sieht
  aus wie „gerade passiert nichts"). Und: `wait_for_events` ist ein **Abholen** — es erreicht
  einen **beschäftigten** Agenten nicht; wecken kann nur dessen Umgebung, am Ende eines
  Hintergrundbefehls → `qtmux-wait.{sh,ps1,cmd}`. 🔑 Vier Fallen, jede erzeugt einen stumm
  nichts meldenden Wächter: `timeoutMs` **unter** dem HTTP-Timeout halten; `nextSeq` **immer**
  fortschreiben (sonst Endlos-Poll über dieselben gefilterten Ereignisse); Gesamt-Deckel auch
  **im laufenden Poll** prüfen (sonst überzieht er um eine Poll-Länge); POSIX-`read` verwirft
  das letzte Element ohne `\n` (`printf '%s'` → leeres `kinds`-Array → serverseitig „kein
  Filter" = alles; nur der **Gegentest** mit einem nicht passenden Ereignis zeigt das).
- **`get_layout`:** liefert `{layout, windowId, activePaneId, sessions}` — der Baum allein
  verschweigt, welche Sessions in **keinem** Pane liegen (laufen, aber unsichtbar).

## E2E-/Test-Fallen (alle Plattformen)

- ⚠️ **`tools\vsdev-build.cmd` baut standardmäßig NUR `qtmux` — Tests brauchen `all`**
  (`tools\vsdev-build.cmd windows all`). Steht im Kopf des Skripts, ist trotzdem passiert
  (2026-07-31): `ctest` lief danach gegen ein **altes** Testbinary, und zwar in beide
  Richtungen — erst meldete es „grün" für Tests, die es noch nicht kannte, dann bestand der
  **Gegentest** mit absichtlich kaputtem Code. Genau dieses „der Gegentest besteht" ist das
  Alarmsignal. Erste Messung darum immer die **mtime des Testbinaries**
  (`(Get-Item build\windows\test_<x>.exe).LastWriteTime`), nicht die ctest-Zusammenfassung.
- 🔑 **QtTest-Binaries schreiben hier nichts auf die Konsole** (`qt_add_executable` macht sie
  auf Windows zu GUI-Programmen) — `./test_x.exe | grep FAIL` liefert **leere** Ausgabe bei
  Exit 3. Ergebnisse mit `-o <datei>,txt` in eine Datei schreiben und die lesen; nur so sieht
  man, **welche** Fälle fielen. (PowerShell verschluckt zusätzlich die Ausgabe, wenn der
  Exit-Code ≠ 0 ist.)
  🔑 **In der CI kann man `-o` nicht nachschieben** — dort meldet `--output-on-failure` nur
  „***Failed" ohne den Fall (genau so bei `test_updateviewmodel` erlebt). Abhilfe für Tests,
  die **keine Prozesse starten**: `set_target_properties(<t> PROPERTIES WIN32_EXECUTABLE FALSE)`
  (seit 2026-08-02 für `test_updater`/`test_updateviewmodel`). Für `test_pty`/`test_session`/
  `test_sessiongroups` gilt das NICHT — die brauchen das GUI-Subsystem wegen der
  ConPTY-Konsolenvererbung.
- **Nach einem Rebuild `open qtmux.app` NICHT auf eine laufende Instanz** — `open`
  aktiviert nur; das alte Binary antwortet dann (z. B. „Unbekanntes Tool"). Erst beenden,
  dann starten.
- ⚠️ **„Fehlt die Funktion, oder nur das Binary?" — erst datieren, dann suchen.** Meldet
  jemand, eine gebaute Funktion sei in der GUI nicht auffindbar, ist die **erste** Messung das
  Alter des laufenden Programms: `ps -o pid,lstart,command -p <pid>` liefert den Pfad,
  `ls -la` darauf die mtime des Binaries — liegt sie **vor** dem Commit, ist die Frage
  beantwortet, ohne eine Zeile Quellcode zu lesen. Beweiskraft gibt der **Gegentest am
  Artefakt**: Dialogtexte sind als Klartext im Binary auffindbar
  (`grep -ac "<Text aus dem Dialog>" …/MacOS/qtmux`) — alt **0**, neu **>0**.
  🔑 `strings | grep` versagt dabei: QML wird per qmlcachegen eingebettet, `strings` fand 0
  Treffer in **beiden** Binaries und hätte den Fehlschluss „auch der neue Build hat es nicht"
  gestützt. `grep -a` direkt auf die Datei trennt sauber. Passiert am 2026-07-29 mit den
  Agenten-Schaltern aus QTMUX-85/98 (Quelle korrekt, Instanz vom Vortag).
- macOS-GUI-E2E: CGEvent-Tool braucht Pause zwischen MouseDown/Up (sonst nur Hover);
  App-Sprache über das App-Menü umstellen, nicht `defaults write` (cfprefsd-Cache);
  Details [[qtmux-gui-test-macos]].
- **Ohne Bedienungshilfen-Recht testen (macOS):** System Events/`osascript`/CGEvent scheitern
  hart (`-1719`, `AXIsProcessTrusted()`=false). Zwei Wege gehen trotzdem:
  (a) **Beenden** per `NSRunningApplication(processIdentifier:)?.terminate()` — dasselbe
  Apple-Event wie Cmd+Q, aber **PID-genau** (`tell application` ginge über die Bundle-ID und
  träfe die produktive Instanz); Beweiskraft nur mit **Gegentest** (mit Rückfrage: Prozess
  lebt; ohne: er endet); Einstellungen vorher per `defaults write` in die Profil-Domain
  (`com.qtmux.QTmux-<profil>`) — QSettings schreibt `/` als `.`.
  (b) **Maus-Gesten** per synthetischem `QMouseEvent` in den eigenen Prozess (s. QTMUX-100
  weiter unten) — braucht ebenfalls kein Recht.
- ⚠️ Ein temporärer Test-Hook kann selbst der Fehler sein: `Dialog.accept()` direkt in
  `onOpened` wird verschluckt (Popup ist mitten im Öffnen) und sah exakt aus wie ein
  kaputter Bestätigen-Pfad. Erst ein Timer (~400 ms) zeigte die Kette vollständig.
- Windows-E2E: Foreground nur zuverlässig mit `AttachThreadInput`; ein Alt-Stoß vor ESC
  schaltet den Qt-Menümodus (ESC schließt dann nur den). Menüs via UIA-`InvokePattern`
  öffnen. Synthetische Tasten erst nach Warteschleife aufs `MainWindowHandle`.
  ⚠️ **Anhängen an den Ziel-Thread allein genügt nicht**, wenn eine andere App den
  Vordergrund hält (hier VS Code): `AttachThreadInput` zusätzlich an den Thread des
  **aktuellen Vordergrundfensters** + `SystemParametersInfo(SPI_SETFOREGROUNDLOCKTIMEOUT,0)`,
  sonst schlägt `SetForegroundWindow` still fehl und die Tasten landen in der IDE.
  Vordergrund **prüfen** (`GetForegroundWindow()`), nicht annehmen — und den PID des
  Vordergrundfensters mitloggen, das benennt den Dieb sofort.
  🔑 **Fokus holen und Bild ziehen trennen.** Der Alt-Stoß schaltet den Qt-Menümodus und
  schließt damit ein gerade geöffnetes Popup: `tests/release-visual-check.ps1` fotografierte
  deshalb korrekte Fenster **ohne Menü** — sah wie „Menü öffnet nicht" aus, war die eigene
  Fokus-Routine. Seit 2026-07-30 hat das Skript `FocusWindow` + `GrabWindow` getrennt, läuft
  gegen eine **isolierte** Instanz (`-QtmuxProfile visualcheck`, Port 7346), beendet sie am
  Ende und killt nur Prozesse **des eigenen EXE-Pfads** (vorher `Get-Process qtmux |
  Stop-Process` — das hätte die Arbeitsinstanz mitgerissen).
  🔑 Die Werkzeug-Fallen dieser Maschine — Defender beendet `SendKeys`-Skripte, PS 5.1 liest
  UTF-8 als CP1252, ohne bedienten Desktop **keine** synthetische Eingabe und **kein**
  Bildschirm-Grab, `GetWindowTextW` braucht `CharSet.Unicode` — stehen vollständig in
  [[gui-testskripte-windows-fallen]]. Was trägt: **UIA-`InvokePattern`** (braucht keinen
  Eingabefokus) und **`PrintWindow`** mit `PW_RENDERFULLCONTENT`.
- **Das EINSTELLUNGSFENSTER fotografieren (Windows):** `--screenshot` greift nur das
  **Root**-Fenster, das Prefs-Fenster ist ein eigenes `Window` und fehlt darin. Weg, der
  trägt: Kategorie **vorher** in die QSettings-Domain schreiben
  (`HKCU:\Software\QTmux\QTmux-<profil>\ui\prefsCategory`), Instanz starten, per UIA
  „Datei" → „Einstellungen …" öffnen, dann per `EnumWindows` **nach Titel** suchen (in der
  UIA-Kinderliste des Desktops taucht es nicht auf) und mit `PrintWindow` greifen.
  Tastatur-Navigation als Steuerweg ist untauglich: die Rail hat nach dem Öffnen keinen Fokus,
  alle Bilder wurden identisch. 🔑 **Menü-Popups gehen mit demselben Weg**: Ein Qt-Menü ist hier
  **kein** eigenes Fenster, sondern ein Item **im** Prefs-Fenster; zur Sicherheit alle
  sichtbaren Fenster des PID einzeln greifen.
- ⚠️ **Native Dateidialoge sind hier nicht automatisierbar** (Folge des fehlenden bedienten
  Desktops): Das Fenster öffnet und ist per Titel auffindbar („Einstellungen exportieren"),
  aber UIA-`ValuePattern.SetValue` auf das Dateinamenfeld läuft in einen Timeout (0x80131505)
  und blockiert bei Wiederholung minutenlang; Einfügen per Zwischenablage + Enter kommt nicht
  an. Solche Pfade **nicht erzwingen**: Logik im Unit-Test beweisen, Dialog-Öffnen per
  Screenshot, Rest auf die Owner-Abnahme.
- ⚠️ **Umleiten von stdout/stderr beendet die App** (nicht nur die Sessions, Stufe-6-Erfahrung):
  `-RedirectStandardError/-Output` reißt die ConPTY-Anbindung → die einzige Session stirbt →
  das letzte Pane/Window schließt → QTmux beendet sich (QTMUX-87, gewolltes Verhalten); die
  leere Sidebar davor sieht wie ein Regressionsbug aus — genau so schon fehlinterpretiert. Für
  QML-Warnungen darum ein **eigener, kurzer Lauf** mit Umleitung: alle Bindungen von `Main.qml`
  **und** `PrefsWindow.qml` werden beim Start ausgewertet (das Einstellungsfenster ist eine
  Instanz in `Main.qml`, nur unsichtbar) — die Warnungen stehen also im Log, bevor die App
  sich beendet. Der eigentliche Interaktionslauf dann **ohne** Umleitung.
- ⚠️ **`--screenshot` NICHT aus dem Bash-Werkzeug starten.** Von dort entsteht kein PNG und
  der Exit-Code führt in die Irre (die GUI-App hängt nicht am Pipeline-Status). Richtig ist
  PowerShell mit `Start-Process … -PassThru -Wait` und danach `$pr.ExitCode` +
  `Test-Path` — so belegt: 46 kB PNG, Exit 0 (2026-07-30, vier Fehlversuche vorher).
- ⚠️ **Offscreen rendert `MultiEffect` NICHT** (2026-07-31, an der Seitenleiste erlebt).
  Alle per `MultiEffect` eingefärbten Icons — Sidebar-Chevron, Delegate-Icons — fehlen im
  `--screenshot`-Bild **ersatzlos und ohne Warnung**. Toolbar-Icons erscheinen trotzdem,
  weil sie über `icon.source`/`icon.color` laufen; das Bild sieht damit völlig plausibel
  aus. Wer daraus „das Element fehlt" schließt, diagnostiziert sein Messmittel — aufgefallen
  nur, weil der Chevron auch im **ausgeklappten** Zustand fehlte, wo er nachweislich seit
  Wochen funktioniert. **Abhilfe:** `QT_QPA_PLATFORM=cocoa` vor den Aufruf setzen —
  [main.cpp](src/app/main.cpp) überschreibt eine **bereits gesetzte** Variable nicht, der
  Flag greift dann am sichtbaren Fenster (derselbe Weg wie bei den README-Bildern). Nur so
  ist eine `MultiEffect`-Änderung abnehmbar.
- ⚠️ **Detektor-Blindheit (QTMUX-86, teuerste Fehldiagnose):** Um „geht Inhalt verloren?" zu
  messen, lief `read_screen` mit **`scrollback: true`** — der Inhalt war aber genau *dorthin*
  verschoben worden. Der Detektor fand die Marken also wieder und meldete „nur ein
  Rendering-Fehler, keine Daten weg". Erst der Blick auf den **Live**-Bildschirm (ohne
  Scrollback) zeigte: 0 Zeichen. **Regel:** Beim Suchen eines Verlusts das Messfenster genau
  so eng wählen wie die Behauptung — sonst beweist man die eigene Vermutung.
- **Maus-Gesten ohne Bedienungshilfen-Recht testen (QTMUX-100):** CGEvent scheitert hier
  (`AXIsProcessTrusted()` = false), aber **synthetische `QMouseEvent` in den eigenen Prozess**
  brauchen kein Recht: temporäres `Q_INVOKABLE dbgMouse(art, x, y)` auf dem AppController →
  `QGuiApplication::sendEvent(window, &ev)` mit press/move/release, dazu ein QML-Timer, der die
  Positionen protokolliert. Damit läuft der **echte** DragHandler gegen das **echte** Modell.
  🔑 Zwei Fallen dabei: (1) Ereignisse **realistisch schnell** schicken (16 ms/Schritt) — bei
  400 ms sieht Flickable keine Geschwindigkeit und verhält sich anders. (2) Ausgabe in eine
  **Datei** leiten, nicht in eine Pipe: wird der Prozess abgeschossen, geht der Pipe-Puffer
  verloren und es sieht aus, als hätte die App nichts gemeldet.
- ⚠️ **Ein Nachbau ist kein Beweis — er kann am Original vorbeigehen (QTMUX-100).** Für den
  Sidebar-Drag stand ein QML-Minimalnachbau, der eine Drift zeigte; die Ursache dort war aber
  die **Zielzeile 0 bei gescrollter Liste**, nicht der eigentliche Fehler. Erst die Messung an
  der echten App mit echten Ereignissen zeigte den wahren Auslöser (letzte Kachel, `contentY`).
  Der Nachbau hatte also *eine* Drift reproduziert und zu einer falschen Diagnose verleitet.
  **Regel:** Am Nachbau darf man Hypothesen bilden, entschieden wird am Original — und die
  Reproduktion muss die **konkret gemeldeten** Bedingungen treffen (hier: 3 Kacheln, keine
  Gruppen, bis an den Rand). Mit 5 Kacheln und einer mittleren Kachel war nichts zu sehen.
- ⚠️ **Instrumentierung ohne Aufrufstelle beweist nichts.** Ein Log meldete 98 abgefangene
  Nullgrößen — daraus schloss ich auf die Ursache. Mit mitgeloggter Aufrufstelle kamen **alle**
  aus einem Aufruf, den *dieselbe Änderung* neu eingeführt hatte; im alten Code gab es sie
  nicht. **Regel:** Jede Diagnose-Zeile trägt die Herkunft, sonst misst man den eigenen Fix.
- ⚠️ **Pixel-Prüfungen sind nur bei entsperrtem Bildschirm gültig.** Ein Lauf hat den
  Windows-**Sperrbildschirm** fotografiert und über alle Runden identische „Tinte"-Werte
  gemeldet, die wie ein Befund aussahen. Screen-Grabs immer gegen das erwartete Fenster
  gegenprüfen (Fenster-Handle + PID des Vordergrundfensters mitloggen).
- ⚠️ **Marker-Kollision = falsch-positiver E2E-Beweis.** Wird eine Marke per Befehl in die
  Session eingerichtet (`Set-PSReadLineKeyHandler … Insert("META_OK")`), steht sie durch das
  **Echo der Befehlszeile** schon auf dem Bildschirm — `read_screen` findet sie, obwohl nie
  eine Taste ankam. Marke im Befehl **zusammensetzen** (`'MET'+'A_OK'`), damit nur die
  tatsächliche Einfügung sie als Ganzes erzeugt. Aufgefallen nur, weil der Gegentest gegen
  das alte Binary „bestanden" meldete.
  ⚠️ Synthetische **Mausrad**-Ereignisse (`mouse_event WHEEL`) nimmt Qt erst nach einer
  **echten Cursorbewegung** an (Hover-Enter) und nur im Vordergrund — sonst verpuffen sie
  spurlos und man hält ein nicht scrollendes Flickable für ein Layout-Problem.
- ⚠️ **`--screenshot` auf Windows: erst Absturz, dann Kästchen — beides behoben (2026-07-30).**
  Warum `QT_QPA_PLATFORM=offscreen` dort zweifach untauglich ist (fehlendes Plugin → stummer
  `qFatal`-Absturz; mit Plugin → keine Fonts, jede Glyphe ein Kästchen bei Exit 0), steht in
  [[offscreen-plattform-windows-fonts]]. **Umsetzung hier:** `main.cpp` setzt unter Windows
  bewusst **kein** offscreen, sondern greift das **sichtbare** Fenster (TCC ist ein
  macOS-Grund); auf macOS/Linux bleibt offscreen, aber nur wenn das Plugin **vorhanden** ist
  (`offscreenPluginAvailable`), sonst derselbe Ausweichweg statt `qFatal`. Das Plugin wird
  trotzdem mitgeliefert (CMake-Post-Build **und** `build-msi.ps1` — das Paket staged separat,
  eine Stelle allein genügt nicht), am **paketierten** Binary gegengeprüft. Eingegrenzt wurde
  es per A/B: **`QTMUX_NO_GPU=1` allein läuft stabil**, der Anwenderfall `gpuRendering=false`
  war nie betroffen.
- **Laufende Instanz fotografieren (Windows):** `--screenshot` startet immer einen **neuen**
  Prozess. Wer eine **laufende** Instanz abbilden will, nimmt `PrintWindow` mit
  **`PW_RENDERFULLCONTENT` (2)** auf `MainWindowHandle` — braucht **keinen Vordergrund** und
  stört damit die Arbeit des Owners nicht (dem `keybd_event`-Weg vorzuziehen).
- ⚠️ **PowerShell-Testskripte: `$args` ist eine automatische Variable.** Ein Parameter dieses
  Namens (`function Mcp($name, $args)`) wird verschluckt — die MCP-Aufrufe gingen **ohne
  Argumente** hinaus. Symptom: `cwd` schien ignoriert, `send_text` tat nichts, `read_screen`
  antwortete „Parameter 'id' fehlt". Sah wie drei Fehler in der App aus, war einer im Skript.
  Dieselbe Klasse wie `$pid`/`$Profile` — Parameternamen in PowerShell-Harnessen präfixen.
- ⚠️ **`wait_for_events` ohne `afterSeq` zeigt NUR künftige Ereignisse.** Der Long-Poll steigt
  beim aktuellen Stand ein; ein soeben gesendetes Ereignis fehlt dann in der Antwort, und das
  sieht exakt aus wie „der Ereignisweg ist kaputt". Verräter ist `nextSeq` — steht es über 0,
  liegen Ereignisse vor. Für eine **Messung** darum `afterSeq: 0` übergeben. (Beim
  QTMUX-38-Nachweis erst als Fehlschlag gelesen.) Zweite Falle daneben: `subscribe_events`
  braucht die `sessionId` als **Argument**; ohne sie antwortet es „Keine Subscriber-Session",
  auch wenn `attach_controller` vorher „ok" meldete.
- MCP-E2E ist der Standard-Verifikationsweg gegen die echte GUI (create_session/send_text/
  read_screen, `scrollback:true` für Historie) — gegen eine **isolierte Testinstanz**
  (s. Build-Abschnitt macOS), nie gegen eine, in der jemand arbeitet. Ergebnisse möglichst
  am **Zustand** messen statt am Screenshot (z. B. Palette-Befehl ausführen → `list_sessions`
  prüfen); rein visuelle Änderungen brauchen `--screenshot`/Screenshot + Anwender-Abnahme.
  ⚠️ **Den MCP-Port VOR dem Messen auf Eigentümerschaft prüfen** (`lsof -nP -iTCP:<port>
  -sTCP:LISTEN`): Läuft dort schon eine fremde Testinstanz (paralleler Worker!), bindet die
  eigene still **nicht** — jede Antwort kommt dann von der fremden Instanz und sieht völlig
  plausibel aus. Genau so ging 2026-07-28 ein kompletter Messdurchlauf an die falsche App.
  🔑 **Am 2026-07-29 erneut hineingelaufen — der häufigste Fall ist die eigene Leiche.**
  Nicht ein fremder Worker, sondern eine **vergessene Instanz aus einem früheren Lauf
  derselben Sitzung** hielt den Port; `kill -TERM` hatte sie zuvor nicht erwischt. Symptom:
  MCP meldete 12 Zeilen mit den erwarteten Marken, während dieselbe Session in C++ nur den
  Prompt hatte. **Regel:** Die Prüfung als PID-**Vergleich** in das Testskript einbauen
  (`lsof`-PID gegen `$!`) und bei Ungleichheit abbrechen — die bloße „ist der Port belegt?"-
  Frage genügt nicht, denn belegt ist er ja, nur vom Falschen. Und der Widerspruch zweier
  Messwege ist das Alarmsignal: Zwei Quellen für dieselbe Session dürfen nie verschiedene
  Inhalte melden.
  ⚠️ **Persistenz nach dem Beenden mit `defaults read <domain>` lesen, NICHT mit `plutil` auf
  der `.plist`** — cfprefsd hält die Datei zurück; die Datei zeigte „gar keine `windows`-
  Schlüssel", während `defaults read` den korrekt geschriebenen Stand lieferte.
- **Agenten-Neustart prüfen (QTMUX-85):** Ein **Stub-Agent** ist der saubere Messfühler —
  eine ausführbare Datei, die wie ein Agent aus der Registry heißt (`opencode`, `qwen`) und
  `pid`, `pwd` und ihre Argumente ausgibt. 🔑 Sie über den **absoluten Pfad** aufrufen, nicht
  über `PATH`: Die Login-Shell baut `PATH` per `path_helper`/`.zshrc` um und stellt den echten
  Agenten davor — sonst startet man versehentlich das Original (passiert). 🔑 Die **PID im
  Marker** ist entscheidend: Der wiederhergestellte Scrollback enthält die *alte* Startzeile,
  ein bloßes „Marker gefunden" beweist also nichts; erst eine **neue** PID belegt einen echten
  Neustart. 🔑 Und `detect` prüft den **ersten** Token — `cd /tmp && opencode` erkennt nichts.
- **Doku-Wächter `test_doc_duplicates`** (QTMUX-34): findet doppelte Überschriften, wie
  sie beim Kompaktieren entstehen (Block eingefügt statt ersetzt → zwei gleichnamige
  Abschnitte mit widersprüchlichem Inhalt; in RAFTNG genau so passiert). Verglichen wird
  der Überschriften-**Pfad**, damit das zweisprachige README keinen Fehlalarm auslöst.
  `file(STRINGS)` braucht dort **`ENCODING UTF-8`** — sonst verschluckt CMake bei Zeilen
  mit Emoji den Zeilenanfang, die `##`-Marke geht verloren und der Pfad verrutscht.
- **CMake-Skripttests (`cmake -P`) laufen ohne Policies** — ohne `cmake_minimum_required`
  im Skript steht CMP0057 auf OLD und `IN_LIST` ist dann kein Operator, sondern ein
  Fehler. Lokal unsichtbar, wenn die eigene CMake neuer ist als die des CI-Runners
  (so brach `test_doc_duplicates` nur den Linux-Job, macOS/Windows waren grün).
  In Skripttests daher `cmake_minimum_required` setzen **und** policy-unabhängige
  Befehle bevorzugen (`list(FIND)` statt `IN_LIST`). Neue CMake-Versionen kennen das
  OLD-Verhalten alter Policies teils nicht mehr → der CI-Zustand ist lokal nicht
  nachstellbar; dann die Ursache strukturell ausschließen statt sie zu reproduzieren.
- Claude-CLI-Fallen (Agenten-Demos): `--settings` braucht eine DATEI; `--allowedTools`
  ist variadisch → Prompt via stdin.
