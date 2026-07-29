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
| `src/server/McpServer.{h,cpp}` | Eingebetteter MCP-Server (37 Tools); Doku `docs/MCP.md` |
| `src/terminal/TerminalItem.{h,cpp}` / `GlyphAtlas.{h,cpp}` | Rendering (GPU-Atlas + Fallback), Selektion, Copy/Paste, Maus-Reporting |
| `qml/Main.qml` / `qml/SplitNode.qml` | App-Shell + rekursiver Split-Layout-Baum |
| `plugins/echo/`, `plugins/macpcan/` | Demo-Plugin (Kopiervorlage) + CAN-Bus-Plugin |
| `installer/build-{dmg.sh,msi.ps1,appimage.sh}` | Installer aller 3 Plattformen (hand-gerollt, bewusst kein CPack) |
| `tools/vsdev-build.cmd` | Windows-Build in der **VS-2022**-Umgebung (vswhere-begrenzt); von der VSCode-Task genutzt, s. Build-Abschnitt (QTMUX-79) |
| `shell-integration/qtmux.{bash,zsh,ps1}`, `qtmux-event.cmd`, `qtmux-emit.{sh,ps1,cmd}`, `qtmux-wait.{sh,ps1,cmd}` | OSC-133-Marker, `qtmux-notify`/`qtmux-event`, Hook-Helfer zum **Senden** (HTTP, QTMUX-30) und zum **Warten** (Hintergrund-Wächter, QTMUX-37) |
| `tests/` | 18 ctest-Tests: 17 QtTest-Binaries (pty, vtscreen, linkdetector, session, sessiongroups, windowmodel, agent, profiles, hotkeys, vault, sftp, plugins, agenteventhub, macpcan, keyencoding, terminalsearch, terminalgrid) + `test_doc_duplicates` (reines CMake-Skript) |

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

> **⚠️ Zwei Visual Studios auf einer Maschine (QTMUX-79).** Neben VS 2022 liegt **VS 18**
> („2026"), und die CMake-Tools-Erweiterung injiziert immer die **neueste** Dev-Umgebung —
> wählbar ist die Installation nicht (1.23.52 kennt nur `useVsDeveloperEnvironment` /
> `preferredGenerators`). VS-2022-`cl.exe` + VS-18-STL ergibt `error STL1001: Unexpected
> compiler version, expected MSVC Compiler 19.50 or newer`. Deshalb: Umgebung kommt aus
> **`tools/vsdev-build.cmd`** (vswhere auf `[17.0,18.0)` begrenzt), aufgerufen von der Task
> „CMake: build"; `useVsDeveloperEnvironment: never`, Debug-Pfad in `launch.json` fest aufs
> `windows`-Preset. VS 2022 bleibt Standard (Qt ist `msvc2022_64`, CI und `build-msi.ps1`
> ebenso).
> ⚠️ **Folgefehler von `never` (2026-07-28) — alle drei Wege müssen durchs Skript.** Ohne
> Injektion baute der **eigene** Prozess der Erweiterung ganz ohne VS-Umgebung: Compiler und
> Linker findet Ninja noch über die absoluten Cache-Pfade, aber `INCLUDE`/`LIB` fehlen →
> `fatal error C1083: "type_traits"` und `LNK1104: "iphlpapi.lib"`. Tückisch: **dasselbe
> Preset im Release „ging"**, weil dort nichts zu übersetzen war (`ninja: no work to do`
> braucht keinen Compiler) — der Fehler sah preset-spezifisch aus, war aber nur
> „Debug hatte etwas zu tun". Gegenprobe, die das festnagelt: **derselbe** Befehl der
> Erweiterung scheitert in einer normalen Shell mit `C1083`, durch den Wrapper (unten) läuft
> er durch — es ist die Umgebung, nicht der Code.
> ⚠️ **`cmake.buildTask: true` löst das NICHT** (2026-07-29 verworfen, war einen Tag lang als
> Abhilfe eingetragen): `findBuildTask()` in `dist/main.js` holt die Task per
> `fetchTasks({ type: "cmake" })` — eine **`type: shell`**-Task findet sie nicht und baut
> **still wieder selbst**. Im Log erkennbar daran, dass dort weiter
> `[proc] Executing command: … cmake.EXE --build …` steht statt eines Task-Terminals.
> ✅ **Was trägt:** `tools/cmake-vsdev.cmd` — ein cmake-**Wrapper**, der vswhere+vcvars64
> (VS 2022) selbst herstellt und alle Argumente durchreicht. Eintrag in die **Benutzer**-
> Einstellungen (nicht ins Repo, `.vscode/settings.json` gilt auch für macOS/Linux):
> `"cmake.cmakePath": "…/tools/cmake-vsdev.cmd"`. Damit läuft **jeder** cmake-Aufruf der
> Erweiterung (Build-Knopf, Palette, Konfigurieren) in der richtigen Umgebung. Unabhängig
> davon bleiben `configureOnOpen`/`configureOnEdit`/`automaticReconfigure` **aus**
> (`automaticReconfigure` feuert beim **Preset-Wechsel**), und Strg+Umschalt+B / F5 gehen
> über die Tasks → `tools/vsdev-build.cmd`. Die Task setzt das **aktive** Preset ein
> (`${command:cmake.activeBuildPresetName}`) — sonst baut sie bei gewähltem Release stumm
> Debug; das Argument ist im Skript **gequotet**, damit ein etwaiger Anzeigename
> („Windows (MSVC)") sauber als `No such preset` scheitert statt die Batch-Zeile zu zerlegen.
> 🔑 **Zweiter, latenter Fehler (2026-07-29 mitgefunden):** Das `windows`-Preset hatte als
> Qt-Fallback `C:/Qt/6.10.3/msvc2022_64` — auf dieser Maschine ist nur **6.11.1** installiert.
> Bestehende Build-Verzeichnisse merkten das nicht (Qt lag schon im Cache), ein **frisches**
> hätte Qt nicht gefunden. Jetzt 6.11.1; die CI ist davon unabhängig (sie ruft `cmake` direkt
> mit `QT_ROOT_DIR` auf, ohne Presets). Eigene Qt-Installation weiter über `QTMUX_QT_PREFIX`.
> 🔑 Batch-Fallen in dieser Datei: **CRLF** Pflicht (bei LF führt cmd.exe Kommentarzeilen
> aus), **rein ASCII**, und `%ProgramFiles(x86)%` **nie in einer `for`-/`if`-Klammer**
> expandieren — das `)` aus „(x86)" schließt sie vorzeitig („Der Befehl `C:\Program` ist
> entweder falsch geschrieben"). vswhere schreibt darum in eine temporäre Datei.

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
  `ReadProcessMemory`) implementiert — **Funktionstest offen = QTMUX-2** (braucht Windows).
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

## CI (GitHub Actions)

`.github/workflows/ci.yml`: Build + headless-Tests (`QT_QPA_PLATFORM=offscreen`) auf
macOS/Windows/Linux; Qt via `jurplel/install-qt-action` (Module **qtserialport** +
**qtshadertools**); Windows-Tests informativ (`continue-on-error`, ConPTY-Konsolen-
Anbindung). Linux-Job baut zusätzlich das AppImage (Artefakt `QTmux-AppImage`).
Actions warnen über Node-20-Deprecation (ab Sept. 2026) — bei Gelegenheit anheben.

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
  Gui-freie Singletons als **Context-Properties** in `main.cpp` registrieren (KEIN
  `qmlRegisterSingletonInstance` in die URI „QTmux" — kollidiert mit der Modul-
  Typregistrierung; Symptom „TerminalItem is not a type").
- **Bei jedem Build-Zyklus auch Release bauen** (Presets `*-release`; Standard-Presets
  sind Debug) — Release-only-Probleme (Optimierung, Asserts, RHI/Shader) fallen sonst
  nicht auf.
- **Versions-Bump-Stellen** (alle zusammen): `CMakeLists.txt` (project VERSION),
  `src/app/main.cpp`, `src/server/McpServer.cpp` (serverInfo), `installer/build-dmg.sh`,
  `installer/build-appimage.sh`, `.github/workflows/ci.yml` (AppImage-Schritt),
  `installer/QTmux.wxs`, `README.md` (DE **und** EN).
- **i18n:** Quellsprache Deutsch; QML `qsTr`, C++ `QCoreApplication::translate("<Kontext>",…)`.
  `cmake --build … --target update_translations` (lupdate scannt automatisch alle Targets
  inkl. `qtmux_core`); `cmake/FinishSourceLanguageTs.cmake` finalisiert die DE-Datei
  automatisch — **nur `i18n/qtmux_en.ts` braucht echte Übersetzungspflege**. Eigennamen
  (PowerShell, Bash, …) bleiben unübersetzt.
- README.md ist **zweisprachig** (DE/EN, Anker `#-deutsch`/`#-english`) — beide Hälften
  pflegen.

## Status (2026-07-29)

**Aktuell:** v1.6.1 ausgeliefert (alle 4 Installer: DMG/MSI+ZIP/AppImage). Phasen 0–6
komplett (Terminal-Kern, Sessions/Sidebar, Agent-Awareness, SSH/Seriell/SFTP, Plugins +
MacPCAN, Installer). CI grün auf macOS/Windows/Linux (Qt 6.10.3). **37 MCP-Tools**
(GUI-MCP-Parität für den geplanten AI-Companion). i18n finalisiert.

**Window-Modell (QTMUX-83, auf `main`, ungereleast):** Kein globales Split-Layout mehr,
sondern das tmux-Modell — Sidebar = **Windows** (Tabs), jedes Window hat sein eigenes
Split-Layout, Splits = Panes **im** Window, `focus_window` schaltet das ganze Layout um.
Gruppen sind seither **Window**-Gruppen. Details/Verifikation:
`docs/design/per-window-layouts/Umsetzung.md`; Mechanik unten in der Feature-Referenz.
🔑 **Zwei Nachbesserungen aus dem Anwendertest (QTMUX-87):** (1) Das **letzte** Fenster zu
schließen **beendet QTmux** (`requestQuit`, normale Rückfrage) — vorher entstand sofort ein
neues, leeres Fenster mit höherer ID, was wie ein durchlaufender Zähler aussah und die
Meldung „Sessions nicht wiederhergestellt" erzeugte (in Wahrheit hatte das Schließen die
Sessions beendet). Über **MCP** beendet `close_window` die App bewusst **nicht** (ein
aufräumender Agent würde sich sonst selbst abschalten) — es meldet einen Hinweis.
(2) Die Kachel zeigt wieder die **Session-ID des aktiven Panes** (QTMUX-44), nicht die
Window-ID: Nur damit weiß man, was man `send_text`/`read_screen` übergibt.
Vorarbeit QTMUX-80/81/82, dabei **stiller Selbst-Screenshot** `--screenshot <png>`
(offscreen `grabWindow`, kein TCC) — der Standardweg für visuelle Abnahmen.
⚠️ **Aber: `--screenshot` setzt `QTMUX_NO_GPU=1`** ([src/app/main.cpp](src/app/main.cpp)) und
fotografiert damit den **QPainter-Fallback**, nicht den GPU-Pfad. Fehler, die im Glyph-Atlas
sitzen (QTMUX-97), sind darauf **prinzipiell unsichtbar** — dafür braucht es den Atlas selbst
als Messobjekt oder die Owner-Abnahme an einer laufenden Instanz.

## Nächster Schritt (Wiedereinstieg nach /compact)

Stand **2026-07-29** · Branch `main`, letzter **Code**-Commit `0dd1cc5` (QTMUX-98), gefolgt von
der Doku-Konsolidierung `a16d8ac`; beides **gepusht**, Working Tree sauber.
macOS: `macos-test` und `macos-release` tragen den
Endstand (18/18 grün, Debug und Release). Windows-Maschine (Stand 2026-07-29): `windows`
(Debug) **und** `windows-release` frisch auf diesem Stand, `ctest -E "^test_pty$"` beidseitig
**17/17 grün** (test_pty fällt hier umgebungsbedingt auch auf unverändertem Stand —
nicht-interaktive Shell, ConPTY). Die Instrumentierung aus der QTMUX-86-Untersuchung ist
**zurückgenommen**. 🔑 Auf dieser Maschine zeigt `"cmake.cmakePath"` in den **Benutzer**-
Einstellungen auf `tools/cmake-vsdev.cmd` — ohne diesen Eintrag scheitert der Build-Knopf der
CMake-Tools an fehlendem `INCLUDE`/`LIB` (Begründung im QTMUX-79-Kasten oben).

⚠️ **Einstellungs-Audit 2026-07-29 — „die Schalter fehlen im Dialog" war die falsche Fährte.**
Anlass: Die Agenten-Optionen aus QTMUX-85/98 waren in der laufenden App nicht auffindbar.
Ursache ist **nicht** der Dialog, sondern das **Alter der laufenden Instanz**: PID 9561 läuft
aus `build/macos`, dessen Binary vom **28.07. 22:30** stammt — also **vor** QTMUX-85 (23:47)
und QTMUX-98 (01:52). Gegentest über die Dialogtexte im Binary
(`grep -ac "Agenten beim Start wiederherstellen"`): `build/macos` **0**, `build/macos-test`
**6**, `build/macos-release` **6**. Die Schalter existieren also, sie sind nur nicht in dem
Programm, das gerade läuft.
Die daraus abgeleitete Dauerregel steht in den E2E-Fallen („Fehlt die Funktion, oder nur das
Binary?").
🔑 **Ergebnis des vollständigen Abgleichs** (`Settings`-Block in [qml/Main.qml](qml/Main.qml)
gegen [qml/prefs/](qml/prefs/)): **alle** Nutzer-Einstellungen liegen im Dialog. Nicht dort sind
nur `newSessionType` und `collapsedGroups` — beides **Laufzeitzustand**, keine Einstellung, also
korrekt. Sprache/Theme (`ui/language`, `ui/themeMode`) sitzen in `CatAllgemein`, MCP-Port und
-Schalter in `CatAgenten`.
🔑 **Eine echte Lücke hat der Audit trotzdem gefunden → QTMUX-99:** Für die **Agenten** gibt es
die Wahl, für die **Sessions** selbst nicht — `window.restoreWindows()` läuft in
[qml/Main.qml](qml/Main.qml) Z. 1331 **bedingungslos**. Wer beim Start bewusst leer anfangen
will, kann das nicht einstellen.

**QTMUX-97 (Emoji-Artefakte im Terminal) ist behoben + Owner-abgenommen (2026-07-28).**
Gelbe Dreiecks-Bruchstücke auf fremden Zeichen und einzelne Buchstaben in Emoji-Farbe.
Ursache **zweistufig**: libvterm gab `⚠️` (U+26A0+VS-16) nur **1 Zelle**, Qt malte die
Emoji-Form **2,15 Zellen** breit → Überhang in die Nachbarkachel des Glyph-Atlas. Fix in
[third_party/libvterm/src/state.c](third_party/libvterm/src/state.c) (TR#51-Breite, s.
libvterm-Abschnitt) + hartes Kachel-Clipping in
[src/terminal/GlyphAtlas.cpp](src/terminal/GlyphAtlas.cpp) (Details in der Feature-Referenz
unter „Rendering"). Debug **und** Release 18/18 grün; neuer Test
`tst_vtscreen::emojiPresentationWidth` (Gegentest ohne Patch: FAIL). Abnahme durch den
Owner am laufenden A/B: Testinstanz **7346** (mit Fix) gegen die alte aus `build/macos`
auf **7347** — „Neu ist gut, alt ist Murx."
⚠️ **Noch nicht in der produktiven Instanz**: die läuft aus `build/macos` (WIP-Stand) und
bekommt den Fix erst bei einem Neubau dieses Verzeichnisses — der die laufenden Sessions
mitreißt, also nur nach Owner-Freigabe.

**QTMUX-86 (leeres Pane beim Window-Wechsel) ist behoben** — Ursache war **nicht** das
Rendering, sondern eine transiente Layout-Größe (2 Zeilen), die bis zur Session durchgereicht
wurde; Details + A/B-Belege in der Feature-Referenz („Session-Größe wird entprellt").
**Owner-Abnahme offen:** in der eigenen Instanz mehrfach zwischen Splitscreen- und
Einzel-Window wechseln — der Prompt muss stehen bleiben. Der Schaden an bereits betroffenen
Sessions bleibt bestehen (Inhalt liegt in deren Scrollback); ein Neustart der Session räumt auf.

**QTMUX-85 + QTMUX-98 (Agenten-Wiederherstellung) sind umgesetzt und verifiziert
(2026-07-28/29), beide Jira dual auf „In Progress"/„In Arbeit" — Owner-Abnahme offen.**
Ein Schalter „Agenten beim Start wiederherstellen" (Vorgabe **AUS**) plus die **Wahl**
„Unterhaltung fortsetzen" mit vier Modi (QTMUX-98, Vorgabe **gar nicht**); beides in
Einstellungen → Agenten & MCP, Menü **Agent**, Palette und Suchindex. Mechanik samt Fallen
in der Feature-Referenz unter „Agenten überleben den Neustart" und „Unterhaltung fortsetzen
ist eine WAHL". Der zweite Teil von QTMUX-85 — die **zuletzt aktive Session** — war bereits
erledigt: `activePaneId` je Window und `windows/activeRow` sind persistiert
([WindowModel.cpp](src/viewmodels/WindowModel.cpp) Z. 213/370/390) und werden in
`restoreWindows`/`loadWindow` ausgewertet; nichts wird heuristisch geraten.
**Abnahme-Rezept:** Schalter in Einstellungen → Agenten & MCP anschalten, mit echtem `claude`
arbeiten, QTmux beenden und neu starten; danach die vier Fortsetzungs-Modi durchspielen (für
Modus 3 muss der Agent vorher per MCP `set_agent_session` seine Kennung gemeldet haben).
⚠️ Geht **nur an einer frisch gebauten Instanz** — die laufende kennt die Schalter nicht
(s. Einstellungs-Audit oben).

**QTMUX-99 (Umfang der Wiederherstellung) ist umgesetzt und verifiziert (2026-07-29),
Owner-Abnahme offen.** Dreiwahl statt Schalter (Owner-Entscheidung); Mechanik und die beiden
Fallen in der Feature-Referenz unter „Umfang der Wiederherstellung ist eine WAHL".

**QTMUX-100 (Sidebar-Drag lief weg) ist behoben und verifiziert (2026-07-29), Owner-Abnahme
offen.** Ursache und Messwerte in der Feature-Referenz („Niemals ein ListView-Delegat als
`DragHandler.target`"); der Messweg für Maus-Gesten steht in den E2E-Fallen.
⚠️ **Nebenbefund, ungeprüft:** Wird eine Kachel auf Zeile 0 geschoben, während die Liste
**gescrollt** ist, driftet `contentY` über mehrere Vorgänge (im Nachbau 160 → 144 → 84 → 24).
Anderer Pfad als der behobene, in der App **nicht** gegengeprüft — im Ticket QTMUX-100 notiert.

**Danach:** (1) **`build/macos` neu bauen** und die Produktivinstanz darauf umstellen — erst
damit sind QTMUX-85/97/98/99 überhaupt bedienbar und abnehmbar (s. Arbeitsstand) ·
(2) Owner-Abnahme QTMUX-85/98/99 und QTMUX-86 · (3) `build/windows` (Debug) auf den aktuellen
`main` heben, sobald die dortige Instanz beendet werden darf — zurzeit sperrt sie
`qtmux.exe`/`qtmux_echo_plugin.dll` · (4) offene Jira nach Priorität: 88/40/38/2/13.

### Arbeitsstand (compact-fest — hier pflegen, nicht im Gespräch lassen)

- ⚠️ **`build/macos` ist der Nachzügler — und die Produktivinstanz läuft daraus.** Stand des
  Binaries: **28.07. 22:30**, also ohne QTMUX-85, -97 und -98. Die laufende Instanz (zuletzt
  PID 9561, gestartet 29.07. 01:55) hat deren Funktionen deshalb **nicht** — genau daraus
  entstand die Fehlannahme, die Einstellungen seien nicht im Dialog gelandet. Verifizierte
  Endstände: `build/macos-test` (Debug) und `build/macos-release`.
  **Vor dem nächsten Prod-Neustart** `build/macos` neu bauen (`cmake --build build/macos`,
  NICHT `--preset` mit `-B`) — erst wenn die laufende Instanz beendet werden darf, denn der
  Neubau überschreibt das Binary und reißt alle Terminal-Sessions mit.
  🔑 **Dauerhafte Konsequenz:** Solange die Produktivinstanz aus einem Build-Verzeichnis läuft,
  ist „steht im Repo" **nie** gleichbedeutend mit „ist in der App". Jede Owner-Abnahme braucht
  darum entweder eine frische Testinstanz (`QTMUX_PROFILE=test QTMUX_MCP_PORT=7346`) oder einen
  Neubau von `build/macos` mit Freigabe.
- **Jira-Stand (2026-07-29):** beide Systeme laufen synchron bis **QTMUX-99**; 97 Done, 85 und
  98 „In Progress"/„In Arbeit" (umgesetzt + verifiziert, Owner-Abnahme offen), 99 neu im
  Backlog. 🔑 **Lektion:** Vor dem Anlegen eines Tickets die höchste Nummer in **beiden**
  Systemen holen (`ORDER BY key DESC`, maxResults 1) — nur solange beide gleich stehen, bleiben
  die Keys deckungsgleich. Und prüfen, ob die Doku bereits höhere Nummern *vergeben* hat.
  Die alte Notiz „QTMUX-46/-79 nicht angelegt" war schlicht falsch, beide existieren und sind
  Done — Behauptungen dieser Art vor dem Handeln gegenprüfen.
  🔑 **Werkzeug-Falle:** Für die **Cloud** schlägt Python-`urllib` hier mit
  `CERTIFICATE_VERIFY_FAILED` fehl (kein Issuer im Store) — ADF-Rumpf mit Python **bauen**,
  aber mit `curl --data @datei` **senden**. On-prem braucht ohnehin `curl -k`.
- **QTMUX-84 fertig + abgenommen (2026-07-28).** Meta-Kodierung umgesetzt, Debug **und**
  Release gebaut, `ctest -E "^test_pty$"` beidseitig 16/16. Abnahme in einer **echten
  Claude-Code-Session** (v2.1.220): Bild in der Zwischenablage → Alt+V → `❯  [Image #1]`
  erscheint. Gegentest gegen das Binary von 11:22:57 zeigte `probe:u` statt der Marke;
  AltGr am deutschen Layout unversehrt (`altgr:@`). Harness-Skripte liegen im Scratchpad
  (`alt-meta-e2e.ps1`, `altgr-e2e.ps1`, `altv-claude-e2e.ps1`) — nicht im Repo.
- **Korrektur eines früheren Befunds:** notiert war „Alt+&lt;Taste&gt; → 0 Bytes" (aus dem
  Code gelesen). Empirisch geht das **nackte Zeichen** raus (`text()` war bei synthetischem
  Alt+u gefüllt) — der Agent sah also ein normales `u` statt des Akkords. Beide Wege sind
  jetzt abgedeckt (text() gefüllt → ESC davor; leer → Zeichen aus dem Key-Code).

**Offene Jira:** **QTMUX-88** (AgentRegistry deckt nur 8 CLI-Agenten ab und enthält einen
Fehler — `cursor` statt `cursor-agent`; Ticket trägt die Arbeitsanweisung inkl. Alias-Umbau,
Recherchestand 2026-07-28 und der Begründung, warum `air`/`q`/`warp` **nicht** hineingehören;
dort gehört auch hinein, für welche Agenten die **Fortsetzungs**-Vorlagen aus QTMUX-98 noch
fehlen — verifiziert sind nur claude/agy/opencode/hermes, codex/gemini/aider/cursor/qwen
sind bewusst leer, s. a. QTMUX-**91**) ·
**QTMUX-40** (OSC-8-Hyperlinks — deferred; die Heuristik-Links aus QTMUX-39
decken den Agenten-Fall ab, OSC-8 bräuchte Cursor-Span-Tracking + neues `Cell`-Feld, teuer da
`VtScreen` den Sichtbereich lazy aus libvterm bildet) · **QTMUX-38** (Shell-Helfer für
Installationsnutzer unerreichbar — nur im Repo, in keinem Paket; AppImage-Mount-Pfad wechselt,
Windows ohne stdout) · **QTMUX-2** (Windows-`currentWorkingDirectory`-Funktionstest via PEB) ·
**QTMUX-13** (native macOS-Menü-Icons — Qt reicht `icon.source`/`icon.name` in nativen Menüs
nicht durch; einziger Weg wäre ein QMenuBar-Umbau, deferred; [[qtmux-native-menu-icons]]).

**Aus der Air-Evaluation (2026-07-28, air.dev):** QTMUX-**89** (Ruhezustand verhindern,
solange Agenten arbeiten) · **90** (Prompt-Queue je Session) · **91** (Agenten-Startprofile —
gehört mit QTMUX-85 zusammen) · **92** (Container-Backend Docker/Podman) · **93** (Spike ACP —
der strukturierte Gegenentwurf zur dokumentierten Schwäche „Worker meldet von sich aus
nichts", berührt 55/73/75/90). Aus der vollständigen Doku-Sichtung (jetbrains.com/help/air,
51 Seiten) zusätzlich **94** (Terminal-Ausgabe als Kontext an einen Agenten — Air holt sie
sich mühsam, bei uns liegt sie in `VtScreen`) · **95** (Auslöser: Zeitplan/Webhook am
vorhandenen MCP-HTTP-Server, lokal statt Cloud) · **96** (Agenten-Befehle aus
`.claude/commands` &amp; Co. in der Palette). Bereits abgedeckt und deshalb NICHT neu angelegt:
Worktrees (72), Diff/Review (73), Agentenfragen (75), Status/Fortschritt (55), Kanban (76),
Ports (69) — 69/72/76/92 haben stattdessen Ergänzungskommentare bekommen.
🔑 **Bewusst nicht übernommen:** alles Editor-artige (Symbols, Go-to-Definition, Datei-Baum,
projektweite Suche, Commit-Erzeugung, Diff-Kommentare) und die Cloud-Hälfte — QTmux ist ein
Terminal-Manager, kein IDE-Ersatz. Diese Linie beim nächsten Feature-Vergleich wiederverwenden.

**Backlog (nicht beauftragt):** SFTP-MCP-Tools (Companion-Prio 2) · Signierung/Notarisierung
(macOS Developer-ID, Windows Authenticode) · MacPCAN-Feinschliff (CAN-FD, ID-Filter,
Konfig-Dialog, DBC-Decoding) · CI-Action-Versionen anheben (Node-20-Deprecation ab Sept. 2026) ·
optional CPack-Distro-Pakete (.deb/.rpm) · **LGPL-Beilagen** fürs gebündelte Qt (Lizenztext +
Quellen-Hinweis) · Screenshot im README.

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
  `TerminalItem` leitet Rad/Klick/Drag bei aktivem Tracking an libvterm (X10/SGR-Sequenzen),
  sonst lokaler Scrollback/Selektion; **Shift+Drag** selektiert immer lokal. macOS:
  Cmd=ControlModifier, physisches Ctrl=Meta. libvterm **entprellt** (Tests brauchen
  press→release-Paare). Hover-only-Tracking (1003 ohne Taste) nicht gemeldet.
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
- `currentWorkingDirectory()`: macOS libproc, Linux `/proc`, Windows PEB (Test offen).

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
  🔑 **Vollständigkeit prüfen** (Audit 2026-07-29, Stand: lückenlos): Die **einzige** Quelle
  echter Nutzer-Einstellungen ist der `Settings`-Block in [qml/Main.qml](qml/Main.qml) (plus
  `ui/language`, `ui/themeMode`, `mcp/port` aus C++). Jeden Alias von dort gegen
  [qml/prefs/](qml/prefs/) greppen — was fehlt, ist entweder eine Lücke oder bewusst
  **Laufzeitzustand** (`newSessionType`, `collapsedGroups`; die gehören NICHT in den Dialog).
  Alles unter `windows/*` in [WindowModel.cpp](src/viewmodels/WindowModel.cpp) ist ebenfalls
  Zustand, keine Einstellung.
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
  🔑 **Kein `qsTr`-Plural in neuen Strings**, solange `FinishSourceLanguageTs.cmake` die
  `numerusform` der **Quellsprache** leer lässt (bestehendes `%n Einträge` steht deshalb bis
  heute auf `unfinished`). Entweder die Zahl erst ab 2 anzeigen und eine feste Form nehmen,
  oder die deutschen Pluralformen von Hand pflegen.
- **Arbeitsverzeichnis (QTMUX-103):** `windowWorkingDir(w)` liefert das CWD des **aktiven**
  Panes, leer bei seriellen/Plugin-Sessions — daran hängen „Arbeitsverzeichnis öffnen" und
  „Pfad kopieren" ihr `enabled`. Geöffnet wird über `App.openLocalPath` (C++,
  `QUrl::fromLocalFile` + `QDesktopServices`) statt per `"file://" + pfad` in QML: nur so
  werden Leerzeichen kodiert und aus `C:\Pfad` ein gültiges `file:///C:/Pfad`.
  ⚠️ `Session::workingDirectory()` ist ein **Cache**, den `SessionModel` alle **1500 ms**
  auffrischt — direkt nach dem Start ist er noch leer. Wer ihn in einem Test ausliest, misst
  sonst „" und hält die Funktion für kaputt (genau so passiert).
- 🔑 `TerminalItem::setSession` ruft `recomputeGrid` **nur bei gültiger Größe** — ein
  ungelayoutetes Item resizte die geteilte Session sonst auf 1×1 und verwarf den Inhalt.
- **Backend-Ownership:** Backend gehört NUR dem `unique_ptr` (kein `setParent`);
  stateChanged-Handler nimmt den State aus dem **Signal-Argument** (feuert während der
  Backend-Zerstörung).

### QML-/Theming-Lektionen
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
- **MacPCAN** (`plugins/macpcan/`, nur APPLE): CAN-Bus als Terminal-Backend; Typen `pcan`
  (Hardware) + `pcan-mock` (Demo, ohne Hardware vorführbar — bewusst getrennt, kein
  stiller Fallback). Vendorte Qt-freie Schicht `vendor/` (Namespace `mac_pcan`);
  **PCBUSB nicht im Repo** — CMake findet es über `QTMUX_PCBUSB_DIR`, sonst wird das
  Plugin still übersprungen; dylib + Lizenz werden ins Bundle kopiert (rpath
  `@loader_path/../Frameworks`). Terminal-UX: candump-Zeilen, `<hexid> b0 b1 …` sendet,
  Befehle `baud <rate>`/`help`/`clear`/`quit`.
  - ⚠️ Nur **ein** Handle pro PCAN-Kanal (eine restaurierte Session blockiert den Kanal);
    PCBUSB meldet einen Kanal **ohne** Hardware optimistisch als „verbunden" (RX leer).
  - v1-offen: CAN-FD, ID-Filter, Konfig-Dialog, DBC.

### Shells (Windows)
- `ShellRegistry`: cmd/PowerShell/pwsh + **„Eingabeaufforderung (Clink)"** wenn Clink
  installiert (GPL — bewusst nicht gebündelt, nur erkannt; `program` = komplette
  Kommandozeile, `PtyBackend` zerlegt via `splitCommand`). AutoRun-Dedup: ist Clink per
  cmd-AutoRun aktiv, wird der redundante Eintrag ausgeblendet.

### MCP-Server (37 Tools)
`src/server/McpServer.{h,cpp}`, HTTP/JSON-RPC auf `127.0.0.1:7345`; Tool-Referenz in
`docs/MCP.md`. Kernpunkte:
- **Controller-Auto-Erkennung** beim `initialize`: TCP-Port → PID → **Prozess-Vorfahren-
  kette** bis zur Session-Shell-PID (macOS gibt Environments fremder Prozesse nicht mehr
  heraus — daher Hierarchie statt `QTMUX_SESSION_ID`-Lesen); Fallback `attach_controller`.
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
- **Ohne Bedienungshilfen-Recht testen:** System Events/`osascript`/CGEvent scheitern hart
  (`-1719`, `AXIsProcessTrusted()`=false). Ein **Beenden** geht trotzdem echt:
  `NSRunningApplication(processIdentifier:)?.terminate()` — dasselbe Apple-Event wie Cmd+Q,
  aber **PID-genau** (`tell application` ginge über die Bundle-ID und träfe die produktive
  Instanz). Beweiskraft nur mit **Gegentest** (mit Rückfrage: Prozess lebt; ohne: er endet);
  Einstellungen dafür vorher per `defaults write` in die Profil-Domain
  (`com.qtmux.QTmux-<profil>`) — QSettings schreibt `/` als `.`.
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
  ⚠️ **Nie mit `-RedirectStandardError`/`-RedirectStandardOutput` starten** — das bricht die
  ConPTY-Anbindung der Kindshells, alle Sessions sterben und die leere Sidebar sieht wie ein
  Regressionsbug aus (2026-07-27 genau so fehlinterpretiert). Qt-Warnungen also anders holen.
  ⚠️ Synthetische **Mausrad**-Ereignisse (`mouse_event WHEEL`) nimmt Qt erst nach einer
  **echten Cursorbewegung** an (Hover-Enter) und nur im Vordergrund — sonst verpuffen sie
  spurlos und man hält ein nicht scrollendes Flickable für ein Layout-Problem.
- MCP-E2E ist der Standard-Verifikationsweg gegen die echte GUI (create_session/send_text/
  read_screen, `scrollback:true` für Historie) — gegen eine **isolierte Testinstanz**
  (s. Build-Abschnitt macOS), nie gegen eine, in der jemand arbeitet. Ergebnisse möglichst
  am **Zustand** messen statt am Screenshot (z. B. Palette-Befehl ausführen → `list_sessions`
  prüfen); rein visuelle Änderungen brauchen `--screenshot`/Screenshot + Anwender-Abnahme.
  ⚠️ **Den MCP-Port VOR dem Messen auf Eigentümerschaft prüfen** (`lsof -nP -iTCP:<port>
  -sTCP:LISTEN`): Läuft dort schon eine fremde Testinstanz (paralleler Worker!), bindet die
  eigene still **nicht** — jede Antwort kommt dann von der fremden Instanz und sieht völlig
  plausibel aus. Genau so ging 2026-07-28 ein kompletter Messdurchlauf an die falsche App.
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
