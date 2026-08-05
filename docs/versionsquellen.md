# Versionsquellen in QTmux — Vorlage für die anderen fünf Repos

> Zuarbeit zur workspace-weiten Build-ID `<version>+<git-short-hash>[-dirty]` (Owner,
> 2026-08-05). **Kein Baustein-Bau:** Den CMake-Teil liefert MacPCAN, QTmux übernimmt ihn
> per Vendoring wie `appupdate`. Hier steht nur, was QTmux beim eigenen Versionsthema
> gelernt hat — damit die anderen fünf dieselbe Erkenntnis nicht noch einmal bezahlen.
>
> Alles unten ist am 2026-08-05 **gemessen** (`grep` über das Repo), nicht erinnert.

## Der Stand: eine Quelle, drei abgeleitete Stellen, fünf manuelle

**Kanonische Quelle:** `project(QTmux VERSION 1.8.0)` in [CMakeLists.txt](../CMakeLists.txt) Zeile 4.

**Automatisch abgeleitet** über `configure_file` → `cmake/Version.h.in` → generiertes
`qtmux_version.h` (`QTMUX_VERSION_STRING`, `_MAJOR`, `_MINOR`, `_PATCH`):

| Stelle | wozu |
|---|---|
| [src/app/main.cpp](../src/app/main.cpp) `setApplicationVersion` | Qt-Anwendungsversion |
| [src/server/McpServer.cpp](../src/server/McpServer.cpp) (2×) | MCP `get_server_info` — **daran liest ein Agent die Version ab** |
| [src/viewmodels/UpdateViewModel.cpp](../src/viewmodels/UpdateViewModel.cpp) `currentVersion` | Vergleich gegen das Update-Manifest |

🔑 **Warum genau diese drei zuerst umgestellt wurden:** Sie zeigten den Wert **hart** und
liefen zwangsläufig auseinander. Die MCP-Antwort und der Update-Vergleich sind dabei die
teuersten: Ein hart kodierter Wert im `UpdateViewModel` hätte die App gegen ein Manifest
vergleichen lassen, das eine andere Version meint — der Update-Weg wäre still falsch
gewesen, ohne dass irgendetwas rot geworden wäre.

**Weiterhin manuell** — sie laufen ohne CMake-Konfiguration, deshalb erreicht sie
`PROJECT_VERSION` nicht:

| Datei | Vorkommen |
|---|---|
| [README.md](../README.md) | **8** — Badge, Statuszeile und je drei Installer-Tabellenzeilen, **zweisprachig** (DE + EN) |
| `installer/build-dmg.sh` | `VERSION="${1:-1.8.0}"` |
| `installer/build-appimage.sh` | `VERSION="${1:-1.8.0}"` |
| `installer/build-msi.ps1` | ~~`$Version = "1.2.0"`~~ → **Pflichtparameter** (2026-08-06 behoben) |
| `.github/workflows/ci.yml` | `bash installer/build-appimage.sh 1.8.0` |
| `installer/QTmux.wxs` | nur im **Kommentar** (Beispielaufruf); der echte Wert kommt über `-d Version=` |

## ✅ Der Befund beim Nachmessen — behoben am 2026-08-06

`installer/build-msi.ps1` hatte als Vorgabe **`1.2.0`**, sechs Minor-Versionen hinter dem
Projekt. Ohne `-Version` aufgerufen wäre daraus ein MSI mit `ProductVersion 1.2.0`
geworden — und weder Build noch Test wären davon rot. Genau die Fehlerklasse, die diese
Anforderung ausgelöst hat, und in QTmux schon zweimal zugeschlagen.

**Gelöst durch `[Parameter(Mandatory = $true)]`** plus `ValidatePattern` auf `x.y.z`, statt
den Wert auf 1.8.0 zu heben: **Ein Pflichtparameter kann nicht veralten.** Geprüft wurde,
dass kein Aufrufer davon bricht — `installer/build-release.ps1` liest die Version ohnehin
aus `CMakeLists.txt` und reicht sie durch; README und Wrapper übergeben sie ebenfalls.
⚠️ Auf Windows nicht ausgeführt (andere Maschine) — die Änderung ist eine Parameterzeile,
und der vorherige Zustand war eine Zeitbombe.

**Für die anderen Repos die Regel dahinter:** Ein Vorgabewert für eine Versionsnummer ist
eine Zeitbombe. Er ist genau so lange richtig, wie ihn jemand pflegt — und niemand merkt es,
wenn nicht.

## 🔑 Die wichtigste Erkenntnis für den Git-Hash

**`configure_file` reicht für `PROJECT_VERSION`, aber NICHT für den Hash.**

`configure_file` läuft zur **Configure**-Zeit. Für die Versionsnummer ist das richtig: Sie
ändert sich nur beim Bump, und der ändert `CMakeLists.txt` → CMake konfiguriert ohnehin neu.

Ein **Commit ändert keine CMake-Datei**. Es läuft also kein Re-Configure, und ein per
`configure_file` eingebetteter Hash bleibt auf dem Stand stehen, den er beim letzten
Konfigurieren hatte. Das Ergebnis wäre **schlimmer als gar keine Angabe**: Die Anwendung
behauptet dann einen Stand, der nicht ihrer ist — und genau das soll die Build-ID ja
verhindern.

**Der Hash muss zur BUILD-Zeit ermittelt werden**, also über ein Target, das bei jedem Build
läuft (`add_custom_target` / `add_custom_command`). Vier Dinge, die dabei sonst weh tun:

1. **Die Header-Datei nur schreiben, wenn sich der Inhalt ändert** (erst in eine temporäre
   Datei, dann `copy_if_different`). Sonst hat die Datei nach jedem Build einen neuen
   Zeitstempel und zieht einen kompletten Rebuild nach sich.
2. **`-dirty` nur aus *verfolgten* Dateien** ableiten (`git status --porcelain
   --untracked-files=no` bzw. `git describe --dirty`). Sonst macht jede herumliegende
   unversionierte Datei jeden Build „dirty" — in QTmux etwa das generierte `.qmlls.ini` oder
   ein `build/`-Verzeichnis im Baum —, und die Markierung verliert genau die Bedeutung, für
   die sie da ist.
3. **Ohne Git-Umgebung sauber auf `unknown` fallen, nie den Build brechen.** Das trifft
   Tarball-Auslieferungen und flache CI-Checkouts. (Steht so auch in der Orchestrator-Regel.)
4. **Ein Wächter-Test**, der die eingebettete Version gegen `PROJECT_VERSION` prüft. Ohne ihn
   merkt niemand, wenn die Ableitung bricht — das ist ja gerade der Fall, der still bleibt.

## Umgesetzt in QTmux (2026-08-06) — alle vier Punkte oben belegt

[cmake/BuildId.cmake](../cmake/BuildId.cmake) + [cmake/BuildId.h.in](../cmake/BuildId.h.in),
angestoßen von einem `add_custom_target(qtmux_buildid ALL …)`. ⚠️ **Vorläufig**: Der
kanonische Baustein kommt aus MacPCAN; getauscht wird dann nur die Quelle, nicht die Anzeige.

| Punkt | Beleg |
|---|---|
| Build-Zeit statt Configure-Zeit | eigenes Target, läuft bei jedem Build |
| kein Rebuild-Sturm | zweiter Build direkt danach: **nur** der Build-ID-Schritt, kein Compile |
| `-dirty` nur aus verfolgten Dateien | `git status --porcelain --untracked-files=no` |
| ohne Git → `unknown` | Fallback ohne Abbruch |

**Sichtbar an drei Stellen:**

- **Fenstertitel** ([qml/Main.qml](../qml/Main.qml)) — gemessen: `QTmux 1.8.0+54c9b81-dirty`
- **MCP `get_server_info`** — meldet `buildId` und `buildDirty` **neben** `version`
- `App.buildId` / `App.buildIsDirty` für die restliche Oberfläche

🔑 **Die Trennung ist am laufenden Programm belegt:** `get_server_info` liefert
`version: 1.8.0` (Vergleichsgröße, unverändert) **und** `buildId: 1.8.0+54c9b81-dirty`
(Anzeige). Käme die Build-ID in `UpdateViewModel::currentVersion`, schlüge der
Manifest-Vergleich fehl oder böte dauerhaft ein „Update" an.

⚠️ **Signierte Artefakte:** Der Hash im Binary ändert dessen SHA-256. Beim Veröffentlichen
also erst bauen, dann Manifest signieren — nie umgekehrt.
