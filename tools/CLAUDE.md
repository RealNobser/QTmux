# tools/ — Werkzeuge

> Schicht-Datei: Claude Code lädt sie, wenn in `tools/` gearbeitet wird. Wer unter Windows baut
> oder den Vendoring-Wächter liest, liest sie vorher auch von anderswo. Die Absätze stammen
> wörtlich aus der Wurzel-`CLAUDE.md` (ausgelagert 2026-11-19).

## Windows-Bauumgebung

Fortsetzung des Windows-Abschnitts der Wurzel-`CLAUDE.md` (dort die Kommandos, die laufende
Instanz als Linker-Sperre und der Qt-Pfad des `windows`-Presets):

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
>
> Der Qt-Pfad des `windows`-Presets (6.10.3 vor 6.11.1) steht in der Wurzel-`CLAUDE.md`.
>

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

## Vendoring-Wächter

- 🔑 **Vendoring-Wächter [tools/check-updater-sync.sh](tools/check-updater-sync.sh)** —
  prüft **vier** Kontrakte (seit 2026-08-12 Updater-Kern `third_party/updater/update/`,
  CAN-Plugin-Auswahl `plugins/macpcan/vendor/`, msiexec-Smoke `installer/…smoke…`; seit
  2026-10-07 die macOS-Signatur `installer/macos/sign-bundle.sh`) jeweils
  auf Byte-Identität zum Hub **und** auf Fremd-`#include`s, die den Kontrakt-Umfang
  verlassen.
  ⚠️ **Warum die Include-Prüfung nötig wurde:** Der Datei-Abgleich allein bemerkt eine neue
  Abhängigkeit nach außen **nicht** — er meldet nur „ABWEICHUNG", man zieht nach, und der
  Compiler sagt dann „file not found", ohne dass jemand den Kontrakt als Ursache erkennt.
  Den Wirkbereich (lokales Werkzeug, bewusst nicht in der CI; Aufruf aus
  `installer/build-dmg.sh`) führt die Wurzel-`CLAUDE.md`, Abschnitt „Konventionen“.
  🔴 **Bis QTMUX-144 (2026-10-10) war der Wächter selbst ein grüner Nachweis ohne Messung:**
  Mit **relativem** `MACPCAN_DIR` scheiterte ein `cd` in der Dateilisten-Gruppe
  `{ cd vendiert && find; cd upstream && find; }` — das zweite `cd` löste den Pfad gegen das
  schon gewechselte Verzeichnis auf, der Fehler ging im `;` unter. Die Hub-Hälfte der Liste
  fehlte, eine im Hub **neue** Datei blieb unsichtbar (auch für `--update`), Ausgabe
  „byte-identisch", Exit 0. Seitdem gilt **Exit 0 = alle vier Kontrakte gemessen · 1 = Drift/
  Fremd-Include · 2 = Werkzeugfehler** (gesetzter, aber unbrauchbarer `MACPCAN_DIR`, `cd`/
  `find`/`cmp`/`grep`/`cp` gescheitert, unbekanntes Argument wie `--updte`, **0 verglichene
  Dateien** in einem Kontrakt — die Positivkontrolle; die Erfolgszeilen nennen die Zahl, am
  2026-10-10 17/8/5/1). Ein relativer `MACPCAN_DIR` gilt gegen das **Aufrufverzeichnis**
  (Unix-Konvention; so las ihn die Existenzprüfung schon immer) und wird sofort absolut;
  `CDPATH` wird verworfen. Verglichen wird per `cmp` (Exit 2 = Fehler) statt zweier Hashes,
  deren leere Werte bei einer unlesbaren Datei „gleich" waren. Selbsttest
  `test_check_updater_sync` (Attrappen-Bäume, kein Hub nötig → läuft in der CI auf macOS/Linux);
  gegen den alten Code fallen 11 von 18 Fällen, darunter genau „relativ + neue Datei",
  „Pfad existiert nicht", „leerer Vendor-Baum".
  **Zum Hub-Paket AP8** (Profillader `DbcLoader`/`JsonLoader` wandern in den Hub):
  **QTmux ist nicht betroffen** — MacPCANs `DbcDecoder` liegt in `src/specs/`, RAFTNGs Lader
  in `src/io/`, beide außerhalb des Kontrakts; QTmux liest keine DBC-Profile. Nachziehen
  wäre nur nötig, wenn der Update-Kern selbst eine Abhängigkeit darauf bekäme — genau das
  meldet der Wächter. Reihenfolge bleibt MacPCAN → RAFTNG → QTmux.
