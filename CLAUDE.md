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

**macOS-Installer (DMG):** `installer/build-dmg.sh [version]` (analog zum Windows-
`build-msi.ps1`). Baut das `macos-release`-Preset, stagt das Bundle nach
`dist/stage-mac/QTmux.app`, macht es per **`macdeployqt -qmldir=qml`** self-contained
(Qt-Frameworks + QML-Module + das Echo-Plugin in `Contents/PlugIns`), **signiert das
Bundle ad-hoc neu** (`codesign --force --deep --sign -` — macdeployqt schreibt rpaths
NACH seiner eigenen Signatur um → die wird ungültig, und Apple Silicon startet nur mit
gültiger Signatur) und baut via `hdiutil` ein „Drag-to-Applications"-DMG
→ `dist/QTmux-<version>-macos.dmg` (git-ignoriert). **Nicht notarisiert** (Early-Adopter,
wie das unsignierte Windows-MSI) → beim ersten Start Rechtsklick→„Öffnen" bzw.
`xattr -dr com.apple.quarantine /Applications/QTmux.app`. macdeployqt-`ERROR`-Zeilen zu
QtVirtualKeyboard/QtMultimedia/QtPdf sind harmlos (optionale, ungenutzte QML-Module).
Verifiziert 2026-06-13: DMG gemountet, App von dort gestartet (self-contained, ohne
Homebrew-Qt im Env), Emoji + Rendering korrekt.

**Linux-Installer (AppImage, QTMUX-10):** `installer/build-appimage.sh [version]` (analog
zu DMG/MSI hand-gerollt — **AppImage ist kein CPack-Generator**; Standard-Toolchain für
Qt ist `linuxdeploy` + `linuxdeploy-plugin-qt`). Baut das `linux-release`-Preset (oder
nutzt `QTMUX_BUILD_DIR`, damit die CI ihr vorhandenes `build/` hereinreicht), stagt ein
`AppDir` (qtmux-Binary + Echo-Plugin nach `usr/bin/plugins`, da PluginHost `<App>/plugins`
sucht) und lässt `linuxdeploy --plugin qt` Qt-Frameworks + QML-Module (`QML_SOURCES_PATHS`)
+ Plattform-/Image-Plugins bündeln → `dist/QTmux-<version>-x86_64.AppImage` (git-ignoriert).
Nutzt `installer/qtmux.desktop` (`Icon=qtmux` ← `resources/appicon/qtmux.png`). **CI-Fallen:**
GitHub-Runner haben **kein FUSE** → `APPIMAGE_EXTRACT_AND_RUN=1` (Tool- **und** appimagetool-
AppImages entpacken-und-ausführen); `ARCH=x86_64` für appimagetool; `qmake` via `QMAKE`/
`QT_ROOT_DIR` gefunden. **Verifiziert 2026-07-11 in der CI** (Linux-Job baut das AppImage aus
dem vorhandenen Build, lädt es als Artefakt `QTmux-AppImage` hoch; kein lokaler Linux-Rechner
nötig): Artefakt 40 MB, heruntergeladen = gültiges **AppImage Typ 2** (`file`: ELF x86-64,
Magic `41 49 02` bei Offset 8). **Nicht signiert** (Early-Adopter). macOS-DMG + Windows-MSI
bleiben ihre getunten Skripte (PCBUSB-Einbettung/Re-Signatur bzw. ConPTY/WiX) — **bewusst
keine CPack-Migration** (würde diese Feinheiten gefährden; AppImage füllte die Linux-Lücke).

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

**Konvention: bei jedem Build-Zyklus zusätzlich Release bauen** (unabhängig vom Installer).
Die Standard-Presets (`windows`/`macos`/`linux`) sind **Debug** (`CMAKE_BUILD_TYPE` im `base`-Preset);
Release-only-Probleme (Optimierung, toter/weg-optimierter Code, fehlende Asserts, RHI/Shader)
fallen im Debug nicht auf. Dafür gibt es dedizierte Release-Presets `windows-release` /
`macos-release` / `linux-release` (eigener binaryDir `build/<preset>`):
```bat
cmake --preset windows-release
cmake --build --preset windows-release
```
Der Installer (`installer/build-msi.ps1`) nutzt **dasselbe** `windows-release`-Preset
(`build/windows-release`) — es gibt also nur zwei Windows-Build-Verzeichnisse:
`build/windows` (Debug) und `build/windows-release` (Release).

**libvterm vendored:** liegt unter `third_party/libvterm/` (0.3.3, BSD, neovim-Mirror)
und wird als kleine C-Lib mitgebaut. libvterm wurde aus vcpkg entfernt; Vendoring hält
alle 3 Plattformen identisch & abhängigkeitsfrei. Wichtig: `project(... LANGUAGES C CXX)`
(ohne `C` ignoriert CMake die `.c`-Dateien → leere `vterm.lib` → Linkfehler).

> **⚠️ Lokaler libvterm-Patch — bei einem Update NICHT verlieren:** libvterm 0.3.3 kennt
> **kein Faint/Dim** (SGR 2) — kein Bit in `VTermScreenCellAttrs`, kein `case 2` im SGR-Parser.
> Dadurch erschien gedimmter Text (z. B. Claudes Eingabe-Vorschläge) in voller Default-Helligkeit
> = weiß. Wir haben Faint **additiv** ergänzt (alle Stellen mit `QTMUX:`-Kommentar markiert,
> `grep -rn "QTMUX:" third_party/libvterm`): Bit `faint` in `VTermScreenCellAttrs`
> (include/vterm.h) + den internen Pens (vterm_internal.h, screen.c `ScreenPen`),
> `VTERM_ATTR_FAINT`(+`_MASK`) **ans Enum-Ende angehängt** (ABI-stabil), SGR `2` (Faint an) und
> erweitertes SGR `22` (hebt Bold **und** Faint auf) in pen.c, plus Durchreichung in
> `resetpen`/`savepen`/`get_penattr`/screen `setpenattr`/Pen-Copy/`get_cell`/`attrs_differ`.
> `VtScreen` liest `vc.attrs.faint` → `Cell.faint`; `TerminalItem::effectiveFg()` dimmt die
> fg dann 45 % Richtung bg (beide Render-Pfade). Echtes 24-Bit-RGB/256-Farben funktionierten
> immer — der Fehler war ausschließlich das verworfene Faint-Attribut. Tests: `tst_vtscreen`
> `trueColorRgb`/`faintAttribute`.

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
  (`C3 83 C2 BC`) — eine Codepage-Verwechslung in conhost/ConPTY. `VtScreen`
  (libvterm `vterm_set_utf8(1)`, `QString::fromUcs4`) dekodiert das Empfangene korrekt; der
  Fehler entsteht VOR QTmux. ASCII ist unbetroffen.
  **Untersuchung 2026-06-12 (Windows, abgeschlossen — kein sauberer Fix shell-seitig):**
  - Umfeld: `chcp` 850, `[Console]::OutputEncoding`/`InputEncoding` = 850, `$OutputEncoding`
    = US-ASCII, **System-ACP = 1252** (UTF-8-Beta „weltweit" ist AUS, also nicht der Auslöser).
  - **Milderungen wirkungslos** (Ausgabe blieb `Ã¤Ã¶Ã¼Ã`): (a) `chcp 65001` (mid-session UND
    als Startkommando), (b) `[Console]::OutputEncoding=[Text.Encoding]::UTF8` (mid UND Start),
    (a)+(b) kombiniert, sowie zusätzlich `InputEncoding=UTF8`.
  - **Entscheidender Test:** rohe UTF-8-Bytes `C3 A4` (=ä) per `[Console]::OpenStandardOutput().Write`
    direkt aufs OS-Handle (PS-Encoding komplett umgangen) erscheinen in QTmux ebenfalls als `Ã¤`.
    → Die Doppelkodierung passiert in der **conhost/ConPTY-Übersetzungsschicht** (sie interpretiert
    die Kind-Ausgabe als System-ANSI **CP1252** — `C3`→`Ã`, `A4`→`¤` — und re-kodiert nach UTF-8),
    **bevor** die Bytes QTmux erreichen. `chcp`/`SetConsoleOutputCP` aus dem Kind ändern diese
    Übersetzung in der Pseudo-Konsole NICHT. Daher kann **keine** Shell-Einstellung es beheben.
  - **Folgerung:** Der naheliegende ShellRegistry-Fix (PS mit
    `-Command "[Console]::OutputEncoding=...UTF8"` starten) wurde **getestet und wirkt nicht** —
    nicht umgesetzt. Ein Daten­strom-Dekodier-Hack in QTmux ist unerwünscht (QTmux dekodiert
    korrekt; der Fehler liegt davor). **Empfohlene echte Abhilfen:** PowerShell 7 (`pwsh`, nutzt
    UTF-8 nativ — auf dieser Maschine nicht installiert, daher nicht gegengeprüft) **oder** die
    System-Option „UTF-8 für weltweite Sprachunterstützung" (setzt ACP=65001 → conhost würde
    die Kind-Bytes dann als UTF-8 interpretieren; zu verifizieren, da systemweit + Reboot).
    Bleibt sonst als bekannte kosmetische Einschränkung bei PS 5.1 bestehen.
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
**⚠️ Entity-Falle bei Anker-basierten Edits:** die **Cloud**-Storage kodiert Umlaute als
HTML-Entities (`n&ouml;tig`), on-prem speichert sie als UTF-8 (`nötig`) — String-Anker mit
Umlauten matchen daher auf der Cloud-Seite nicht. Lösung: Anker zusätzlich in der
Entity-Variante probieren (ä→`&auml;` usw.); *eingefügter* UTF-8-Text wird von beiden akzeptiert.

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

**Kanban-Pflege (Anwender-Vorgabe):** Tickets bei jedem Bearbeitungsfortschritt **dual eine
Spalte weiterschieben** — Arbeitsbeginn → on-prem „In Progress" (Transition 31) / Cloud
„In Arbeit" (21); fertig + verifiziert → „Done" (41) / „Erledigt" (41), mit Kurzkommentar.
Transitions je Issue via `GET /rest/api/<2|3>/issue/<key>/transitions` ermitteln (IDs können
je Workflow abweichen).

**MCP:** Atlassian bietet einen Remote-MCP-Server, aber **nur für Cloud** und mit interaktivem
OAuth (headless unzuverlässig) — deckt die on-prem-Hälfte nicht ab. Für die Dual-Pflege ist der
**einheitliche REST-Weg** (oben) besser; kein Atlassian-MCP in der Session verbunden.

## Status (Stand: 2026-07-11)

> ⏭️ **Nächste Aufgabe:** offen — z. B. Phase-6-Rest (Signierung/Notarisierung der Installer:
> macOS Developer-ID, Windows Authenticode) oder MacPCAN-Feinschliff (CAN-FD, ID-Filter,
> Konfig-Dialog statt `baud`-Befehl). **Offene Jira: QTMUX-2** (Windows-CWD-PEB-Funktionstest,
> braucht Windows) **und QTMUX-13** (native Menü-Icons, s. u.). **QTMUX-10 (Linux-AppImage /
> Phase-6-Packaging) am 2026-07-11 erledigt** — `installer/build-appimage.sh` (linuxdeploy +
> Qt-Plugin), CI baut+verifiziert das AppImage (Artefakt `QTmux-AppImage`, gültiges Typ-2-Image);
> Installer damit für alle 3 Plattformen fertig (DMG/MSI/AppImage). Committet `666d426`+Doku.
> **QTMUX-13 (native macOS-Menü-Icons) bleibt Backlog** — am 2026-07-11
> empirisch bestätigt, dass Qt 6.11 in nativen Menüs **weder `icon.source` noch `icon.name`**
> durchreicht (isoliert bewiesen: `QIcon::fromTheme`+qrc-Fallback löst auf, aber
> `QQuickNativeIconLoader` gibt es nicht ans NSMenu); einziger Weg wäre der große
> Widgets/`QMenuBar`-Umbau (bewusst deferred, s. [[qtmux-native-menu-icons]]).
>
> **macOS-Session 2026-07-11: QTMUX-9 (MacPCAN-Plugin) erledigt — erstes echtes Plugin,
> gegen ECHTE PCAN-USB-Hardware verifiziert.** Ein CAN-Bus als Terminal-Backend über das
> Plugin-SDK (QTMUX-8). Liegt unter [plugins/macpcan/](plugins/macpcan/).
> - **Aufbau:** Die Qt-freie CAN-Zugriffsschicht aus dem MacPCAN-Projekt (github …/MacPCAN)
>   ist nach `plugins/macpcan/vendor/` **vendoriert** (6 Dateien, Namespace `mac_pcan`:
>   `core/CanFrame.hpp`, `core/ICanDevice.hpp`, `core/CanService.{hpp,cpp}` = Producer/
>   Consumer-Worker mit `drain()`/`send()`, `drivers/PcanDevice.{hpp,cpp}` = einzige
>   PCBUSB-Stelle, `drivers/MockDevice.{hpp,cpp}` = synthetisch). `MacPcanPlugin.cpp` =
>   `PluginInterface` + `CanBackend : ITerminalBackend`; `CanText.h` = testbares (PCBUSB-
>   freies) Frame-Format/Parsing.
> - **Zwei Backend-Typen** (im „+"-Menü, datengetrieben über `backendTypes()` — keine
>   QML-Änderung nötig): **`pcan`** „CAN-Bus (PCAN-USB)" (echte Hardware) und **`pcan-mock`**
>   „CAN-Bus (Demo)" (synthetische Frames, **ohne Hardware** nutz-/vorführbar). Bewusst getrennt,
>   kein stiller Fallback (Anwender-Wahl).
> - **Terminal-UX:** RX-Frames strömen als candump-nahe Zeilen (`  ts  #bus  ID  [len]  bytes  flags`);
>   getippte Zeile `<hexid> b0 b1 …` (hex, bis 8 Byte, `x`-Präfix/ID>0x7FF ⇒ Extended) sendet
>   einen Frame (`send()`), lokales Echo. Befehle: **`baud <rate>`** (125k/250k/500k/1M — öffnet
>   das Gerät zur Laufzeit mit neuer Bitrate neu, da v1 keinen Konfig-Dialog hat), `help`,
>   `clear`, `quit`, Strg+D. `pump()` löscht/rekonstruiert die Prompt-Zeile, damit die Eingabe
>   beim asynchronen Frame-Zustrom erhalten bleibt.
> - **Lizenz/Build:** **PCBUSB** (UV Software, proprietär aber redistributierbar) wird **NICHT
>   ins Repo gelegt** (wie GPL-Clink) — CMake findet es bedingt über `QTMUX_PCBUSB_DIR`
>   (Default: `…/MacPCAN/third_party/PCBUSB`), sonst wird das Plugin **still übersprungen**;
>   nur `APPLE`. Die universelle `libPCBUSB.0.13.dylib` (install_name bereits `@rpath/…`) +
>   `LICENSE`/`COPYRIGHT` werden ins Bundle kopiert; Plugin-rpath `@loader_path/../Frameworks`
>   (Bundle) + `@loader_path` (Build-Baum). `.gitignore` um `third_party/PCBUSB/` ergänzt.
> - **Verifiziert (macOS):** Debug **und** Release je **11/11 ctest** (neu `test_macpcan`:
>   Frame-Format/Parser, PCBUSB-/hardwarefrei → läuft überall). **E2E über MCP gegen die GUI:**
>   `list_plugins` zeigt beide Typen; Demo-Session streamt synthetische Frames + `123/x18DAF110`-
>   TX + `help` + Parsefehler; **ECHTE Hardware** (PCAN-USB-FD des Anwenders): `baud 125k` → reale
>   Bus-Frames (`200 [8] 11 22 …`), **TX auf den Bus** (`7AA [2] DE AD`, vom aktiven Bus quittiert),
>   im GPU-Terminal sauber gerendert. **Lektion:** nur **ein** Handle pro PCAN-Kanal — eine
>   restaurierte `pcan`-Session hält den Kanal, eine zweite bekommt „keine Hardware"; PCBUSB
>   meldet einen Kanal ohne Hardware zudem optimistisch als „verbunden" (RX bleibt dann leer).
> - **Bewusst v1-offen:** CAN-FD-TX/RX (PcanDevice-Gerüst da), ID-Filter, Konfig-Dialog
>   (statt `baud`-Befehl), DBC-Decoding, Progress.
> - **EA-Release v1.3.0 (2026-07-11):** Version 1.2.0→1.3.0 (CMakeLists/main.cpp/MCP/build-dmg.sh/
>   QTmux.wxs; committet `0fd913d`, gepusht). **DMG gebaut+verifiziert** `dist/QTmux-1.3.0-macos.dmg`
>   (56 MB): das Plugin + `libPCBUSB.0.13.dylib` + PCBUSB-Lizenz sind self-contained im Bundle
>   (`macdeployqt` bereinigt den absoluten Dev-rpath auf `@loader_path/../Frameworks`), aus dem
>   gemounteten DMG gestartet → MCP meldet v1.3.0 und lädt `macpcan` (beide Typen; das Laden beweist
>   die PCBUSB-Auflösung aus dem Bundle), Demo-Frames laufen. Projekteigene DUAL-Doku: Entwickler-Doc
>   (on-prem v14/Cloud v13) + Benutzer-Doc (v6/v5) + DMG als Anhang an beide Benutzer-Docs. **Offen:**
>   Firmen-Confluence-Firmen-Confluence (Windows-Download-Kanal) auf 1.3.0 — von hier nicht erreichbar,
>   Windows-/Heim-Task (wie bei früheren Releases).
> **Vorherige Session-Notizen:**
> **macOS-Session 2026-07-09/10: Maus-/Scrollrad-Reporting an Apps + „Neuer Tab erbt CWD",
> committet+gepusht (`c2dbe77` v1.2.0, `5b90749`).** Zwei Anwender-Befunde, E2E verifiziert.
> 1. **Maus-/Scrollrad-Reporting (`c2dbe77`, Version 1.2.0)** — Anwender-Frage: „Scrollback mit
>    zsh geht nicht" bzw. „werden Scrollrad-Infos nicht in eine Fullscreen-App (Claude Code)
>    transportiert?". **Diagnose:** der Scrollback-**Puffer** war korrekt (per MCP `read_screen
>    scrollback:true` bewiesen: 203 Zeilen mit zsh gespeichert). Das Problem: QTmux hatte **KEIN
>    Maus-Reporting** — `TerminalItem::wheelEvent` scrollte bedingungslos den lokalen Scrollback,
>    der im **Alternate Screen** (den Claude Code/vim/less/htop nutzen) leer ist; `VtScreen`
>    ignorierte `VTERM_PROP_MOUSE`. Die App bekam nie Maus-Events → Rad wirkte „tot". **Fix (volle
>    Weiterleitung):** `VtScreen` trackt jetzt `VTERM_PROP_MOUSE` (`mouseTracking()` 0/1/2/3 aus
>    DECSET 1000/1002/1003); neue `mouseButton()`/`mouseMove()` reichen Events an libvterm, das die
>    X10-/SGR-Sequenzen erzeugt und über den bestehenden Output-Callback (`cbOutput`→`outputToPty`)
>    an die PTY schickt. `TerminalItem`: `wheelEvent` + `mousePress/Move/Release` leiten bei aktivem
>    Tracking weiter (Rad=Button 4/5, Klick/Drag=1/2/3), sonst lokaler Scrollback/Selektion;
>    **Shift+Drag** selektiert immer lokal (Terminal-Konvention). Qt→VTermModifier (macOS: physisches
>    Ctrl=Meta→`VTERM_MOD_CTRL`). **Lektion:** libvterm **entprellt** — ein zweites Press desselben
>    Buttons ohne Release ist ein No-op (Tests brauchen saubere press→release-Paare). Test
>    `tst_vtscreen::mouseReporting` (DECSET 1000/1006, SGR `\e[<0;11;6M`, Wheel-Code 64, Tracking-
>    aus). **GUI-E2E:** Maus-Tracking-App gestartet, per CGEvent-Scroll gescrollt → App empfängt
>    `WHEEL_UP/DOWN` korrekt; ohne Tracking keine rohe Sequenz in der Shell. **Bonus:** Maus jetzt
>    auch in vim/htop/tmux/less. **Grenze:** reines Hover-Tracking (DECSET 1003, ohne gedrückte
>    Taste) wird nicht gemeldet (bräuchte Hover-Events); Drag (1002) funktioniert. Beim Push
>    Rebase auf Remote-v1.1.2 nötig (nur Versions-Konflikte → 1.2.0 gewinnt).
> 2. **Neue Shell erbt CWD der aktiven Session (`5b90749`)** — wie „Neuer Tab" in Terminal.app/
>    iTerm: eine neu geöffnete Shell startet im Live-Arbeitsverzeichnis der zuvor aktiven Session
>    statt immer im Home. In `SessionModel::createShellSession`: bei leerem `workingDir` wird das
>    `currentWorkingDirectory()` (libproc/`/proc`/PEB) der aktiven Session übernommen. Absicherungen:
>    explizites Verzeichnis (MCP `cwd`/Profil) hat Vorrang; **nur Shell-Quellen** (SSH/Seriell/Plugin
>    haben kein lokales CWD → Home); **nicht beim Restore** (`m_restoring`-Guard — jede Session behält
>    ihr gespeichertes Verzeichnis); Home-Fallback ohne aktive Session/CWD. Gilt für alle Wege
>    (Toolbar/Menü/MCP), da alle durch `createShellSession` laufen. E2E: `cd /tmp` in Session A →
>    neue Shell startet dort; restaurierte Sessions unberührt. 10/10 Tests grün.
> **Windows-Session 2026-06-26: Tastatur-Unterstützung für modale Dialoge (Enter=OK,
> ESC=Abbrechen).** Alle modalen Dialoge basieren auf der Inline-Komponente `AppDialog`
> ([qml/Main.qml](qml/Main.qml)). **ESC** lief schon immer über die `closePolicy`
> (`Popup.CloseOnEscape` ist Default → `reject()`/`close()`); jetzt explizit gesetzt. **Enter=OK**
> war der eigentliche Bedarf und brauchte ZWEI Mechanismen, weil sich Qt hier sperrt:
> (1) **In-Dialog-`Shortcut`** (`sequences:["Return","Enter"]`, `enabled: visible && hasAccept`,
> `onActivated: accept()`) — **innerhalb** des Dialogs deklariert, da ein **fensterweiter**
> Shortcut über einem **modalen Popup NICHT feuert** (empirisch belegt). `hasAccept` = Dialog hat
> Ok/Save/Yes (Close-only-Dialoge wie Einstellungen/Verbindungen bewusst ohne Enter). (2) Für
> Dialoge mit **einzeiligem `TextField`-Fokus** greift der Shortcut NICHT, weil das fokussierte
> Feld Return per **ShortcutOverride** selbst beansprucht → dort bestätigt **`TextField.onAccepted:
> <dlg>.accept()`** (SSH-Felder, Geheimnis-Name/-Wert). SSH fokussiert via `onOpened` das erste
> Feld (UX). **Mehrzeiliges `TextArea`** (Login-Skript im Profil-Editor) behält Enter für
> Zeilenumbrüche (es beansprucht Return ebenfalls selbst), Enter bestätigt dort nur außerhalb des
> Felds — bewusst. Aufnahme-Dialog (Tastenkürzel) behandelt Enter weiter selbst (eigene
> `captureArea`-Logik). **Verworfene Irrwege:** fensterweiter Shortcut (feuert nicht über Modal),
> Eltern-`Keys.onReturnPressed` (TextField propagiert Return nicht), OK-Button fokussieren
> (Qt-Quick-`Button` reagiert im Fokus nur auf **Leertaste**, nicht Enter). **Verifikation
> (Windows):** Debug+Release je **10/10 ctest** (ctest braucht Qt-bin im PATH, sonst
> `0xc0000135`); **E2E** `dist/_dlgkeys-e2e.ps1` (öffnet SSH via Ctrl+Shift+S [Feld-Pfad] und
> Seriell via Ctrl+Shift+R [Shortcut-Pfad], prüft ESC+Enter per UIA + Screenshot) **grün**;
> visuell bestätigt (Dialog nach Enter zu). **E2E-Falle:** synthetische Tasten an die qtmux-GUI
> nur zuverlässig mit `AttachThreadInput`-Foreground (statt blankem `SetForegroundWindow`) und
> Warteschleife aufs `MainWindowHandle`; ein **Alt**-Stoß zum Lösen des Foreground-Locks vor einem
> **ESC** ist tückisch (schaltet den Qt-Menümodus → ESC verlässt nur den, schließt nicht).
> Feature `4c39486` (QML); **Version 1.1.2** (CMakeLists/main.cpp/MCP-serverInfo/Installer).
> **EA-Installer 1.1.2 gebaut** (`dist/QTmux-1.1.2-win64.msi` 28,4 MB + `…-portable.zip` 33,5 MB).
> **Firmen-Confluence noch auf 1.1.1** (bei Bedarf `dist/_qtmux_pub2.py` `VER=1.1.2` + Changelog).
> macOS-Gegenprüfung (Dialog-Tastatur) offen.
> **Windows-Session 2026-06-25: Funktionstasten-Fix + v1.1.1 (committet + gepusht).**
> Befund (Anwender): F-Tasten kamen NICHT an cmd/Clink an. Ursache: `TerminalItem::encodeKey`
> ([src/terminal/TerminalItem.cpp](src/terminal/TerminalItem.cpp#L738)) kannte F1–F12 nicht →
> leere Bytes → nichts ans PTY; zusätzlich war **F1 global als „Über"-Kürzel** belegt
> ([HotkeyRegistry.cpp](src/core/HotkeyRegistry.cpp)). Fix: F1–F12 als xterm/VT220-Sequenzen
> (`F1=ESC O P` … `F12=ESC[24~`; ConPTY übersetzt sie in Konsolen-Tastenereignisse) + `actAbout`
> ohne Standard-Kürzel (F-Tasten gehören im Terminal der Shell; „Über" via Menü/Palette). **Real
> gegen Clink verifiziert** (F7 öffnet das History-Popup, Screenshot `dist/fkey-check/`); Debug+
> Release je 10/10 ctest. **Version 1.1.1** (CMakeLists/main.cpp/MCP-serverInfo/Installer).
> Commits `d08205d` (F-Tasten+v1.1.1), `2201852` (launch.json: Debug-Pfad über
> `${command:cmake.buildDirectory}` statt `…activeConfigurePresetName` — robuster gegen
> Preset-Reload). Run&Debug danach vom Anwender **als wieder funktionierend bestätigt**.
> **EA-Installer 1.1.1 gebaut** (`dist/QTmux-1.1.1-win64.msi` 29,8 MB + `…-portable.zip` 35,1 MB;
> `installer/build-msi.ps1 -Version 1.1.1`). **Firmen-Confluence (<space-key>) auf 1.1.1 aktualisiert**
> (`dist/_qtmux_pub2.py`, `VER=1.1.1`): Hauptseite <seiten-id> v7 (alte 1.1.0-Anhänge gelöscht,
> Changelog „Änderungen in 1.1.1" = F-Tasten), Anwender-Doku <seiten-id> v7 (F-Tasten-Infobox +
> korrigierte Hilfe/Über-Zeile), Entwickler-Doku <seiten-id> v4 (Versionsbump). **Projekteigene
> DUAL-Doku (confluence.intern.example + Atlassian-Cloud) für 1.1.1 NOCH OFFEN** — von der
> Firmen-Confluence-/Windows-Maschine aus NICHT machbar: die `Credential-{Confluence,Atlassian}.txt` liegen
> hier nicht und beide Endpunkte sind unerreichbar (`confluence.intern.example` = privates Heimnetz;
> `<cloud-instanz>.atlassian.net` übers Firmennetz geblockt). Gehört in eine macOS-/Heim-Session.
> macOS-Gegenprüfung (F-Tasten + Inter-Agenten-Feature) weiterhin offen.
> **Windows-Session 2026-06-18: Inter-Agenten-Benachrichtigung (NEU, umgesetzt + verifiziert;
> NOCH NICHT committet/gepusht).** Ein Agent/eine Shell wird benachrichtigt, wenn ein Agent in einer ANDEREN Shell
> **fertig** ist oder eine **Frage** hat; der benachrichtigte (MCP-)Agent erhält die
> **Quell-Session-ID** und arbeitet dort per `send_text`/`read_screen`/`focus_session` weiter.
> Architektur (Plan `~/.claude/plans/joyful-bubbling-sketch.md`):
> 1. **`AgentEventHub`** (neu, `src/core`, **Gui-frei** wie HotkeyRegistry; Context-Property
>    `AgentEvents`): Ereignis-Ringpuffer (Cap 256, monotone `seq` = Long-Poll-Cursor) + Abos
>    (Filter Quelle/Art `done|question|error|info`; leer = alle; **eigene Ereignisse nie an sich
>    selbst**). MVP **laufzeit-flüchtig** (Session-IDs nicht neustart-stabil). Gui-Freiheit
>    bewiesen: `test_agenteventhub` linkt nur `qtmux_core+Qt6::Test` und ist grün.
> 2. **Quelle:** OSC-Konvention `OSC 777 ; qtmux-event ; <kind> ; <text>` (erweitert den OSC-777-
>    Parser in `VtScreen::cbOsc` → Signal `agentEvent` → `Session::onAgentEvent` → Hub). Shell-Hook
>    **`qtmux-event done|question|… "Text"`** in `qtmux.{bash,zsh}` (für Claude-Stop-/Notification-
>    Hooks). Alternativ MCP-Tool **`post_event`**.
> 3. **Zustellung:** MCP-**Long-Poll `wait_for_events`** — im `McpServer` VOR `callTool`
>    abgezweigt (Socket bleibt offen; `PendingPoll`+`QTimer`, Default 25 s/Deckel 55 s), Wakeup
>    über `AgentEventHub::eventPosted`; `disconnected`-Handler räumt wartende Polls ab (kein
>    Use-after-free). Liefert `{events:[{sourceSessionId,kind,text,timestamp,seq}], nextSeq}`.
>    Zusätzlich neue Tools `subscribe_events`/`unsubscribe_events`/`list_subscriptions`;
>    `list_sessions`/sessionInfo um `lastAgentEvent{Kind,Text,Seq}` erweitert. Subscriber-Session
>    via optionalem `sessionId`-Arg (`$QTMUX_SESSION_ID`) + Vorfahren-Fallback (`sessionIdForClientPort`,
>    aus `detectController` refaktoriert). `Session` bekam `Q_PROPERTY sessionId` (für die QML-UI).
> 4. **UI:** Einstellungen-Sektion „Agenten-Benachrichtigungen" (je Session An/Aus + Arten-Filter
>    done/Frage/Fehler, reaktiv über `AgentEvents.subscriptionsChanged`; jede Zeile mit
>    `#<id> · <cwd>` zur Unterscheidung gleichnamiger Sessions). i18n DE/EN.
> **Windows-Shell-Helfer (2026-06-19):** `shell-integration/qtmux.ps1` (PowerShell: OSC-133-Prompt-
>    Marker + `qtmux-notify` + `qtmux-event`), `qtmux-event.cmd` (cmd, OSC), und **für Hooks**
>    `qtmux-emit.ps1`/`.cmd` (MCP `post_event` via HttpClient `UseProxy=false`, liest
>    `$QTMUX_SESSION_ID`) + `shell-integration/README.md`. **Wichtige Erkenntnis:** der **stdout
>    eines KI-Agenten-Hooks wird gekapselt** → OSC aus einem Hook erreicht QTmux NICHT; Hooks
>    müssen `post_event` (HTTP) nutzen. In docs/MCP.md dokumentiert (Claude-Stop-Hook-Beispiel).
> **Verifiziert (Windows):** Debug+Release je **10/10 ctest** (neu `test_agenteventhub` +
> `tst_vtscreen::oscAgentEvent` + `tst_session::agentEventReachesHub`). **E2E gegen die Release-GUI
> über MCP** (`dist/_agentnotify-e2e.ps1`): Long-Poll-Wakeup prompt (1,46 s) mit korrekter
> `sourceSessionId`, Timeout→`events:[]`, Self-Exclusion, Client-Abbruch ohne Crash + sauberer
> Shutdown. **Live-OSC durch die GUI** (rohe `]777;qtmux-event` aus einer Session → Subscriber).
> **ECHTER ZWEI-CLAUDE-AGENTEN-LAUF** (`dist/agent-demo/_run.ps1`, je `claude --print --model haiku`):
> Worker-Stop-Hook → `qtmux-emit.ps1` → `post_event done`; Supervisor ruft via QTmux-MCP
> (`--mcp-config` + `--allowedTools`, Prompt über stdin) `subscribe_events`+`wait_for_events` und
> meldet korrekt `sourceSessionId=1 kind=done`. (CLI-Fallen: `--settings` braucht eine DATEI, nicht
> JSON-String; `--allowedTools` ist variadisch → Prompt via stdin. `--dangerously-skip-permissions`
> vom Auto-Classifier geblockt → `--allowedTools` ist der saubere Weg.) **macOS-Gegenprüfung steht aus**
> (nur `#ifdef`-neutraler Code). docs/MCP.md + README erweitert. Memory `[[qtmux-agent-notify]]`.
> **Auf `main` committet + gepusht** (`a75876d` launch.json-Fix, `7579d43` Feature+Helfer+v1.1.0).
> **Version 1.1.0** (Minor-Feature-Bump; CMakeLists/main.cpp/MCP-serverInfo/Installer). **EA-Installer
> 1.1.0 gebaut** (`dist/QTmux-1.1.0-win64.msi` 28,4 MB + `…-portable.zip` 33,5 MB). **Firmen-Confluence
> (<space-key>) auf 1.1.0 aktualisiert** (`dist/_qtmux_pub2.py`, `VER=1.1.0`): Hauptseite <seiten-id> v6
> (alte 1.0.2-Anhänge gelöscht, Changelog), Anwender-Doku <seiten-id> v6 (neuer Abschnitt
> „Inter-Agenten-Benachrichtigung"), Entwickler-Doku <seiten-id> v3 (5 neue MCP-Tools + Workflow +
> Hook-Caveat). Davor: Pane-Prune-Fix (`8acfd6d`) Windows-verifiziert, 1.0.1/1.0.2-EA-Releases (s. u.).
> **Session 2026-06-14/15 (macOS): drei Bugfixes + App-Icon, alle committet+gepusht
> (`e57f928`, `a466dbc`, `de27c69`).** Befunde vom Anwender, alle E2E auf macOS verifiziert.
> 1. **Login-Shell-Fix (`e57f928`)** — lokale Shells starteten als Nicht-Login-Shell
>    (`argv[0]="/bin/zsh"` ohne `-`/`-l`) → `~/.zprofile`, `/etc/zprofile` (= `path_helper`/
>    Homebrew-PATH), `~/.zlogin` wurden NICHT geladen; aus dem Finder/Dock erbte die Shell nur
>    die magere launchd-Umgebung (PATH ohne `/opt/homebrew/bin`). Fix wie Terminal.app/iTerm:
>    Login-Shell-Markierung über `argv[0]` mit führendem `-`. `Pty::start` bekam einen
>    optionalen `argv0`-Parameter (UnixPty exec't via `execvp` mit separatem Such-Pfad, sodass
>    der Name `-zsh` sein darf; WindowsPty ignoriert ihn); `PtyBackend::setLoginShell(true)`
>    baut `-<shell>` NUR für echte Shells ohne eigene Args (SSH/Clink unberührt), gesetzt in
>    `SessionModel::createShellSession`. Verifiziert: `$0=-zsh`, `[[ -o login ]]` true, PATH
>    enthält Homebrew. Gilt macOS+Linux (gemeinsamer `forkpty`-Pfad).
> 2. **Menü-Sprache folgt der App-Sprache (`e57f928`)** — bei „English" blieben manche Menüs
>    deutsch. ZWEI Ursachen: (a) das QML wurde GELADEN, bevor der `QTranslator` installiert war
>    → die ins native App-Menü promoteten Items (Über/Einstellungen/Beenden) landeten dort mit
>    dem Quelltext, und `retranslate()` erreicht gerade diese promoteten Items nicht mehr (die
>    regulären File/Edit/View schon). Fix: Translator + `singletonInstance(App)` VOR
>    `loadFromModule`. (b) Die NATIVEN macOS-App-Menü-Standarditems (Über/Einstellungen/Dienste/
>    Ausblenden/Beenden) lokalisiert macOS über die **AppleLanguages-Preference** (System-UI-
>    Sprache), NICHT über unseren Translator. Fix in `main.cpp`: AppleLanguages aus `ui/language`
>    (Default-`QSettings`, gleiche Domain wie der AppController) VOR `QGuiApplication` per
>    `CFPreferencesSetAppValue(…, kCFPreferencesCurrentApplication)` setzen (CoreFoundation, kein
>    ObjC). **Lektionen:** ein eingespeistes `-AppleLanguages`-**argv wirkt NICHT** (NSUserDefaults
>    liest das ECHTE OS-Prozess-argv via `_NSGetArgv`, nicht Qts argv); der Wert persistiert in
>    `com.qtmux.app.plist` (cfprefsd-Flush) — unkritisch, da bei jedem Start neu gesetzt.
>    **Grenze:** ein LAUFZEIT-Sprachwechsel stellt die regulären Menüs sofort um, das native
>    App-Menü erst nach **Neustart** (AppleLanguages wird beim Start gelesen). Per System-Events
>    verifiziert: en+de durchgängig. (Verifikations-Falle: `defaults write`/PlistBuddy auf die
>    plist greift wegen cfprefsd-Cache nicht zuverlässig — Sprache über das App-Menü umstellen.)
> 3. **GUI-Freeze beim Schließen prozessbaum-schwerer Sessions (`a466dbc`)** — schloss man (z. B.
>    via MCP) mehrere Sessions mit Claude/node + vielen Subprozessen, fror die ganze App ein
>    (bunter Kreisel), force-quit nötig. **Per Stack-`sample` bewiesen:** der GUI-Thread hing in
>    `__wait4` bei `UnixPty.cpp` im blockierenden `waitpid(pid,&status,0)` nach SIGKILL, aufgerufen
>    aus `~Session` (via `deleteLater`). Ein per SIGKILL beendeter Prozess mit vielen Threads/
>    Mach-Ports (node-Baum) hängt sekundenlang im Kernel-Teardown (Prozesszustand `?E`=„exiting"),
>    `waitpid` kehrt erst danach zurück → GUI blockiert. Fix in `UnixPty::terminate()`: das
>    Abernten (Gnadenfrist + SIGKILL + `waitpid`) läuft im Normalbetrieb in einem **detached
>    Thread** (GUI-Thread sendet nur die Signale, kehrt sofort zurück; Shell wird geerntet → keine
>    Zombies, reparentete Nachfahren erledigt launchd). **App-Quit-Sonderpfad** über das statische
>    `Pty::s_quitting` (gesetzt in `SessionModel::shutdownAll`): synchron + nicht-blockierend
>    (SIGKILL an den Baum, KEIN `waitpid` — der Prozess endet, das OS reapt), damit auch
>    HUP-ignorierende Nachfahren (`nohup`) VOR dem Exit sterben (ein detached Thread liefe sonst
>    evtl. nicht mehr, bevor `main()` endet → hätte das verifizierte nohup-Cleanup gebrochen).
>    `emit finished(-1)` statt echtem exitCode (Closed-Zustand braucht ihn nicht; `Pty::finished`
>    wird nur für `setState(Closed)` genutzt). Mit echten node-Bäumen verifiziert: Schließen
>    **0,03 s statt 60 s Hang**, App responsiv, 0 Zombies; App-Quit 0,64 s inkl. `nohup`. Gilt
>    macOS+Linux. **Windows unangetastet** (eigene synchrone Teardown-Logik mit Reader-/Waiter-
>    Thread-Joins; `s_quitting` dort ungenutzt, schadet nicht).
> 4. **App-Icon „Multi-Agent-Ring" (`de27c69`)** — erstes App-Icon. Design: dunkles Squircle,
>    zentraler Prompt-Chevron `❯` + Cursor-Block in Akzent-Cyan, umgeben von drei Status-Bögen
>    (grün/blau/orange = die cmux-Status-Ringe laufend/aktiv/wartend). Quelle `resources/appicon/
>    qtmux.svg` → `qtmux.icns` (macOS, 10 Größen), `qtmux.ico` (Windows, 7 Größen, Vista-PNG),
>    `qtmux.png` (Linux 512). Da KEIN rsvg/inkscape/magick auf den Build-Maschinen ist, rastert
>    ein winziges Qt-Tool `svgrender.cpp` (QtSvg, dieselbe Engine wie die App) — `generate.sh`
>    erzeugt alle Formate reproduzierbar. **Einbindung (`CMakeLists.txt`):** macOS `.icns` →
>    `Contents/Resources` + `MACOSX_BUNDLE_ICON_FILE` (im Dock verifiziert); Windows `.rc`
>    (`IDI_ICON1 ICON qtmux.ico`) in die `.exe` kompiliert — `installer/QTmux.wxs` extrahiert ihr
>    Icon BEREITS aus der `.exe` (`SourceFile=…qtmux.exe`), MSI/Verknüpfung nutzen es ohne
>    Änderung; DMG nimmt es übers Bundle mit; Linux-PNG für `.desktop`/AppImage (Phase 6) bereit.
> **Build-Helfer:** ein Qt-SVG→PNG-Renderer lässt sich Homebrew-Qt direkt bauen:
> `clang++ -std=c++17 svgrender.cpp -F$(brew --prefix qt)/lib -framework QtCore -framework QtGui
> -framework QtSvg -Wl,-rpath,$(brew --prefix qt)/lib` (Framework-Header via `-F`, Includes als
> `<QtGui/QGuiApplication>`; `QT_QPA_PLATFORM=offscreen`).
> **Session 2026-06-13 (macOS):** zwei Punkte erledigt.
> 1. **Mehrfarbige Emojis im GPU-Pfad gefixt** — der Glyph-Shader verwarf die Atlas-RGB
>    (nur Alpha × fg) → Emojis waren einfarbige Blobs (Fallback war korrekt). `GlyphAtlas`
>    erkennt Farb-Glyphen per Pixel-Scan; Vertex-Alpha dient als Mono/Farb-Selektor; Shader
>    nutzt für Farb-Glyphen die Atlas-RGB direkt. Beide Pfade (zellweise + Ligatur) verifiziert
>    (Details im QTMUX-6-Eintrag). Hinweis: der Anwender hatte GPU-Glyphen in den Settings als
>    Workaround deaktiviert — mit dem Fix ist der GPU-Pfad (Default) wieder sicher.
> 2. **macOS-Installer (DMG)** — `installer/build-dmg.sh`: macdeployqt-self-contained +
>    Ad-hoc-Signatur + hdiutil-DMG (Drag-to-Applications). `dist/QTmux-1.0.1-macos.dmg`
>    gebaut + aus dem gemounteten DMG verifiziert (s. Build-&-Test-macOS-Abschnitt).
> **Windows-Session 2026-06-12 (Abend, v1.0.1-EA-Release):** Pane-Prune-Fix (`8acfd6d`)
> **Windows-verifiziert** (Details im macOS-Abnahme-Block unten), **EA-Installer 1.0.1**
> gebaut (`dist/QTmux-1.0.1-win64.msi` + portable ZIP) und **Firmen-Confluence-Confluence komplett
> aktualisiert**: Hauptseite <seiten-id> (v4, 1.0.0-Anhänge GELÖSCHT → nur noch 1.0.1 +
> Changelog), Anwender-Doku <seiten-id> (v4, stark erweitert: Splits/Profile/Vault/SFTP/
> Broadcast/Palette/Hotkey-Tabelle/Agenten-Funktionen/EA-Einschränkungen inkl. PS-5.1-
> Mojibake-Hinweis), **NEU Entwickler-Doku <seiten-id>** (MCP-Tools komplett + Plugin-SDK;
> gleiche Hierarchie unter der Hauptseite). Publish-Skript `dist/_qtmux_pub2.py`
> (löscht alte Release-Anhänge per DELETE auf die Attachment-ID). Projekteigene
> DUAL-Doku weiterhin offen (s. u.).
> **macOS-Abnahme-Session 2026-06-12 (nach der Windows-Welle): alles gepullt, ausgiebig
> getestet, 3 Befunde gefixt.** Builds Debug **und** Release (neue Preset-Konvention) je 9/9
> Tests grün. E2E auf macOS/Metal (Release-Build) verifiziert: Faint/SGR-2 (dim weiß + dim
> rot), Sidebar-CWD je Tab, **Soft-Wrap-Copy** (150-Zeichen-Wrap → Clipboard = 1 Zeile ohne
> `\n`), **scroll-feste Selektion** (Copy vor/nach Scroll identisch), Attention-Pulse (Bell in
> inaktiver Session → blau pulsierender Tab), MCP-Erweiterung komplett (list_shells/-serial_
> ports/-plugins, `create_session type=plugin` → Echo-Session, read_screen `scrollback:true`,
> send_text `broadcast` in alle Sessions, workingDir in list_sessions), Menü-Theming Hell +
> Dunkel inkl. nativer Kürzel (⌘C…), Cmd+1..9-Direktsprung. **Gefundene + gefixte Befunde:**
> 1. **Session-Navigation auf macOS wirkungslos:** Default „Ctrl+Tab" = Cmd+Tab gehört dem
>    System-App-Switcher; physisches Ctrl+Tab ist in Qt „Meta+Tab". Fix: macOS-Default in
>    `HotkeyRegistry` per `#ifdef` auf `Meta+Tab`/`Meta+Shift+Tab` — funktional verifiziert
>    (Marker-Test: Vorwärts- und Rückwärts-Umlauf über MCP-read_screen bewiesen).
> 2. **Über-Dialog zeigte „Qt 1.0.0":** `Qt.application.version` ist die App-Version, war
>    aber mit „Qt %1" beschriftet → jetzt „Version %1" (DE+EN).
> 3. **Command-Palette-Kürzel hartkodiert:** zeigten rohe Strings (auch nach Re-Binding
>    falsch) → jetzt an `Hotkeys.bindings` gebunden + `App.shortcutText` (native ⌘-Symbole;
>    Palette zeigt ^⇥/^⇧⇥ für die Session-Navigation).
> 4. **Panes „übernehmen" beim Session-Entfernen eine fremde Session (User-Report,
>    Repro: Session + 2 Splits untereinander → die neuen Sessions entfernen → Panes
>    bleiben, zeigen alle dieselbe Rest-Session, Anzeige verzerrt):** `onRowsRemoved`
>    bog die Blätter ENTFERNTER Sessions per `adjust()` auf eine überlebende Zeile um —
>    mehrere Panes teilten sich dann eine Session und kämpften mit verschiedenen Grids
>    um deren `resize()` (daher die Verzerrung). Fix: neuer Helfer `pruneLeaves(pred)`
>    entfernt die Blätter der gelöschten Sessions aus dem Layout-Baum (kollabiert
>    Splits; Baum komplett leer → ein Blatt mit der Folge-Session) und wählt das aktive
>    Pane neu; nur noch VERBLIEBENE Blätter werden index-nachgeführt. E2E am
>    Repro-Szenario verifiziert (exit in beiden Split-Sessions → Panes kollabieren
>    sauber auf eines, Inhalt intakt). **Windows-verifiziert 2026-06-12** (Pull
>    `8acfd6d`, Debug+Release je 9/9 Tests; E2E am Original-Repro gegen die
>    Release-GUI via MCP+SendKeys: 2× Ctrl+Shift+O → exit in beiden Split-Sessions
>    → Panes kollabieren 3→2→1 ohne Verzerrung, Screenshots `dist/panefix-check/`,
>    Original-Session-Inhalt per read_screen intakt).
> **Windows-Session 2026-06-12 (Menü-/MCP-/Doku-Welle, V1.0.0): committet + gepusht** (3 Commits:
> MCP+Soft-Wrap-Copy · Menüs/Palette/i18n · Version/Installer/Release-Check/Doku). Alle Punkte
> unten gebaut (Debug+Release) + 9/9 Tests grün + visuell in beiden Themes verifiziert.
> **Offen:** projekteigene DUAL-Doku (confluence.intern.example + Atlassian-Cloud) zu den neuen
> Features (MCP-Tools, Menü-Überarbeitung, v1.0.0) noch nachziehen; Jira-Abgleich bei Gelegenheit.
> 1. **Version 1.0.0** (CMakeLists/main.cpp/MCP/Installer/About-Fallback). Installer neu gebaut
>    (`dist/QTmux-1.0.0-win64.msi` + portable ZIP).
> 2. **MCP erweitert** ([McpServer.cpp](src/server/McpServer.cpp)): `create_session` dokumentiert
>    jetzt **ssh** + neuen Typ **plugin** (+ `loginScript`); `list_sessions` liefert zusätzlich
>    `workingDir` + Progress; `send_text` kann **broadcast**; `read_screen` optional **scrollback**
>    (neu `VtScreen::scrollbackText`/`Session::scrollbackText`); neue Discovery-Tools
>    **list_shells / list_serial_ports / list_plugins**. Live gegen die GUI verifiziert.
> 3. **Soft-Wrap-Copy (QTMUX-fix):** eine über Autowrap umbrochene Kommandozeile wird beim
>    Kopieren als EINE logische Zeile behandelt (kein \n am weichen Umbruch) → Paste ergibt wieder
>    genau einen Befehl (löste auch das „Paste landet nicht in der History"-Symptom). Umsetzung:
>    libvterm-`sb_pushline4` (continuation-Flag) statt `sb_pushline`, `vterm_screen_callbacks_has_pushline4`,
>    `VtScreen::lineContinuation`/`scrollbackContinuation`, `TerminalItem::selectedText` überspringt
>    \n an Continuation-Grenzen. Test `tst_vtscreen::lineWrapContinuation`.
> 4. **Menüs gründlich überarbeitet** (alle visuell in BEIDEN Themes verifiziert — s.
>    [[qtmux-menu-popup-theming]]): Tastenkürzel werden in den Menüs angezeigt
>    (`App.shortcutText` formatiert String- UND StandardKey-Shortcuts nativ; Copy/Paste via
>    `shortcutOverride`), **dynamische Menübreite** (`window.sizeMenu` setzt `contentWidth` explizit
>    — QQuickMenu nimmt NICHT zuverlässig das Maximum → sonst abgeschnitten/überlappend),
>    **Icon-Farbe folgt App-Theme** (Theme::menuIcon plattformabhängig: macOS=systemDark, sonst
>    `dark()`), **ThemedMenu** (Popups erben die ApplicationWindow-palette NICHT → eigene
>    themengebundene palette + AppPopupBg-Hintergrund), **eigener Highlight-Hintergrund** in
>    ShortcutMenuItem (Basic-Style nutzt sonst palette.light/midlight → schwarzer Selektionsbalken
>    im Hell-Modus), **Alt-Mnemonics** (`&` in den Titeln: D/B/A/S/G/T/H bzw. EN F/E/V/L/G/C/H),
>    redundanten „Helles Design"-Eintrag entfernt, Icons für die Bearbeiten-Optionen.
> 5. **Command-Palette-Parität:** Kopieren/Einfügen, Nächste/Vorige Session, Design-Modi explizit,
>    Sprache DE/EN, Edit-Umschalter und Plugin-Sessions ergänzt (alle Funktionen auch über die
>    Palette erreichbar, nicht nur MCP).
> 6. **i18n:** neue Strings via lupdate erfasst, EN vollständig übersetzt (196/196 finished).
> 7. **Release-Visual-Check als Standard:** [tests/release-visual-check.ps1](tests/release-visual-check.ps1)
>    startet die Release-EXE und screenshottet ALLE Menüs in Dunkel+Hell + MCP-Smoke. **Vor jedem
>    Release laufen lassen** (Menü-/Theming-Regressionen sind unit-test-unsichtbar). Technik:
>    UI-Automation `InvokePattern` öffnet Menüs trotz Foreground-Lock (Alt-Tastendruck löst den Lock).
> 8. **Firmen-Confluence-Confluence-Doku (<space-key>, on-prem `<firmen-confluence>`):** Hauptseite **QTMux**
>    (<seiten-id>) mit Kurzbeschreibung + **Download-Links/Anhängen** des Installers (MSI+ZIP);
>    Unterseite **QTmux - Benutzerdokumentation** (<seiten-id>) mit ausführlicher Doku + 2 Screenshots.
>    **Weg:** Python `requests` (verify=False, trust_env=False, proxies leer) wie
>    `VisualStudioExtension/_publish.py` — curl-POST hängt hier (Expect/Harness), Python nicht.
>    Credentials in `confluence.env` (git-ignoriert, **`*.env` in .gitignore ergänzt**). Dies ist die
>    **Firmen-Confluence** — getrennt von der projekteigenen DUAL-Doku (confluence.intern.example +
>    Atlassian-Cloud), die hier NICHT angefasst wurde.
> **Windows-Session 2026-06-12 (Fortsetzung, alles gepusht):** mehrere UX-Verbesserungen
> + Aufräumen, je Debug+Release gebaut, 9/9 Tests:
> 1. **Faint/Dim (SGR 2)** — Claudes gedimmte Vorschläge erschienen weiß; libvterm 0.3.3
>    kannte kein Faint. Vendored libvterm additiv gepatcht (s. ⚠️-Block im libvterm-Abschnitt),
>    `Cell.faint`, `TerminalItem::effectiveFg()` dimmt 45 % Richtung bg. RGB/256 funktionierten
>    immer. Tests `trueColorRgb`/`faintAttribute`. (Visuell auf Windows bestätigt.)
> 2. **Arbeitsverzeichnis pro Tab** — Sidebar zeigt klein (gedimmt, ElideLeft) das CWD jeder
>    Shell-Session; `Session::workingDirectory` (gecacht) + Poll-Timer (1,5 s) im SessionModel
>    (`WorkingDirRole`). Nur Shell (SSH/Seriell/Plugin haben kein sinnvolles lokales CWD).
> 3. **Attention-Tab pulsiert blau** — `needsAttention`-Tabs bekommen einen blau pulsierenden
>    Rahmen (Theme.accent). Rot bleibt dem MCP-Controller vorbehalten.
> 4. **Selektion scroll-fest + Smart Ctrl+C/V** — Maus-Selektion liegt jetzt in ABSOLUTEN
>    Inhalts-Zeilen (bleibt beim Scrollen am Text); Ctrl+C kopiert bei Auswahl, sonst SIGINT,
>    Ctrl+V fügt ein (Win/Linux; macOS Cmd). `absCellAt()` in TerminalItem.
> 5. **Tastenkürzel für alle Funktionen** — HotkeyRegistry erweitert: Session-Nav Ctrl+Tab/
>    Ctrl+Shift+Tab + Ctrl+1…9; SSH/Seriell/Verbindungen/Vault/MCP/Über (Ctrl+Shift+S/R/M/K/A, F1),
>    alle in den Einstellungen konfigurierbar. Vault bewusst NICHT Ctrl+Shift+V (Paste-Kollision).
> 6. **Version 0.9.0** (CMakeLists/main.cpp/MCP/Installer). **Build-Konvention:** immer auch
>    Release bauen — dedizierte Presets `windows-release`/`macos-release`/`linux-release`. Der
>    **Installer nutzt dasselbe `windows-release`-Preset** → nur noch 2 Win-Build-Verzeichnisse
>    (`build/windows` Debug, `build/windows-release` Release; `build/release-win` entfernt).
> **Offen (vom Anwender interaktiv zu testen):** das *visuelle* Drücken der neuen Kürzel und die
> Maus-Selektion-beim-Scrollen — in der Automatisierung wegen Windows-Foreground-Lock nicht
> auslösbar; Logik analytisch geprüft, Builds/Tests grün.
> Session 2026-06-12: **Plugin-System (QTMUX-8) erledigt** — SDK (`QTmuxPlugin.h`) +
> `PluginHost` (QPluginLoader) + Echo-Demo-Plugin + Session/QML-Integration inkl.
> Persistenz-Restore; 9 Test-Binaries grün, E2E auf macOS (Details im Feature-Eintrag).
> Bewusst zurückgestellt: PS-5.1-Mojibake (**Untersuchung
> abgeschlossen** — conhost/ConPTY doppelkodiert selbst, KEIN shell-seitiger Fix möglich,
> s. Build-&-Test-Windows-Abschnitt) und native macOS-Menü-Icons (großer QApplication/
> Widgets-Umbau, kosmetisch — eigener dedizierter Schritt). Jira: QTMUX-6 + QTMUX-25 am
> 2026-06-12 **dual auf Done gesetzt** (on-prem „Done"/Cloud „Erledigt", je mit Kommentar).
> Confluence-Entwicklerdoku am 2026-06-12 **dual nachgezogen** (on-prem v11, Cloud v10):
> GPU-Ligaturen + Verifikations-Lektion, Damage-Gating, Clink-AutoRun/erledigt, CWD-PEB,
> Mojibake-Abschluss, neue Abschnitte Vault→Profil + SFTP-Browser.
> Noch offen: Windows-Funktionstest von `Pty::currentWorkingDirectory()` (Restore ins
> letzte Verzeichnis).
> **Windows-Test-Session 2026-06-12** (Commits `f3dbc6b`…`aaf2fa5`, alles gepusht):
> 1. **`useGpu()`-Fix (`f3dbc6b`):** Commit `2d6c51b` hatte nur den Kommentar geändert, die
>    Bedingung blieb `m_gpu && !m_ligatures` → GPU-Ligatur-Code war toter Code (Details/Lektion
>    im QTMUX-6-Eintrag). Nach dem Fix **D3D11-verifiziert** (Cascadia Code); **Metal-E2E am
>    2026-06-12 auf dem Mac nachgeholt** (FiraCode: Ligaturen + bold + CJK, Log sauber) — die
>    ursprüngliche „Metal-Verifikation" war wegen des toten Codes unbemerkt der Fallback.
> 2. **QTMUX-25 Clink (`da68fe5`):** AutoRun-Dedup — bei Clink-Injektion via cmd-AutoRun wird
>    der redundante Shell-Eintrag ausgeblendet. **Windows-verifiziert → QTMUX-25 erledigt.**
> 3. **MCP-Fix (`82b47fe`):** `focusRequested` lädt die Session jetzt via `assignToActivePane`
>    ins sichtbare Pane (vorher nur Sidebar-Markierung — Terminal zeigte die alte Session).
> 4. **PS-5.1-Mojibake (`aaf2fa5`):** Untersuchung abgeschlossen — Beweis per roher UTF-8-Bytes
>    direkt aufs OS-Handle: conhost/ConPTY interpretiert Kind-Ausgabe als CP1252 und re-kodiert
>    nach UTF-8, BEVOR QTmux die Bytes sieht. Keine Shell-Einstellung wirkt (alles getestet);
>    echte Abhilfen: PowerShell 7 oder System-UTF-8-Option. Bleibt bekannte Einschränkung.
> Session 2026-06-11 (Fortsetzung): **Ligaturen im GPU-Pfad (QTMUX-6-Folgeoptimierung)**
> implementiert — Glyph-Index-Atlas (`GlyphAtlas::glyphByIndex`) + `QTextLayout`-Run-Shaping
> (Verifikation: s. Windows-Session oben — erst dort wurde der Pfad real aktiv). Zusätzlich
> **`Pty::currentWorkingDirectory()` für Windows** per PEB-Abfrage implementiert (Funktionstest
> auf Windows noch offen; CI-Build grün). Beides committet/gepusht.
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
- ✅ **Clink für cmd.exe (QTMUX-25) — Windows-verifiziert 2026-06-12.** Bietet auf
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
  - **AutoRun-Dedup** (`da68fe5`): ist Clink per cmd.exe-**AutoRun** installiert (HKCU/HKLM
    `…\Command Processor\AutoRun` verweist auf clink), lädt JEDE cmd.exe Clink ohnehin — das
    zusätzliche `clink inject` der eigenen Shell meldete nur „Clink already loaded in process N".
    `clinkAutoRunActive()` (QSettings NativeFormat, Gui-frei) erkennt das und blendet den
    redundanten Eintrag aus; ohne AutoRun erscheint er weiter.
  - i18n DE/EN ergänzt (Kontext „Shells"). **Windows-verifiziert 2026-06-12:** Dropdown ohne
    Clink-Eintrag bei aktivem AutoRun; normale Eingabeaufforderung lädt Clink (Banner) fehlerfrei.
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
    Damit ist `useGpu()` = `m_gpu` (Ligaturen erzwingen NICHT mehr den Fallback).
    **⚠️ Verifikations-Lektion:** Der ursprüngliche Commit (`2d6c51b`) änderte an `useGpu()` nur
    den *Kommentar*, die Bedingung blieb `m_gpu && !m_ligatures` → der neue GPU-Ligatur-Code war
    **toter Code**, und die erste „Metal-E2E" lief unbemerkt über den QPainter-Fallback (der
    Ligaturen ebenfalls formt — darum sah alles korrekt aus). Auf Windows gefunden + gefixt
    (`f3dbc6b`). Merksatz: *bei Renderpfad-Tests immer beweisen, dass der erwartete Pfad aktiv
    ist* (z. B. Fallback absichtlich brechen oder loggen), nicht nur das Endbild ansehen.
    Danach echt verifiziert auf **Windows/D3D11** (Cascadia Code: `-> ⇒ ≠ ≥ ≤ ≡`) und
    **macOS/Metal** (FiraCode, 2026-06-12): Ligaturen + **bold** + **CJK-Doppelbreite
    (日本語/中文) + Ligatur in einer Zeile** korrekt, rastertreu, ASCII scharf, keine Asserts.
    `gpuRendering=false` bleibt der QPainter-Fallback (rendert Ligaturen via Run-Shaping).
  - **Eigenes `QSGMaterial`/`QSGMaterialShader`** (file-lokal in `TerminalItem.cpp`) +
    **RHI-Shader** `src/terminal/shaders/glyph.{vert,frag}` (`#version 440`), via
    **`qt_add_shaders`** (BATCHABLE) zu `.qsb` kompiliert und unter `:/shaders/` eingebettet
    (`find_package(... ShaderTools)`). Der Fragment-Shader multipliziert für **Mono-Glyphen**
    die **Atlas-Deckung (Alpha) mit der Per-Vertex-Vordergrundfarbe** → **ein Atlas färbt
    beliebige fg-Farben**. Custom-Vertex-Layout (pos `vec2` + tex `vec2` + color `ubyte4`
    normalisiert).
  - **Mehrfarbige Emojis (Farb-Glyphen, Fix 2026-06-13):** Der Atlas wird via `QPainter`
    gerastert, der **Farb-Emojis (Apple Color Emoji) bereits in Farbe** zeichnet — die echten
    RGBA-Pixel liegen also im Atlas. Der alte Shader verwarf jedoch die Textur-RGB und nutzte
    nur den Alpha-Kanal × fg → Emojis erschienen als **einfarbige Blobs** (im GPU-Pfad; der
    QPainter-Fallback war immer korrekt). Fix: `GlyphAtlas::tileHasColor()` erkennt nach dem
    Rastern eine Farb-Glyphe (irgendein Pixel mit R≠G≠B; Mono ist premultipliziert grau) und
    setzt `Entry/IndexedEntry::color`. Der Renderer kodiert das in die **Vertex-Alpha als
    Typ-Selektor** (255 = Mono → fg tönen, 0 = Farbe → Atlas-RGB direkt; Alpha war vorher
    ungenutzt, Opazität kommt aus `qt_Opacity`). Shader: `mix(tex, vec4(fg*tex.a, tex.a),
    color.a)`. Verifiziert (Metal, beide Pfade): 😀🎉🔴🚀🟢🟦 mehrfarbig, ASCII/ANSI-Farben +
    Ligaturen unverändert getönt.
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
- ✅ **Phase 5 — Plugin-System (QTMUX-8) + MacPCAN-Plugin (QTMUX-9) FERTIG.** MacPCAN
  ist das erste echte Plugin (CAN-Bus als Terminal-Backend, gegen echte PCAN-USB-Hardware
  verifiziert) — Details im Status-Block oben (Session 2026-07-11), Code unter
  [plugins/macpcan/](plugins/macpcan/).
  Plugin-Host + SDK; ein Plugin liefert **AppBackends** (neue Session-Typen), die wie
  Shell/SSH/Seriell laufen — Sidebar/Rendering/Splits funktionieren automatisch (alles
  über `ITerminalBackend`). Komponenten:
  - **SDK** ([src/plugins/QTmuxPlugin.h](src/plugins/QTmuxPlugin.h)): `PluginInterface`
    (id/name/backendTypes/createBackend) + `PluginBackendType` + IID
    `com.qtmux.PluginInterface/1.0` (`Q_DECLARE_INTERFACE`; bei inkompatiblen Änderungen
    hochzählen). Plugin = Qt-Plugin (`Q_PLUGIN_METADATA` + `Q_INTERFACES`), linkt
    `qtmux_core` statisch (liefert das Meta-Objekt von `ITerminalBackend`).
  - **PluginHost** ([src/plugins/PluginHost.{h,cpp}](src/plugins/PluginHost.cpp), Gui-frei,
    in `qtmux_core`): Singleton, `loadAll()` (idempotent; Duplikate über kanonischen
    Dateipfad + Plugin-ID erkannt), Suchpfade: `QTMUX_PLUGIN_DIR` (Env, Dev/Tests) →
    `<App>/plugins` → macOS `Contents/PlugIns` → `<AppData>/plugins`. Q_PROPERTYs
    `backendTypes`/`plugins` (QVariantList) für QML; `createBackend(pluginId, typeId)`.
    Context-Property **`Plugins`** in `main.cpp` (loadAll VOR dem QML-Laden → Menü +
    Restore kennen die Typen sofort).
  - **Demo-Plugin** ([plugins/echo/](plugins/echo/EchoPlugin.cpp)): Echo-Backend
    (spiegelt Eingaben, Strg+D beendet) — beweist die Kette und ist die Kopiervorlage
    für echte Plugins (MacPCAN). `qt_add_plugin` (KEIN `CLASS_NAME` mit Namespace —
    CMake lehnt `qtmux::…` ab; nur für statische Plugins relevant). Output nach
    `<build>/plugins`; auf macOS POST_BUILD zusätzlich ins Bundle `Contents/PlugIns`
    (Windows/Linux: `<build>/plugins` liegt neben der exe → Suchpfad 2 greift).
  - **Integration:** `SessionModel::createPluginSession(pluginId, typeId)` →
    `Session::Type::App` (war seit Phase 0 vorgesehen); Persistenz/Restore über
    `pluginId`/`pluginType` in der Session-Config — fehlt das Plugin beim Restore,
    wird der Eintrag still übersprungen (createPluginSession → -1). QML: je Plugin-Typ
    ein Eintrag „<Name> (Plugin)" im „+"-Dropdown (erzeugt sofort, kein Default-Typ).
  - Tests: `tst_plugins` lädt das ECHTE Demo-Plugin (Test-Env `QTMUX_PLUGIN_DIR` →
    `<build>/plugins`, `add_dependencies` aufs Plugin): Laden/Typen/Idempotenz,
    Backend-I/O-Roundtrip, unbekannte IDs → nullptr. **9 Test-Binaries grün.**
    E2E auf macOS verifiziert: Dropdown zeigt „Echo (Demo) (Plugin)" (aus dem Bundle
    geladen), Session im Pane mit Banner, Echo der Eingabe, Sidebar-Titel/Typ korrekt
    (MCP-geprüft), Strg+D → Auto-Remove, **Persistenz-Roundtrip über App-Neustart**
    (Echo-Session restauriert), MCP `focus_session` lädt sie ins Pane. i18n DE/EN.
  - **Scope v1 / offen:** Backend-Provider-Plugins; kein UI-Beitrag/keine Parameter-
    Dialoge (createBackend bekommt eine leere `params`-Map — Erweiterungspunkt), kein
    MCP `create_session type=plugin`, kein Plugin-Manager-UI. **MacPCAN als erstes
    echtes Plugin** ist der nächste Phase-5-Schritt (braucht die MacPCAN-Codebase).
- 🟡 **Phase 6** — Politur & Distribution: **Installer für alle 3 Plattformen fertig**
  — DMG (`build-dmg.sh`), MSI/ZIP (`build-msi.ps1`), **AppImage (`build-appimage.sh`,
  QTMUX-10, CI-verifiziert)** — + CI-Matrix (QTMUX-11). **Offen:** Signierung/Notarisierung
  (macOS Developer-ID/Notarisierung, Windows Authenticode) und optional CPack-Distro-Pakete
  (.deb/.rpm). Bewusst hand-gerollte Skripte statt CPack (Plattform-Feinheiten).

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
