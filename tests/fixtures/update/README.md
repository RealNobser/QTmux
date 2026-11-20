# Update-Fixture (QTMUX-125, seit K2 für `tst_updatequit`)

Ein Miniatur-Webspace für den Selbsttest am echten Beenden-Weg
(`qtmux --selftest-update <ordner> --update-base-url file://…/good`). Der Baum trägt
ein `qtmux/`-Unterverzeichnis, weil der Kern an die Basis-URL fest
`/<produkt>/manifest.json` anhängt:

| Baum | Version | Zweck |
|---|---|---|
| `good/` | 9.9.9 | Neuer als jede echte Version → Update verfügbar, Download, SHA-256, „Bereit“ |

Die Manifest-Notizen nennen noch den früheren Test `tst_updateviewmodel`: Sie stehen
unter der Signatur und lassen sich nur mit einer neuen Signatur ändern.

Bis K2 lagen hier auch `old/`, `sigfail/` und `shamismatch/` für den eigenen
Update-Automaten von QTmux. Diese Fälle prüft seither der gemeinsame Ablauf im Hub
mit eigenen Suiten (`tst_updater` erzeugt seine Bäume zur Laufzeit).

## ⚠️ Signiert mit dem PRODUKTIONS-Schlüssel — und das ist Absicht

Die App prüft mit dem in `third_party/updater/update/UpdateKeys.hpp` eingebauten
Schlüssel; ein Wegwerf-Schlüssel wäre im Selbsttest gar nicht einspeisbar. Geprüft
wird gerade, dass die **ausgelieferte** App ein **echt signiertes** Manifest annimmt.

Folge: Wird der Signierschlüssel gewechselt, wird `tst_updatequit` **rot** (der
Ablauf erreicht „Bereit“ nie). Das ist der gewollte Alarm: Die vendierte
`UpdateKeys.hpp` wäre dann veraltet und QTmux könnte kein echtes Update mehr
prüfen. Reihenfolge der Behebung: Hub-Kern aktualisieren →
`tools/check-updater-sync.sh --update` → Fixture hier neu signieren.

## Neu erzeugen

Die Artefakte sind absichtlich winzig (eine Textzeile) — geprüft wird der
SHA-256-Abgleich, nicht der Durchsatz. Größe und Hash im Manifest müssen zu den
Dateien unter `<version>/` passen.

```sh
openssl pkeyutl -sign -rawin -inkey <privater Update-Schlüssel> \
    -in tests/fixtures/update/good/qtmux/manifest.json \
    -out tests/fixtures/update/good/qtmux/manifest.json.sig
```

Die URLs im Manifest sind **relativ** (`<version>/<datei>`) — der Kern löst sie
gegen `<base>/<produkt>/` auf. Nur dadurch ist der Baum verschiebbar; die absoluten
URLs aus `publish.py --local-out` würden ihn an eine Maschine binden.

Der private Schlüssel gehört **nie** in ein Repository.
