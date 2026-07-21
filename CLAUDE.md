# QTmux — Projektkontext für Claude

> Diese Datei wird zu Beginn jeder Session geladen. **Kompaktfassung** (Datei-Hygiene
> 2026-07-19): nur noch das für die Weiterentwicklung Nötige — Architektur, Build/CI,
> Konventionen, Lektionen/Fallen, Status. Die vollständige Historien-Fassung (1426 Zeilen,
> Stand `d5c96d1`) liegt als Backup in Confluence (DUAL: Seite **„CLAUDE.md-Archiv"** unter
> der Entwicklerdokumentation, on-prem <seiten-id> / Cloud <seiten-id>, je mit Datei-Anhang)
> und in der Git-Historie dieser Datei.

## Was ist QTmux?

Ein **plattformübergreifender Multi-KI-Agenten-Terminal-Manager** auf **Qt 6 / C++20**.
Inspiriert von [cmux](https://cmux.com/de) (Agenten-Handling, vertikale Tabs, Status-Ringe)
und [Tabby](https://tabby.sh/) (SSH/Serial/Telnet, Split-Panes, Plugins).
Zielplattformen: **macOS, Windows, Linux**. Prio 1: stabile Terminal-Integration.
Ursprungsplan: `~/.claude/plans/neue-projekt-idee-eine-qt-version-radiant-wind.md`.

## Architektur

**Entscheidungen:** Terminal-Kern = `libvterm` (vendored, BSD) + **eigener PTY-Layer**
(`forkpty` Unix / ConPTY Windows — kein `ptyqt`, das ist Qt6-inkompatibel) + eigenes
Rendering (Scene-Graph/GPU-Glyph-Atlas mit `QPainter`-Fallback `gpuRendering=false`).
UI: Qt Quick/QML. Lizenz: Open Source, Qt LGPLv3 dynamisch. Build: CMake + Presets,
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
| `src/core/Session.{h,cpp}` | Backend + VtScreen; Activity/Attention/Progress; Login-Script; SSH-Passwort-Auto-Fill |
| `src/viewmodels/SessionModel.{h,cpp}` | QAbstractListModel Sidebar; Persistenz/Restore; CWD-Vererbung |
| `src/viewmodels/Theme.{h,cpp}` | QML-Singleton `Theme.*`; leitet ALLE Chrome-Farben aus dem aktiven Color-Scheme ab |
| `src/viewmodels/AppController.{h,cpp}` | QML-Singleton `App.*`: Sprache, `shortcutText`, `keyChord`, Clipboard |
| `src/viewmodels/SftpClient.{h,cpp}` | SFTP-Browser (treibt System-`sftp` interaktiv im PTY) |
| `src/core/{AgentRegistry,ShellRegistry,ColorScheme,HotkeyRegistry,ConnectionProfile,SecretsVault,AgentEventHub,GlobalHotkey,ProcessInfo}.{h,cpp}` | Gui-freie Registries/Helfer (Details: Feature-Referenz) |
| `src/plugins/QTmuxPlugin.h` / `PluginHost.{h,cpp}` | Plugin-SDK (IID `com.qtmux.PluginInterface/1.0`) + Loader |
| `src/server/McpServer.{h,cpp}` | Eingebetteter MCP-Server (22 Tools); Doku `docs/MCP.md` |
| `src/terminal/TerminalItem.{h,cpp}` / `GlyphAtlas.{h,cpp}` | Rendering (GPU-Atlas + Fallback), Selektion, Copy/Paste, Maus-Reporting |
| `qml/Main.qml` / `qml/SplitNode.qml` | App-Shell + rekursiver Split-Layout-Baum |
| `plugins/echo/`, `plugins/macpcan/` | Demo-Plugin (Kopiervorlage) + CAN-Bus-Plugin |
| `installer/build-{dmg.sh,msi.ps1,appimage.sh}` | Installer aller 3 Plattformen (hand-gerollt, bewusst kein CPack) |
| `shell-integration/qtmux.{bash,zsh,ps1}`, `qtmux-event.cmd`, `qtmux-emit.{sh,ps1,cmd}` | OSC-133-Marker, `qtmux-notify`/`qtmux-event`, Hook-Helfer (HTTP; `.sh` = Unix, QTMUX-30) |
| `tests/` | QtTest: 11 Binaries (pty, vtscreen, session, agent, profiles, hotkeys, vault, sftp, plugins, agenteventhub, macpcan) |

## Build & Test (macOS)

Abhängigkeiten: `brew install qt ninja cmake` (libvterm vendored).

```bash
cmake --preset macos && cmake --build --preset macos
ctest --test-dir build/macos --output-on-failure
open ./build/macos/qtmux.app
QT_QPA_PLATFORM=offscreen ./build/macos/qtmux.app/Contents/MacOS/qtmux   # headless
```

> **⚠️ Läuft eine produktive Instanz aus `build/macos`, dort NICHT hineinbauen** — das
> Überschreiben des Binaries reißt den laufenden Prozess mit (und mit ihm alle
> Terminal-Sessions). Vorher `lsof -nP -iTCP:7345 -sTCP:LISTEN` bzw. `ps -o command`
> prüfen und in ein eigenes Verzeichnis bauen: `cmake --preset macos -B build/macos-test`.
> **Zweite Instanz zum Testen** (QTMUX-30 ff.): `QTMUX_PROFILE=test QTMUX_MCP_PORT=7346`
> — `QTMUX_MCP_PORT` wählt den MCP-Port (vor der Einstellung `mcp/port`, sonst 7345),
> `QTMUX_PROFILE` hängt einen Suffix an den App-Namen und trennt damit die ganze
> QSettings-Domain (sonst überschreibt die Testinstanz beim Beenden die gespeicherte
> Session-Liste der produktiven).

**DMG:** `installer/build-dmg.sh [version]` — baut `macos-release`, `macdeployqt -qmldir=qml`
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

`windeployqt` läuft als Post-Build. **MSI/ZIP:** `installer/build-msi.ps1 -Version <ver>`
(WiX v5 als dotnet-Tool; nutzt dasselbe `windows-release`-Preset → nur 2 Build-Dirs:
`build/windows` Debug, `build/windows-release` Release). Unsigniert (Early-Adopter).
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
- **PS-5.1-Umlaut-Mojibake** („für"→„fÃ¼r"): conhost/ConPTY interpretiert Kind-Ausgabe als
  CP1252 und re-kodiert nach UTF-8, BEVOR die Bytes QTmux erreichen (per rohen UTF-8-Bytes
  aufs OS-Handle bewiesen). **Keine Shell-Einstellung hilft** (alles getestet — chcp 65001,
  Console.OutputEncoding etc.). Echte Abhilfen: PowerShell 7 oder System-Option „UTF-8 für
  weltweite Sprachunterstützung". Bekannte kosmetische Einschränkung; kein QTmux-Bug —
  bewusst KEIN Dekodier-Hack im Datenstrom.
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

## CI (GitHub Actions)

`.github/workflows/ci.yml`: Build + headless-Tests (`QT_QPA_PLATFORM=offscreen`) auf
macOS/Windows/Linux; Qt via `jurplel/install-qt-action` (Module **qtserialport** +
**qtshadertools**); Windows-Tests informativ (`continue-on-error`, ConPTY-Konsolen-
Anbindung). Linux-Job baut zusätzlich das AppImage (Artefakt `QTmux-AppImage`).
Actions warnen über Node-20-Deprecation (ab Sept. 2026) — bei Gelegenheit anheben.

> **⚠️ `env.QT_VERSION` ist bewusst gewählt — nicht blind hochziehen** (2026-07-19 mühsam
> eingegrenzt): **Nicht 6.8.x** — dessen CMake-Config verlinkt das aus dem macOS-SDK
> entfernte **AGL-Framework** → `ld: framework 'AGL' not found` (fiel lokal nicht auf, da
> Homebrew-Qt 6.11 AGL-frei ist). Ausweichen auf `macos-13`-Runner funktioniert NICHT
> (werden nicht mehr zugeteilt → 24-h-Queue-Timeout). **Nicht 6.11.x** — Windows-Metadaten
> via aqtinstall nicht abrufbar (`Failed to locate XML data`; aqtinstall 3.3.0 ist bereits
> das neueste 3.x). **Aktuell 6.10.3** (überall auflösbar, AGL-frei, Lauf 2026-07-19 grün).
> Künftige Bumps vorher **lokal** prüfen: `pip install "aqtinstall==3.3.*"` →
> `aqt list-qt <windows|mac|linux> desktop --arch <ver>` — Fehler = Version unbrauchbar.

## Projekt-Doku: Confluence (DUAL: on-prem + Cloud)

Bei jeder Doku-Änderung **beide** aktualisieren (identischer Storage-Inhalt). Token nur
einlesen, **nie ausgeben/committen**; Credential-Dateien in der Repo-Wurzel, git-ignoriert.

1. **On-prem** — Space **QTMUX**, `https://confluence.intern.example`,
   `Credential-Confluence.txt` (**Bearer**, `verify_ssl=false` → `curl -k`):
   Home <seiten-id> · Benutzerdoku <seiten-id> · Entwicklerdoku <seiten-id> ·
   CLAUDE.md-Archiv <seiten-id>.
2. **Cloud** — `https://<cloud-instanz>.atlassian.net`, Space-Key **`<space-key>`**,
   `Credential-Atlassian.txt` (**Basic** `email:api_token`), Pfade unter
   `/wiki/rest/api/content/<id>`: QTmux <seiten-id> · Benutzerdoku <seiten-id> ·
   Entwicklerdoku <seiten-id> · CLAUDE.md-Archiv <seiten-id>.

**Update:** `GET …?expand=version` → `PUT` mit `version.number+1`, `body.storage` =
XHTML-Storage. **Neue Seite:** `POST` mit `space.key` + `ancestors:[{id:<parent>}]`.
Makro-Differenz: Mermaid heißt on-prem `mermaid-macro`, Cloud `mermaid-cloud`.
**⚠️ Entity-Falle:** Cloud-Storage kodiert Umlaute als HTML-Entities (`n&ouml;tig`),
on-prem als UTF-8 — String-Anker mit Umlauten auf der Cloud-Seite zusätzlich in der
Entity-Variante probieren; eingefügter UTF-8-Text wird von beiden akzeptiert.

**Firmen-Confluence (Firmen-Confluence, getrennt davon):** <space-key> auf `<firmen-confluence>`
(Hauptseite <seiten-id>, Anwender-Doku <seiten-id>, Entwickler-Doku <seiten-id>) — Windows-
Download-Kanal, nur aus dem Firmennetz erreichbar; Publish-Skript `dist/_qtmux_pub2.py`
(Creds `confluence.env`). **Steht auf 1.1.2-Ära** — Aktualisierung auf 1.3.x ist ein
Windows-/Firmen-Task.

## Jira (DUAL: on-prem + Cloud)

Beide Projekte Key **QTMUX**, identischer Issue-Satz (Abgleich per Summary), Typ **Task**.
- **On-prem** `https://jira.intern.example`, `Credential-Jira.txt` (**Bearer**-PAT,
  `verify_ssl=false`), API `/rest/api/2/`, description = Klartext.
- **Cloud** `https://<cloud-instanz>.atlassian.net` (Board <id>), `Credential-Atlassian.txt`
  (**Basic**), API `/rest/api/3/`, **description braucht ADF** (`{type:doc,version:1,…}`).
  Suche: on-prem `GET /search?jql=…`, Cloud `POST /search/jql`.

**Kanban-Konvention (Anwender-Vorgabe):** bei jedem Fortschritt **dual** weiterschieben —
Arbeitsbeginn → „In Progress" (on-prem Transition 31) / „In Arbeit" (Cloud 21); fertig +
verifiziert → „Done" (41) / „Erledigt" (41) mit Kurzkommentar. Transition-IDs je Issue via
`GET /issue/<key>/transitions` prüfen. Idempotent anlegen (Summaries vorher holen).
Kein Atlassian-MCP nutzen (nur Cloud, interaktives OAuth) — einheitlicher REST-Weg.

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
  `src/main.cpp`, `src/server/McpServer.cpp` (serverInfo), `installer/build-dmg.sh`,
  `installer/build-appimage.sh`, `.github/workflows/ci.yml` (AppImage-Schritt),
  `installer/QTmux.wxs`, `README.md` (DE **und** EN).
- **i18n:** Quellsprache Deutsch; QML `qsTr`, C++ `QCoreApplication::translate("<Kontext>",…)`.
  `cmake --build … --target update_translations` (lupdate scannt automatisch alle Targets
  inkl. `qtmux_core`); `cmake/FinishSourceLanguageTs.cmake` finalisiert die DE-Datei
  automatisch — **nur `i18n/qtmux_en.ts` braucht echte Übersetzungspflege**. Eigennamen
  (PowerShell, Bash, …) bleiben unübersetzt.
- README.md ist **zweisprachig** (DE/EN, Anker `#-deutsch`/`#-english`) — beide Hälften
  pflegen.

## Status (2026-07-21)

**v1.4.0.** Phasen 0–5 komplett (Terminal-Kern, Sessions/Sidebar, Agent-Awareness,
SSH/Seriell/SFTP, Plugin-System + MacPCAN); Phase 6: Installer aller 3 Plattformen fertig
(DMG/MSI+ZIP/AppImage), CI grün auf allen 3 Plattformen (Qt 6.10.3). 22 MCP-Tools
(GUI-MCP-Parität für den geplanten **AI-Companion**, wie RaftNG). i18n 208/208.

**QTMUX-30…34 erledigt (2026-07-21)** — Befunde aus dem ersten echten Mehragenten-Betrieb
(RAFTNG steuerte über QTmux zwei Worker; Fremdbericht `docs/mcp-controller-feedback-
2026-07-21.md`): `send_text`-Enter abgesetzt · Ereignis-Kanal ehrlich statt stumm ·
ID-Fehlermeldungen · `get_layout` mit Sitzungsübersicht · Doku-Wächter `test_doc_duplicates`.
MCP-Port jetzt konfigurierbar (`QTMUX_MCP_PORT`/`QTMUX_PROFILE`) — Voraussetzung, um die
MCP-Schicht zu testen, ohne die produktive Instanz anzufassen.

**Offene Jira:** **QTMUX-2** (Windows-Funktionstest `currentWorkingDirectory` via PEB —
braucht eine Windows-Session) · **QTMUX-13** (native macOS-Menü-Icons — Qt 6.11 reicht in
nativen Menüs weder `icon.source` noch `icon.name` durch, empirisch bewiesen; einziger Weg
wäre der Widgets/`QMenuBar`-Umbau, bewusst deferred, s. [[qtmux-native-menu-icons]]).

**Backlog (nicht beauftragt):** SFTP-MCP-Tools (Companion-Prio 2) · Signierung/
Notarisierung (macOS Developer-ID, Windows Authenticode) · MacPCAN-Feinschliff (CAN-FD,
ID-Filter, Konfig-Dialog statt `baud`-Befehl, DBC-Decoding) · CI-Action-Versionen anheben ·
optional CPack-Distro-Pakete (.deb/.rpm) · Firmen-Confluence-Confluence auf 1.3.x (Windows-Task).

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

### Terminal-Verhalten
- **Scrollback** (Cap 10000) in `VtScreen`; Selektion in **absoluten** Inhalts-Zeilen
  (scroll-fest); **Soft-Wrap-Copy** via `sb_pushline4`-Continuation-Flags (eine logische
  Zeile ohne `\n` am weichen Umbruch).
- **Maus-Reporting:** `VtScreen` trackt `VTERM_PROP_MOUSE` (DECSET 1000/1002/1003);
  `TerminalItem` leitet Rad/Klick/Drag bei aktivem Tracking an libvterm (X10/SGR-Sequenzen),
  sonst lokaler Scrollback/Selektion; **Shift+Drag** selektiert immer lokal. macOS:
  Cmd=ControlModifier, physisches Ctrl=Meta. libvterm **entprellt** (Tests brauchen
  press→release-Paare). Hover-only-Tracking (1003 ohne Taste) nicht gemeldet.
- **Tasten:** F1–F12 als xterm/VT220-Sequenzen in `encodeKey` (F-Tasten gehören der Shell —
  keine globalen F-Tasten-Shortcuts); Copy/Paste macOS Cmd+C/V, sonst Ctrl+Shift+C/V;
  Smart Ctrl+C (Auswahl→Copy, sonst SIGINT). Bracketed Paste + Multiline-Warnung;
  Copy-on-Select + Rechtsklick-Paste optional.

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
- **Agent-Awareness:** OSC 133 (Prompt-Marker → Activity-Ring), OSC 9/777 (Notify),
  OSC 9;4 (Progress-Balken), Bell → Attention-Pulse (blau); MCP-Controller-Tab rot.
- **AgentEventHub** (Gui-frei, Ringpuffer 256, monotone `seq`): Inter-Agenten-Ereignisse
  `done|question|error|info` via OSC `777;qtmux-event` oder MCP `post_event`; Zustellung
  über MCP-Long-Poll `wait_for_events`. **⚠️ Hook-stdout wird vom Agenten gekapselt** —
  aus KI-Hooks immer `post_event` (HTTP) statt OSC nutzen.
- **Split-Layout:** rekursiver JS-Baum (`window.layout`: Blatt `{paneId, sessionRow}` /
  Split `{orientation, children}`), QML-Rekursion via Loader — **`setSource(url,{props})`
  VOR dem Laden** (sonst evaluieren Bindungen mit `win===undefined` und brechen dauerhaft).
  `pruneLeaves(pred)` entfernt Blätter gelöschter Sessions (sonst teilen sich Panes eine
  Session und kämpfen um `resize()` → Verzerrung). Pane-Reorder: `DragHandler(target:null)`
  + manueller Szenen-Hit-Test (Qt-`Drag`/`DropArea` war fragil).
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

### MCP-Server (22 Tools)
`src/server/McpServer.{h,cpp}`, HTTP/JSON-RPC auf `127.0.0.1:7345`; Tool-Referenz in
`docs/MCP.md`. Kernpunkte:
- **Controller-Auto-Erkennung** beim `initialize`: TCP-Port → PID → **Prozess-Vorfahren-
  kette** bis zur Session-Shell-PID (macOS gibt Environments fremder Prozesse nicht mehr
  heraus — daher Hierarchie statt `QTMUX_SESSION_ID`-Lesen); Fallback `attach_controller`.
- **Long-Poll `wait_for_events`**: vor `callTool` abgezweigt, Socket bleibt offen
  (`PendingPoll` + QTimer, Default 25 s); `disconnected`-Handler räumt Polls ab.
- **QTMUX-29 (Layout/Profile):** Der Layout-Baum lebt in QML → Tools laufen über
  *Requested-Signale, deren QML-Handler **synchron** (Direct-Connection) ausgeführt werden
  und via **`provideResult`-Brücke** (`bridgedCall` in McpServer.h) antworten; ohne UI →
  „UI nicht verbunden". `list_profiles` ohne Geheimniswerte (nur Flags); `connect_profile`
  löst Vault-Passwörter **intern** (nie über MCP). `ConnectionProfileRegistry::indexOf`
  ist privat → Existenzprüfung via `profile(name).isEmpty()`.
- **QTMUX-31 (`send_text`):** Das Enter geht **zeitlich abgesetzt** raus
  (`Session::writeWithEnter`, Vorgabe 60 ms, Tool-Parameter `enterDelayMs`). TUI-Apps
  (belegt mit Claude Code) werten einen in EINEM Rutsch ankommenden Block als
  Einfügevorgang → das `\r` wurde zum Zeilenumbruch im Eingabefeld statt zum Absenden,
  und der Aufruf meldete trotzdem `ok`. Regressionstest bricht bei `enterDelayMs: 0`.
- **QTMUX-30 (Ereignis-Kanal):** Der Kanal ist korrekt — es fehlte die **Quelle**.
  QTmux leitet **nichts** aus Bildschirm/Prozesszustand ab (Claude-Code-Worker senden
  von sich aus nichts, auch keine Bell/OSC 9). Belegt: Worker beendet Aufgabe → Hub
  bleibt leer. Antwort darauf ist Ehrlichkeit statt erzwungener Ereignisse:
  `subscribe_events` meldet je Quelle `hasPostedEvents` + `sourcesWithEventsSoFar`,
  `wait_for_events` bricht ohne Abo **sofort** ab (statt 25 s Stille) und legt bei
  Leerlauf einen `hinweis` bei. Worker ereignisfähig machen: Stop-Hook auf
  `shell-integration/qtmux-emit.sh` (Unix) / `qtmux-emit.ps1` (Windows) — **als Skript,
  nicht als curl-Einzeiler**: die dreifache Escape-Verschachtelung im Hook scheitert
  still, und das sieht exakt aus wie „gerade passiert nichts".
- **QTMUX-33 (`get_layout`):** liefert `{layout, activePaneId, sessions}` — der Baum
  allein verschweigt, welche Sessions in **keinem** Pane liegen (laufen, aber unsichtbar).

## E2E-/Test-Fallen (alle Plattformen)

- **Nach einem Rebuild `open qtmux.app` NICHT auf eine laufende Instanz** — `open`
  aktiviert nur; das alte Binary antwortet dann (z. B. „Unbekanntes Tool"). Erst beenden,
  dann starten.
- macOS-GUI-E2E: CGEvent-Tool braucht Pause zwischen MouseDown/Up (sonst nur Hover);
  App-Sprache über das App-Menü umstellen, nicht `defaults write` (cfprefsd-Cache);
  Details [[qtmux-gui-test-macos]].
- Windows-E2E: Foreground nur zuverlässig mit `AttachThreadInput`; ein Alt-Stoß vor ESC
  schaltet den Qt-Menümodus (ESC schließt dann nur den). Menüs via UIA-`InvokePattern`
  öffnen. Synthetische Tasten erst nach Warteschleife aufs `MainWindowHandle`.
- MCP-E2E ist der Standard-Verifikationsweg gegen die echte GUI (create_session/
  send_text/read_screen; `read_screen scrollback:true` für Historie). Dafür eine
  **zweite Instanz** starten (`QTMUX_PROFILE`/`QTMUX_MCP_PORT`) — nie gegen eine
  Instanz testen, in der jemand arbeitet.
- **Doku-Wächter `test_doc_duplicates`** (QTMUX-34): findet doppelte Überschriften, wie
  sie beim Kompaktieren entstehen (Block eingefügt statt ersetzt → zwei gleichnamige
  Abschnitte mit widersprüchlichem Inhalt; in RAFTNG genau so passiert). Verglichen wird
  der Überschriften-**Pfad**, damit das zweisprachige README keinen Fehlalarm auslöst.
  `file(STRINGS)` braucht dort **`ENCODING UTF-8`** — sonst verschluckt CMake bei Zeilen
  mit Emoji den Zeilenanfang, die `##`-Marke geht verloren und der Pfad verrutscht.
- Claude-CLI-Fallen (Agenten-Demos): `--settings` braucht eine DATEI; `--allowedTools`
  ist variadisch → Prompt via stdin.
