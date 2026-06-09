# QTmux — Projektkontext für Claude

> Diese Datei wird zu Beginn jeder Session geladen. Sie fasst Ziel, Architektur,
> Build und Status zusammen, damit der Kontext einen Compact übersteht.

## Was ist QTmux?

Ein **plattformübergreifender Multi-KI-Agenten-Terminal-Manager** auf **Qt 6 / C++20**.
Inspiriert von [cmux](https://cmux.com/de) (Agenten-Handling, vertikale Tabs, Status-Ringe)
und [Tabby](https://tabby.sh/) (SSH/Serial/Telnet, Split-Panes, Plugins).
Zielplattformen: **macOS, Windows, Linux**. **Prio 1: stabile Terminal-Integration.**

Detaillierter Plan: `~/.claude/plans/neue-projekt-idee-eine-qt-version-radiant-wind.md`.

## Festgelegte Architektur-Entscheidungen

- **Terminal-Kern:** `libvterm` (VT-Parser, BSD) + **eigener PTY-Layer** + eigenes Rendering.
- **UI:** Qt Quick / QML (Qt 6), GPU-beschleunigt.
- **Lizenz:** Open Source, Qt unter LGPLv3 (dynamisch gelinkt) — Komponenten BSD/MIT.
- **Build/IDE:** VSCode + CMake + Presets überall; MSVC (Win), Clang (mac), GCC/Clang (Linux).

### Zwei bewusste Abweichungen vom Ursprungsplan
1. **Kein `ptyqt`.** Es nutzt `QProcess::setupChildProcess()`, das in **Qt6 entfernt** wurde →
   baut nicht mit Qt 6.11. Stattdessen **eigener, dependency-freier PTY-Layer**
   (`forkpty` auf Unix, ConPTY auf Windows). Erfüllt das Plan-Ziel robuster.
2. **Rendering via `QQuickPaintedItem`** (GPU-getextured, robust) statt direkt QSGRenderNode.
   GPU-Glyph-Atlas ist die spätere Performance-Stufe.

## Architektur-Schichten

```
QML UI (qml/Main.qml)  — Sidebar, vertikale Tabs, Status-Ringe
   │
TerminalItem (src/terminal/)  — QQuickItem: Rendering + Tastatur/Resize
   │
VtScreen (src/core/)  — libvterm-Wrapper: Screen-State, Farben, Scrollback, Cursor
   │
ITerminalBackend (src/core/)  — Abstraktion: alles, was Bytes liest/schreibt
   ├─ PtyBackend   (lokale Shell/Agenten)  [FERTIG]
   ├─ SshBackend   (libssh2)               [Phase 4]
   ├─ SerialBackend(QtSerialPort)          [Phase 4]
   └─ AppBackend   (Custom-Apps/MacPCAN)   [Phase 5]
   │
Pty (src/core/)  — Pty.h + UnixPty.cpp (forkpty) / WindowsPty.cpp (ConPTY)
```

**Kernidee:** Sidebar, Layout und Rendering funktionieren für *alle* Backend-Typen identisch,
weil alles über `ITerminalBackend` läuft.

## Wichtige Dateien

| Datei | Rolle |
|---|---|
| `src/core/ITerminalBackend.h` | Zentrale Backend-Abstraktion + `BackendState` (für Status-Ringe) |
| `src/core/Pty.h` | Plattformneutrale PTY-Schnittstelle |
| `src/core/UnixPty.cpp` | `forkpty`-Implementierung (macOS/Linux) — FERTIG |
| `src/core/WindowsPty.cpp` | ConPTY-Skeleton (Windows) — **noch zu vervollständigen** |
| `src/core/PtyBackend.{h,cpp}` | Lokale Shell/Agenten über PTY |
| `src/core/VtScreen.{h,cpp}` | libvterm-Wrapper; `Cell` = Qt-freundliche Zelle |
| `src/core/Session.{h,cpp}` | Bündelt Backend + VtScreen; UI spricht nur mit Session |
| `src/viewmodels/SessionModel.{h,cpp}` | QAbstractListModel für die Sidebar; `sessionAt()` bindet an `TerminalItem.session` |
| `src/viewmodels/Theme.{h,cpp}` | QML-Singleton `Theme.*`: Dark/Light-Palette, via QSettings persistiert |
| `src/viewmodels/AppController.{h,cpp}` | QML-Singleton `App.*`: UI-Sprache; Translator-Wechsel in `main.cpp` |
| `src/core/AgentRegistry.{h,cpp}` | Bekannte Agenten-CLIs (claude, codex, gemini, **agy**=AntiGravity, …); `detect()` |
| `i18n/qtmux_{de,en}.ts` | Übersetzungen; via `qt_add_translations` zu `:/i18n/*.qm` kompiliert/eingebettet. **Quellsprache Deutsch.** Strings in QML via `qsTr`; in C++ via `QCoreApplication::translate("<Kontext>", "…")` (Kontext `Shells` für „Eingabeaufforderung"→„Command Prompt"). `qt_add_translations` sammelt per `qt6_collect_translation_source_targets` **automatisch alle Targets im Verzeichnisbaum** — `qtmux_core` (z. B. `Session.cpp`/`ShellRegistry.cpp`) wird also **mit-gescannt**; `cmake --build … --target update_translations` (lupdate) extrahiert die C++-Strings korrekt (verifiziert: 46 Texte, 0 verloren). Eigennamen (PowerShell, Bash, …) bleiben bewusst unübersetzt. |
| `src/terminal/TerminalItem.{h,cpp}` | QML-`TerminalItem`; rendert `Session`, Maus-Selektion + Copy/Paste (`QClipboard`) |
| `qml/Main.qml` | App-Shell: Toolbar + datengetriebene Sidebar + Terminal der aktuellen Session |
| `resources/icons/*.svg` | Phosphor-Icons (eingebettet als `qrc:/icons/`), via `icon.source`/`icon.color` |
| `tests/` | QtTest: `tst_pty`, `tst_vtscreen`, `tst_session` (E2E) |

## Build & Test (macOS)

Abhängigkeiten: `brew install qt ninja cmake` (libvterm ist **mitgeliefert**, s.u.).

```bash
cmake --preset macos
cmake --build --preset macos
ctest --test-dir build/macos --output-on-failure
open ./build/macos/qtmux.app          # sichtbar starten
QT_QPA_PLATFORM=offscreen ./build/macos/qtmux.app/Contents/MacOS/qtmux   # headless smoketest
```

## Build & Test (Windows, MSVC) — verifiziert 2026-06-08

Voraussetzungen: VS 2022 (MSVC + CMake + Ninja), Qt 6.x `msvc2022_64` **inkl.
Add-on „Qt Serial Port"** (Online-Installer macht es opt-in → sonst CMake-Fehler
„Qt6SerialPort missing"; Nachinstallation: `C:\Qt\MaintenanceTool.exe … install
qt.qt6.<ver>.addons.qtserialport`). `CMakePresets.json` → Windows-`CMAKE_PREFIX_PATH`
ggf. an die eigene Qt-Version anpassen. **Kein vcpkg** (libvterm vendored, libssh2 ungenutzt).

In einer Developer-Shell (`vcvars64`, damit MSVC+Ninja im PATH):
```bat
cmake --preset windows
cmake --build --preset windows
ctest --test-dir build\windows --output-on-failure   :: Qt-bin muss im PATH sein
.\build\windows\qtmux.exe
```
`windeployqt` läuft als Post-Build-Schritt → Qt-DLLs/QML/Plugins liegen neben der exe.

**libvterm vendored:** liegt unter `third_party/libvterm/` (0.3.3, BSD, neovim-Mirror)
und wird als kleine C-Lib mitgebaut. libvterm wurde aus vcpkg entfernt; Vendoring hält
alle 3 Plattformen identisch & abhängigkeitsfrei. Wichtig: `project(... LANGUAGES C CXX)`
(ohne `C` ignoriert CMake die `.c`-Dateien → leere `vterm.lib` → Linkfehler).

**Windows-Lektionen (ConPTY, Prio 1 — teuer erkauft):**
- **qtmux MUSS `WIN32_EXECUTABLE` (GUI-Subsystem) sein.** Eine Konsolen-App (CUI) erbt/
  erhält eine Konsole, an die sich die per ConPTY gestarteten Kindshells hängen statt an
  die Pseudo-Konsole → Terminal bekommt **keine** Ein-/Ausgabe. Als GUI-App gibt es keine
  Konsole zum Erben → ConPTY greift. (Symptom war: Shell-Banner landet auf der Host-Konsole,
  `read_screen` leer.)
- **Prozessende-Erkennung:** Anders als Unix (EOF am Master-FD) schließt conhost die
  Ausgabe-Pipe NICHT, wenn sich die Shell selbst beendet (`exit`). `WindowsPty` hat dafür
  einen **Waiter-Thread** (`WaitForSingleObject` auf das Prozess-Handle) → löst
  `finished`/Closed aus. Ohne ihn blieb der Zustand „Running" + langer terminate-Hänger.
- **terminate-Reihenfolge:** `ClosePseudoConsole` **vor** `TerminateProcess` (+ `CancelSynchronousIo`).
- **PTY-Unit-Tests** (`test_pty`/`test_session`) sind auf Windows `WIN32_EXECUTABLE` UND
  werden via `tests/run_detached.ps1` losgelöst gestartet (sonst erben sie die ctest-Konsole).
- Verifikation lief über den **MCP-Server** (create_session/send_text/read_screen) gegen die
  echte GUI-App; sauberer Shutdown (Prozessbaum) bestätigt.
- Offen (kosmetisch): Umlaute der **PowerShell-5.1**-Ausgabe erscheinen als Mojibake
  („für"→„fÃ¼r"). Analyse: die empfangenen Bytes sind bereits **doppelt-kodiertes UTF-8**
  (`C3 83 C2 BC`) — eine Codepage-Verwechslung in PowerShell 5.1/conhost. `VtScreen`
  (libvterm `vterm_set_utf8(1)`, `QString::fromUcs4`) dekodiert das Empfangene korrekt; der
  Fehler entsteht VOR QTmux. Mögliche spätere Milderung: ConPTY/Shell auf UTF-8 zwingen
  (`chcp 65001` bzw. UTF-8-CP), oder PowerShell 7 nutzen. ASCII ist unbetroffen.
- Offen: `Pty::currentWorkingDirectory()` auf Windows noch leer (PEB-Abfrage zurückgestellt).

Konventionen: deutsche Kommentare/Kommunikation; `qtmux_core` bleibt **Gui-frei**
(nur Qt6::Core) → Farben als `quint32` 0xRRGGBB, nicht `QRgb`. Code-Referenzen als
Markdown-Links. Commit-Trailer: `Co-Authored-By: Claude …`.

## Projekt-Dokumentation (Confluence — DUAL: on-prem + Cloud)

Die Doku wird **parallel an zwei Stellen** gepflegt (bei jeder Doku-Änderung beide
aktualisieren; identischer Storage-Inhalt). Token nur einlesen, **nie ausgeben/committen**.
Beide Credential-Dateien liegen in der Repo-Wurzel und sind **git-ignoriert** (`Credential-*.txt`).

**1) On-prem Confluence Server/DC** — Space **QTMUX**, `https://confluence.intern.example`,
`Credential-Confluence.txt` (**Bearer**-Token, `verify_ssl=false` → `curl -k`):
- **QTmux Home** (id `<seiten-id>`) · **Benutzerdokumentation** (id `<seiten-id>`) ·
  **Entwicklerdokumentation** (id `<seiten-id>`).
- Muster: `curl -k -H "Authorization: Bearer <token>" $BASE/rest/api/content/<id>?expand=version`.

**2) Atlassian **Cloud** Confluence** — `https://<cloud-instanz>.atlassian.net`, Space-Key **`<space-key>`**
(Anzeigename „Entwicklung"), `Credential-Atlassian.txt` (**Basic**-Auth `email:api_token`,
`email=<e-mail>`, gültiges TLS):
- **QTmux** (Hauptseite, id `<seiten-id>`) ↔ on-prem Home · **Benutzerdokumentation** (id `<seiten-id>`) ·
  **Entwicklerdokumentation** (id `<seiten-id>`).
- Muster: `Authorization: Basic base64(email:token)`, Pfade unter `/wiki/rest/api/content/<id>`.

**Update (beide):** `GET …?expand=version` → `PUT /…/content/<id>` mit `version.number+1`,
`body.storage` = XHTML-Storage-Format. Neue Seite: `POST /…/content` mit `space.key` +
`ancestors:[{id:<parent>}]`. Storage ist Server↔Cloud weitgehend kompatibel (Vorsicht nur bei
Makros: Mermaid heißt on-prem `mermaid-macro`, Cloud `mermaid-cloud`). Das `children`-Makro
funktioniert auf beiden. Bei `/feierabend` beide Seiten-Sätze mitpflegen.

## Jira (DUAL: on-prem + Cloud)

Tickets werden **parallel in beiden Jira** gepflegt (Backlog/Status synchron halten). Beide
Projekte haben den Key **`QTMUX`** und denselben Issue-Satz (QTMUX-1…25, identische Summaries
→ Abgleich per Summary). Issue-Typ: **Task** (in beiden vorhanden). Erledigt: **QTMUX-1**
(ConPTY Windows verifiziert), **QTMUX-5** (Scrollback). **QTMUX-15…25**: aus dem Tabby-Feature-
Abgleich (Hotkeys, Bracketed/Copy-on-Select-Paste, Color-Schemes, Ligaturen, Quake-Modus,
Broadcast-Input, Secrets-Vault, Login-Scripts, Progress-Anzeige, Clink/cmd.exe).

- **On-prem Jira Server/DC** — `https://jira.intern.example`, `Credential-Jira.txt`
  (**Bearer**-PAT, `verify_ssl=false`), API `/rest/api/2/`. Projekt-ID 10201.
- **Atlassian Cloud Jira** — `https://<cloud-instanz>.atlassian.net`, Board `…/projects/QTMUX/boards/36`,
  `Credential-Atlassian.txt` (**Basic** `email:token`), API `/rest/api/3/`. **Achtung:** die
  `description` braucht hier **ADF** (`{type:doc,version:1,content:[…]}`), on-prem nimmt Klartext.

**Muster:** Anlegen `POST /rest/api/<2|3>/issue` mit `fields.project.key=QTMUX`,
`issuetype.name=Task`, `labels`. Suche/Abgleich: `GET /rest/api/2/search?jql=project=QTMUX`
(on-prem) bzw. Cloud `POST /rest/api/3/search/jql`. Idempotent: vor dem Anlegen vorhandene
Summaries holen und Duplikate überspringen. Token nur einlesen, nie ausgeben/committen.

**MCP:** Atlassian bietet einen Remote-MCP-Server, aber **nur für Cloud** und mit interaktivem
OAuth (headless unzuverlässig) — deckt die on-prem-Hälfte nicht ab. Für die Dual-Pflege ist der
**einheitliche REST-Weg** (oben) besser; kein Atlassian-MCP in der Session verbunden.

## Status (Stand: 2026-06-08)

### Windows-Test-Session 2026-06-08 (alles committet + gepusht, GitHub `RealNobser/QTmux`)
Erstmaliger Windows-Lauf erfolgreich; Build/Tests/GUI verifiziert (MSVC, Qt 6.11.1). Geliefert:
- **Build cross-platform reparabel**: libvterm vendored (`third_party/libvterm`, vcpkg raus),
  `LANGUAGES C CXX`, Qt-Prefix im Windows-Preset, Qt-**SerialPort**-Add-on nötig.
- **ConPTY verifiziert** (Prio 1): qtmux ist `WIN32_EXECUTABLE` (sonst erben Kindshells eine
  Konsole → Terminal stumm); Waiter-Thread für Prozessende-Erkennung; `windeployqt` Post-Build.
  Verifikation über MCP-Server + Screenshots (PrintWindow/CopyFromScreen).
- **Shell-Wahl** (cmd/PowerShell/pwsh, Windows): `ShellRegistry`, global („Datei→Standard-Shell")
  + pro Session im „+"-Menü, persistiert; MCP `create_session program`.
- **Auto-Fokus**: neue/aktive Session + Fenster-Aktivierung fokussieren das Pane (kein Klick nötig).
- **Lesbare/lokalisierte Titel**: `prettifyTitle` (Session.cpp) — Pfad→Name für Shells+Agenten;
  i18n „Eingabeaufforderung"→„Command Prompt" (Kontext `Shells`).
- **Restore-Bugfix**: `m_shuttingDown` verhindert, dass `shutdownAll` den gespeicherten
  Session-Zustand per Auto-Remove leert (war: schwarzer Schirm/verlorene Sessions beim Neustart).
- **Installer**: unsigniertes **MSI** (WiX **v5** als dotnet-Tool — v6/v7 = OSMF-Fee, später Lizenz)
  via `installer/QTmux.wxs` + `installer/build-msi.ps1`; Artefakte unter `dist/` (git-ignoriert):
  `QTmux-0.1.0-win64.msi` + `…-portable.zip`. Signing offen (Authenticode, Early-Adopter-Build).
- **Offen/zurückgestellt**: Umlaut-Mojibake bei **PowerShell 5.1** (doppelkodiertes UTF-8 vom
  conhost — kein QTmux-Bug, ASCII unbetroffen; Milderung: UTF-8-CP/PowerShell 7);
  `Pty::currentWorkingDirectory()` auf Windows leer (PEB-Abfrage); Confluence/Jira-Doku noch
  nicht für diese Session nachgezogen (Dual-Pflege bei Bedarf).

- ✅ **Phase 0** — Gerüst (CMake/Presets, Qt-Quick-Shell, .vscode; libvterm vendored, kein vcpkg)
- ✅ **Phase 1** — Terminal-Kern: PTY + libvterm + TerminalItem; 3 Tests grün; läuft auf macOS
- ✅ **Phase 1 (Windows)** — ConPTY in `WindowsPty.cpp`, **real verifiziert 2026-06-08**
  (Win 11 23H2, MSVC, Qt 6.11.1): Terminal-I/O, Prozessende-Erkennung und sauberer
  Prozessbaum-Shutdown funktionieren (geprüft über den MCP-Server gegen die GUI-App;
  `ctest` 4/4 grün). Umsetzung: CreatePipe ×2 → `CreatePseudoConsole` → `STARTUPINFOEX`
  mit `PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE` → `CreateProcessW` (kein `bInheritHandles`).
  Reader-Thread (blockierendes `ReadFile`) + **Waiter-Thread** (`WaitForSingleObject` auf
  das Prozess-Handle, erkennt Selbst-Beenden der Shell — conhost gibt dabei KEIN Pipe-EOF),
  Daten per `QMetaObject::invokeMethod(..., Qt::QueuedConnection)` in den GUI-Thread.
  `resize`→`ResizePseudoConsole`; `terminate`: `ClosePseudoConsole` **vor**
  `TerminateProcess` (+ `CancelSynchronousIo`), dann `descendantPids`-Baum beenden.
  **Schlüssel-Lektion:** qtmux ist `WIN32_EXECUTABLE` (GUI-Subsystem) — als Konsolen-App
  erben die ConPTY-Kindshells eine Konsole und das Terminal bleibt stumm. Details +
  Test-Setup s. Abschnitt „Build & Test (Windows)". Offen: `currentWorkingDirectory()`
  (PEB-Abfrage zurückgestellt → Shell startet im Home), Umlaut-Codepage (kosmetisch).
- 🟡 **Phase 2** — Session + SessionModel + datengetriebene Sidebar + Session-Wechsel: FERTIG.
  Sidebar-**Split-Button** „+ &lt;Typ&gt;" mit ▾-Dropdown (Shell/SSH/Seriell, gemerkt via Settings).
  Split-Panes + Sidebar-Reorder FERTIG (siehe unten).
  **Layout-Lektion:** verschachtelte `RowLayout` mit `fillHeight`-Kindern braucht
  `Layout.maximumHeight`/`fillHeight:false`, sonst wuchert sie in der `ColumnLayout`.
- ✅ **UI-Basis** — Theme `System/Hell/Dunkel` (`Theme.mode`, System folgt OS via
  `QStyleHints::colorScheme`, Ctrl+D), Terminal-Farben folgen dem Theme
  (`TerminalItem.background/foregroundColor` ← `Theme.terminalBg/Fg`), MenuBar mit allen
  Befehlen, i18n DE/EN (`App`-Singleton + `qt_add_translations`, Laufzeit-Umschaltung),
  Agent-Erkennung (`AgentRegistry.detect()`; `agy`→AntiGravity). 5 Tests grün.
  Sessions werden bei Shell-Ende automatisch entfernt (SessionModel) + „×" pro Sidebar-Zeile.
- ✅ **Toolbar + Icons** — obere `ToolBar` (`ApplicationWindow.header`) mit Schnellzugriff
  (Neu/Typ-Caret/SSH/Seriell/Schließen · rechts Theme-Toggle/MCP/Über). Icon-System:
  **Phosphor-SVGs** unter `resources/icons/*.svg` → `qrc:/icons/<name>.svg`, eingebettet via
  `qt_add_resources` (braucht **Qt6::Svg**). Genutzt über das Qt-`icon`-System
  (`icon.source` + `icon.color`). Helfer `window.icon(name)` baut den qrc-Pfad;
  Inline-Komponente `IconToolButton`. Sidebar-„×" tönt sein SVG per `MultiEffect`
  (`QtQuick.Effects`). Tooltips/Strings in i18n.
  **Lektion:** `header: ToolBar` mit innen `anchors.fill`-Layout braucht feste `height`,
  sonst kollabiert die Toolbar auf 0 (zirkuläre Größenabhängigkeit).
- ✅ **Dropdowns + Dialoge theme-gerecht** — themengebundene `palette` am
  `ApplicationWindow` (window/base/text/button/highlight…) → alle **In-Window**-Basic-Controls
  (Dialoge, ComboBoxen, Textfelder, Buttons, das `typeMenu`-Popup) erben die Theme-Farben.
  Inline-Komponenten: `AppDialog` (abgerundet/erhoben/gerahmt, gestylter Titel, abgedunkelter
  `Overlay.modal`), `AppComboBox` (gerahmtes Feld + Caret-Icon + abgerundetes Popup),
  `AppMenuItem`/`AppPopupBg`. SSH-/Seriell-/Über-Dialog nutzen `AppDialog`.
- 🟡 **Menü-Icons (System-Theme) → Backlog.** `Theme.systemDark`/`Theme.menuIcon` getönt
  nach **OS-Schema** (nicht App-Theme); `AA_DontShowIconsInMenus=false` in `main.cpp`.
  **Aber:** Qt Quick rendert in **nativen macOS-Menüs keine `icon.source`-Icons**
  (`QQuickNativeIconLoader::toQIcon()` baut nur `QIcon::fromTheme(name, fallback)`, `image()`
  bleibt leer — verifiziert: File-Menü zeigt keine Icons). Lösung wie **RAFTNG** (Widgets):
  QApplication+Qt6::Widgets, C++-`PhosphorIcon` mit `QIcon::setIsMask(true)` (folgt dann
  je Kontext System- bzw. App-Palette), native `QMenuBar`. Bewusst zurückgestellt
  (großer Umbau, Aktionen müssten in C++ verdrahtet werden). Auf Windows/Linux ist die
  QML-`MenuBar` In-Window und rendert die `icon.source`-Icons bereits.
- ✅ **Phase 3 (Agent-Awareness)** — vollständige OSC-Erkennung:
  - `VtScreen` fängt unbekannte OSC via `vterm_screen_set_unrecognised_fallbacks` ab →
    Signale `notify()` (OSC 9 / 777) und `promptMarker(kind,exit)` (OSC 133).
  - `Session`: `Activity`-Zustand (Running/Error/Closed, Sidebar-Ring) aus OSC 133;
    `lastNotification` aus OSC 9/777; `needsAttention` (blau, pulsierend) aus Bell,
    Notification oder Befehls-Ende (OSC 133;D) einer inaktiven Session.
  - `SessionModel`: `attentionRaised(row)` → `window.alert()` (Dock/Taskbar) wenn QTmux
    nicht im Vordergrund. Sidebar zeigt Notification-Text.
  - Shell-Integration: `shell-integration/qtmux.{bash,zsh}` (OSC 133 Marker + `qtmux-notify`).
  - **Wichtige Lektion (Bugfix):** Backend wird NUR vom `unique_ptr` besessen (kein
    `setParent`); stateChanged-Handler nimmt den `BackendState` aus dem Signal-Argument
    (nicht `m_backend->state()`), da das Signal während der Backend-Zerstörung feuert.
  - 9 Tests grün (test_pty/vtscreen/session/agent).
- ⬜ **Phase 3** — Agent-Awareness (OSC 133/9, Status-Ringe, Notifications)
- ✅ **Phase 4** — Serial (`SerialBackend`) + **SSH** (`SshBackend`) FERTIG.
  SSH läuft über den **System-`ssh`-Client im PTY** (Passwort/Key/known_hosts/ssh-config/
  Agent-Forwarding „funktionieren einfach"); `SshBackend : PtyBackend` baut die ssh-Argumente.
  Dialoge für beide unter „Datei". Persistenz inkl. host/port/user/identity. MCP
  `create_session type=ssh`. Offen: Connection-Manager/Profile, libssh2-Variante (SFTP).
- ✅ **MCP-Schnittstelle** — `src/server/McpServer.{h,cpp}`: eingebetteter MCP-Server
  (HTTP/JSON-RPC 2.0) auf `127.0.0.1:7345`, Menü „Agent-Steuerung". Tools: list/create/
  close/focus_session, send_text, read_screen, **attach_controller**, set_theme. Session hat
  stabile `id()` + `screenText()`. Doku: `docs/MCP.md`. End-to-end mit curl verifiziert.
- ✅ **MCP-Controller-Tab (rot)** — Session, in der der steuernde MCP-Agent läuft, bekommt
  links einen **roten Tab** (`#e5534b`) in der Sidebar. `Session::mcpController` (Property/Signal,
  NICHT persistiert) → Rolle `McpControllerRole` → Delegate-Balken (`qml/Main.qml`).
  - **Auto-Erkennung (Standard):** beim MCP-`initialize` ermittelt `McpServer::detectController`
    den Client-Prozess über `procinfo::pidOfTcpClient` (TCP-Port→PID) und ordnet ihn per
    **Vorfahrenkette** (`procinfo::ancestorPids`) der Session zu, deren `Session::processId()`
    (= Shell-PID, via `ITerminalBackend::processId`/`PtyBackend`) in der Kette liegt.
  - **Wichtige Lektion:** Auf aktuellem macOS liefert `KERN_PROCARGS2` **kein** Environment
    fremder Prozesse mehr (auch `ps eww` nicht) — daher Zuordnung über die Prozesshierarchie,
    NICHT über das Lesen von `QTMUX_SESSION_ID` aus dem Client-Env.
  - **Manuell (Fallback):** `QTMUX_SESSION_ID` wird in jede Shell injiziert
    (`SessionModel::createShellSession` → `PtyBackend::setExtraEnv`); Agent ruft
    `attach_controller(id)`. Plattform-Helfer: `src/core/ProcessInfo.{h,cpp}` (macOS libproc,
    Linux /proc, Windows-Stub).
- ✅ **Split-Panes** — Hauptbereich ist ein `SplitView` mit `Repeater` über `paneModel`
  (ListModel, ein Eintrag je Pane mit `sessionRow`). `window.activePane`/`activeTerminal`
  halten das fokussierte Pane; `currentRow` folgt dessen Session. Aktives Pane bei
  Mehrfach-Layout durch **Akzentrahmen** markiert; Fokus (Klick/Tab) aktiviert ein Pane
  (`TerminalItem.onActiveFocusChanged`). Aktionen `actSplitH`/`actSplitV` (Ctrl+Shift+E/O)
  legen ein neues Pane mit **neuer Shell-Session** an (Orientierung gilt für alle Panes,
  SplitView hat eine Achse); `actClosePane` (Ctrl+Shift+W) schließt das aktive Pane (letztes
  → normale Session-Schließung). Sidebar-Klick lädt die Session ins **aktive** Pane
  (`assignToActivePane`). `onRowsRemoved` führt **alle** Panes auf gültige Session-Reihen
  nach (verschobene Indizes). Copy/Paste/Kontextmenü nutzen `window.activeTerminal`. Toolbar-
  + „Ansicht"-Menü-Buttons mit neuen Icons `split-h`/`split-v`. E2E auf macOS verifiziert
  (2× nebeneinander, 3× untereinander, sauberes Beenden aller Shells). Offen: rekursive
  Misch-Layouts (H+V verschachtelt), Pane-Reorder.
- ✅ **Sidebar-Reorder (Drag)** — `SessionModel::moveSession(from,to)` (begin/endMoveRows,
  `QList::move`, führt `m_activeRow` nach, persistiert). Im Delegate ein `DragHandler`
  (nur Y-Achse, hebt die Kachel via `z`/`opacity`/`scale` an); beim Loslassen wird die
  Zielzeile aus `tile.y / (height+spacing)` berechnet und `window.moveSession()` gerufen,
  das zusätzlich **currentRow + alle Pane-`sessionRow`s** auf die neue Reihenfolge remappt
  (gleiche Indexlogik wie `onRowsRemoved`). E2E auf macOS verifiziert (markierte Session per
  Drag von unten nach oben; Auswahl/currentRow folgen korrekt). `TapHandler` (Auswahl) und
  `DragHandler` koexistieren über die Drag-Schwelle.
- ✅ **Einstellungen-Dialog (QTMUX-12, Settings-UI-Teil)** — `settingsDialog` (`AppDialog`)
  bündelt die persistierten Optionen: **Erscheinungsbild** (Theme `Wie System/Hell/Dunkel`,
  Sprache DE/EN), **Terminal** (Schriftgröße-`SpinBox` 6–40, Standard-Shell wenn `hasShellChoice`),
  **Eingabe & Zwischenablage** (Copy-on-Select, Rechtsklick-Paste, Multiline-Warnung). Zwei-Wege-
  Bindung an `window.*`/`Theme`/`App` → Änderungen wirken sofort. Erreichbar via Toolbar-Zahnrad,
  Menü „Ansicht → Einstellungen …" und **Cmd/Strg+,** (bewusst KEIN `StandardKey.Preferences`:
  macOS verschiebt das sonst ins App-Menü und der In-Window-Shortcut greift nicht — Komma lief
  ins Terminal). Inline-Komponente `SectionLabel`. E2E verifiziert.
- ✅ **Command-Palette (QTMUX-12, abgeschlossen) — VSCode-Stil** — **dauerhaftes Such-/Befehlsfeld**
  (`cmdBar` + `cmdInput`) mittig in der Toolbar (zentriert via zwei `fillWidth`-Spacer), immer
  sichtbar mit getöntem `command`-Icon und „⌘K/Ctrl+K"-Hinweis. Bei Fokus (Klick oder
  **Strg/Cmd+K** via `actCommandPalette` → `forceActiveFocus`+`selectAll`) klappt darunter das
  **`cmdPopup`** auf (nicht-modal, `focus:false` → Feld behält Tastaturfokus). `buildCommands()`
  stellt feste Befehle (Neue Session, SSH/Seriell, Teilen, Zoom, Broadcast, Theme, Einstellungen,
  MCP, Beenden …) **plus je offener Session einen „Wechseln zu: …"-Sprung** zusammen (lädt die
  Session ins aktive Pane). Teilstring-Filter (case-insensitive), ↑/↓ navigiert die `ListView`
  (`cmdList`, Fokus bleibt im Feld), Enter führt aus, Esc schließt + refokussiert das Pane.
  `onActiveFocusChanged` öffnet/schließt das Popup (Klick ins Terminal schließt; Item-Klicks im
  Popup nehmen keinen Fokus). `runCurrent()` schließt + leert das Feld, führt via `Qt.callLater`
  aus (Folge-Dialoge nicht verdeckt). Auch über Menü „Ansicht → Befehlspalette …" erreichbar.
  **Icon-Tinting-Lektion:** monochrome SVGs (`fill="currentColor"` → schwarz) im ListView-Delegate
  **nicht** mit `layer.effect` tönen (greift dort nicht zuverlässig — Icons blieben schwarz),
  sondern mit der **expliziten `MultiEffect`-Form** (eigenes `Item`, `source:` = unsichtbares
  `Image`, `colorizationColor` = `Theme.textBright`/`Theme.accent`). i18n DE/EN ergänzt.
  **Damit ist QTMUX-12 vollständig** (Settings-UI + Command-Palette). E2E auf macOS verifiziert
  (Cmd+K fokussiert → „spli" filtert auf 2 Treffer mit getönten Icons → Enter teilt + schließt).
- ✅ **Color-Schemes (QTMUX-18)** — wählbare ANSI-Farbpaletten + Import. Kern: `src/core/ColorScheme.{h,cpp}`
  (Gui-frei) — `struct ColorScheme` (fg/bg/cursor + 16 ANSI als `quint32`) und `ColorSchemeRegistry`
  (prozessweiter Singleton via `instance()`, QObject nur Qt Core). Eingebaute Schemata (QTmux
  Hell/Dunkel, Solarized Dark/Hell, Dracula, Gruvbox, Nord, One Dark); aktuelles Schema + importierte
  via QSettings persistiert (`colorSchemes/current` + `…/imported`). `VtScreen::applyColorScheme()`
  setzt die libvterm-Palette (`vterm_state_set_palette_color` ×16 + `vterm_state_set_default_colors`)
  und stößt ein Full-Repaint an. `Session` wendet beim VtScreen-Aufbau das aktuelle Schema an und
  hört auf `ColorSchemeRegistry::changed` (Live-Wechsel aller Sessions). `Theme.terminalBg/Fg/Cursor`
  liefern jetzt die Schema-Farben (unabhängig vom App-Hell/Dunkel); `Theme` verbindet sich mit der
  Registry → QML-Bindings (inkl. neuem `TerminalItem.cursorColor`) aktualisieren live. **Registry-QML-
  Brücke:** als **Context-Property** `ColorSchemes` in `main.cpp` (`setContextProperty`) — bewusst
  KEIN `qmlRegisterSingletonInstance` in die URI „QTmux", das kollidiert mit der auto-generierten
  Modul-Typregistrierung (Symptom: „TerminalItem is not a type"). UI: Einstellungen → „Erscheinungsbild"
  mit Schema-`AppComboBox` (`ColorSchemes.names`/`.current`) + „Importieren …" (`FileDialog`,
  `import QtQuick.Dialogs`) + 16-Farben-Vorschau. Import-Parser in `ColorScheme.cpp`: iTerm
  `.itermcolors` (XML-Plist via `QXmlStreamReader`), Xresources (`*color0:`) und Ghostty
  (`palette = 0=#…`).
  **Ganze App folgt dem Schema (erweitert):** Der Anwender wählt **je ein Schema für Hell und für
  Dunkel** (`ColorSchemeRegistry.darkScheme`/`lightScheme`, je via QSettings `colorSchemes/dark`+`/light`).
  Der bestehende Modus-Schalter (System/Hell/Dunkel) bleibt und bestimmt nur, **welches** der beiden
  Schemata aktiv ist: `Theme` meldet seinen effektiven Modus per `ColorSchemeRegistry::setDark(dark())`
  (im ctor, bei `setMode` und OS-Wechsel); `currentScheme()` = dark/light-Auswahl je nach Modus.
  **`Theme` leitet ALLE Chrome-Farben aus dem aktiven Schema ab** (nicht mehr hartkodiert hell/dunkel):
  Flächen als `mix(bg→fg)`-Schattierungen (Sidebar/Elevated/Hover/Border), Auswahl als `mix(bg→Akzent)`,
  Akzent = ANSI-Blau (Index 4), `textBright/Dim` aus fg. Kette nicht-zirkulär: Modus (mode/OS) → wählt
  Schema → färbt alles. Settings: zwei Combos „Farbschema (Dunkel/Hell)" mit je 16-Farben-Vorschau +
  ein „Importieren …" (importiertes Schema landet je nach Helligkeit im Dunkel-/Hell-Slot).
  E2E auf macOS verifiziert: Dunkel-Slot=Dracula → komplette App (Toolbar/Sidebar/Terminal) violett mit
  violettem Akzent; Ctrl+D → Hell-Modus nutzt Hell-Schema → ganze App hell mit blauem Akzent.
- ✅ **Terminal-Schriftart + Ligaturen (QTMUX-19)** — wählbare Monospace-Schrift und opt-in
  Programmier-Ligaturen. `TerminalItem`: `fontFamily` (Default = `QFontDatabase::systemFont(FixedFont)`)
  und `ligatures` (Default aus). **Run-basiertes Rendering** in `paint()`: zusammenhängende,
  gleich attributierte (fg/bold/italic/underline) Nicht-Leerzeichen einer Zeile werden in EINEM
  `drawText` gezeichnet → ermöglicht Ligaturen UND ist effizienter (die in den offenen Notizen
  genannte Optimierung); Runs brechen an Lücken/Leerzeichen, breiten Zeichen (CJK, einzeln gezeichnet)
  und Attributwechseln. Bei Monospace stimmt der Glyph-Vorschub mit dem Zellraster überein → keine
  Verschiebung. `applyFontFeatures()` schaltet `liga`/`calt`/`dlig` per `QFont::setFeature`/`unsetFeature`
  (Qt 6.7+): aus = Ligaturen unterdrückt (Default), an = Font formt sie im Run. Schriftliste:
  `AppController::monospaceFonts()` (`QFontDatabase::isFixedPitch`) + `defaultMonospaceFont()`. Global
  + persistiert (`window.terminalFontFamily/terminalLigatures`), an alle Panes gebunden; Settings →
  „Terminal" (Schriftart-Combo + Ligaturen-Checkbox). E2E auf macOS verifiziert (Run-Rendering korrekt
  & rastertreu mit Menlo; Schriftwechsel Menlo→Courier New sichtbar). Ligatur-*Bilden* nicht gezeigt
  (kein Ligatur-Font installiert), Mechanik aber verdrahtet.
- ✅ **Bracketed Paste + Multiline-Warnung (QTMUX-16)** — `VtScreen::startPaste()/endPaste()`
  rufen `vterm_keyboard_start/end_paste`; libvterm gibt die Klammern `ESC[200~`/`ESC[201~`
  **nur** aus, wenn die App DECSET 2004 aktiviert hat (Output-Callback → `outputToPty` → Backend).
  `TerminalItem::doPaste()` klammert die Einfügung entsprechend (im Broadcast roh). **Multiline-
  Warnung:** `paste()` erkennt mehrzeiligen Inhalt (enthält `\r`), hält ihn in `m_pendingPaste`
  zurück und meldet `multilinePasteWarning(lines)`; QML-`AppDialog` bestätigt → `confirmPaste()`
  bzw. `cancelPaste()`. Setting `pasteWarnMultiline` (Menü „Bearbeiten", persistiert). E2E
  verifiziert (Modus 2004 an → Einfügung als `^[[200~hello^[[201~`; 3-Zeilen-Dialog → Einfügen
  ohne Auto-Ausführung dank Bracketing).
- ✅ **Copy-on-Select + Rechtsklick-Paste (QTMUX-17)** — `TerminalItem`-Properties `copyOnSelect`
  (Auswahl beim Loslassen automatisch in die Zwischenablage) und `rightClickPaste` (Rechtsklick
  fügt ein statt Kontextmenü, PuTTY-Stil). Beide via Settings persistiert + Toggles im Menü
  „Bearbeiten"; an alle Panes gebunden. E2E verifiziert (Drag-Select kopiert, Rechtsklick fügt ein).
- ✅ **Broadcast-/Sync-Input (QTMUX-21)** — Modus „Eingabe an alle Sessions" (`window.broadcastInput`,
  Toggle via Toolbar-Icon `broadcast-input`, Menü „Ansicht" und **Ctrl/Cmd+Shift+B**; bewusst NICHT
  persistiert). `TerminalItem` hat `broadcast`-Property + Signal `inputForBroadcast(QByteArray)`:
  `sendInput()` routet Tastatur- UND Paste-Eingabe im Broadcast-Modus nach außen statt an die
  eigene Session; QML verteilt via `SessionModel::writeToAll()` an **alle** Sessions. Warn-Banner
  (Akzentfarbe) über den Panes, solange aktiv. **Lektion:** QByteArray-Signal → QML-`writeToAll`
  reicht Steuerbytes (`\r`) verlustfrei durch (Qt6 ArrayBuffer-Mapping; E2E bestätigt: `echo`+Enter
  lief in 2 Panes). E2E verifiziert (2 Panes, einmal getippt → beide führen aus).
- ✅ **Terminal-Zoom (QTMUX-14)** — globale Schriftgröße `window.terminalFontSize` (6..40 pt,
  via QML `Settings` persistiert), alle Pane-`TerminalItem.pointSize` binden daran. Aktionen
  `actZoomIn`/`actZoomOut` (`StandardKey.ZoomIn/ZoomOut` = Cmd/Strg +/−) + `actZoomReset`
  (Ctrl+0), im „Ansicht"-Menü. **Cmd/Strg+Mausrad** zoomt: `TerminalItem` sendet
  `zoomRequested(±1)` (sonst scrollt das Rad), QML ruft `window.zoomTerminal()`. E2E verifiziert
  (Cmd++ vergrößert, Cmd+0 zurück). i18n DE/EN ergänzt.
- ✅ **Scrollback (QTMUX-5)** — `TerminalItem` rendert jetzt die Historie: Scroll-Offset
  `m_scrollOffset` (0 = Live-Boden, >0 = Zeilen in die Historie). `viewportSource(row)` mappt
  jede sichtbare Zeile auf Scrollback (`VtScreen::scrollbackLine`) oder Live-Screen
  (`cell`); `paint()` und `selectedText()` nutzen es (Kopieren funktioniert auch gescrollt).
  **Mausrad** (`wheelEvent`, 3 Zeilen/Tick) + **Tastatur** (Shift+PageUp/Down seitenweise,
  Shift+Home/End an Anfang/Boden). Cursor nur im Live-Boden; dezenter **Scrollbalken** rechts.
  Eingabe (Tippen) springt zum Boden; bei neuem Output in der Historie hält `onDamaged()` den
  Anker (Offset += neue Scrollback-Zeilen), am Boden läuft die Ansicht mit. E2E verifiziert
  (seq 1 300, hochscrollen → Historie + Balken, runter → Live-Prompt). Scrollback-Cap 10000.
- ✅ **Copy & Paste im Terminal** — `TerminalItem`: Maus-**Selektion** (Press/Move/Release,
  Strom-/Zeilen-Auswahl) mit Highlight in `paint()`; Text via `VtScreen::cell` extrahiert
  (rechte Leerzeichen getrimmt). `copy()`/`paste()` (Q_INVOKABLE) über `QClipboard`; Paste
  wandelt `\n`/`\r\n` → `\r`. **Shortcuts plattformkorrekt:** macOS Cmd+C/V (kapert NICHT
  das Terminal-Ctrl+C/SIGINT, da Cmd=ControlModifier, physisches Ctrl=MetaModifier); sonst
  Ctrl+Shift+C/V im `keyPressEvent`. Menü „Bearbeiten" (`actCopy`/`actPaste`, Shortcut nur
  macOS via `StandardKey`) **und Rechtsklick-Kontextmenü** (`TerminalItem::contextMenuRequested`
  → `termContextMenu.popup()`). `hasSelection`-Property für Menü-Aktivierung. E2E verifiziert.
- ✅ **Prozess-Cleanup beim Quit** — `onClosing` ruft `saveState()` dann `SessionModel::shutdownAll()`
  (→ `Session::shutdown` → `ITerminalBackend::terminate`). `Pty::terminate` (UnixPty) erfasst via
  `procinfo::descendantPids` den **ganzen Prozessbaum** und beendet ihn (SIGHUP, kurze Gnadenfrist,
  dann SIGKILL) — auch HUP-ignorierende Prozesse (`nohup`). Verifiziert.
- ✅ **Session-Persistenz** — `SessionModel` speichert die Session-Liste (Typ, Serial-Port/
  Baud, **Arbeitsverzeichnis**) + aktive Zeile via QSettings bei jeder Änderung + `onClosing`;
  `restoreState()` beim Start. Fenstergeometrie via QML `Settings` (QtCore).
  **CWD-Wiederherstellung:** `Pty::currentWorkingDirectory()` fragt das Verzeichnis des
  Shell-Prozesses live ab (macOS `libproc`/`proc_pidinfo`, Linux `/proc/<pid>/cwd`);
  beim Neustart startet die Shell wieder dort. Terminal-*Inhalt* bleibt nicht erhalten.
  Shell-Sessions ohne gespeichertes Verzeichnis starten im Home (`QDir::homePath()`).
  MCP `create_session` akzeptiert zusätzlich `cwd`.
- ⬜ **Phase 5** — Plugin-System (QPluginLoader), MacPCAN-Integration
- ⬜ **Phase 6** — Politur & Distribution (CPack: DMG/MSI/AppImage, CI-Matrix)

## Offene technische Notizen

- Rendering ist aktuell zellweise (ausreichend für Phase 1). Optimierung: Runs gleicher
  Attribute zusammenfassen, dann GPU-Glyph-Atlas.
- Scrollback wird in `VtScreen` gespeichert (`sb_pushline`), aber noch nicht gerendert/gescrollt.
- Font: `QFontDatabase::systemFont(FixedFont)`; offscreen warnt über fehlende Family „Monospace" (harmlos).
