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
> Seit 2026-11-19 steht Schicht-Wissen in Schicht-`CLAUDE.md`-Dateien und die Erzählung
> (Release-Verläufe, alter Wiedereinstieg) in [docs/HISTORY.md](docs/HISTORY.md); die Tabelle
> „Wo das Detail steht“ am Ende dieser Datei nennt jede Datei.

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
| `src/core/ScreenLinks.{h,cpp}` | Gui-frei: Zellen ↔ Link (Hyperlinks des Programms per OSC 8 zuerst, sonst logische Zeile über weiche Umbrüche, Emoji-feste Spalten; ein am Rand abgeschnittener Text-Link ist keiner) + Link-Gesten aller drei Plattformen |
| `src/core/TerminalSelection.{h,cpp}` | Gui-frei: Auswahl, die genau den markierten Text trägt oder verschwindet (stabile Zeilennummern + Inhaltsvergleich); wo beim Kopieren kein Zeilenumbruch steht, sagt `VtScreen::joinsPreviousLine` |
| `src/core/FileManagerReveal.{h,cpp}` | Gui-frei: „Im Finder/Explorer/Dateimanager zeigen" — Befehl je Plattform + lokale Existenzprüfung (QTMUX-138, Mechanik in der Feature-Referenz) |
| `src/core/ExecutableTarget.{h,cpp}` | Gui-frei: Würde das System die Datei beim Öffnen ausführen? Regel je Plattform (Endungen, Ausführungsrecht, Bündel, Datenströme) für die Rückfrage vor dem Klick auf einen Datei-Link (QTMUX-151); der Dialog ist `qml/dialogs/ExecOpenDialog.qml` |
| `src/core/OwnerDraft.{h,cpp}` | Gui-frei: Merker „der Mensch hat getippt und noch nicht abgeschickt“ als reine Zustandsfunktion (QTMUX-157) — solange er steht, wartet die Warteschlange und MCP `send_text`/`send_keys` lehnen ab. Eingänge: `Session::writeFromUser`/`pasteFromUser` |
| `src/core/AgentInbox.{h,cpp}` + `src/server/AgentInboxClient.{h,cpp}` | Nachrichten an einen Agenten OHNE Eingabezeile (QTMUX-157, MCP `send_message`/`set_agent_inbox`): Regeln + Rahmen Gui-frei im Kern, das Schreiben auf den Sitzungs-Socket von Claude Code (`QLocalSocket`) beim Server. ⚠️ Rahmenformat undokumentiert, belegt an 2.1.323 |
| `src/core/Session.{h,cpp}` | Backend + VtScreen; Activity/Attention/Progress; Login-Script; SSH-Passwort-Auto-Fill |
| `src/viewmodels/SessionModel.{h,cpp}` | QAbstractListModel Sidebar; Persistenz/Restore; CWD-Vererbung |
| `src/viewmodels/Theme.{h,cpp}` | QML-Singleton `Theme.*`; leitet ALLE Chrome-Farben aus dem aktiven Color-Scheme ab; Statusrollen `ok/warn/danger/muted` mit festem Hell-/Dunkel-Paar (QTMUX-142, Vertrag in `Theme.h`) |
| `src/viewmodels/AppController.{h,cpp}` | QML-Singleton `App.*`: Sprache, `shortcutText`, `keyChord`, Clipboard |
| `src/viewmodels/SftpClient.{h,cpp}` | SFTP-Browser (treibt System-`sftp` interaktiv im PTY) |
| `src/core/{AgentRegistry,ShellRegistry,ColorScheme,HotkeyRegistry,ConnectionProfile,SecretsVault,AgentEventHub,GlobalHotkey,ProcessInfo,KeyEncoding}.{h,cpp}` | Gui-freie Registries/Helfer (Details: Feature-Referenz) |
| `src/plugins/QTmuxPlugin.h` / `PluginHost.{h,cpp}` | Plugin-SDK (IID `com.qtmux.PluginInterface/1.0`) + Loader |
| `src/server/McpServer.{h,cpp}` + `McpAccess.{h,cpp}` | Eingebetteter MCP-Server (43 Tools, in der Fassung 2026-07-28 zusätzlich die Tasks `await_events`/`await_message`, also 45; Doku `docs/MCP.md`) + dessen Gui-freie **Zugriffsregeln** (Bind-Adresse, Token-Pflicht, QTMUX-127) |
| `third_party/updater/update/` + `src/viewmodels/UpdateBridge.{h,cpp}` + `qml/dialogs/UpdateDialog.qml` | Online-Update (QTMUX-125, seit 1.10.0 Hub-Ablauf `UpdateFlow`, QTMUX-160): vendierter Kern (Target `qtmux_updater`) + Verdrahtung/QML; Abgleich `tools/check-updater-sync.sh`, Herkunft `third_party/updater/UPSTREAM.md` |
| `src/terminal/TerminalItem.{h,cpp}` / `GlyphAtlas.{h,cpp}` | Rendering (GPU-Atlas + Fallback), Selektion, Copy/Paste, Maus-Reporting |
| `qml/Main.qml` / `qml/SplitNode.qml` | App-Shell + rekursiver Split-Layout-Baum |
| `plugins/echo/`, `plugins/macpcan/` | Demo-Plugin (Kopiervorlage) + CAN-Bus-Plugin |
| `installer/build-{dmg.sh,msi.ps1,appimage.sh}` | Installer aller 3 Plattformen (hand-gerollt, bewusst kein CPack) |
| `tools/vsdev-build.cmd` | Windows-Build in der **VS-2022**-Umgebung (vswhere-begrenzt); von der VSCode-Task genutzt, s. Build-Abschnitt (QTMUX-79) |
| `.qmllint.ini` + `.vscode/settings.json` (+ generierte `.qmlls.ini`) | Editor-Diagnosen für QML: abgeschaltete Kategorien mit Begründung, Ausschluss von `build/`, Importpfade für qmlls (s. QML-Lektionen) |
| `shell-integration/qtmux.{bash,zsh,ps1}`, `qtmux-event.cmd`, `qtmux-emit.{sh,ps1,cmd}`, `qtmux-wait.{sh,ps1,cmd}` | OSC-133-Marker, `qtmux-notify`/`qtmux-event`, Hook-Helfer zum **Senden** (HTTP, QTMUX-30) und zum **Warten** (Hintergrund-Wächter, QTMUX-37). Stecken seit QTMUX-38 als **Ressource im Binary** — `src/core/ShellIntegration.*` schreibt sie per `qtmux --install-shell-integration` heraus |
| `src/core/StallDetector.{h,cpp}` | Gui-frei: Stillstand-Erkennung (QTMUX-139) — Agent zeigt „arbeitet" (Merkmal `AgentInfo::workingMarkers`), Bildschirm steht seit N min still → `needsAttention`; Mechanik in der Feature-Referenz |
| `src/core/{GitInfo,ProjectCommands,PromptQueue}.{h,cpp}` | Gui-freie Kerne (QTMUX-58/96/90): Branch aus `.git/HEAD` ohne git-Prozess · Scanner für `.claude/commands`, `.claude/skills`, `.gemini/commands`, `.junie/commands`, `.agents/skills` (+ `filterForAgent`) · FIFO-Warteschlange + `mayDispatchNext`. Alle drei sind angebunden (Kachel, Palette, Session/MCP) |
| `tests/` | **73** ctest-Tests auf macOS (Stand 1.11.0). Die vollständige Liste der Test-Binaries und die Sonderfälle (`I18n.*`, `test_doc_duplicates`, `test_check_updater_sync`, `test_i18n`) stehen in [tests/CLAUDE.md](tests/CLAUDE.md). Zahl per `ctest -N` gegenprüfen, nicht schätzen |

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

**DMG:** `installer/build-dmg.sh [version]`, Release-Bau immer mit `FAMILY_CODESIGN=require`
und dem Qt der CI aus `QTMUX_QT_MAC` (seit 1.10.1, nie Homebrew).
Ablauf (macdeployqt, PCBUSB, Re-Signatur, Designated Requirement), die `@rpath`-Falle der
zweiten Qt-Kette und warum `QT_QPA_PLATFORM=offscreen` am Bundle scheitert:
[installer/CLAUDE.md](installer/CLAUDE.md), Abschnitt „DMG“.

## Build & Test (Linux)

Lokal wie macOS: `cmake --preset linux` + `--build` + `QT_QPA_PLATFORM=offscreen ctest`, sofern
Qt im System liegt. Der Linux-Bau-Server baut seit 2026-07-31 **im Docker-Container**
(#14s `buildenv.sh`, Qt im `qtcache`-Volume) statt auf dem Host — Aufruf, sudo-Bedarf und die
qtserialport/qtshadertools-Ergänzung stehen in `CLAUDE.local.md`.

**AppImage:** `installer/build-appimage.sh [version]`. Ablauf, CI-Fallen und der familienweite
Pin der linuxdeploy-Tools: [installer/CLAUDE.md](installer/CLAUDE.md), Abschnitt „AppImage“.

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
> 🔑 **Qt-Pfad:** Das `windows`-Preset nimmt `$env{QTMUX_QT_PREFIX}` und fällt sonst auf
> **beide** bekannten Installationen zurück — `C:/Qt/6.10.3/msvc2022_64;C:/Qt/6.11.1/msvc2022_64`.
> `CMAKE_PREFIX_PATH` ist eine **Liste**: CMake nimmt den ersten Treffer und überspringt
> nicht existierende Pfade. Damit stimmt derselbe Preset auf beiden Windows-Maschinen — ein
> **einzelner** Wert war zweimal falsch und wurde hin- und hergedreht (Entwicklungsrechner
> hatte nur 6.11.1, Windows-Bau-Rechner nur 6.10.3, `QTMUX_QT_PREFIX`
> nirgends gesetzt). **6.10.3 steht bewusst vorn**: das ist die Version der CI
> (`env.QT_VERSION`), des Windows-Bau-Rechners und damit der Installer — lokal gegen ein
> *neueres* Qt zu bauen ist die klassische Quelle für „geht hier, bricht in der CI".
> Seit 2026-07-30 ist 6.10.3 auch auf dem Entwicklungsrechner installiert (per `aqtinstall`,
> s. u.); wer gegen 6.11.1 bauen will, setzt `QTMUX_QT_PREFIX`.
>
> Zwei Visual Studios auf einer Maschine (QTMUX-79, `error STL1001`), `tools/vsdev-build.cmd`
> und `tools/cmake-vsdev.cmd`, warum ein bestehendes Build-Verzeichnis eine Preset-Änderung nie
> merkt, Qt nachinstallieren per `aqtinstall` und die Batch-Fallen der `.cmd`-Skripte
> (CRLF-Pflicht): [tools/CLAUDE.md](tools/CLAUDE.md), Abschnitt „Windows-Bauumgebung“.

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
  auf Version **und** ein neues Merkmal. 🔑 **Messwege je Paket** (2026-08-10 erprobt):
  das portable ZIP nutzt **Backslash-Separatoren** (PowerShell-Archiv) — macOS-`unzip`
  scheitert, Python `zipfile` liest es; das **MSI** auf dem Windows-Bau-Rechner per
  `msiexec /a <msi> /qn TARGETDIR=…` administrativ entpacken und im extrahierten
  `qtmux.exe` messen; ein **AppImage** nur auf Linux per `--appimage-extract`.
- **Fernsteuern per SSH** (Windows-OpenSSH beendet Kindprozesse, PowerShell-Fallen):
  [installer/CLAUDE.md](installer/CLAUDE.md), Abschnitt „Windows: Fernsteuern per SSH“.

Vor Windows-Releases: `tests/release-visual-check.ps1` (screenshottet alle Menüs in
beiden Themes + MCP-Smoke — Theming-Regressionen sind unit-test-unsichtbar).

**ConPTY-Lektionen (teuer erkauft):**
- Die übrigen ConPTY-Lektionen (GUI-Subsystem `WIN32_EXECUTABLE`, Waiter-Thread,
  terminate-Reihenfolge, PTY-Tests losgelöst über `tests/run_detached.ps1`, VS-Code-Debugger,
  PS-5.1-Umlaut-Mojibake, `QSGGeometryNode`-Assert) stehen in
  [src/core/CLAUDE.md](src/core/CLAUDE.md), Abschnitt „ConPTY-Lektionen“. Hier bleibt die
  Messung des Arbeitsverzeichnisses:
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
  in der Feature-Referenz. Am echten PowerShell 5.1 belegt: **ohne** die
  Shell-Integration bleibt die Anzeige nach `Set-Location` auf `C:\` stehen, **mit** ihr folgt
  sie (`…\tests` → `…\src\core` → `C:\Windows`). ⚠️ Voraussetzung ist also die gesourcte
  `qtmux.ps1` — ohne sie gilt der Absatz oben unverändert weiter.
  🔑 Testfalle dabei: `cd H:\…` **ohne `/d`** wechselt in `cmd` das Laufwerk nicht — der
  Prompt bleibt stehen, und QTmux meldet korrekt *keine* Änderung. Sah zunächst wie ein
  Fehler aus; der Bildschirminhalt hat es aufgeklärt.

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
> **Patches 2–5** (Emoji-Presentation, DECSET 1007, Reflow-Absicherung, Hyperlink-Kennung für
> OSC 8) mit Dateien, Tests und Mutationsproben: [src/core/CLAUDE.md](src/core/CLAUDE.md),
> Abschnitt „libvterm: Patches 2–5“. **Vor jedem libvterm-Update dort lesen.**

## CI (GitHub Actions)

`.github/workflows/ci.yml`: Build + headless-Tests (`QT_QPA_PLATFORM=offscreen`) auf
macOS/Windows/Linux; Qt via `jurplel/install-qt-action` (Module **qtserialport** +
**qtshadertools**). Linux-Job packt zusätzlich bei jedem Lauf das AppImage (Wächter für
`build-appimage.sh` + linuxdeploy-Pin) — **hochgeladen wird es dort nicht**.

Alles Weitere zur CI steht in [.github/CLAUDE.md](.github/CLAUDE.md): Runner-Pin
`ubuntu-24.04`, AppImage als Release-Asset, TMPDIR im macOS-Job, Flakes und ihr Beweis, Rerun
gegen Befund, blinde Negativprüfungen, Qt-Version der CI. **Bevor du einen roten Lauf deutest
oder `ci.yml` änderst, diese Datei lesen.**

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
  `.github/workflows/ci.yml` (AppImage-Schritt) · `README.md` (**9** Zeilen, DE **und** EN —
  am 2026-08-10 nachgezählt, Badge kam hinzu).
  🔑 `src/app/main.cpp`, `src/server/McpServer.cpp` und `src/viewmodels/UpdateBridge.cpp`
  stehen seit QTMUX-125 **nicht mehr** in der Liste: alle drei lesen `QTMUX_VERSION_STRING`
  aus dem generierten `qtmux_version.h` (`cmake/Version.h.in` ← `PROJECT_VERSION`). Die
  übrigen laufen ohne CMake-Konfiguration und bleiben darum manuell.
  Warum `installer/QTmux.wxs` keine Bump-Stelle ist, warum `-Version` in `build-msi.ps1` ein
  Pflichtparameter ist, und der Zeiger auf die Analyse
  [docs/versionsquellen.md](docs/versionsquellen.md): [installer/CLAUDE.md](installer/CLAUDE.md),
  Abschnitt „Versions-Bump: Einzelheiten“.
- **Vendoring-Wächter** [tools/check-updater-sync.sh](tools/check-updater-sync.sh): vier
  Kontrakte, Exit 0/1/2, Selbsttest `test_check_updater_sync`. Kontrakte, Include-Prüfung und
  der grüne Nachweis ohne Messung bis QTMUX-144 stehen in [tools/CLAUDE.md](tools/CLAUDE.md),
  Abschnitt „Vendoring-Wächter“. Hier bleibt der Wirkbereich:
  🔑 **Wirkbereich (Owner-Entscheid „lokal, aber laut", 2026-08-12): Der Wächter ist ein
  LOKALES Werkzeug und hängt bewusst NICHT in der CI.** QTmux ist öffentlich und baut auf
  github-hosted Runnern; der Hub ist privat — ein CI-Checkout bräuchte ein Token für ein
  privates Repo im Secret-Store eines öffentlichen (Owner hat abgelehnt). Ein CI-Schritt,
  der mangels Hub **immer** übersprungen würde, wäre selbst ein grünes Versprechen ohne
  Deckung (Fehlerklasse „grüner Nachweis, der nichts nachweist", Deskstarter 2026-08-12).
  Stattdessen: (1) Ohne Hub am Standardort `../MacPCAN` meldet das Skript **unübersehbar
  „NICHTS GEPRUEFT"** für alle vier Kontrakte (Exit bleibt 0, damit Werkzeugketten es nicht
  abschalten);
  (2) `installer/build-dmg.sh` ruft ihn **vor** jedem Release-Build auf — die einzige
  Maschine, die DMGs baut, hat den Hub daneben, und Drift bricht dort ab. Auf den
  Windows-/Linux-Paketwegen (Windows-Bau-Rechner, CI-AppImage) läuft er mangels Hub nicht — dort
  schützt nur das Nachziehen über den Hub-Workflow.
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
  Warum der MCP-Server frei von dieser Bauart ist, welche vier Stellen gehärtet sind, der
  Prüfer `tools/fuzz_mcp.py` und der fehlende jetsam-Eintrag:
  [src/server/CLAUDE.md](src/server/CLAUDE.md).
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
  🔑 **Anrede: Sie** (Owner-Entscheid 2026-10-13, familienweit, QTMUX-145) — Wächter
  `I18n.GermanAddressesWithSie` prüft Katalog UND Quelltext-Literale. Weil Deutsch die
  Quellsprache ist, ändert jede Umformulierung den Schlüssel: `<source>` in **beiden** `.ts`
  mit umbenennen, sonst verliert lupdate die englische Übersetzung (Feature-Referenz).
  Warum `SOURCE_TARGETS` Pflicht ist (ein Test als Übersetzungsquelle, die Reihenfolge der
  Translator in `main.cpp`, die Kontrollgröße): [i18n/CLAUDE.md](i18n/CLAUDE.md).
- README.md ist **zweisprachig** (DE/EN, Anker `#-deutsch`/`#-english`) — beide Hälften
  pflegen.

## Reifegrad (dauerhaft — der Zahlenstand steht NUR im „Arbeitsstand")

**Phasen 0–6 komplett** (Terminal-Kern, Sessions/Sidebar, Agent-Awareness, SSH/Seriell/SFTP,
Plugins + MacPCAN, Installer), dazu **Online-Update** (QTMUX-125) und die **GUI-Auffrischung
Design 1a/2a** (einklappbare Seitenleiste + Flyout, Statusleiste, sechs Menüs, neugestaltetes
Einstellungsfenster, Reset/Import/Export — Details im Abschnitt „Design 1a/2a" unten).
**43 MCP-Tools, 45 mit Tasks** (GUI-MCP-Parität für den geplanten AI-Companion), i18n finalisiert,
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

## Arbeitsstand & Wiedereinstieg (2026-11-28)

> Die EINE Stelle für den aktuellen Stand (Pflegeregeln 2–4 oben). Verlauf steht in
> Git/Jira/Confluence; Feature-Mechanik in der Feature-Referenz; Abnahme-Rezepte in
> [docs/owner-abnahmen.md](docs/owner-abnahmen.md).

**Ausgeliefert: v1.11.0** (2026-11-28, QTMUX-162): Der MCP-Server spricht zusätzlich die
Fassung 2026-07-28 mit Tasks (`await_events`, `await_message`; dazu `message_status` auf beiden
Wegen), MCP-Anfragen nur noch per POST auf `/mcp` und `/`. Davor v1.10.1 (Hub 0.8.1, macOS ab
13 mit dem Qt der CI, QTMUX-161).
Artefakte mit Größen und SHA256, CI-Läufe, Abnahme am Live-Manifest und die Release-Kette bis
v1.9.0 stehen in [docs/HISTORY.md](docs/HISTORY.md). Den Stand nicht von hier ablesen, sondern
messen (die Windows-Session pusht ebenfalls):

```bash
git fetch && git log --oneline -3 origin/main
git log --oneline origin/main..HEAD                   # Ungepushtes, muss leer sein
git describe --tags --abbrev=0 origin/main            # letzter Release-Tag
gh release view -R RealNobser/QTmux --json tagName,isDraft,publishedAt
gh run list -R RealNobser/QTmux --limit 3             # danach je Job lesen
ctest --test-dir <build> -N | tail -1                 # Testzahl
```

Jira: die höchste Nummer in **beiden** Systemen live holen (Abschnitt „Jira-Statuskonvention“
unten); der zuletzt aufgeschriebene Zählstand steht in docs/HISTORY.md.

**Teststände (1.11.0):** **73** auf macOS, CI-Linux **67**, CI-Windows **68** (ohne `test_pty`;
welche fehlen: [tests/CLAUDE.md](tests/CLAUDE.md)); im Linux-Container und auf Windows lokal
mit 1.11.0 nicht gemessen. Der
Verlauf der Zahlen und der Grün-Messungen steht in [docs/HISTORY.md](docs/HISTORY.md),
Fallzahlen einzelner Programme ebenso. Je Plattform: macOS
lässt `test_pty` mitlaufen; Linux (Container des Bau-Servers) und Windows nehmen ihn per `-E` aus
(umgebungsbedingt: nicht-interaktive Shell/ConPTY; unter Windows braucht `ctest` zusätzlich
Qt-`bin` im PATH, sonst `0xc0000135`) — dort eine um 1 kleinere Zahl erwarten. Aktuellste
Messungen: docs/HISTORY.md. Ein Befund aus dem Lauf zu 1.9.7:
⚠️ im main-Lauf `37685160935` desselben Commits fiel Windows-`test_session` einmal (19,68 s
statt ~8–13 s, Rerun grün, Bau-Rechner 5/5 grün) — Warteschleifen-Klasse unter Runner-Last,
bei Wiederkehr die Wartestelle in `tst_session` suchen; davor main-Lauf `37670220323`
(QTMUX-139) und Tag-Lauf `36775188600` (1.9.6, 33/32/33); lokal
nach QTMUX-139 34/34 in `build/macos-test` (Debug) **und** `build/macos-release` (2026-10-07).
🔑 Der **CI**-Linux-Job ist nicht der Container des Bau-Servers: dort läuft `test_pty` mit und
besteht. Eine kleinere Zahl aus dem Container ist kein Widerspruch, sondern die
Ausnahme per `-E`. **Zahl immer per `ctest -N` gegenprüfen, nie schätzen.**
Warum „CI grün auf allen drei Plattformen“ kein Vollständigkeitsbeleg ist (Release-Bau sieht
keine Debug-only-Asserts): [tests/CLAUDE.md](tests/CLAUDE.md).

### Release-Lehren (wörtlich aus den Release-Absätzen)

Weitere Release-Lehren stehen in [installer/CLAUDE.md](installer/CLAUDE.md), Abschnitt
„Release-Lehren“; die Erzählung der Releases (Artefakte, Läufe, Abnahmen) in
[docs/HISTORY.md](docs/HISTORY.md).

🔑 **AppImage lokal messbar** ohne Linux-Rechner: `unsquashfs -o <ELF-Ende>` (Offset =
`e_shoff + e_shentsize·e_shnum`, hier 944 632) — die ERSTE `hsqs`-Fundstelle liegt in der
Runtime und ist falsch („Can't find a valid SQUASHFS superblock").

Davor: **v1.9.7** — Stillstand erkennen + feste macOS-Signatur QTMUX-139/140 (2026-10-07,
`961d262`, Build-ID `1.9.7+961d262`; Artefakttabelle und Belege in der Git-Historie dieser
Datei). Dauerhaft daraus: 🔑 **Agentenerkennung braucht GETIPPTE Eingabe**
(`Session::observeInput`, erstes Wort) — `send_text` (Einfügung) und `cd …; ./claude` erkennt
sie nicht. ⚠️ Die DMG-Größe verschiebt sich auch **zwischen Releases ohne Inhaltsänderung**
(Werkzeug/OS, ~1,5 MB) — nur der Inhaltsvergleich trägt. ⚠️ **Windows-Bau-Rechner nur per
`ssh -4`** (Einzelheiten in `CLAUDE.local.md`); `build_msi.cmd` ruft `-File` auf, für
`$ProgressPreference` die Wrapper-Schritte in einer gehaltenen `-EncodedCommand`-Session
fahren (1.9.8: Bau inkl. Riegel ~3 min, Vorlage `git pull --ff-only` + HEAD-Prüfung +
`& .\installer\build-msi.ps1` im selben Lauf).

Davor: **v1.9.3** — Wartungsrelease der Familien-Runde nach MacPCAN 0.5.0 (2026-09-04,
`d875816`, Build-ID `1.9.3+d875816`; CAN-Vendor auf Hub-Stand MAC-44, Plugin-Target im
lupdate-Scan, AppImage-Upload nur auf Tag-Läufen). ⚠️ Beim 1.9.3-Fernbau brach der erste
`build-msi.ps1`-Lauf mit „Build fehlgeschlagen" **ohne Compiler-Ausgabe** ab (Configure-
Ausgabe per `>nul 2>&1` verschluckt), ein identischer zweiter lief sauber — bei
Wiederholung Configure separat sichtbar fahren (`diag_configure.cmd` beim Wrapper, Ort in
`CLAUDE.local.md`); bei 1.9.4 trat es nicht auf.

### Nächster Schritt (Wiedereinstieg nach /compact)

Zustand statt Kette. Die Absätze, aus denen diese Liste entstand, stehen wörtlich in
[docs/HISTORY.md](docs/HISTORY.md), Abschnitt „Wiedereinstieg vor der Diät“.

**Rolle:** Standby-Worker des Orchestrators. Rückmeldungen als
`MELDUNG QTMUX [FERTIG|FRAGE|ABBRUCH]: …` auf dem Weg, den der Auftrag nennt. Arbeit kommt per
Owner-Zuruf (Individual-Entwicklung).

**Offen je Ticket.** Alle sind umgesetzt und selbst verifiziert; die Mechanik steht in der
Feature-Referenz, die Rezepte in [docs/owner-abnahmen.md](docs/owner-abnahmen.md).
- **QTMUX-160** (Vertrag V2, 1.10.0): Selbst-Ersetzen wirkt erst ab dem Update danach, am echten
  Rechner nicht gesehen. **QTMUX-161**: Szene-15-Skips mit Hub 0.8.1 zurückgenommen.
- **QTMUX-40, 154, 155** (OSC 8, umbrochene Links, Kopieren unter ConPTY; 1.9.12): Owner-Abnahme.
  Weiter offen: Die Kopier-Regel ist einzeln nicht entschieden (ausgeliefert ist „so lassen“);
  ein neueres ConPTY mitliefern, das die Ausgabe durchreicht (nicht gemessen). Am echten Fenster
  ist nichts davon gesehen.
- **QTMUX-157** (Vorrang der Eingabe, `send_message`; 1.9.14): Owner-Abnahme am echten Fenster
  (QTmux kann keine Tasten des Menschen nachstellen); laut Repo offen ist auch der
  SessionStart-Hook `qtmux-inbox.sh` in den Einstellungen des Owners. Das Rahmenformat des
  Sitzungs-Sockets ist undokumentiert und an Claude Code 2.1.323 belegt: nach einem
  Claude-Code-Update die Probe (c) aus der Abnahme fahren. Folgeticket **QTMUX-158** (Windows über
  Named Pipe, Zustellbeleg, Empfänger mit freien Rechten).
- **QTMUX-151 §8.4** (Rückfrage vor ausführbaren Dateien; 1.9.13): Der Dialog ist am echten
  Fenster nie gesehen. Die Umkehrung (Erlaubnisliste) ist **QTMUX-156**, nicht umgesetzt.
- **QTMUX-150…152** (Auswahl, Links, Netzpfade; 1.9.11): Das zeitweise Versagen von Strg-Klick
  ist unter Windows nicht am echten Desktop reproduziert. Nebenbefund: Die Scrollback-Suche
  (`absLineText`) setzt Treffer hinter Emoji daneben, `m_matches` rutschen an der
  Scrollback-Grenze.
- **QTMUX-153** (Artefakte bei Windows + Clink + Resize): Ursache ist Claude Codes klassischer
  Renderer. Empfehlung: `/tui fullscreen` beim Owner messen, mit neuem Verlauf.
- **QTMUX-149** (ConPTY-Reflow; 1.9.10): Owner-Test unter Windows. Symptom „Auto-Copy“: Einzeltest
  beim Owner mit `set CLAUDE_CODE_NO_FLICKER=1`, dann `claude`.
- **QTMUX-147**: Ob Windows eine offen gehaltene Datei ersetzt oder ablehnt, steht nicht im
  CI-Log; gemessen ist nur „keine Hänger, kein Rest“.
- **Kandidaten, nicht beauftragt:** `AppComboBox` ohne eigenen Disabled-Zustand (QTMUX-145) ·
  harter CRT-Riegel in `installer/build-msi.ps1` nach RAFTNG-Vorlage RAF-181 · `timeout-minutes`
  für die CI-Jobs ([.github/CLAUDE.md](.github/CLAUDE.md)).

Offen aus 1.9.7: ob die TCC-Ordnerdialoge nach dem Owner-Update wirklich nur noch EINMAL
kamen, ist erst am Owner-Update 1.9.7 → 1.9.8 am lebenden Objekt belegbar (die DR ist in
beiden Releases identisch gemessen; kommen die Dialoge doch, ist das ein Befund).
Offen aus 1.9.6: QTMUX-138 unter Windows/Linux — der echte Explorer-/D-Bus-Aufruf ist nur per
Befehlstest + CI belegt, nicht am lebenden Objekt (Rezept `docs/owner-abnahmen.md`); das
Windows-Icon im Startmenü/Explorer nach echter MSI-Installation ist nur am Paketinhalt
gemessen, nicht angesehen.

🔑 **Der eigene Commit-Hash steht hier bewusst NICHT** — ein `--amend` ändert ihn, und der
Anker wäre im selben Moment falsch (2026-08-07 genau so passiert). Beim Wiedereinstieg
zusätzlich `git log --oneline -3` gegenlesen — die Windows-Session pusht ebenfalls.

🚢 **Publish-Mechanik fürs nächste Release:** DMG **immer** mit
`FAMILY_CODESIGN=require installer/build-dmg.sh <v>` bauen (fester Schritt — ohne `require`
fällt das Skript bei fehlendem Schlüsselbund nur LAUT auf ad-hoc zurück, und ein ad-hoc-Release
würfe beim Owner alle TCC-Ordnererlaubnisse wieder weg) und am gemounteten DMG
`codesign -d -r- QTmux.app` → `certificate root = H"015c9a18…"` (kein `cdhash`) belegen,
dazu die Zählung aller Mach-O-Objekte (`Authority=` + DR je Objekt; 1.9.7: 168/168). Der
Einmal-Dialog-Wechsel ist mit 1.9.7 verbraucht; ab 1.9.8 dürfen die TCC-Dialoge **nicht**
mehr kommen — kommen sie doch, ist die DR gewandert (Befund) · MSI und ZIP seit 1.10.0 aus einem
**frischen Klon** mit Wrapper-Kopie (der alte Klon lief zuletzt für 1.9.10; Weg in
`CLAUDE.local.md`); `build_msi.cmd` **verlangt die Version** (sonst `VERSION_ARG_FEHLT`) · das **AppImage**
kommt seit QTMUX-136 aus dem **Draft-Release** des Tags, nicht mehr aus einem
Actions-Artefakt: nach grünem Tag-Lauf (Job `linux-release`)
`gh release download v<v> -R RealNobser/QTmux -p 'QTmux-*-x86_64.AppImage' -D dist/`
— `gh release download` sieht Drafts; sha256 gegen die `sha256sum`-Zeile im Log des
Jobs und gegen GitHubs `digest` halten, Build-ID am extrahierten Image messen; dieser
**absolute** Pfad geht an `--artifact linux-x86_64=` · den **Upload**
fährt diese Session **selbst** mit dem Hub-Werkzeug `tools/updates/publish.py`, und zwar **exakt
in der Befehlsform aus `CLAUDE.local.md`**, sonst greift die Owner-Permission-Regel nicht
(Orchestrator-Vorgabe 2026-09-28) — **kein** `cd … &&` und **keine** Env-Zuweisung davor; blockt
der Harness doch, nicht umgehen, sondern dem Orchestrator den fertigen Befehl melden · Slot
strikt seriell, **`index.json` per Cache-Bust VOR dem Upload sichern** und
danach hart diffen · eigene Abnahme nie der `verify OK`-Zeile glauben (Rezept im
„Ausgeliefert"-Absatz) · **GitHub-Release fertigstellen:** das Draft des Tags existiert
schon (vom Tag-Job, trägt das AppImage) — **NICHT** `gh release create` (legte ein
zweites Release an), sondern `gh release upload v<v> <dmg> <msi> <zip>`, Titel/Text per
`gh release edit`, Assets je Rückladung + `cmp`, und erst **nach** Update-Server-Upload
und eigener Abnahme `gh release edit v<v> --draft=false` · Confluence-Benutzerdoku dual (Stand-Absatz; Cloud nur
per `curl`, urllib scheitert am Zertifikat).

**Nächster eigener Punkt (sobald kein Orchestrator-Auftrag anliegt): QTMUX-94** —
Terminal-Ausgabe als Agenten-Kontext.
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

**Danach:** Owner-Durchklick der fertigen Tickets
([docs/owner-abnahmen.md](docs/owner-abnahmen.md)) · Windows-/Linux-Zweig des
Update-Wegs am lebenden Objekt belegen · QTMUX-127-Rest (Prefs-Sichtprüfung, pf-Installation).
**Wartet auf Zulieferung:** nichts Externes.

### Offene Owner-Entscheide (blockieren nichts, aber warten)

1. **System-Modus für die Sprache von RAFTNG übernehmen?** RAFTNG bietet seinen
   `Mode::System` an (`QLocale::system()`, EN/DE/SV); QTmux kann nur fest Deutsch/Englisch.
   RAFTNG nennt es ausdrücklich eine **Produktentscheidung**, keine technische, und liefert
   auf Zuruf. Nicht eigenmächtig übernommen.
2. ✅ **Produktivinstanz ist auf 1.9.8** — der Owner hat über den eingebauten Updater
   gehoben (DMG geladen 2026-10-11 11:12, Binary-ctime 11:13; s. „Zustand" unten). Offen
   bleibt nur die Frage, ob dabei die TCC-Ordnerdialoge ausblieben — das weiß allein der
   Owner (Erwartung: ja, die DR ist in 1.9.7 und 1.9.8 identisch gemessen).

#### Zustand, der nicht aus Code/Git hervorgeht

- ⚠️ **Die Produktivinstanz läuft aus `/Applications/QTmux.app`** (Port 7345,
  **`1.9.8+58931b0`** — gemessen 2026-10-14 OHNE MCP-Zugriff: Binary per `strings`, und
  der laufende Prozess hat per `lsof -p <pid>` genau dessen Inode geladen, Prozessstart
  13.10. 10:33; der Anker „1.9.7" vom 10.10. war damit veraltet). Sie trägt
  die laufende Orchestrierung — nie hineinbauen/updaten, ohne dass der Owner die
  Terminal-Sessions opfern will.
  🔑 Vor jeder Diagnose an ihr die Build-ID gegen `git log` halten; PID über
  `lsof -nP -iTCP:7345 -sTCP:LISTEN` holen, **nie** eine notierte PID verwenden (die hier
  eingetragene war zweimal veraltet). ⚠️ `lsof` kann auf dieser Maschine minutenlang hängen
  (Time-Machine-SMB-Mount) — darum immer mit hartem Timeout
  (`perl -e 'alarm 30; exec @ARGV' lsof …`) und mit **Positivkontrolle**: die Zeile für 7345
  muss erscheinen, sonst ist ein leeres Ergebnis kein „Port frei".
  ⚠️ **`netstat -anv -p tcp` liefert seit Darwin 27 GAR NICHTS** (0 Zeilen bei Exit 0,
  2026-10-07 nachgemessen) — die frühere Empfehlung hier („liefert dieselbe Antwort sofort")
  machte jeden Port-frei-Check blind, ohne dass etwas rot wurde.


### Offene Fäden (dauerhaft relevant)

- ⚠️ **QTMUX-130 wirkt nicht rückwirkend** (Diagnose-Falle bei Owner-Abnahmen): Vorhandene
  Verlaufs-Dumps tragen die eingefrorenen 80er-Umbrüche als Inhalt — erst der **zweite**
  Neustart nach dem Update ist sauber; wer das übersieht, hält den Fix für wirkungslos.
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
  sie niemand. Nachholen: auf dem Windows-Bau-Rechner bzw. im Linux-Container eine Instanz mit älterer
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

- **`build/macos` ist wieder frei** — die Produktivinstanz läuft seit 2026-08-09 aus
  `/Applications/QTmux.app` (s. „Zustand"-Abschnitt oben; vor einem Neubau trotzdem den
  Port-Check machen, die Regel bleibt: nie in ein Verzeichnis bauen, aus dem eine Instanz
  läuft). Verifizierte Endstände sind `build/macos-test` und `build/macos-release`.
  🔑 **Dauerhafte Konsequenz gilt weiter:** „steht im Repo" ist **nie** gleich „ist in
  der App" — jede Owner-Abnahme braucht eine frische Testinstanz
  (`QTMUX_PROFILE=test QTMUX_MCP_PORT=7346`) oder die installierte App nach einem Update.
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
- **`build/macos-test` und `build/macos-release` waren am 2026-10-13 verschmutzt**
  (inkrementell gebaut stürzten `test_windowmodel`/`test_sessiongroups` ab, frisch gebaut grün):
  vor der nächsten Messung dort neu anlegen. Wortlaut und Verdacht: [docs/HISTORY.md](docs/HISTORY.md),
  Absatz zu QTMUX-145.

**Offene Jira (geführt wird in Jira, hier nur Zeiger):**
**156** (Datei-Links: Erlaubnisliste statt Verbotsliste für die Rückfrage vor dem Öffnen — Owner-Entscheid
2026-11-15: als Ticket anlegen, nicht umsetzen; offene Fragen stehen im Ticket) ·
**13** (native macOS-Menü-Icons — deferred; Qt reicht `icon.source`/`icon.name` in nativen
Menüs nicht durch, einziger Weg wäre ein QMenuBar-Umbau; [[qtmux-native-menu-icons]]) ·
**126** (Marken-Badge „Q" in Menüleiste + App-Icon; Farbe **`#B239EA`** (Violett), festgelegt 2026-08-05 nachdem `#0284C7` sich als die Farbe des **Deskstarters** herausstellte. 🔑 Eine weitere **Blaustufe war rechnerisch nicht möglich**: Der Bereich H 184–275 ist von MacPCAN, Deskstarter und AstroCAN dreifach belegt, jede Blaustufe scheitert am Abstand zu MacPCAN oder geht im **Dunkel**-Design unter. Belege — ΔE2000 und WCAG-Kontrast gegen **beide** Designs samt Kopfleiste — stehen in der Spec; Backlog; Spec
außerhalb des Repos, Pfad in `CLAUDE.local.md`).
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
(.deb/.rpm) · **LGPL-Beilagen** fürs gebündelte Qt (Lizenztext + Quellen-Hinweis) ·
**DMG-Bundle ausdünnen** (ungenutzte Plugins raus bzw. Homebrew-`LC_RPATH` löschen, beides
**vor** der Re-Signatur — Begründung und Messweg im DMG-Abschnitt oben).

### Design 1a/2a (GUI-Umbau) — abgeschlossen

Siebenstufiger Umbau (Original im Claude-Design-Projekt
`ab66e9b5-053b-4e81-9e4a-c45752fd42d1`, über `DesignSync` `get_file`); Stufen und Commits
stehen in der Git-Historie, die Mechanik in der Feature-Referenz.
Die dauerhaften Randbedingungen für QML-Arbeit (Qt Quick Controls **Basic**, Farben nur über
`Theme.*`, Wächter `test_statuscolors`, jede Zeichenkette in `qsTr`) und die bewussten
Abweichungen vom Design stehen in [qml/CLAUDE.md](qml/CLAUDE.md).

**Offen (bewusst nicht behauptet):** der **Owner-Durchklick** der GUI — nativen macOS-Menüs,
Flyout, Statusleiste, Einstellungsfenster, `App.reduceMotion` in beiden Designs, dazu
Export/Import durch die **echten** Dateidialoge (auf keiner Maschine automatisierbar).
Rezepte: [docs/owner-abnahmen.md](docs/owner-abnahmen.md).

### Zusammenarbeit Windows ↔ macOS

Zwei Claude-Sessions teilen die Arbeitskopie (Windows-Maschine + Mac). **Arbeitsteilung
(2026-07-30):** Windows schreibt Code und baut/testet **lokal auf dem Windows-Entwicklungsrechner**
(Debug **und** Release); der Mac verifiziert dieselbe Änderung auf den zwei
Plattformen, die Windows nicht sieht — macOS lokal, Linux im Container des Bau-Servers — macht die
GUI-Abnahmen und pflegt **Jira/Confluence** (geht nur dort, `CLAUDE.local.md` existiert nur auf
dem Mac). ⚠️ **Der Windows-Bau-Rechner ist NICHT die Testmaschine**, sondern der Release-Rechner für die
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
Confluence-Entwicklerdoku-Unterseiten: „GUI-Auffrischung Design 1a/2a",
„Agenten-Wiederherstellung", „MCP im Netzwerk: Bind-Adresse, Token, pf (QTMUX-127)"
und **„Online-Update (QTMUX-125)"** (Seiten-IDs in `CLAUDE.local.md`). Die
**Benutzerdoku** trägt seit 2026-08-03 zusätzlich den Abschnitt „Aktuell bleiben
(Online-Update)" — beide Systeme gepflegt.

### Owner-Abnahmen offen — **29 Tickets**, je umgesetzt + selbst verifiziert

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
- **Release:** Seit QTMUX-136 legt der **Tag-Lauf** das Release als **Draft** an und
  hängt das AppImage an (Job `linux-release`, Mechanik im CI-Abschnitt). Der Release-Weg
  holt es von dort (`gh release download v<ver> -p 'QTmux-*-x86_64.AppImage'`), hängt
  DMG + MSI + portables ZIP per `gh release upload` an **dasselbe** Draft und
  veröffentlicht erst nach Upload + Abnahme (`gh release edit v<ver> --draft=false`).
  ⚠️ **Nicht mehr `gh release create`** — das Draft existiert schon, ein zweites Release
  wäre die Folge. (Wer doch einmal ein Release ohne Tag-Job anlegt: `--target <voller
  SHA>`, ein **Kurz-SHA wird abgelehnt**, HTTP 422.) Bis 1.9.5 kam das AppImage als
  Actions-Artefakt (`gh run download <id> -n QTmux-AppImage`) — dieser Weg ist tot, der
  Upload-Schritt ist entfernt.
  Releases existieren ab **v1.4.0** (je 4 Assets); für 1.0.1/1.3.0 gibt es seit der
  Repo-Neuaufsetzung **weder** Releases **noch** lokale Kopien (dist/-Aufräumen 2026-08-10,
  Owner-Freigabe) — diese Binaries sind endgültig weg.
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
- **`git push` kennt kein `--ff-only`** (druckt nur die Hilfe, pusht nichts); ein normaler Push
  ohne Force verweigert Nicht-Fast-Forwards ohnehin. Herkunft: Absatz zu QTMUX-147 in
  [docs/HISTORY.md](docs/HISTORY.md).

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
| **[docs/update-dialog-spec.md](docs/update-dialog-spec.md)** | **Bevor du den Update-Dialog anfasst.** Er ist seit 2026-08-07 die **Vorlage für alle drei Desktop-Apps** (Owner); der geteilte Widgets-Dialog entsteht danach im Hub. Die Spec beschreibt Zustände, Layout, alle Texte in DE/EN, Verhalten und die Gründe. Wer den Dialog ändert, ändert die Vorlage — Spec mitziehen. |
| **[docs/e2e-fallen.md](docs/e2e-fallen.md)** | **Bevor du irgendetwas misst**, das über `ctest` hinausgeht — MCP-E2E, `--screenshot`, GUI-Automatisierung, Messung an einer laufenden Instanz. Enthält die Fälle, in denen das **Messmittel** log (falsch-positive Gegentests, Detektor-Blindheit, Screenshot-Eigenheiten je Plattform). |

⚠️ **Die Auslagerung ist keine Erlaubnis, sie zu überspringen.** Wer eine Änderung an einem
Feature plant oder eine Verifikation aufsetzt, ohne den zuständigen Abschnitt gelesen zu
haben, läuft mit hoher Wahrscheinlichkeit in eine dort bereits dokumentierte Falle — mehrere
davon erzeugen ein **grünes, aber wertloses** Ergebnis. Beide Dateien hängen im Doku-Wächter
`test_doc_duplicates`, werden also wie diese Datei auf Kompaktierungs-Duplikate geprüft.

## Wo das Detail steht (Schicht-Dateien)

Claude Code lädt eine Schicht-`CLAUDE.md` erst, wenn in ihrem Verzeichnis gearbeitet wird. **Wer
über ihr Thema entscheidet, ohne dort zu arbeiten** (CI-Lauf deuten, Release bauen, libvterm
aktualisieren), **liest die Datei vorher selbst.**

| Datei | Inhalt |
|---|---|
| [installer/CLAUDE.md](installer/CLAUDE.md) | DMG und Signatur · AppImage und linuxdeploy-Pin · Fernsteuern per SSH · Bump-Einzelheiten · Release-Lehren |
| [.github/CLAUDE.md](.github/CLAUDE.md) | CI: Runner, Release-Asset, Flakes, Rerun gegen Befund, Qt-Version |
| [src/core/CLAUDE.md](src/core/CLAUDE.md) | ConPTY-Lektionen · libvterm-Patches 2–5 |
| [src/server/CLAUDE.md](src/server/CLAUDE.md) | Fremde Pfade im MCP-Server, `tools/fuzz_mcp.py` |
| [tools/CLAUDE.md](tools/CLAUDE.md) | Windows-Bauumgebung · Vendoring-Wächter |
| [tests/CLAUDE.md](tests/CLAUDE.md) | Liste der Tests, Grenzen der CI als Beleg |
| [qml/CLAUDE.md](qml/CLAUDE.md) | Randbedingungen aus Design 1a/2a |
| [i18n/CLAUDE.md](i18n/CLAUDE.md) | `SOURCE_TARGETS` |
| [docs/HISTORY.md](docs/HISTORY.md) | Release-Kette, Testzahl-Verlauf, alter Wiedereinstieg |

🔴 **Keine `CLAUDE.md` in `third_party/`, `plugins/macpcan/`, `installer/smoke/` oder
`installer/macos/`**: vendiert bzw. von `tools/check-updater-sync.sh` gegen den Hub verglichen.
`shell-integration/` bleibt ebenfalls frei, sein Inhalt wird ins Binary eingebettet.
