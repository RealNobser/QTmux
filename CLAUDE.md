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
| `src/server/McpServer.{h,cpp}` | Eingebetteter MCP-Server (36 Tools); Doku `docs/MCP.md` |
| `src/terminal/TerminalItem.{h,cpp}` / `GlyphAtlas.{h,cpp}` | Rendering (GPU-Atlas + Fallback), Selektion, Copy/Paste, Maus-Reporting |
| `qml/Main.qml` / `qml/SplitNode.qml` | App-Shell + rekursiver Split-Layout-Baum |
| `plugins/echo/`, `plugins/macpcan/` | Demo-Plugin (Kopiervorlage) + CAN-Bus-Plugin |
| `installer/build-{dmg.sh,msi.ps1,appimage.sh}` | Installer aller 3 Plattformen (hand-gerollt, bewusst kein CPack) |
| `tools/vsdev-build.cmd` | Windows-Build in der **VS-2022**-Umgebung (vswhere-begrenzt); von der VSCode-Task genutzt, s. Build-Abschnitt (QTMUX-79) |
| `shell-integration/qtmux.{bash,zsh,ps1}`, `qtmux-event.cmd`, `qtmux-emit.{sh,ps1,cmd}`, `qtmux-wait.{sh,ps1,cmd}` | OSC-133-Marker, `qtmux-notify`/`qtmux-event`, Hook-Helfer zum **Senden** (HTTP, QTMUX-30) und zum **Warten** (Hintergrund-Wächter, QTMUX-37) |
| `tests/` | 15 ctest-Tests: 14 QtTest-Binaries (pty, vtscreen, linkdetector, session, sessiongroups, agent, profiles, hotkeys, vault, sftp, plugins, agenteventhub, macpcan, keyencoding) + `test_doc_duplicates` (reines CMake-Skript) |

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
> „CMake: build"; `useVsDeveloperEnvironment: never`, `configureOnOpen: false`, Debug-Pfad
> in `launch.json` fest aufs `windows`-Preset. **Bauen/Konfigurieren also über F5 bzw.
> Strg+Umschalt+B**, nicht über die CMake-Statusleisten-Knöpfe. VS 2022 bleibt Standard
> (Qt ist `msvc2022_64`, CI und `build-msi.ps1` ebenso).
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

## Status (2026-07-28)

**Aktuell:** v1.6.1 ausgeliefert (alle 4 Installer: DMG/MSI+ZIP/AppImage). Phasen 0–6
komplett (Terminal-Kern, Sessions/Sidebar, Agent-Awareness, SSH/Seriell/SFTP, Plugins +
MacPCAN, Installer). CI grün auf macOS/Windows/Linux (Qt 6.10.3). **36 MCP-Tools**
(GUI-MCP-Parität für den geplanten AI-Companion). i18n finalisiert.

**Window-Modell (QTMUX-83, auf `main`, ungereleast):** Kein globales Split-Layout mehr,
sondern das tmux-Modell — Sidebar = **Windows** (Tabs), jedes Window hat sein eigenes
Split-Layout, Splits = Panes **im** Window, `focus_window` schaltet das ganze Layout um.
Gruppen sind seither **Window**-Gruppen. Details/Verifikation:
`docs/design/per-window-layouts/Umsetzung.md`; Mechanik unten in der Feature-Referenz.
Vorarbeit QTMUX-80/81/82, dabei **stiller Selbst-Screenshot** `--screenshot <png>`
(offscreen `grabWindow`, kein TCC) — der Standardweg für visuelle Abnahmen.

## Nächster Schritt (Wiedereinstieg nach /compact)

Stand **2026-07-28** · Branch `main`, letzter Commit `eef2f99`, **synchron mit origin** ·
Working Tree: nur diese CLAUDE.md-Aufräumung (uncommitted). Windows-Maschine: Debug- und
Release-Build frisch, `ctest -E "^test_pty$"` **14/14 grün** (test_pty fällt hier
umgebungsbedingt auch auf unverändertem Stand — nicht-interaktive Shell, ConPTY).

**Nächster Punkt (Kandidat, vom Owner NICHT beauftragt):** Meta-Kodierung `Alt+<Taste>` →
`ESC`+Taste, damit Claude Codes **Alt+V** (Bild aus der Zwischenablage) und die
readline-Kürzel Alt+B/F/D überhaupt beim Agenten ankommen.
- Einstieg: [src/core/KeyEncoding.cpp](src/core/KeyEncoding.cpp) `encodeKeyBytes` —
  Alt wird dort **nur** bei Enter behandelt, sonst greift `text()` (unter Windows bei
  Alt+Buchstabe leer → 0 Bytes gehen raus).
- Vorgehen: Alt + druckbares Zeichen → `\x1b` + Zeichen; **nur Windows/Linux** (`#ifdef`),
  macOS unverändert (Option erzeugt dort Sonderzeichen, physisches Ctrl ist schon Meta);
  Alt+Enter (QTMUX-43) und Ctrl-Steuercodes nicht anfassen; Fälle in
  [tests/tst_keyencoding.cpp](tests/tst_keyencoding.cpp) ergänzen.
- Bauen/Testen: `tools\vsdev-build.cmd windows all` dann
  `ctest --test-dir build\windows -E "^test_pty$"`; Release: `… windows-release`.
- Abnahme: Screenshot in die Zwischenablage, in einer Claude-Code-Session Alt+V drücken,
  per MCP `read_screen` prüfen, ob die `[Image #1]`-Markierung erscheint. Bilddaten laufen
  **nie** durchs PTY — Claude Code liest die Zwischenablage selbst.
- Beachten: neue Nummer vergeben (höchste belegte ist **QTMUX-83**), Jira dual anlegen
  (nur auf dem Mac möglich), i18n nicht betroffen.

**Danach:** (1) Jira-Nachträge QTMUX-46 + QTMUX-79 (Mac) · (2) `build/macos` aus dem finalen
`main`-Stand neu bauen (s. Arbeitsstand) · (3) offene Jira QTMUX-40/38/2/13 nach Priorität.

### Arbeitsstand (compact-fest — hier pflegen, nicht im Gespräch lassen)

- ⚠️ **`build/macos` trägt einen WIP-Zwischenstand** (früher `-B`-Fehler; Produktivinstanz
  PID 72801 läuft aus dem Memory-Image weiter). Verifizierter Endstand: `build/macos-test`.
  Vor dem nächsten Prod-Neustart `build/macos` neu bauen (`cmake --build build/macos`,
  NICHT `--preset` mit `-B`) — erst wenn die laufende Instanz beendet werden darf.
- **Jira-Nachtrag offen** (Windows-Maschine hat keine Credentials — `CLAUDE.local.md` und
  `Credential-*.txt` liegen nur auf dem Mac): **QTMUX-46** (Paritätslücken MCP/Palette/
  Einstellungen) und **QTMUX-79** (VSCode-Build auf VS 2022 festgenagelt) sind umgesetzt +
  gepusht, aber in keinem der beiden Jira angelegt.
- **Befund ohne Auftrag (2026-07-28): Alt+&lt;Taste&gt; wird gar nicht ans PTY gemeldet.**
  `encodeKeyBytes` behandelt Alt nur bei Enter; sonst greift `text()`, das unter Windows bei
  Alt+Buchstabe leer ist → 0 Bytes. Damit erreicht **Alt+V** (Claude Codes Bild-Einfügen
  unter Windows, weil Ctrl+V dort Text-Paste ist) den Agenten nie, und readline-Kürzel
  (Alt+B/F/D) fehlen ebenso. Alt+V ist **nicht** doppelt belegt: Mnemonics sind
  D/B/A/S/H, in der Hotkey-Registry existiert kein Alt-Kürzel. Fix wäre die
  xterm-Meta-Kodierung Alt+X → `ESC`+x, **nur Windows/Linux** (auf macOS erzeugt Option
  Sonderzeichen und physisches Ctrl ist schon Meta). Bilddaten laufen nie durchs PTY —
  Claude Code liest die Zwischenablage selbst, QTmux muss nur die Taste melden.

**Offene Jira:** **QTMUX-40** (OSC-8-Hyperlinks — deferred; die Heuristik-Links aus QTMUX-39
decken den Agenten-Fall ab, OSC-8 bräuchte Cursor-Span-Tracking + neues `Cell`-Feld, teuer da
`VtScreen` den Sichtbereich lazy aus libvterm bildet) · **QTMUX-38** (Shell-Helfer für
Installationsnutzer unerreichbar — nur im Repo, in keinem Paket; AppImage-Mount-Pfad wechselt,
Windows ohne stdout) · **QTMUX-2** (Windows-`currentWorkingDirectory`-Funktionstest via PEB) ·
**QTMUX-13** (native macOS-Menü-Icons — Qt reicht `icon.source`/`icon.name` in nativen Menüs
nicht durch; einziger Weg wäre ein QMenuBar-Umbau, deferred; [[qtmux-native-menu-icons]]).

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
- **Session-ID in der Kachel (QTMUX-44):** Jede Sidebar-Kachel zeigt neben dem Titel klein
  und monospaced `#<id>` — die **stabile** `Session::id()`, also genau die Nummer, mit der
  man die Session per MCP anspricht (`send_text`, `set_session_group` …). Model-Rolle
  `IdRole`/`"sessionId"`; im Delegate `required property int sessionId`. Bewusst NICHT der
  Zeilenindex (der wandert beim Umsortieren/Gruppieren).
- **Beenden mit Rückfrage (QTMUX-41):** Dialog listet die offenen Sitzungen auf, bevor
  alles geschlossen wird; abschaltbar (`window/confirmQuit`, Einstellungen → Fenster).
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
- **Agent-Awareness:** OSC 133 (Prompt-Marker → Activity-Ring), OSC 9/777 (Notify),
  OSC 9;4 (Progress-Balken), Bell → Attention-Pulse (blau); MCP-Controller-Tab rot.
  🔑 **Reduzierte Bewegung (QTMUX-47):** `App.reduceMotion` (AppController, beim Start
  ermittelt — macOS CoreFoundation `com.apple.universalaccess/reduceMotion`, Windows
  `SPI_GETCLIENTAREAANIMATION`, sonst false; Env-Override `QTMUX_REDUCE_MOTION` für Tests)
  schaltet die drei Sidebar-Puls-Animationen ab (`running: … && !App.reduceMotion`) → Ring
  in Akzentfarbe und Rahmen statisch statt pulsierend.
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

### MCP-Server (36 Tools)
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
