# Vendiert: `MacPCANUpdater` — der geteilte Desktop-Update-Kern (`appupdate`)

`update/` ist eine **byte-identische Kopie** von `MacPCAN/src/update/`. Kanonische
Quelle ist MacPCAN; QTmux teilt mit MacPCAN/RAFTNG **keine** Submodule, deshalb der
Vendoring-Weg — dasselbe Muster wie bei `third_party/libvterm`.

## Gepinnter Stand

| | |
|---|---|
| Repository | `MacPCAN` (Worker-Checkout `/Users/nobser/Projects/_ClaudeWorkspace/MacPCAN`) |
| Commit | `796575fbdd2961c6129e70f547e87e2e40b57023` |
| Datum | 2026-08-11 |
| Betreff | `fix(update): msiexec braucht native Pfadtrenner — Windows-Update war seit 0.2.0 tot` |

Mit diesem Stand kam der **msiexec-Pfadtrenner-Fix** herüber (eine Datei,
`InstallerLauncher.cpp`): Qt liefert auf Windows Vorwärts-Slashes, msiexec'
eigener Parser lehnt so einen Pfad als **Fehler 1619** ab („Dieses
Installationspaket konnte nicht geöffnet werden"), obwohl das MSI intakt ist.
Der msi-Zweig zieht den Download-Pfad jetzt per `QDir::toNativeSeparators()`
auf Backslashes — Argumente **und** description; dmg/appimage unverändert.
Am echten msiexec auf RTZBLD01 bewiesen (Slash→Exit 1619, nativ→Exit 0).
Windows-Selbst-Update war damit seit der ersten Auslieferung tot.

Mit dem vorigen Stand `58df9e4` (2026-08-07) kam die **Fehler-Klassifikation** herüber (MacPCAN `9952202`,
„404 ist kein Signaturfehler"): `UpdateChecker` bildet Transportfehler jetzt über
`classifyFailure()` auf das ab, was der Betreiber tun soll, und `ErrorKind` bekam
dafür **`NotPublished`**. Vorher las sich ein Produkt ohne Release als
„signature fetch failed: … 404" — und schickte den Leser auf die Suche nach einem
Signaturproblem, das es nicht gibt.
⚠️ **`NotPublished` steht MITTEN im Enum** (hinter `None`, vor `Network`) und
verschiebt damit alle folgenden Zahlenwerte. Für QTmux geprüft und **unschädlich**:
Der einzige Zahlen-Umweg ist `proxyErrorText(int)` in `UpdateViewModel` — die Zahl
entsteht und zerfällt zwischen zwei Zeilen desselben Übersetzungsvorgangs, die
Funktion ist **privat** (kein `Q_INVOKABLE`, kein QML-Zugriff), und kein
`ErrorKind` wird je persistiert oder über MCP gereicht. Wäre eines davon anders,
hätte dieser Sync stillschweigend Fehlertexte vertauscht.
📋 Offener Folgepunkt (klein, nicht dringend): `proxyErrorText` kennt
`NotPublished` nicht und fällt auf den Rohtext zurück — der ist englisch
(„no release published for this product yet"). Ein übersetzter eigener Satz wäre
die Kür; ein Rückschritt ist es nicht, vorher stand dort ebenfalls Englisch.

### 🔑 Wogegen ein Vendoring-Konsument misst — die Lektion vom 2026-08-07

Bei der Push-Runde am 2026-08-07 lautete die Ansage aus der Koordination: „Der Guard
liegt in `src/ota/`, die REST-Felder in der Control-Schicht — **keine Betroffenheit**
erwartet." Der Wächter schlug trotzdem an. Beides war richtig, weil beide Seiten gegen
**verschiedene Basen** maßen:

| Basis | `src/update/` | Antwort |
|---|---|---|
| Sprung der Push-Runde (`c487394..58df9e4`, 8 Commits) | unberührt | nicht betroffen |
| **Unser Pin** (`80c19ee..58df9e4`) | 2 Dateien | betroffen |

Der Drift kam aus `9952202` (2026-08-06), einem **Vorfahren von `c487394`** — er lag
längst auf origin. Nicht die Push-Runde hatte den Kontrakt berührt, **unser Pin hing
einen Tag hinterher**.

⚠️ **Daraus folgt die Regel:** Die Betroffenheit eines Konsumenten ergibt sich NIE aus
dem Sprung einer Push-Runde, sondern **ausschließlich aus dem Abstand zum eigenen Pin**.
Eine Ankündigung „diese Commits fassen euren Bereich nicht an" ist deshalb kein Beleg für
„nichts zu tun" — sie beantwortet eine andere Frage. Belastbar ist nur
`git diff --stat <unser-Pin>..<neuer-Stand> -- src/update/` bzw. der Wächter selbst.
🔑 Und der Umkehrschluss: Ein Wächter-Ausschlag ist **kein** Vorwurf an die Push-Runde.
Wer ihn so meldet (wie hier zunächst geschehen), schickt die anderen Konsumenten in den
falschen Commit-Bereich.

Mit dem Stand davor, `80c19ee` (2026-08-06), kam die **Proxy-Unterstützung** herüber (MAC-36, in MacPCAN als
`0934eff` eingeführt): neu `ProxyConfig.{hpp,cpp}`, dazu am `UpdateChecker`
`setProxyConfig()`, `setProxyCredentialProvider()` und das erweiterte
`ErrorKind`-Enum. Die QTmux-Hälfte dazu liegt in `UpdateViewModel` und
`src/core/ProxyCredentials.{h,cpp}` (QTMUX-129).
🔑 `ProxyConfig.cpp` landete **ohne CMake-Pflege** im Target — seit dem
GLOB-Umbau (`3e1af94`) wird `update/` gerastert statt aufgezählt. Genau dafür
war er gedacht.

Enthält den **Produktions-Public-Key** (`update/UpdateKeys.hpp`, Ed25519, 32 Byte,
Owner-Schlüssel vom 2026-08-02) — kein Platzhalter mehr.

## Umfang des Kontrakts — und was AP8 (Profillader in den Hub) daran ändert

QTmux vendiert **ausschließlich `MacPCAN/src/update/`**. Alles andere im Hub ist
nicht Teil der Abmachung.

**Zum Paket „Geräte-Updates zum sauberen Abschluss", AP8** (RAFTNGs `DbcLoader`/
`JsonLoader` werden kanonisch in den Hub gehoben, MacPCANs `DbcDecoder` geht
darin auf): Am 2026-08-06 geprüft — **QTmux ist davon nicht betroffen**.
MacPCANs `DbcDecoder` liegt in `src/specs/`, RAFTNGs Lader in `src/io/`; beide
liegen **außerhalb** von `src/update/`. QTmux flasht keine Geräte und liest keine
DBC-Profile, es braucht den Lader also auch fachlich nicht.

⚠️ **Der eine Weg, auf dem es uns doch träfe:** Wenn der Update-Kern selbst eine
Abhängigkeit auf den Lader bekäme (`#include "specs/…"`). Dann müsste QTmux
Dateien mitvendieren, die nicht im Kontrakt stehen. Genau dafür hat
`tools/check-updater-sync.sh` seit 2026-08-06 einen **Kontrakt-Wächter**: Er
meldet jeden Include, der aus `update/` herausführt, mit klarer Ansage — statt
den Fall dem Compiler und seinem „file not found" zu überlassen.
Heutiger Stand: Der Kern ist **in sich geschlossen**, alle Includes zeigen auf
`update/…` oder auf die mitvendierten Monocypher-Dateien.

🔑 **Reihenfolge beim Nachziehen:** MacPCAN zuerst, dann RAFTNG (Submodul), dann
QTmux (Vendoring). Ein Sync-Lauf hier ist erst sinnvoll, wenn MacPCAN gepusht
hat — und nur nötig, wenn sich `src/update/` überhaupt geändert hat.

## ⚠️ Einbahnstraße

Dateien unter `update/` werden **nie lokal editiert**. Jede Änderung gehört nach
`MacPCAN/src/update/` und kommt von dort zurück:

```bash
tools/check-updater-sync.sh            # SHA-256-Abgleich, Exit 1 bei Drift
tools/check-updater-sync.sh --update   # Änderungen von MacPCAN übernehmen
```

Danach den Commit-Block oben nachziehen. Ohne MacPCAN-Checkout beendet sich das
Skript mit Exit 0 und einer Meldung (Build-Maschinen und CI haben ihn nicht — ein
Test, der dort rot würde, meldete ein Umgebungsproblem als Regression).

QTmux-spezifisches liegt **außerhalb** dieses Verzeichnisses:
[`src/viewmodels/UpdateViewModel.{h,cpp}`](../../src/viewmodels/UpdateViewModel.h)
(QSettings-Persistenz, Q_PROPERTYs) und [`qml/dialogs/UpdateDialog.qml`](../../qml/dialogs/UpdateDialog.qml).

## Warum ein `update/`-Unterverzeichnis

Der Kern inkludiert sich selbst mit dem Präfix `update/`
(`#include "update/UpdateChecker.hpp"`) — der Include-Pfad ist bei MacPCAN `src/`.
Byte-identisch heißt: auch diese Zeilen bleiben unangetastet. Deshalb liegt die
Kopie in einem Verzeichnis **namens `update`**, und `qtmux_updater` exportiert
`third_party/updater` als Include-Pfad. Ein flaches Vendieren nach
`third_party/updater/*.hpp` würde jede `#include`-Zeile ändern und die
Byte-Identität — und damit den Sync-Wächter — aufgeben.

## Baustein

CMake-Target **`qtmux_updater`** (STATIC, `Qt6::Core` + `Qt6::Network`), definiert in
der Haupt-`CMakeLists.txt`. Bewusst **nicht** Teil von `qtmux_core`: der Kern bleibt
Qt6::Core-only (Projektregel, `CLAUDE.md`), `UpdateChecker` braucht aber
`QNetworkAccessManager`.

`update/ed25519/` ist **Monocypher 4.0.2** (C, dual BSD-2-Clause/CC0) — Herkunft und
die Falle „`crypto_ed25519_check` statt `crypto_eddsa_check`" stehen in
[`update/ed25519/README.md`](update/ed25519/README.md).
