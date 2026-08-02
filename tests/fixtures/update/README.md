# Update-Fixtures (QTMUX-125)

Drei Miniatur-Webspaces für `tst_updateviewmodel`. Jeder Baum trägt ein eigenes
`qtmux/`-Unterverzeichnis, weil der Kern an die Basis-URL fest
`/<produkt>/manifest.json` anhängt:

| Baum | Version | Zweck |
|---|---|---|
| `good/` | 9.9.9 | Neuer als jede echte Version → Update verfügbar, Download, SHA-256 |
| `old/` | 0.0.1 | Älter als jede echte Version → Downgrade-Warnung |
| `sigfail/` | 9.9.9 | Gültiges Manifest, Signatur über **andere** Bytes |

Der Test hängt `update/baseUrl` auf den jeweiligen Baum (`file://…`) — derselbe
Codepfad wie im Betrieb, nur ein anderes URL-Schema.

## ⚠️ Signiert mit dem PRODUKTIONS-Schlüssel — und das ist Absicht

`UpdateViewModel` baut seinen `UpdateChecker` **ohne** Schlüssel-Parameter, nimmt
also den in `third_party/updater/update/UpdateKeys.hpp` eingebauten. Ein
Wegwerf-Schlüssel wäre hier gar nicht einspeisbar — und wertlos: geprüft wird ja
gerade, dass die **ausgelieferte** App ein **echt signiertes** Manifest annimmt
(`openssl pkeyutl -sign -rawin` signiert, das vendierte Monocypher verifiziert).

Folge: Wird der Signierschlüssel des Owners gewechselt, wird dieser Test **rot**.
Das ist kein Wartungsärgernis, sondern der gewollte Alarm — die vendierte
`UpdateKeys.hpp` wäre dann veraltet und QTmux könnte kein echtes Update mehr
prüfen. Reihenfolge der Behebung: MacPCAN-Kern aktualisieren →
`tools/check-updater-sync.sh --update` → Fixtures hier neu signieren.

Die reine **Krypto**-Prüfung hängt dagegen nicht am Owner-Schlüssel: `tst_updater`
erzeugt seine Schlüssel zur Laufzeit und prüft zusätzlich einen
RFC-8032-Testvektor.

## Neu erzeugen

Die Artefakte sind absichtlich winzig (eine Textzeile) — geprüft wird der
SHA-256-Abgleich, nicht der Durchsatz. Größe und Hash im Manifest müssen zu den
Dateien unter `<version>/` passen, sonst schlägt der Download-Test zu Recht fehl.

```sh
K=~/.keys/updates_ed25519_private.pem
for t in good old; do
    openssl pkeyutl -sign -rawin -inkey "$K" \
        -in tests/fixtures/update/$t/qtmux/manifest.json \
        -out tests/fixtures/update/$t/qtmux/manifest.json.sig
done
# sigfail/ behaelt ein GUELTIGES Manifest mit einer Signatur ueber ANDERE Bytes:
printf 'nicht das Manifest' > /tmp/other.bin
openssl pkeyutl -sign -rawin -inkey "$K" -in /tmp/other.bin \
    -out tests/fixtures/update/sigfail/qtmux/manifest.json.sig
rm /tmp/other.bin
```

Die URLs in den Manifesten sind **relativ** (`<version>/<datei>`) — der Kern löst
sie gegen `<base>/<produkt>/` auf. Nur dadurch ist der Baum verschiebbar; die
absoluten URLs aus `publish.py --local-out` würden ihn an eine Maschine binden.

Der private Schlüssel liegt unter `~/.keys/updates_ed25519_private.pem` und gehört
**nie** in ein Repository.
