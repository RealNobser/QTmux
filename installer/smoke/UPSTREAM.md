# Vendiert: msiexec-Pfad-Smoke (1619-Riegel)

Fünf Dateien sind **byte-identisch** aus MacPCAN übernommen — dasselbe
Einbahnstraßen-Muster wie `third_party/updater/` und `plugins/macpcan/vendor/`:

| QTmux | MacPCAN (kanonische Quelle) |
|---|---|
| `installer/msiexec-path-smoke.ps1` | `platform/windows/msiexec-path-smoke.ps1` |
| `installer/msi-shortcut-icon-check.ps1` | `platform/windows/msi-shortcut-icon-check.ps1` |
| `installer/smoke/MacPCAN-PathSmoke.msi` | `platform/windows/smoke/MacPCAN-PathSmoke.msi` |
| `installer/smoke/marker.txt` | `platform/windows/smoke/marker.txt` |
| `installer/smoke/msi-path-smoke.wxs` | `platform/windows/smoke/msi-path-smoke.wxs` |

Das Skript liegt bewusst **eine Ebene über** `smoke/` (in `installer/`, neben
`build-msi.ps1`): Sein Fixture-Default ist der Relativpfad
`smoke\MacPCAN-PathSmoke.msi` — nur mit dieser Struktur bleibt die Datei
byte-identisch übernehmbar. Der MacPCAN-Name der Fixture bleibt aus demselben
Grund stehen; sie ist **kein** QTmux-Installer, sondern das kleinste gültige
MSI, an dem `msiexec /a` messbar ist (RAFTNG ruft dieselben Dateien direkt aus
seinem MacPCAN-Submodul auf).

## Gepinnter Stand

| | |
|---|---|
| Repository | `MacPCAN` (Worker-Checkout `/Users/nobser/Projects/_ClaudeWorkspace/MacPCAN`) |
| Commit | `e53ec8462894a26a833fc795d99afd69d86196ce` |
| Datum | 2026-10-10 |

Abgleich 2026-10-10 (QTMUX-143): `git log 8242342..e53ec84 -- platform/windows/`
nennt genau `e53ec84` (MAC-74) — er bringt `msi-shortcut-icon-check.ps1` neu
hinzu (Verknüpfungs-Symbol-Riegel, aufgerufen in `build-msi.ps1`) und lässt die
vier bisherigen Dateien unberührt; Wächter „byte-identisch", die Datei ist in
seiner Liste ergänzt (fehlt sie oder weicht sie ab: Exit 1, beides geprobt).

Abgleich 2026-09-28 (Release 1.9.4): `git log c40742c..8242342 -- platform/windows/`
ist **leer**, Wächter „byte-identisch" — nur der Pin wandert.

Abgleich 2026-09-04 (Release 1.9.3): `git log fdefeda..c40742c -- platform/windows/`
über die vier Dateien der Liste ist **leer**, Wächter „byte-identisch" — nur
der Pin wandert.

## Wozu das Gate

Regressions-Riegel für den Windows-Update-Ausfall von 2026-08-11: Qt liefert
Pfade mit Vorwärts-Slashes, msiexec (eigener Kommandozeilen-Parser) antwortet
darauf mit 1619, obwohl das Paket intakt ist. Ein Unit-Test kann nur die
eigene Zeichenkette prüfen, nie msiexecs Parser — deshalb fragt das Gate das
echte Programm, in **beide** Richtungen: Vorwärts-Slash muss scheitern, der
native Pfad muss Exit 0 liefern **und Dateien entpacken** (positiver Anker).

Verdrahtet in [`build-msi.ps1`](../build-msi.ps1) gegen das frisch gebaute
QTmux-MSI; Fehlschlag bricht den Bau ab. **Bewusst nicht im GitHub-Actions-Job:**
im Dienstkontext des Runners antwortet msiexec in beiden Richtungen 1601
(gemessen 2026-08-11, Lauf 31485093468) — dort wäre das Gate blind grün. Es
gehört auf den RTZBLD01-Bauweg.

## ⚠️ Einbahnstraße

Diese Dateien werden **nie lokal editiert**. Änderungen gehören nach
`MacPCAN/platform/windows/` und kommen von dort zurück;
[`tools/check-updater-sync.sh`](../../tools/check-updater-sync.sh) wacht über
die Byte-Identität (dritter Kontrakt, explizite Dateiliste — diese
UPSTREAM.md selbst ist QTmux-eigen und steht nicht auf der Liste).
