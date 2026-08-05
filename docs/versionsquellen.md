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
| `installer/build-msi.ps1` | `[string]$Version = "1.2.0"` ⚠️ s. u. |
| `.github/workflows/ci.yml` | `bash installer/build-appimage.sh 1.8.0` |
| `installer/QTmux.wxs` | nur im **Kommentar** (Beispielaufruf); der echte Wert kommt über `-d Version=` |

## ⚠️ Der Befund beim Nachmessen: ein Default, der sechs Minor-Versionen hinterherhinkt

`installer/build-msi.ps1` hat als Vorgabe **`1.2.0`**, während das Projekt bei `1.8.0` steht.
Wer das Skript **ohne** `-Version` aufruft, baut ein MSI mit `ProductVersion 1.2.0` — und
weder der Build noch ein Test wird davon rot. Genau die Fehlerklasse, die diese ganze
Anforderung ausgelöst hat, und in QTmux schon zweimal zugeschlagen (in der `CLAUDE.md`
unter „Fehler, die wie ein normaler Lauf aussehen").

**Nicht behoben, bewusst:** Der richtige Fix ist nicht „Default auf 1.8.0", sondern *gar
kein Default* (Pflichtparameter kann nicht veralten) bzw. der Wert aus der einen Quelle.
Beides gehört in den Baustein, den MacPCAN liefert — und ich kann es hier nicht testen, das
Skript läuft nur auf der Windows-Maschine.

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

## Was QTmux beim Anschluss noch zu tun hat

- **Fenstertitel:** heute statisch `title: "QTmux"` in [qml/Main.qml](../qml/Main.qml) Zeile 14.
  Die Build-ID gehört dort hinein, nicht nur in den About-Dialog.
- **MCP `get_server_info`** sollte die volle Build-ID melden — ein Agent stellt dieselbe
  Frage wie der Mensch.
- ⚠️ **Update-Vergleich getrennt halten:** `UpdateViewModel::currentVersion` speist den
  Versionsvergleich gegen das Manifest. Dort darf **nur `1.8.0`** ankommen, niemals
  `1.8.0+8b48a04` — sonst schlägt der Vergleich fehl oder bietet dauerhaft ein „Update" an.
  Die Build-ID ist eine **Anzeige**, keine Vergleichsgröße. Das ist die Falle, die QTmux als
  Update-Konsument als Erster treffen würde.
- **Signierte Artefakte:** Der Hash im Binary ändert dessen SHA-256. Beim Veröffentlichen
  also erst bauen, dann Manifest signieren — nie umgekehrt.
