# Update-Weg: Fünf-Minuten-Regressionsliste

> **Wozu:** QTmux ist das einzige Produkt der Familie, dessen **voller** Update-Zyklus schon
> am lebenden Objekt lief — damit ist es die Referenz dafür, dass die Kette Ende zu Ende
> trägt. Diese Liste belegt an einem **1.9.0**-Build, dass Manifest-Abruf, Signaturprüfung,
> Versionsvergleich und der „ist aktuell"-Fall unverändert funktionieren.
>
> **Anlass:** MacPCAN baut die Proxy-Unterstützung in `appupdate` ein. Sie wirkt auf
> **beide** Netzwege — Manifest-Abruf **und** Artefakt-Download. QTmux ist der Konsument,
> bei dem eine Regression am schnellsten auffällt; die Liste ist der schnelle Gegencheck
> nach jedem Re-Vendoring (`tools/check-updater-sync.sh`).
>
> 🔑 **Der Server ist scharf.** „QTmux 1.9.0 ist aktuell" ist damit der **echte** Fall und
> kein Platzhalter: Manifest wurde geholt, Signatur geprüft, Version verglichen. Eine
> Fehlermeldung an dieser Stelle ist ein Befund, kein Normalzustand.
>
> Alle Sollwerte unten sind am **2026-08-10** gemessen, nicht geschätzt.

## A · Ohne GUI, ohne Build — die Kette in 60 Sekunden (Werkzeugweg)

Das ist der Teil, der auch auf einer Maschine ohne QTmux läuft und der bei einer
Proxy-Regression zuerst kippt. In einer Proxy-Umgebung dieselben Befehle **mit**
`https_proxy=…` wiederholen — die Sollwerte ändern sich dadurch nicht.

```bash
QTMUX_REPO="$PWD"            # aus dem Repo-Wurzelverzeichnis starten — Schritt 2 braucht es
cd "$(mktemp -d)"
curl -sS -o manifest.json     -w "manifest HTTP %{http_code} %{size_download}B\n" https://nobser.de/updates/qtmux/manifest.json
curl -sS -o manifest.json.sig -w "sig      HTTP %{http_code} %{size_download}B\n" https://nobser.de/updates/qtmux/manifest.json.sig
```

| Prüfpunkt | Sollwert (2026-08-10) |
|---|---|
| Manifest | HTTP **200**, **1582 B** |
| Signatur | HTTP **200**, **genau 64 B** (rohe Ed25519-Signatur, keine Base64-Hülle) |
| `product` / `version` / `published` | `qtmux` / **1.9.0** / `2026-08-10` |
| Artefaktschlüssel | `macos-universal`, `win-x86_64`, `linux-x86_64` — **alle drei** |
| Cache-Bust | `…/manifest.json?ts=<epoch>` ⇒ **200**. Der Kern hängt das an **http(s)** an, weil `manifest.json` die einzige veränderliche Datei auf dem Webspace ist. ⚠️ Ein Proxy, der die Abfrage verschluckt oder die URL normalisiert, liefert ein **altes** Manifest aus — die Prüfung meldet dann „aktuell", obwohl es das nicht ist. Genau hier fällt eine Proxy-Regression zuerst auf |
| Artefakt erreichbar (ohne 60 MB zu laden) | `curl -I …/1.9.0/QTmux-1.9.0-macos.dmg` ⇒ **200**, `content-length: 60814940`, `accept-ranges: bytes` |

**Signatur gegen den Produktionsschlüssel** — der Schlüssel steht im vendierten Kern
([UpdateKeys.hpp](../third_party/updater/update/UpdateKeys.hpp), 32 rohe Bytes); für
`openssl` bekommt er den 12-Byte-SubjectPublicKeyInfo-Vorsatz `302a300506032b6570032100`:

```bash
QTMUX_REPO="$QTMUX_REPO" python3 - <<'EOF'
import re, base64, os, pathlib
src = pathlib.Path(os.environ['QTMUX_REPO'], 'third_party/updater/update/UpdateKeys.hpp').read_text()
key = bytes(int(n, 16) for n in re.findall(r'0x([0-9A-Fa-f]{2})', src)[:32])
assert len(key) == 32, f'Public Key ist {len(key)} Byte statt 32 — Datei geaendert?'
der = bytes.fromhex('302a300506032b6570032100') + key
pathlib.Path('pub.pem').write_text('-----BEGIN PUBLIC KEY-----\n'
    + base64.b64encode(der).decode() + '\n-----END PUBLIC KEY-----\n')
EOF
openssl pkeyutl -verify -pubin -inkey pub.pem -rawin -in manifest.json -sigfile manifest.json.sig
```

⇒ **`Signature Verified Successfully`**

🔑 **Ohne den Gegentest belegt das nichts.** Ein gekipptes Bit **muss** die Prüfung brechen —
sonst misst man nur, dass irgendetwas „ok" sagt:

```bash
python3 -c "b=bytearray(open('manifest.json','rb').read()); b[100]^=1; open('kaputt.json','wb').write(bytes(b))"
openssl pkeyutl -verify -pubin -inkey pub.pem -rawin -in kaputt.json -sigfile manifest.json.sig
```

⇒ **`Signature Verification Failure`** (erwartet). Damit ist belegt, dass die Signatur über
die **exakten** Manifest-Bytes steht — und genau deshalb ist jede Byte-Veränderung auf dem
Transportweg tödlich. Siehe die `.gitattributes`-Lektion (`-text`): CRLF-Umschreibung machte
aus 935 Byte 966 und riss sechs Tests mit.

## B · Am 1.9.0-Build durchklicken (drei Minuten)

Voraussetzung: **frische, isolierte** Instanz — nicht die Produktivinstanz.

```bash
QTMUX_PROFILE=updatecheck QTMUX_MCP_PORT=7346 ./build/macos-release/qtmux.app/Contents/MacOS/qtmux
```

1. **Hilfe → „Nach Updates suchen …"** ⇒ Dialog meldet **„QTmux 1.9.0 ist aktuell."**
   Kein Fehlertext, kein Hänger, kein Fortschrittsbalken. Das ist der Beleg für
   Abruf + Signatur + Versionsvergleich in einem Schritt.
   ⚠️ Der Knopf muss **beim ersten Klick** wirken. Tut der erste Knopf im geöffneten Dialog
   nichts, ist das der `busy()`-Fehler (MacPCAN `59a9e35`): Der Kern hielt seine Reply bis
   zur nächsten Event-Loop-Runde, meldete also „beschäftigt", während der Aufrufer schon
   „fertig" hörte. Trat **nur über HTTP** auf und sah zwei Läufe lang wie ein Flake aus.
2. **Downgrade wird nicht angeboten.** Der Dialog darf von sich aus keine ältere Version
   vorschlagen — nur auf ausdrückliche Anforderung und mit Warnung.
3. **Einstellungen → Allgemein → „Aktualisierung"**: Schalter **aus**, QTmux neu starten
   ⇒ beim Start passiert **nichts** (kein Netzverkehr, kein Dialog).
4. **Zeitstempel-Drosselung:** Schalter wieder an, zweimal hintereinander starten
   ⇒ der stille Check läuft **höchstens einmal am Tag**. `update/lastCheck` wird **auch nach
   einem Fehlschlag** geschrieben — sonst wird aus „1×/Tag" bei unerreichbarem Server
   „bei jedem Start".
5. **Fehler bleiben still.** Netz trennen, QTmux starten ⇒ **kein** Fehlerdialog. Ein
   manueller Check danach **darf** den Fehler zeigen — das ist der Unterschied zwischen
   stillem und angefordertem Weg.

Danach aufräumen: Prozess beenden, `defaults delete com.qtmux.QTmux-updatecheck`.

## C · Was diese Liste NICHT belegt

Ehrlich halten, sonst wiegt sie in Sicherheit:

- **Der Installationsschritt** wird hier nie ausgelöst. Der volle Zyklus ist bisher **nur
  auf macOS** am lebenden Objekt gefahren; `msiexec /i` (Windows) und der
  **AppImage-Selbsttausch** (Linux) sind bloß als *Start-Plan* geprüft.
- **`file://`-Fixtures ersetzen den Transportweg nicht.** Zwei Dinge gehen daran vorbei:
  die Cache-Bust-Abfrage (nur an http(s) angehängt) und ein in Häppchen ankommender
  Download, der überhaupt erst Fortschritt meldet. Deshalb hat `tst_updateviewmodel` einen
  eigenen In-Process-HTTP-Server — dort ist der `busy()`-Fehler aufgeschlagen.
- **Ein sporadischer Fehlschlag auf genau EINEM Transportweg ist kein Rauschen, sondern ein
  Timing-Fehler.** Diese Regel hat den `busy()`-Fall aufgeklärt.
- **Unit-Tests decken die Krypto ab, nicht die Kette:** `test_updater` (13 Fälle, Kern) und
  `test_updateviewmodel` (11 Fälle, App-Seite gegen mit dem **Produktionsschlüssel**
  signierte Fixtures). Letzteres ist zugleich der Interop-Nachweis — ein Schlüsselwechsel
  macht den Test rot, und das ist der gewollte Alarm.

## D · Nach dem Proxy-Re-Vendoring zusätzlich

Sobald MacPCAN die Proxy-Konfiguration geliefert hat und sie hier vendiert ist
(Anforderungen: [workorder-online-update.md](workorder-online-update.md), Abschnitt
„Zuarbeit an MacPCAN"):

1. `tools/check-updater-sync.sh` ⇒ **byte-identisch**, und `UPSTREAM.md`-Pin nachgezogen.
   ⚠️ Das Skript nimmt neue Dateien selbst mit — **`CMakeLists.txt` nicht**: dort ist jede
   Updater-Quelldatei einzeln gelistet. Eine neue `.cpp` wäre sync-grün und trotzdem nicht
   übersetzt.
2. Abschnitt A **ohne** Proxy wiederholen ⇒ unverändert. Das ist der eigentliche Punkt:
   Proxy-Unterstützung darf den proxylosen Fall nicht anfassen.
3. Abschnitt A **mit** Proxy (`https_proxy=…`) ⇒ dieselben Sollwerte, **inklusive** der
   `?ts=`-Zeile und des `accept-ranges`-Kopfes am Artefakt.
4. Abschnitt B Punkt 1 ⇒ weiterhin „ist aktuell" beim **ersten** Klick.
5. **Genau ein** Anmeldeversuch bei Proxy-Auth — wiederholtes Anmelden sperrt in
   AD-Umgebungen das Domänen-Konto (dieselbe Lektion wie beim SSH-Passwort-Auto-Fill).
