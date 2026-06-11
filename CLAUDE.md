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
2. **Rendering via Scene-Graph + GPU-Glyph-Atlas** (QTMUX-6, erledigt): `TerminalItem` ist
   ein `QQuickItem` mit eigenem RHI-Shader-Material (Atlas-Alpha × Vertex-Farbe). Der frühere
   `QQuickPaintedItem`/`QPainter`-Pfad bleibt als umschaltbarer **Fallback** erhalten
   (`gpuRendering=false`). Ligaturen werden inzwischen **auch im GPU-Pfad** über einen
   Glyph-Index-Atlas + Run-Shaping unterstützt (s. QTMUX-6-Eintrag).

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
   ├─ SshBackend   (System-ssh im PTY; SFTP via System-sftp) [FERTIG]
   ├─ SerialBackend(QtSerialPort)          [FERTIG]
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
| `i18n/qtmux_{de,en}.ts` | Übersetzungen; via `qt_add_translations` zu `:/i18n/*.qm` kompiliert/eingebettet. **Quellsprache Deutsch.** Strings in QML via `qsTr`; in C++ via `QCoreApplication::translate("<Kontext>", "…")` (Kontext `Shells` für „Eingabeaufforderung"→„Command Prompt"). `qt_add_translations` sammelt per `qt6_collect_translation_source_targets` **automatisch alle Targets im Verzeichnisbaum** — `qtmux_core` (z. B. `Session.cpp`/`ShellRegistry.cpp`) wird also **mit-gescannt**; `cmake --build … --target update_translations` (lupdate) extrahiert die C++-Strings korrekt (verifiziert: 46 Texte, 0 verloren). **Quellsprachen-Automatik:** nach jedem lupdate-Lauf finalisiert `cmake/FinishSourceLanguageTs.cmake` die `qtmux_de.ts` automatisch (Übersetzung = Quelltext, finished — POST_BUILD an `QTmux_lupdate`, deferred angehängt, da `qt_add_translations` die Targets erst am Scope-Ende anlegt); sonst warnt lrelease bei jedem Build über unfinished/ignorierte DE-Strings. Nur die EN-Datei braucht echte Übersetzungspflege. Eigennamen (PowerShell, Bash, …) bleiben bewusst unübersetzt. |
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
- `Pty::currentWorkingDirectory()` auf Windows **implementiert** (PEB-Abfrage via
  `NtQueryInformationProcess` + `ReadProcessMemory`, s. „Session-Persistenz") — **Windows-Test
  ausstehend** (am Mac nur durch `#ifdef` mitkompiliert, nicht ausführbar; CI baut Windows).

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

## CI (GitHub Actions)

`.github/workflows/ci.yml` baut QTmux bei jedem Push/PR auf `main` (+ manuell) auf
**macOS, Windows und Linux** und führt die Tests headless aus. Qt via
`jurplel/install-qt-action` (Module **qtserialport** + **qtshadertools** für
`qsb`/`qt_add_shaders`; qtsvg/qttools/qtdeclarative kommen mit dem Base). Ninja via
`gha-setup-ninja`; Windows MSVC via `ilammy/msvc-dev-cmd`; Linux GL/EGL/xkbcommon/
fontconfig-Devpakete. `ctest` mit `QT_QPA_PLATFORM=offscreen`; **Windows-Tests
informativ** (`continue-on-error`) wegen der ConPTY-Konsolen-Anbindung (s. u.).
**Erstlauf 2026-06-10 grün** auf allen drei Plattformen → der GPU-Glyph-Atlas (QTMUX-6,
inkl. Shader-Kompilierung) und der Windows-`#ifdef`-Clink-Code (QTMUX-25) **kompilieren
plattformübergreifend**. (Laufzeit-Verhalten — RHI-Rendering, Clink-Injektion — bleibt
manuell zu prüfen.) Hinweis: die Actions warnen über Node-20-Deprecation (ab Sept. 2026),
unkritisch — bei Gelegenheit Action-Versionen anheben.

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

## Status (Stand: 2026-06-11)

> ⏭️ **Nächste Aufgabe:** offen — z. B. Phase 5 (Plugin-System) oder Rest von Phase 6
> (CPack-Pakete, MSI-Signing). Bewusst zurückgestellt (mit Begründung, s. u.): PS-5.1-Mojibake
> (gehört in die Nutzerumgebung, kein blinder Hack) und native macOS-Menü-Icons (großer
> QApplication/Widgets-Umbau, kosmetisch — eigener dedizierter Schritt).
> Session 2026-06-11 (Fortsetzung): **Ligaturen im GPU-Pfad (QTMUX-6-Folgeoptimierung)** erledigt
> — Glyph-Index-Atlas (`GlyphAtlas::glyphByIndex`) + `QTextLayout`-Run-Shaping; `useGpu()` ist
> jetzt nur noch `m_gpu` (Ligaturen erzwingen keinen Fallback mehr). E2E auf macOS/Metal mit
> FiraCode verifiziert (Ligaturen formen, ASCII scharf, bold + CJK-Doppelbreite + Ligatur in
> einer Zeile, keine Asserts). Zusätzlich **`Pty::currentWorkingDirectory()` für Windows** per
> PEB-Abfrage implementiert (Windows-Test offen; CI baut Windows). Beides committet/gepusht.
> Session 2026-06-11: **SFTP-Browser (QTMUX-7-Rest)** erledigt — Dateitransfer über den
> System-`sftp`-Client (kein libssh2/keine Krypto-Abhängigkeit), Remote-Browser mit
> Navigation/Download/Upload. Unit-Test (ls-Parsing) + **echter E2E gegen einen lokalen
> rootlosen sshd** + UI-E2E auf macOS (siehe Feature-Eintrag). Die programmatische
> libssh2-Variante bleibt bewusst ungebaut (dependency-free-Linie).
> Session 2026-06-11: **Vault→Profil-Integration** erledigt — SSH-Passwort-Auto-Fill aus dem
> Secrets-Vault (nur Geheimnis-Name im Profil; Prompt-Erkennung + einmaliges Auto-Send),
> Unit-Test mit echtem PTY + E2E auf macOS (siehe Feature-Eintrag).
> **Windows-Test-Session 2026-06-11** (alles committet + gepusht): Stand `96676c9` von
> der Mac-Seite gepullt, Windows-Build/Tests grün gehalten und **zwei Windows-only-Bugs
> gefixt** (`e48a581`, `6aff105`):
> 1. **GPU-Glyph-Atlas (QTMUX-6) crashte auf Debug-Qt sofort** — Assert „QSGGeometryNode
>    is missing geometry" (qsgnode.cpp:407): `appendChildNode` bekam einen GeometryNode
>    ohne Geometry. Fix: jedem `QSGGeometryNode` in `updatePaintNode` beim Anlegen sofort
>    eine leere `QSGGeometry` setzen. Release-Qt (macOS) prüft das nicht → fiel dort nie auf.
> 2. **ConPTY-Terminals blieben unter dem VS-Code-Debugger (F5/cppvsdbg) leer** (nur Cursor,
>    keine Eingabe), standalone lief alles. Ursache: cppvsdbg startet im Default
>    `console:internalConsole` und **leitet die Standard-Handles um** → stört die ConPTY-
>    Pseudo-Konsolen (Kindshell hängt nach dem Init-Frame auf Konsolen-LPC `EventPairLow`).
>    **Fix: `.vscode/launch.json` (Windows) `"console": "externalTerminal"`** → keine
>    Umleitung, Breakpoints bleiben; Fallback „Run Without Debugging" (Strg+F5). Diagnose-
>    Signal: `stdout`-Handle ≠ 0 bei internalConsole, 0 bei externalTerminal. Defensiv:
>    `FreeConsole()` in main.cpp (war hier wirkungslos, consoleWnd stets 0). **Nicht** Clink
>    (nicht installiert), **nicht** Konsolen-Vererbung, **nicht** das Rendering.
> Die i18n-Quellsprachen-Automatik (lupdate→`FinishSourceLanguageTs.cmake`) hat beim Pull
> sauber gegriffen: trotz +40 neuer Strings meldet lrelease 159/159 finished, keine Warnung.
> Session 2026-06-10: **CI-Matrix** (GitHub Actions, macOS/Windows/Linux) eingerichtet —
> Erstlauf grün; bestätigt, dass QTMUX-6 + QTMUX-25 auf allen drei Plattformen bauen.
> Session 2026-06-10: QTMUX-6 (GPU-Glyph-Atlas) erledigt — Scene-Graph-Renderer mit
> dynamischem Glyph-Atlas + eigenem RHI-Shader-Material; QPainter-Fallback bleibt
> umschaltbar. E2E auf macOS verifiziert (siehe Feature-Eintrag). Jira dual noch auf Done
> setzen.
> Session 2026-06-10: QTMUX-25 (Clink für cmd.exe) **implementiert — Windows-Test noch
> ausstehend** (am Mac nicht testbar; Clink ist Windows-only). Erkennung + Shell-Eintrag
> „Eingabeaufforderung (Clink)" (siehe Feature-Eintrag unten). macOS-Build + alle 7 Tests
> grün; Jira erst nach dem Windows-Test auf Done setzen.
> Session 2026-06-10: QTMUX-22 (Secrets-Vault) erledigt — Pure-Qt-Master-Passwort-Vault
> (PBKDF2-HMAC-SHA512 + HMAC-SHA256-Keystream/Encrypt-then-MAC), Verwaltungs-UI, E2E verifiziert.
> Session 2026-06-10: QTMUX-23 (Login-Scripts pro Profil) erledigt — Auto-Befehle nach
> Verbindungsaufbau (Session sendet sie am ersten Prompt bzw. per Fallback-Timer),
> E2E verifiziert. QTMUX-7 + QTMUX-15 zusätzlich auf **Windows** verifiziert.
> Session 2026-06-10: QTMUX-15 (konfigurierbare Hotkeys inkl. Multi-Chord) erledigt —
> HotkeyRegistry + Aufnahme-Dialog + Settings-Liste, QSettings-persistiert, E2E verifiziert.
> QTMUX-7 (Connection-Manager/Profile) erledigt — Profile-Editor + Manager-Dialog +
> Schnellverbinden, QSettings-persistiert, E2E verifiziert.
> Session 2026-06-09: QTMUX-3 (verschachtelte H+V-Layouts) + QTMUX-4 (Pane-Reorder per
> Drag) erledigt; davor QTMUX-18 (Color-Schemes), QTMUX-19 (Schriftart+Ligaturen),
> QTMUX-20 (Quake-Modus), QTMUX-24 (Progress OSC 9;4), Command-Palette (QTMUX-12) —
> alles committet/gepusht, Jira dual erledigt.

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
  conhost — kein QTmux-Bug, ASCII unbetroffen; Milderung gehört in die Nutzerumgebung:
  UTF-8-CP/PowerShell 7 — ein blinder Daten-Strom-Hack würde legitime Sequenzen gefährden, daher
  bewusst NICHT eingebaut); `Pty::currentWorkingDirectory()` auf Windows **inzwischen
  implementiert** (PEB, Windows-Test offen); Confluence/Jira-Doku bei Bedarf nachziehen.

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
  Test-Setup s. Abschnitt „Build & Test (Windows)". `currentWorkingDirectory()` inzwischen
  per PEB implementiert (Windows-Test offen); Umlaut-Codepage (kosmetisch) weiterhin offen.
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
  `create_session type=ssh`. SFTP siehe nächster Punkt (System-`sftp`).
- ✅ **SFTP-Datei-Browser (QTMUX-7-Rest)** — Remote-Dateitransfer über den **System-`sftp`-
  Client** statt libssh2 (konsistent mit dem System-`ssh`-Ansatz, **keine Krypto-Abhängigkeit**).
  Kern: `src/viewmodels/SftpClient.{h,cpp}` (`QML_ELEMENT`, nutzt den `Pty`-Layer aus core).
  - **Steuerung:** treibt `sftp` **interaktiv** im PTY; jeder Befehl wird geschrieben, die
    Antwort bis zum nächsten **`sftp> `-Prompt** gesammelt und ausgewertet. Ablauf nach Connect:
    `pwd` (Heimatpfad) → `ls -la`. Operationen: `cd`/`cdUp` (absolute Pfade, in `"`-Quotes),
    `refresh`, `download`=`get`, `upload`=`put`. Auth „funktioniert einfach" (Key/Agent/
    known_hosts); ein Passwort (z. B. aus dem Vault) wird an die `password:`-Abfrage gesendet.
    Host-Key bei Erstkontakt via `-o StrictHostKeyChecking=accept-new` (TOFU, sonst hinge der
    nicht-interaktive Browser an einem yes/no-Prompt). 25-s-Connect-Timeout; Fehler vor dem
    ersten Prompt → `error()` mit der letzten Ausgabezeile.
  - **`parseListing()`** (statisch, Gui-frei): parst `ls -la` (Größe = Feld 4, Name ab Feld 8,
    Typ aus dem Perms-Block; „."/".." + Echo-/Prompt-/Fehlerzeilen übersprungen; Symlink-Name
    ohne „-> Ziel"). In `tst_sftp` ohne Server getestet (macOS- **und** Linux-`ls`-Format).
  - **UI** ([qml/Main.qml](qml/Main.qml)): `sftpDialog` mit Navigationsleiste (hoch/Pfad/
    aktualisieren + `BusyIndicator`), `ListView` der Einträge (Ordner-/Datei-Icon + menschliche
    Größe, Doppelklick auf Ordner = `cd`), Buttons Herunterladen (`FolderDialog` → Zielordner) /
    Hochladen (`FileDialog` → Datei) + Status-/Fehlerzeile. Erreichbar über den **„SFTP"-Button
    je SSH-Profil** im Verbindungen-Manager (`window.openSftp`, löst das Vault-Passwort wie beim
    Verbinden auf). Eine prozessweite `SftpClient`-Instanz `sftpClient`.
  - **Tests/E2E:** `tst_sftp` = Parsing (serverlos, immer) + **Live-Roundtrip** (env-gesteuert
    `QTMUX_SFTP_HOST…`, in CI übersprungen): connect→list→cd→**upload→download**, Inhalt
    verifiziert. **8 Test-Binaries grün.** Verifiziert gegen einen **lokalen rootlosen sshd**
    (Port 2222, Key-Auth, `sftp-server`-Subsystem) — Core-Live-Test + UI-E2E (Browser listet
    `/Users/nobser`, hoch-Navigation, Icons/Größen); i18n DE/EN (Kontext `SftpClient`+`Main`).
  - **Bewusst NICHT:** programmatische libssh2-Variante (zöge OpenSSL + Build/Signing über alle
    3 Plattformen nach → bricht die vendored/dependency-free-Linie). **Grenzen:** keine
    Transfer-Fortschrittsanzeige (sftp-Batch); Name-Parsing bei Leerzeichen im Besitzer/Gruppe
    theoretisch unscharf; ein interaktiver Host-Key-Wechsel würde nicht abgefragt (accept-new).
- ✅ **Connection-Manager / Profile (QTMUX-7)** — gespeicherte, wiederverwendbare
  Verbindungsvorlagen für **Shell/SSH/Seriell**. Kern: `src/core/ConnectionProfile.{h,cpp}`
  (Gui-frei, nur Qt Core) — `struct ConnectionProfile` (name + type 0=Shell/1=Ssh/2=Serial +
  alle typ­spezifischen Felder) und `ConnectionProfileRegistry` (prozessweiter Singleton via
  `instance()`, **QSettings**-Array `profiles`, `saveProfile`-Upsert über den Namen,
  `removeProfile`, `profilesVariant()` für QML). **Bewusst entkoppelt:** die Registry kennt
  KEINE Sessions — das Starten macht QML, indem es das Profil liest und die passende
  `SessionModel::create{Shell,Ssh,Serial}Session` ruft (`window.connectProfile`). QML-Brücke
  wie bei den Farbschemata als **Context-Property `Profiles`** in `main.cpp` (kein
  `qmlRegisterSingletonInstance`, das kollidiert mit der Modul-Typregistrierung). UI in
  [qml/Main.qml](qml/Main.qml): **Manager-Dialog** (`connectionsDialog` — Liste der Profile mit
  Typ-Icon/Ziel-Zusammenfassung + „Verbinden/Bearbeiten/Löschen", „Neu …", Leer-Hinweis) und
  **Profil-Editor** (`profileEditDialog` — Name + Typ-`AppComboBox`, danach **bedingte Felder**
  je Typ: SSH host/user/port/identity · Shell program/workingDir · Seriell port/baud; beim
  Umbenennen wird das alte Profil entfernt). Erreichbar über Toolbar-**Lesezeichen-Icon**
  (`bookmark.svg`), Menü „Datei → Verbindungen verwalten …" und die **Command-Palette**
  (fester Eintrag + je Profil „Verbinden: <name>"). Test `tst_profiles` (Upsert/Persistenz/
  Entfernen, QSettings-Testmodus) → **5 Test-Binaries grün** (neu: `tst_profiles`). E2E auf macOS verifiziert: SSH- und
  Shell-Profil angelegt/persistiert/gelistet, „Verbinden" lädt die Session ins aktive Pane
  (Shell-Profil mit `workingDir=/tmp` → `pwd`=`/private/tmp`), Löschen leert die Liste; i18n
  DE/EN ergänzt. **Lektion (GUI-Test):** das CGEvent-Maus-Tool braucht eine kurze Pause
  zwischen `leftMouseDown`/`leftMouseUp` (sonst nur Hover, kein Klick) — siehe Memory
  `qtmux-gui-test-macos`.
- ✅ **Secrets-Vault (QTMUX-22)** — verschlüsselter Geheimnis-Speicher hinter einem Master-
  Passwort, **dependency-frei** (nur Qt Core, kein OpenSSL). Kern: `src/core/SecretsVault.{h,cpp}`
  (Gui-frei, prozessweiter Singleton). **Krypto-Konstruktion (bewusst pure-Qt):**
  - KDF: **PBKDF2-HMAC-SHA512** (RFC 2898), selbst auf `QMessageAuthenticationCode` implementiert
    — `QPasswordDigestor` lebt in QtNetwork, das würde `qtmux_core` eine Modul-Abhängigkeit
    aufzwingen; PBKDF2 ist ein eindeutiger Standard, Sicherheits-Primitiv bleibt Qt's HMAC.
    Zufalls-Salt (16 B), 210 000 Iterationen → 64 B Schlüsselmaterial = encKey(32)+macKey(32).
  - Cipher: **HMAC-SHA256 als PRF im CTR-Modus** erzeugt einen Keystream (frischer 16-B-Nonce pro
    Schreibvorgang), XOR mit dem Klartext. **Encrypt-then-MAC:** tag = HMAC-SHA256(macKey,
    salt|nonce|ct); beim Entsperren konstantzeitig geprüft → falsches Passwort/Manipulation
    schlägt fehl. **Hinweis:** standardnahe, aber selbstgebaute Cipher-Konstruktion (kein AES) —
    bewusste Abwägung zugunsten der dependency-free-Linie (vom Anwender so gewählt).
  - Der ganze Geheimnis-Satz wird als ein JSON-Blob ver-/entschlüsselt und in
    `AppData/QTmux/QTmux/vault.json` (base64-Felder: salt/nonce/ct/tag) abgelegt. Schlüssel +
    Klartext liegen nur entsperrt im Speicher und werden beim Sperren mit 0 überschrieben.
  - API: `create`/`unlock`/`lock`/`changeMasterPassword`/`secret`/`setSecret`/`removeSecret`,
    `Q_PROPERTY exists/unlocked/names`. Als Context-Property `Vault` in `main.cpp`.
  - UI ([qml/Main.qml](qml/Main.qml)): `vaultDialog` mit zwei Zuständen (gesperrt/anlegen ↔
    entsperrt) — **innere ColumnLayouts in einer äußeren**, damit der unsichtbare Zustand aus
    dem Layout fällt (sonst stimmt die Dialoghöhe nicht). Passwortfeld wird **beim Öffnen
    fokussiert** (`forceActiveFocus`). Geheimnis-Liste mit Anzeigen (Auge-Icon)/Kopieren
    (`App.copyToClipboard`)/Bearbeiten/Löschen; Editor `secretEditDialog`; Master-Passwort-Wechsel
    `vaultChangePwDialog`. Toolbar-Schlüssel-Icon (`key.svg`, `active: Vault.unlocked`), Menü- +
    Command-Palette-Eintrag. Neuer Clipboard-Helfer `AppController::copyToClipboard`.
  - **Scope v1:** Vault-Speicher + Verwaltung. **Profil-Integration erledigt** (siehe
    „Vault→Profil-Integration" unten). Test `tst_vault` (Anlegen, Upsert, Persistenz-Roundtrip, falsches
    Passwort, **Manipulationserkennung**, Passwortwechsel; prüft auch, dass die Datei den
    Klartext nicht enthält) → **7 Test-Binaries grün**. E2E auf macOS verifiziert: anlegen →
    Geheimnis hinzufügen → anzeigen → sperren → mit Master-Passwort wieder entsperren (Geheimnis
    aus der verschlüsselten Datei korrekt zurückerhalten); i18n DE/EN.
- ✅ **Vault→Profil-Integration (SSH-Passwort-Auto-Fill, QTMUX-22-Folgeschritt)** — ein SSH-
  Profil kann auf ein **Vault-Geheimnis** verweisen, dessen Wert bei der Passwortabfrage
  automatisch gesendet wird. **Bewusst gespeichert wird NUR der Geheimnis-Name** im Profil
  (`ConnectionProfile::passwordSecret`, persistiert) — das Klartext-Passwort steht nie im Profil
  und verlässt den Vault nur flüchtig zur Session.
  - **Prompt-Erkennung** ([Session.cpp](src/core/Session.cpp), `scanForPasswordPrompt`): an
    `dataReceived` gehängt, hält einen gleitenden 512-B-Output-Puffer und prüft (case-insensitive)
    auf `password:` (deckt „<user>@<host>'s password:" und „Password:" ab). Bei Treffer wird das
    Geheimnis **genau einmal** + CR direkt ans Backend geschrieben — exakt wie ein getipptes
    Passwort (der System-`ssh` liest es von seinem PTY, Echo aus). **Einmal** senden vermeidet
    Lockout-Schleifen bei falschem Passwort. `setSshPassword()` vor `start()`; **nicht
    persistiert** (restaurierte Sessions füllen nicht erneut aus — wie beim Login-Script).
  - **Durchreichung:** `SessionModel::createSshSession(...,password)` → `Session::setSshPassword`.
    `window.connectProfile` löst `Vault.secret(p.passwordSecret)` **nur bei entsperrtem Vault**
    auf und reicht den Klartext durch (sonst leer → keine Auto-Fill).
  - **UI** ([qml/Main.qml](qml/Main.qml)): Profil-Editor zeigt für SSH eine Combo „Passwort
    (Vault)" (`(keines)` + `Vault.names`; gespeicherter Name bleibt auch bei gesperrtem Vault
    sichtbar) + Hinweis, wenn der Vault gesperrt ist. Tests: `tst_session::sshPasswordAutoFillOnPrompt`
    (echtes PTY-Skript `printf 'Password: '; read p; echo PWGOT:$p` → Auto-Send beweist die Kette)
    + `passwordSecret`-Roundtrip in `tst_profiles`. 7 Test-Binaries grün; E2E auf macOS verifiziert
    (Editor zeigt die Vault-Auswahl + Sperr-Hinweis); i18n DE/EN.
- ✅ **Login-Scripts pro Profil (QTMUX-23)** — Auto-Befehle nach Verbindungsaufbau. Das
  Profil (`ConnectionProfile`) hat ein Feld `loginScript` (eine Zeile = ein Befehl, persistiert).
  `Session::setLoginScript()` + Logik: das Script wird **einmal** gesendet, sobald die Shell
  bereit ist — bei Shell-Integration am **ersten OSC-133-Prompt** (`onPromptMarker` 'A'/'B' →
  `runLoginScript`), sonst per **Fallback-Timer** (800 ms nach dem ersten Output, via
  `armLoginScript` an `dataReceived` gehängt). `runLoginScript` leert `m_loginScriptPending`,
  sodass nur der erste Trigger feuert; jede nicht-leere Zeile wird als Befehl + CR direkt ans
  Backend geschrieben. `SessionModel::create{Shell,Ssh,Serial}Session` nehmen einen optionalen
  `loginScript`-Parameter; `window.connectProfile` reicht ihn durch. **Bewusst NICHT pro Session
  persistiert** — Quelle ist das Profil; restaurierte Sessions führen das Script nicht erneut aus
  (vermeidet u. a. Fehl-Sends in eine Passwortabfrage beim Restore). Profil-Editor: mehrzeiliges
  `TextArea`-Feld „Befehle nach Verbindung". **Grenze:** bei interaktiver Passwortabfrage ohne
  Shell-Integration könnte der Fallback-Timer zu früh senden → gedacht für key-/agent-Auth bzw.
  nicht-interaktive Verbindungen. Tests: `tst_session::loginScriptRunsOnConnect` (echte Shell,
  Auto-Send) + loginScript-Roundtrip in `tst_profiles`. E2E auf macOS verifiziert.
- ✅ **Konfigurierbare Hotkeys inkl. Multi-Chord (QTMUX-15)** — frei belegbare Tastenkürzel
  für 11 Aktionen. Kern: `src/core/HotkeyRegistry.{h,cpp}` (**Gui-frei**, nur Qt Core) — hält
  die Default-Sequenzen je Aktions-ID plus benutzerdefinierte Overrides, persistiert **nur die
  Overrides** via QSettings (Gruppe `hotkeys`). `Q_PROPERTY(QVariantMap bindings)` (NOTIFY
  `changed`) liefert die effektiven Sequenzen; QML bindet `Action.shortcut: Hotkeys.bindings[id]`
  → eine Neubelegung greift **sofort**. `setBinding` (Upsert; leer/Default entfernt den Override),
  `reset`/`resetAll`, `conflict(seq, exceptId)` (case-insensitiver Vergleich). **Multi-Chord:**
  eine Sequenz darf aus mehreren kommagetrennten Akkorden bestehen (QKeySequence-Format, z. B.
  `Ctrl+K, Ctrl+P`, max. 4). **Gui-frei-Trick:** das Bilden des Akkord-Strings aus einem
  Tasten-Event macht `AppController::keyChord(key, modifiers)` (QtGui-`QKeySequence`,
  PortableText) — die Registry bleibt dadurch testbar/Gui-frei. Als Context-Property `Hotkeys`
  in `main.cpp` registriert. UI in [qml/Main.qml](qml/Main.qml): Settings-Abschnitt
  „Tastenkürzel" (Liste je Aktion mit aktueller Sequenz + „Standard"-Button bei Abweichung +
  „Alle Kürzel zurücksetzen"); der Settings-Dialog ist jetzt **scrollbar** (Flickable,
  Höhe auf `window.height-180` begrenzt). **Aufnahme-Dialog** `hotkeyCaptureDialog`: ein
  fokussierter Item fängt `Keys.onPressed`, baut über `App.keyChord` die Akkorde (Esc bricht ab,
  Enter bestätigt, reine Modifier werden ignoriert), zeigt Live-Konfliktwarnung. **Wichtig:**
  solange der Aufnahme-Dialog offen ist (`capturing`), sind **alle** App-Shortcuts via
  `enabled: !hotkeyCaptureDialog.capturing` deaktiviert — sonst würde eine aufzunehmende Taste
  zugleich die zugehörige Aktion auslösen. Bewusst NICHT konfigurierbar: Zoom +/− und Copy/Paste
  (bleiben auf `StandardKey`, plattform-/terminal-sensibel). Test `tst_hotkeys` (Defaults,
  Upsert/Persistenz, reset, Konflikt, Multi-Chord) → **6 Test-Binaries grün**. E2E auf macOS
  verifiziert: „Split side by side" via Aufnahme auf Cmd+Shift+J (→ `Ctrl+Shift+J`) umgelegt,
  Liste zeigt „Standard"-Button, neues Kürzel teilt **live**, „Alle zurücksetzen" stellt die
  Defaults wieder her; i18n DE/EN ergänzt.
- 🟡 **Clink für cmd.exe (QTMUX-25) — implementiert, Windows-Test ausstehend.** Bietet auf
  **Windows** zusätzlich die Shell „Eingabeaufforderung (Clink)" an, wenn [Clink](https://github.com/chrisant996/clink)
  (Readline-Completion/History für cmd.exe, **GPL-3.0**) installiert ist. **Bewusst NICHT
  gebündelt** (GPL + Signing/Größe) — nur eine vorhandene Installation wird erkannt; als
  separater Prozess gestartet berührt das QTmux' Lizenz nicht. **Umsetzung — fügt sich in die
  bestehende `ShellRegistry`/Shell-Profil-Mechanik:**
  - **Erkennung** ([ShellRegistry.cpp](src/core/ShellRegistry.cpp), `findClinkLauncher()`,
    Windows-only): zuerst `QStandardPaths::findExecutable("clink")` (deckt **scoop-/winget-Shims**
    und manuelles PATH ab; bei `clink.exe` wird die `clink.bat` im selben Ordner bevorzugt),
    dann bekannte Installationsorte (`%LOCALAPPDATA%\clink`, `…\Programs\clink`, scoop
    `%USERPROFILE%\scoop\apps\clink\current`, `%ProgramFiles%`/`(x86)`).
  - **Shell-Eintrag:** das `program` des `ShellInfo` ist hier eine **vollständige Kommandozeile**
    `cmd.exe /k "<clink.bat>" inject --quiet`. Damit bleibt der gesamte bestehende, auf einem
    einzelnen `program`-String basierende Mechanismus (Default-Shell-Dedup, QSettings-Persistenz,
    Restore, QML-Auswahl) **unverändert** — die Clink-Shell ist über ihre eindeutige Kommandozeile
    von `cmd.exe` unterscheidbar.
  - **Zerlegung beim Start** ([PtyBackend.cpp](src/core/PtyBackend.cpp)): sind keine Argumente
    separat gesetzt, wird das `program` per `QProcess::splitCommand` in Programm + Args zerlegt
    (respektiert Anführungszeichen → Pfade mit Leerzeichen bleiben zusammen). Ein einfacher
    Programmname/-pfad ergibt genau ein Token und verhält sich wie bisher; `SshBackend` setzt
    `m_args` explizit und ist nicht betroffen. **Grenze:** ein unzitierter Programmpfad mit
    Leerzeichen würde fälschlich zerlegt — solche Pfade sind aber für `CreateProcessW`/`exec`
    ohnehin mehrdeutig; zitiert (oder Standardfall) ist alles korrekt.
  - **Titel:** `prettifyTitle` ([Session.cpp](src/core/Session.cpp)) erkennt die Clink-
    Kommandozeile (enthält „clink"+„cmd") → „Eingabeaufforderung (Clink)" statt eines aus der
    Kommandozeile geschnitzten Basisnamens (greift nur initial; ein OSC-Titel der Shell ersetzt ihn).
  - i18n DE/EN ergänzt (Kontext „Shells"). macOS-Build + alle 7 Tests grün. **Offen:** Verifikation
    auf Windows mit installiertem Clink (Erkennung + tatsächliche Injektion in cmd.exe).
- ✅ **GPU-Glyph-Atlas (QTMUX-6)** — Terminal-Rendering über den **Scene-Graph** statt
  `QQuickPaintedItem`/`QPainter`. `TerminalItem` ist jetzt ein **`QQuickItem`** mit
  `updatePaintNode()`. Komponenten:
  - **`src/terminal/GlyphAtlas.{h,cpp}`**: rastert jede genutzte Glyphe (Zeichen × bold/italic)
    **zellweise** per `QPainter` als Alpha-Maske in eine gemeinsame Textur (ARGB32-Premultiplied,
    weiß auf transparent) und cached ihr **Pixel-Rechteck**. Shelf-Packer, feste Breite (1024),
    wächst in der Höhe (alten Inhalt erhaltend → Pixel-Rechtecke bleiben gültig, nur die Textur-
    `generation` steigt). DPR-genau (Kacheln in Geräte-Pixeln). Zellweise = maximal cachebar.
  - **Ligaturen im GPU-Pfad (Folgeoptimierung, erledigt):** zusätzlich zum zellweisen Cache
    rastert `GlyphAtlas::glyphByIndex()` **einzelne, bereits geformte Glyphen per Glyph-Index**
    (aus einer `QRawFont`, via `QPainter::drawGlyphRun` weiß in eine **tinte-enge** Kachel;
    `IndexedEntry` merkt Pixel-Rechteck + **logischen Pen-Versatz/Größe**). Bei `m_ligatures`
    formt `updatePaintNode` jede Zell-Folge (gleiche fg/bold/italic, einbreite Zellen) per
    `QTextLayout`/`QTextLine::glyphRuns()` und legt die geformten Glyphen einzeln über den
    Index-Atlas ab (Baseline robust auf `rowY+m_baseline` verankert, per-Glyph-Versatz aus der
    Shaping-Position relativ zur `line.ascent()`; Breitzeichen/Lücken/Attributwechsel brechen
    den Run, Breitzeichen werden einzeln geformt). **Schlüsselvorteil ggü. einem Run-Text-Cache:**
    der Atlas bleibt durch die **Glyph-Zahl des Fonts** begrenzt, nicht durch die Textvielfalt.
    Damit ist `useGpu()` = `m_gpu` (Ligaturen erzwingen NICHT mehr den Fallback). E2E auf macOS
    (Metal, FiraCode) verifiziert: `-> => != >= <= === !== <=> |> :: ++ --` formen korrekt,
    rastertreu; ASCII unverändert/scharf; **bold + Ligaturen** und **CJK-Doppelbreite (日本語/中文)
    + Ligatur in einer Zeile** korrekt; keine Asserts. `gpuRendering=false` bleibt der QPainter-
    Fallback (unverändert, rendert Ligaturen ebenfalls via Run-Shaping).
  - **Eigenes `QSGMaterial`/`QSGMaterialShader`** (file-lokal in `TerminalItem.cpp`) +
    **RHI-Shader** `src/terminal/shaders/glyph.{vert,frag}` (`#version 440`), via
    **`qt_add_shaders`** (BATCHABLE) zu `.qsb` kompiliert und unter `:/shaders/` eingebettet
    (`find_package(... ShaderTools)`). Der Fragment-Shader multipliziert die **Atlas-Deckung
    (Alpha) mit der Per-Vertex-Vordergrundfarbe** → **ein Atlas färbt beliebige fg-Farben**.
    Custom-Vertex-Layout (pos `vec2` + tex `vec2` + color `ubyte4` normalisiert).
  - **`updatePaintNode`** baut vier Geometrie-Knoten in Zeichenreihenfolge: **Hintergrund**
    (ganzflächiger Default-bg + Nicht-Default-Zellen, `QSGVertexColorMaterial`), **Glyphen**
    (eigenes Material, Atlas-Textur), **Unterstreichung** (`QSGVertexColorMaterial`, liegt über
    den Glyphen) und **Overlay** (Selektion/Cursor/Scrollbalken, `QSGVertexColorMaterial`).
    Vertexfarben **vormultipliziert** (Scene-Graph-Konvention). Glyph-Quads werden zunächst mit
    **Pixel-UV** gesammelt und erst **nach** der Schleife mit der endgültigen Atlas-Größe
    normalisiert (der Atlas kann während der Schleife wachsen).
  - **Damage-Gating (Optimierung):** Der teure **Inhalt** (Hintergrund/Glyphen/Unterstreichung,
    iteriert alle Zellen + Atlas-Lookups) wird nur neu gebaut, wenn `m_geomDirty` gesetzt ist
    (Damage/Scroll/Resize/Font/Farbe). Das **dynamische Overlay** (Selektion/Cursor/Scrollbalken)
    wird bei JEDEM Update neu gebaut, ist aber **zell-zugriffsfrei** — die Selektion als ein
    Quad **je Zeile** über den Spaltenbereich (Zeilen-Span), Cursor/Scrollbalken als Einzelquads.
    So kosten Cursor-Bewegung und Maus-Selektion keinen Glyph-/Atlas-Aufbau. E2E verifiziert
    (Selektion über Wide-Chars korrekt, Unterstreichung im eigenen Knoten, Inhalt intakt).
  - **🔑 Schlüssel-Lektion (Bug):** bei einem **eigenen** Material muss die Textur selbst per
    **`QSGTexture::commitTextureOperations(state.rhi(), state.resourceUpdateBatch())`** in
    `updateSampledImage` auf die GPU geladen werden — `QSGSimpleTextureNode` macht das intern,
    ein Custom-Material nicht. Ohne den Commit bleibt die Atlas-Textur leer → **Glyphen
    unsichtbar** (Hintergrund/Cursor rendern, Text fehlt). War exakt das beobachtete Symptom.
  - **Fallback (umschaltbar):** `gpuRendering`-Property (Default an, via Settings „Terminal →
    Rendering" persistiert; Env-Override `QTMUX_NO_GPU=1`). Aus → `paintContents()` (die alte
    Run-basierte `QPainter`-Logik) rendert in ein `QImage` → `QSGSimpleTextureNode`. Bei aktiven
    Ligaturen wird intern immer der Fallback genutzt. Renderpfad-Wechsel zur Laufzeit über
    `dynamic_cast` auf den Wurzelknoten (Typ-Mismatch → alten Knoten verwerfen, neuen bauen).
  - E2E auf macOS (Metal) verifiziert: Glyphen scharf, ANSI-Farben (rot/grün/blau), **bold**,
    **unterstrichen**, **CJK-Doppelbreite (日本語)**, Cursor, viele Glyphen/Scrollback (Atlas-
    Wachstum), **Laufzeit-Umschaltung GPU↔Fallback ohne Crash**. 7 Test-Binaries grün.
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
- ✅ **Split-Panes — verschachtelte H+V-Layouts (QTMUX-3) + Pane-Reorder (QTMUX-4)** —
  Der Hauptbereich ist jetzt ein **rekursiver Layout-Baum** statt eines einachsigen
  `SplitView`. Datenmodell (`window.layout`, reines JS-Objekt): Knoten ist entweder ein
  **Blatt** `{paneId, sessionRow}` (ein Terminal) oder ein **Split** `{orientation, children:[…]}`.
  Beliebige Mischungen (H in V usw.) möglich. UI: rekursive QML-Komponente
  [qml/SplitNode.qml](qml/SplitNode.qml) — Blatt → Pane mit Kopf+`TerminalItem`,
  Split → `SplitView` mit `Repeater` über die Kinder. **QML verbietet die statische
  Selbst-Instanziierung eines Typs** → die Rekursion läuft über einen `Loader` mit
  `source`-URL; node/win **per `setSource(url,{props})` VOR dem Laden** setzen (sonst
  evaluieren innere Bindungen kurz mit `win===undefined` und brechen dauerhaft → weißer
  Header/kein Inhalt). Strukturänderungen mutieren den JS-Baum und rufen
  `window.rebuildLayout()` (Loader-`sourceComponent` kurz `null`); Tastaturfokus wird über
  die Registry `paneItems` (paneId→Term) wiederhergestellt. Operationen in
  [qml/Main.qml](qml/Main.qml): `splitPane(orientation)` (ersetzt aktives Blatt durch
  `[Blatt, neu]`; gleiche Orientierung im Eltern-Split → nur Geschwister anfügen),
  `closePane()` (Blatt entfernen, Eltern-Split mit 1 Kind via `collapseSplit` kollabieren),
  `assignToActivePane`, `moveSession`/`onRowsRemoved` (remappen alle Blatt-`sessionRow`s).
  Shortcuts `Cmd/Strg+Shift+E`/`O` (teilen), `Cmd/Strg+Shift+W` (Pane schließen). Aktives
  Blatt durch Akzentrahmen + getönten Kopf markiert.
  **Pane-Reorder (QTMUX-4):** jeder Pane-Kopf hat links einen **Greifpunkt** (sechs Punkte).
  Statt fragilem Qt-`Drag`/`DropArea` (ParentChange/Hotspot) ein **`DragHandler`
  (`target:null`) + manueller Hit-Test**: `beginPaneDrag`/`updatePaneDrag`/`endPaneDrag`
  in Main.qml ermitteln über `paneIdAt(scenePt)` (Szenen-Rechtecke aller `paneItems`) das
  überfahrene Pane (Live-Akzent-Highlight) und **tauschen beim Loslassen die `sessionRow`s**
  beider Blätter (`swapPanes`). **Wichtiger Bugfix** [TerminalItem.cpp](src/terminal/TerminalItem.cpp):
  `setSession` ruft `recomputeGrid` **nur bei gültiger Größe** (`width/height>0`) — ein noch
  ungelayoutetes, neu erzeugtes Item resizte sonst die (geteilte) Session auf 1×1 und verwarf
  ihren Inhalt; `geometryChange` synchronisiert mit der echten Größe → **Inhalt bleibt über
  Rebuild/Reorder/Close erhalten** (nur Scroll-Offset springt auf den Boden). E2E auf macOS
  verifiziert: H+V verschachtelt (links V-Stapel neben Pane), Drag-Reorder tauscht Inhalte
  sichtbar (`LINKS`↔`OBEN_RECHTS`), Schließen kollabiert den Split — überall Inhalt intakt.
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
- ✅ **Progress-Anzeige im Tab (QTMUX-24)** — Fortschrittsbalken in der Sidebar via **OSC 9;4**
  (ConEmu/Windows-Terminal-Protokoll: `OSC 9 ; 4 ; <state> ; <progress>`; state 1=normal, 2=Fehler,
  3=unbestimmt, 4=pausiert, 0=aus). `VtScreen::cbOsc` unterscheidet im OSC-9-Zweig `4;…` (→ Signal
  `progress(state,value)`) von `OSC 9;<text>` (weiterhin `notify`). `Session` hält `progressActive/
  progressState/progressValue` (Q_PROPERTY, NICHT persistiert) via `onProgress`; `SessionModel` reicht
  sie als Rollen `progressActive/State/Value` durch (dataChanged bei `progressChanged`). Sidebar-
  Delegate zeigt unten einen dünnen Balken: Breite = value %, Farbe nach state (normal=Akzent,
  2=rot, 4=gelb, 3=unbestimmt → voller Balken pulsierend). 5 Tests grün (neuer `oscProgress`-Test in
  `tst_vtscreen`: `9;4;1;42`→progress, `9;<text>`→weiterhin notify). E2E auf macOS verifiziert
  (`printf '\e]9;4;1;60\a'` → 60%-Akzentbalken; `…;2;85` → roter Balken).
- ✅ **Quake-Modus (QTMUX-20)** — global per Hotkey ein-/ausblendbares Fenster. Plattform-Hotkey
  `src/core/GlobalHotkey.{h,cpp}`: macOS via Carbon `RegisterEventHotKey` (Ctrl+` = keyCode 0x32 +
  `controlKey`; `InstallApplicationEventHandler`, Callback → `activated()` per `QMetaObject::invokeMethod`
  Queued in den GUI-Thread); **funktioniert ohne Bedienungshilfen-Rechte und systemweit** (auch wenn
  QTmux nicht vorn ist). Windows/Linux vorerst Stub (Feature dort deaktiviert, Checkbox disabled).
  Carbon via `if(APPLE) target_link_libraries(qtmux_core PRIVATE "-framework Carbon")`. Als
  Context-Property `QuakeHotkey` in `main.cpp` (Instanz auf dem Stack in `main`, lebt bis `exec()`
  endet). QML: `window.quakeMode` (persistiert) → `QuakeHotkey.setEnabled`; `Connections{onActivated}`
  → `toggleQuake()`: sichtbar+aktiv → `hide()`, sonst `showNormal()`+`raise()`+`requestActivate()`+
  Fokus aufs Pane. Settings → „Fenster"-Schalter (nur macOS aktiv). E2E auf macOS verifiziert
  (Ctrl+` blendet aus → blendet wieder ein; Backtick landet NICHT im Terminal, Hotkey konsumiert ihn).
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
  Shell-Prozesses live ab (macOS `libproc`/`proc_pidinfo`, Linux `/proc/<pid>/cwd`,
  **Windows** via PEB: `NtQueryInformationProcess` → PEB → `ProcessParameters` →
  `CurrentDirectory.DosPath` mit `ReadProcessMemory`, Offset 0x38, x64↔x64; **implementiert,
  Windows-Test ausstehend**); beim Neustart startet die Shell wieder dort. Terminal-*Inhalt*
  bleibt nicht erhalten.
  Shell-Sessions ohne gespeichertes Verzeichnis starten im Home (`QDir::homePath()`).
  MCP `create_session` akzeptiert zusätzlich `cwd`.
- ⬜ **Phase 5** — Plugin-System (QPluginLoader), MacPCAN-Integration
- ⬜ **Phase 6** — Politur & Distribution (CPack: DMG/MSI/AppImage, CI-Matrix)

## Offene technische Notizen

- Rendering: GPU-Glyph-Atlas über den Scene-Graph (QTMUX-6, erledigt) mit umschaltbarem
  QPainter-Fallback. **Damage-gating umgesetzt:** Inhalt (Hintergrund/Glyphen/Unterstreichung)
  wird nur bei `m_geomDirty` neu gebaut (Damage/Scroll/Resize/Font/Farbe), Cursor-/Selektions-
  Updates rebuilden nur das billige Overlay (Selektion als Zeilen-Spans). **Ligaturen im
  GPU-Pfad erledigt** (Glyph-Index-Atlas `glyphByIndex` + `QTextLayout`-Run-Shaping; Atlas durch
  die Glyph-Zahl des Fonts begrenzt, nicht durch die Textvielfalt — s. QTMUX-6-Eintrag).
- Scrollback wird in `VtScreen` gespeichert und gerendert/gescrollt (QTMUX-5).
- Font: `QFontDatabase::systemFont(FixedFont)`; offscreen warnt über fehlende Family „Monospace" (harmlos).
- **GPU-Atlas/Custom-Material:** Atlas-Textur in `updateSampledImage` per `commitTextureOperations`
  hochladen (sonst leere Textur). Auf Windows/Linux RHI-Backend (D3D/Vulkan/GL) noch verifizieren.
