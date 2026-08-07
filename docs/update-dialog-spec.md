# Update-Dialog — Design-Spezifikation

> **Wozu:** Der QTmux-Update-Dialog wird das Vorbild für alle drei Desktop-Apps
> (Owner, 2026-08-07). Übertragen wird das **Design**, nicht der Code: QTmux ist Qt Quick,
> RAFTNG und MacPCAN sind Widgets; der geteilte Widgets-Dialog entsteht danach im Hub.
> Dieses Dokument beschreibt den Dialog vollständig genug, dass ihn jemand nachbauen kann,
> der QTmux nie gesehen hat.
>
> **Stand:** QTmux 1.8.1 (`3744c38`). Quellen:
> [qml/dialogs/UpdateDialog.qml](../qml/dialogs/UpdateDialog.qml) ·
> [src/viewmodels/UpdateViewModel.h](../src/viewmodels/UpdateViewModel.h) ·
> [qml/Ui/AppDialog.qml](../qml/Ui/AppDialog.qml). Die Texte unten sind aus
> `i18n/qtmux_{de,en}.ts` gezogen, nicht abgeschrieben.
>
> ⚠️ **Der Dialog wird für diese Spezifikation NICHT geändert.** Er ist Teil der laufenden
> Auslieferung; die Spec beschreibt den Ist-Zustand. Wo der Ist-Zustand fragwürdig ist,
> steht das unter „Bekannte Schwächen" — nicht als stille Korrektur im Text.

---

## 1 · Was dieser Dialog ist

**Ein** Dialog für den ganzen Vorgang — Prüfen, Anmerkungen lesen, Herunterladen,
Installieren. Kein Assistent mit Seiten, kein Dialog je Schritt. Begründung steht in
Abschnitt 9; sie ist die wichtigste Einzelentscheidung des Entwurfs.

**Modal**, zentriert im Hauptfenster, feste Breite **540 px**, Höhe wächst mit dem Inhalt.

### Einstiegspunkte

| Weg | Auslöser | Besonderheit |
|---|---|---|
| Menü **Hilfe → „Nach Updates suchen …"** | `actCheckUpdates` | ignoriert Tagesdrossel **und** übersprungene Version |
| Befehlspalette, Eintrag „Nach Updates suchen …" | dieselbe Aktion | — |
| Einstellungen → Allgemein → Aktualisierung → „Jetzt nach Updates suchen" | über die Fenster-Brücke dieselbe Aktion | — |
| **Stiller Start-Check** | 3 s nach Programmstart, höchstens 1×/Tag | öffnet den Dialog **nur**, wenn es etwas zu holen gibt |

🔑 **Es gibt keine Statuszeile für den stillen Fund.** Der Fund öffnet den Dialog direkt
(`onUpdateFound() { updateDialog.open() }`). Wer aus RAFTNG kommt, erwartet hier eine
Statuszeile — das ist deren Mechanik, nicht unsere. Beim Nachbau ist das eine bewusste
Wahl, keine Auslassung: siehe 9.2.

---

## 2 · Zustandsmodell

Ein **Enum**, keine Bündel von Booleschen. Der Dialog zeigt je Zustand genau eine Sache,
und widersprüchliche Kombinationen (`busy` **und** fertig) können gar nicht entstehen.

| # | Zustand | Bedeutung |
|---|---|---|
| 0 | `Idle` | nichts unternommen |
| 1 | `Checking` | Manifest wird geholt und die Signatur geprüft |
| 2 | `UpToDate` | geprüft, nichts Neueres da |
| 3 | `Available` | neuere Version verfügbar (oder angeforderte Rückstufung) |
| 4 | `Downloading` | Artefakt wird geladen |
| 5 | `Ready` | heruntergeladen **und** SHA-256-geprüft |
| 6 | `Failed` | Fehler, Text in `lastError` |

`busy` ist abgeleitet: **`Checking` oder `Downloading`**. Nur davon hängt ab, ob der
Abbrechen-Knopf erscheint — nicht von einer eigenen Variablen.

### Übergänge

```
                    checkNow() / checkOnStartup()
        Idle ─────────────────────────────────────► Checking
                                                       │
                        ┌──────────────────────────────┼───────────────────────┐
                        │ nichts Neueres               │ neuer                 │ Fehler
                        ▼                              ▼                       ▼
                    UpToDate                       Available ◄──┐           Failed
                                                       │        │              │
                                              download()│        │              │ „Erneut
                                                       ▼        │              │  versuchen"
                                                  Downloading   │              │
                                                       │        │              ▼
                                    ┌──────────────────┼────────┘          Checking
                            Fehler /│ fertig + SHA ok  │ abort()
                            SHA-Bruch│                 │
                                     ▼                 ▼
                                  Failed             Idle
                                                       
        Ready ──── launchInstaller() ok ────► Dialog schliesst sich, Zustand bleibt Ready
              └─── launchInstaller() nicht moeglich ──► Failed
```

**Wichtige Kanten, die man beim Nachbau übersieht:**

- `abort()` führt **nicht** nach `Failed`, sondern zurück nach `Idle`. Ein Abbruch ist kein
  Fehler und darf keinen roten Text hinterlassen.
- Nach erfolgreichem `launchInstaller()` **schließt sich der Dialog**, aber der Zustand
  bleibt `Ready`. QTmux beendet sich **nicht** selbst (9.4).
- `Failed` → „Erneut versuchen" springt nach `Checking`, **nicht** zurück in den
  vorherigen Zustand — auch wenn der Fehler beim *Download* auftrat. Neu geprüft wird von
  vorn.
- Aus `UpToDate` führt keine Kante außer „Schließen". Der manuelle Check ist der einzige
  Weg zurück.

---

## 3 · Layout und Hierarchie

### Rahmen (aus `AppDialog`, gilt für alle Dialoge der App)

| Eigenschaft | Wert |
|---|---|
| Hintergrund | `Theme.bgElevated`, **Radius 12**, 1 px Rand `Theme.border` |
| Abdunklung dahinter | `#88000000` (53 % Schwarz) |
| Innenabstand | **20 px** rundum |
| Kopfzeile | Titel, **16 px fett**, `Theme.textBright`, Innenabstand 20 px, **unten 6 px**, rechts elidiert |
| Schließen | **Esc** und Klick außerhalb |
| Position | zentriert im Elternfenster |

`Theme.bgElevated` = Hintergrund des Farbschemas, **10 %** Richtung Vordergrund gemischt;
`Theme.border` = **24 %**; `Theme.textDim` = Vordergrund **45 %** Richtung Hintergrund.
Alle Chrome-Farben leiten sich aus dem aktiven Terminal-Farbschema ab — der Dialog hat
**keine** eigene Palette. Ausnahme sind die zwei Signalfarben unten.

### Inhaltsspalte

Eine einzige senkrechte Spalte, Breite **500 px**, Abstand zwischen den Blöcken **12 px**.
Reihenfolge von oben nach unten — **immer dieselbe**, unabhängig vom Zustand; Blöcke
erscheinen und verschwinden, sie tauschen nie die Plätze:

| # | Block | Schrift | Farbe |
|---|---|---|---|
| 1 | **Versionszeile** | 14 px | `textBright` |
| 2 | **Veröffentlichungszeile** | 11 px | `textDim` |
| 3 | **Downgrade-Warnung** (Kasten) | 12 px | `#e5534b` auf `rgba(230,84,74,0.14)`, Rand `#e5534b`, Radius 6, Innenabstand 8, Höhe = Text + 16 |
| 4 | **„kein Paket für dieses System"** | 12 px | `textDim` |
| 5 | **Abschnittstitel „Was ist neu"** | 11 px fett, 6 px Abstand nach oben | `textDim` |
| 6 | **Anmerkungen** (rollbar, max. **200 px** hoch, sonst Inhaltshöhe + 8) | 12 px | `textBright` |
| 7 | **Fortschritt**: Balken + Zeile darunter, Abstand 4 px | 11 px | `textDim` |
| 8 | **„kann sich nicht selbst ersetzen"** | 12 px | `textDim` |
| 9 | **Fehlertext** | 12 px | `#e5534b` |
| 10 | **Betriebssystem-Hinweis** | 11 px | `textDim` |
| 11 | **Knopfreihe**, Abstand 8 px, 4 px nach oben | — | — |

Alle Textblöcke brechen um (`WordWrap`) und füllen die Breite.

### Versions-Gegenüberstellung Ist/Neu

Bewusst **keine** zweispaltige Tabelle und **keine** Pfeil-Darstellung, sondern **ein Satz**
in Block 1:

> **QTmux 1.8.1 ist verfügbar — installiert ist 1.8.0.**

Die neue Version steht **vorn** (sie ist die Nachricht), die installierte hinten als
Bezugsgröße. Im Zustand `UpToDate` kollabiert der Satz auf **„QTmux 1.8.1 ist aktuell."** —
dieselbe Zeile, dieselbe Stelle, kein eigenes Layout für den Normalfall.

### Wo die Build-ID erscheint: **gar nicht**

⚠️ Der Dialog zeigt **ausschließlich** die dreistellige Version (`1.8.1`), nie die Build-ID
`1.8.1+3744c38`. Das ist keine Auslassung, sondern Bedingung: `currentVersion` ist die
**Vergleichsgröße** gegen das Manifest. Mit angehängtem Hash schlüge der Vergleich fehl oder
böte dauerhaft ein „Update" an.

Die Build-ID gehört an die Stellen, wo man sie zur **Diagnose** braucht: **Fenstertitel** und
MCP `get_server_info` (`buildId`, `buildDirty`). Beim Nachbau gilt: Anzeige der Build-ID ja,
aber **nie** in dem Feld, das verglichen wird.

---

## 4 · Elementmatrix je Zustand

`●` sichtbar · `–` ausgeblendet · `(x)` nur unter der genannten Bedingung.

| Block | Idle | Checking | UpToDate | Available | Downloading | Ready | Failed |
|---|:--:|:--:|:--:|:--:|:--:|:--:|:--:|
| 1 Versionszeile | ● | ● | ● | ● | ● | ● | ● |
| 2 Veröffentlicht | – | – | (a) | ● | ● | ● | (a) |
| 3 Downgrade-Warnung | – | – | – | (b) | – | – | – |
| 4 kein Paket | – | – | – | (c) | – | – | – |
| 5+6 Was ist neu | (d) | (d) | (d) | ● | ● | ● | (d) |
| 7 Fortschritt | – | – | – | – | ● | ● | – |
| 8 kann sich nicht ersetzen | – | – | – | – | – | (e) | – |
| 9 Fehlertext | – | – | – | – | – | – | ● |
| 10 OS-Hinweis | – | – | – | ● | – | ● | – |

(a) nur wenn ein Manifest gelesen wurde (`published` **und** `remoteVersion` gefüllt) — die
Größenangabe entfällt in `UpToDate` bewusst, s. 9.6 · (b) nur bei angeforderter Rückstufung ·
(c) nur wenn das Manifest für dieses Betriebssystem keinen Schlüssel führt ·
(d) nur wenn Anmerkungen vorliegen — nach einem Check bleiben sie stehen ·
(e) nur wenn der Installer nicht selbst gestartet werden kann (Linux ohne `$APPIMAGE`).

### Knöpfe je Zustand

Die Reihe hat **zwei Gruppen**, getrennt durch einen dehnbaren Zwischenraum: links die
**vorwärts führende** Handlung, rechts die **abschließenden**.

| Zustand | links | rechts |
|---|---|---|
| `Idle` | – | **Schließen** |
| `Checking` | – | **Abbrechen** |
| `UpToDate` | – | **Schließen** |
| `Available`, Paket vorhanden | **Herunterladen** | **Version überspringen** · **Später** |
| `Available`, kein Paket | – | **Version überspringen** · **Später** |
| `Available`, Rückstufung | **Herunterladen** | **Später** *(kein Überspringen)* |
| `Downloading` | – | **Abbrechen** |
| `Ready`, startbar | **Installieren …** | **Später** |
| `Ready`, nicht startbar | – | **Später** |
| `Failed` | **Erneut versuchen** | **Schließen** |

🔑 **Kein `standardButtons`.** Die Beschriftungen hängen am Zustand („Herunterladen" ist
etwas anderes als „Installieren …"), und „Version überspringen" hat in Qts Standardsatz kein
Gegenstück. Beim Widgets-Nachbau heißt das: `QDialogButtonBox` nur mit selbst gesetzten
Rollen, nicht mit `StandardButtons`.

🔑 **Der letzte Knopf wechselt die Beschriftung**, nicht seine Position: **„Später"** in
`Available`/`Ready` (es gibt etwas, das man aufschiebt), sonst **„Schließen"**.

---

## 5 · Layoutskizzen je Zustand

Maßstabstreu in der Reihenfolge, nicht in den Pixeln. `≡≡≡` = mehrzeiliger Fließtext.

**Prüfung läuft** (`Checking`)

```
┌────────────────────────────────────────────────────────────┐
│ Nach Updates suchen …                                      │  16 px fett
│                                                            │
│ QTmux fragt den Update-Server …                            │  14 px hell
│                                                            │
│                                            [ Abbrechen ]   │
└────────────────────────────────────────────────────────────┘
```

**Aktuell** (`UpToDate`)

```
┌────────────────────────────────────────────────────────────┐
│ Kein Update verfügbar                                      │
│                                                            │
│ QTmux 1.8.1 ist aktuell.                                   │
│ Veröffentlicht am 2026-08-07                               │  11 px gedämpft
│                                                            │
│                                            [ Schließen ]   │
└────────────────────────────────────────────────────────────┘
```

**Update gefunden** (`Available`) — der Normalfall

```
┌────────────────────────────────────────────────────────────┐
│ Update verfügbar                                           │
│                                                            │
│ QTmux 1.8.1 ist verfügbar — installiert ist 1.8.0.         │
│ Veröffentlicht am 2026-08-07 · 57,8 MB                     │
│                                                            │
│ WAS IST NEU                                                │  11 px fett gedämpft
│ ┌────────────────────────────────────────────────────────┐ │
│ │ QTmux 1.8.1 — Nachlese zu 1.8.0.                       │ │  rollbar, max 200 px
│ │ - MCP-Server im Netzwerk: einstellbare Bind-Adresse …  ↕│ │
│ └────────────────────────────────────────────────────────┘ │
│                                                            │
│ ≡ Hinweis: Das Paket ist nicht notariell beglaubigt. …    ≡│  11 px gedämpft
│                                                            │
│ [ Herunterladen ]      [ Version überspringen ] [ Später ] │
└────────────────────────────────────────────────────────────┘
```

**Download läuft** (`Downloading`)

```
│ Update wird geladen                                        │
│ QTmux 1.8.1 ist verfügbar — installiert ist 1.8.0.         │
│ Veröffentlicht am 2026-08-07 · 57,8 MB                     │
│ WAS IST NEU  ┌──────────────────────────────────────────┐  │
│              └──────────────────────────────────────────┘  │
│ ████████████████████████░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░   │  Balken, 0…1
│ 43 %                                                       │  11 px gedämpft
│                                            [ Abbrechen ]   │
```

**Bereit zur Installation** (`Ready`)

```
│ Update bereit zur Installation                             │
│ QTmux 1.8.1 ist verfügbar — installiert ist 1.8.0.         │
│ ██████████████████████████████████████████████████████████ │  voll
│ Heruntergeladen und geprüft: /Users/…/QTmux-1.8.1-mac.dmg  │  Mitte elidiert
│ ≡ Hinweis: Das Paket ist nicht notariell beglaubigt. …    ≡│
│ [ Installieren … ]                            [ Später ]   │
```

**Bereit, aber nicht startbar** (`Ready`, Linux ohne `$APPIMAGE`)

```
│ ██████████████████████████████████████████████████████████ │
│ Heruntergeladen und geprüft: /home/…/QTmux-1.8.1.AppImage  │
│ ≡ QTmux läuft nicht aus einem AppImage und kann sich       │
│   deshalb nicht selbst ersetzen. Die Datei oben ist        │
│   geprüft und kann von Hand installiert werden.           ≡│
│                                               [ Später ]   │  kein Installieren-Knopf
```

**Fehler** (`Failed`)

```
│ Update fehlgeschlagen                                      │
│ QTmux 1.8.0                                                │
│ ≡ Der Proxy proxy.firma.local antwortet nicht.            ≡│  12 px, Signalrot
│   (manifest: Connection refused)                           │
│ [ Erneut versuchen ]                       [ Schließen ]   │
```

**Rückstufung** (`Available`, nur über den manuellen Check erreichbar)

```
│ Update verfügbar                                           │
│ QTmux 1.7.1 ist verfügbar — installiert ist 1.8.1.         │
│ ┌────────────────────────────────────────────────────────┐ │
│ │ Achtung: Das ist eine ÄLTERE Version als die           │ │  Kasten, Signalrot
│ │ installierte. Eine Rückstufung kann Einstellungen …    │ │  auf 14 % Rotfläche
│ └────────────────────────────────────────────────────────┘ │
│ [ Herunterladen ]                             [ Später ]   │  kein Überspringen
```

---

## 6 · Texte, vollständig in DE und EN

Platzhalter `%1`, `%2` in der angegebenen Reihenfolge.

### Fenstertitel

| Zustand | DE | EN |
|---|---|---|
| `Checking` | Nach Updates suchen … | Check for Updates … |
| `UpToDate` | Kein Update verfügbar | No update available |
| `Downloading` | Update wird geladen | Downloading update |
| `Ready` | Update bereit zur Installation | Update ready to install |
| `Failed` | Update fehlgeschlagen | Update failed |
| sonst (`Idle`, `Available`) | Update verfügbar | Update available |

### Versionszeile

| Fall | DE | EN |
|---|---|---|
| `Checking` | QTmux fragt den Update-Server … | QTmux is contacting the update server … |
| `UpToDate` | QTmux %1 ist aktuell. | QTmux %1 is up to date. |
| kein Manifest | QTmux %1 | QTmux %1 |
| sonst | QTmux %1 ist verfügbar — installiert ist %2. | QTmux %1 is available — you have %2. |

### Veröffentlichungszeile

| Fall | DE | EN |
|---|---|---|
| mit Größe | Veröffentlicht am %1 · %2 | Published %1 · %2 |
| ohne Größe | Veröffentlicht am %1 | Published %1 |

Größeneinheiten: **%1 MB** / **%1 kB** / **%1 Bytes** (EN: `%1 bytes`).

### Blöcke

| Block | DE | EN |
|---|---|---|
| Abschnitt | Was ist neu | What's new |
| Downgrade | Achtung: Das ist eine ÄLTERE Version als die installierte. Eine Rückstufung kann Einstellungen und gespeicherte Sitzungen betreffen, die eine neuere Fassung geschrieben hat. | Careful: this is an OLDER version than the one installed. Downgrading can affect settings and saved sessions that a newer release has written. |
| kein Paket | Für dieses Betriebssystem liegt in dieser Veröffentlichung kein Paket bereit. | This release carries no package for your operating system. |
| Fortschritt | %1 % | %1 % |
| fertig | Heruntergeladen und geprüft: %1 | Downloaded and verified: %1 |
| nicht startbar | QTmux läuft nicht aus einem AppImage und kann sich deshalb nicht selbst ersetzen. Die Datei oben ist geprüft und kann von Hand installiert werden. | QTmux is not running from an AppImage and therefore cannot replace itself. The file above has been verified and can be installed manually. |

### Betriebssystem-Hinweis (fest sichtbar in `Available` und `Ready`)

**Windows** — DE: Hinweis: Das Paket ist nicht signiert. Windows SmartScreen meldet beim
Start „Der Computer wurde geschützt" — über „Weitere Informationen" → „Trotzdem ausführen"
fortfahren. QTmux prüft den Download selbst über eine Ed25519-Signatur und eine
SHA-256-Summe.
EN: Note: the package is not signed. Windows SmartScreen will report "Windows protected your
PC" — continue via "More info" → "Run anyway". QTmux verifies the download itself with an
Ed25519 signature and a SHA-256 checksum.

**macOS** — DE: Hinweis: Das Paket ist nicht notariell beglaubigt. macOS Gatekeeper
verweigert den ersten Start — die App im Finder mit Rechtsklick → „Öffnen" starten. QTmux
prüft den Download selbst über eine Ed25519-Signatur und eine SHA-256-Summe.
EN: Note: the package is not notarised. macOS Gatekeeper will refuse the first launch —
right-click the app in Finder and choose "Open". QTmux verifies the download itself with an
Ed25519 signature and a SHA-256 checksum.

**Linux/übrige** — DE: Hinweis: Das Paket ist nicht signiert. QTmux prüft den Download selbst
über eine Ed25519-Signatur und eine SHA-256-Summe.
EN: Note: the package is not signed. QTmux verifies the download itself with an Ed25519
signature and a SHA-256 checksum.

### Knöpfe

| DE | EN |
|---|---|
| Herunterladen | Download |
| Installieren … | Install … |
| Erneut versuchen | Try again |
| Abbrechen | Cancel |
| Version überspringen | Skip this version |
| Später | Later |
| Schließen | Close |

### Fehlertexte der App-Hälfte

| DE | EN |
|---|---|
| Der Proxy verlangt eine Anmeldung. | The proxy requires authentication. |
| Der Proxy hat die Anmeldung abgelehnt. | The proxy rejected the sign-in. |
| Der Proxy %1 antwortet nicht. | The proxy %1 is not responding. |
| Die Proxy-Einstellung des Systems ließ sich nicht ermitteln. Häufigste Ursache ist eine hinterlegte Konfigurationsdatei (PAC/WPAD), die nicht erreichbar ist — „Direkt" umgeht sie. | The system proxy settings could not be determined. The most common cause is a configured auto-config file (PAC/WPAD) that is unreachable — "Direct" bypasses it. |
| Für dieses Betriebssystem liegt kein Paket bereit. | No package is available for this operating system. |
| QTmux läuft nicht aus einem AppImage und kann sich deshalb nicht selbst ersetzen. Die geprüfte Datei liegt hier — bitte von Hand installieren:\n%1 | QTmux is not running from an AppImage and therefore cannot replace itself. The verified file is here — please install it manually:\n%1 |

⚠️ **Die Fehlertexte des vendierten Kerns sind englisch und NICHT übersetzt** —
`sha256 mismatch (expected …, got …)`, `download failed: …`, `manifest signature
verification failed`, `no release published for this product yet`, `signature has %1 bytes,
expected 64`. In deutscher Oberfläche erscheinen sie also englisch. Das ist **kein
Designmerkmal**, sondern eine bekannte Schwäche (11.3) — beim Nachbau nicht mit übernehmen,
sondern im Hub gleich richtig lösen.

**Zusammengesetzt** wird ein Fehler so: sprechender Proxy-Grund, darunter der rohe Kerntext
in Klammern — `„Der Proxy … antwortet nicht.\n(manifest: Connection refused)"`. Ohne den
sprechenden Teil liest sich ein falsch eingestellter Proxy als „Server nicht erreichbar",
und man sucht an der falschen Stelle.

---

## 7 · Verhalten

### 7.1 Fortschrittsanzeige

- Balken über die volle Breite, Wertebereich **0…1**.
- Darunter **nur der Prozentsatz**, ganzzahlig gerundet: `43 %`.
- **Keine** Byte-Zahlen, **keine** Übertragungsrate, **keine** Restzeit.
- **Keine** Festbreitenziffern — die Zeile ist linksbündig und kurz genug, dass das Springen
  der Ziffernbreite nicht auffällt. (Bei rechtsbündiger Ausrichtung oder mit angehängter
  Byte-Zahl wäre Festbreite Pflicht; das ist der Grund, warum es hier keine gibt.)
- Die **Paketgröße** steht stattdessen einmal in der Veröffentlichungszeile — dort, wo sie
  eine Entscheidung stützt: *vor* dem Herunterladen.
- Im Zustand `Ready` bleibt der Balken sichtbar und **voll**; die Zeile darunter wechselt vom
  Prozentsatz auf den geprüften **Dateipfad**, in der **Mitte** elidiert (`…`), damit
  Anfang und Dateiname lesbar bleiben.

### 7.2 „Version überspringen"

- Merkt sich **genau diese** Versionsnummer.
- Der **stille** Start-Check schweigt dazu, bis eine **andere** Version erscheint.
- Der **manuelle** Check zeigt sie weiterhin — sonst wäre ein Fehlklick unwiderruflich.
- Der Knopf **schließt** den Dialog sofort mit.
- **Nicht verfügbar bei einer Rückstufung**: Eine ältere Version zu „überspringen" ergibt
  keinen Sinn — sie wird ohnehin nie von selbst angeboten.

### 7.3 Was „Abbrechen" je Phase tut

| Phase | Wirkung |
|---|---|
| `Checking` | bricht die Netzanfrage ab → zurück nach `Idle`. Kein Fehlertext. |
| `Downloading` | bricht den Download ab → zurück nach `Idle`. Die Teildatei wird verworfen. |
| sonst | **Der Knopf existiert nicht.** Er hängt allein an `busy`. |

Der Abbrechen-Knopf ist damit **nie** ein „Nein danke" — dafür gibt es „Später" und „Version
überspringen". Er beendet ausschließlich eine *laufende* Übertragung.

### 7.4 Nach der Übergabe an den Installer

1. `launchInstaller()` startet den plattformüblichen Weg:
   **Windows** `msiexec /i <pfad>` · **macOS** `open <dmg>` · **Linux** Selbstersetzung des
   AppImage über `$APPIMAGE`.
2. Bei Erfolg **schließt sich der Dialog**. Der Zustand bleibt `Ready`.
3. ⚠️ **QTmux beendet sich NICHT von selbst.** Der Anwender beendet die App, wenn der
   Installer danach fragt. Begründung in 9.4.
4. Schlägt der Start fehl, wird `lastError` gefüllt und der Zustand wechselt nach `Failed` —
   der Dialog bleibt offen und zeigt den Grund.
5. Kann der Installer prinzipiell nicht gestartet werden (Linux ohne `$APPIMAGE`), erscheint
   **gar kein Knopf**, sondern der erklärende Text mit dem geprüften Dateipfad.

### 7.5 Prüfkette vor jeder Anzeige

Bevor überhaupt etwas angeboten wird: **Ed25519-Signatur** über die exakten Manifest-Bytes,
danach **SHA-256** des heruntergeladenen Artefakts gegen den Wert aus dem Manifest. Bei
Abweichung wird die Datei **gelöscht** — ein beschädigter Installer darf nicht liegen
bleiben — und der Zustand geht nach `Failed`.

---

## 8 · Sprache der Anmerkungen

Das Manifest führt `notes.de` und `notes.en`. Der Dialog bindet die Auswahl an die
**Oberflächensprache** der App, nicht an die Systemsprache und nicht an eine eigene
Einstellung. Wer die App auf Englisch stellt, liest auch englische Anmerkungen.

---

## 9 · Bewusste Entscheidungen — das WARUM

> Dieser Abschnitt ist der eigentliche Wert der Spezifikation. Wer nur die Form übernimmt,
> verliert die Gründe und baut sie beim nächsten Umbau wieder aus.

### 9.1 Ein Dialog für alles, kein Assistent

Der Anwender erlebt Prüfen → Lesen → Laden → Installieren als **einen** Vorgang. Ein
Assistent mit Seiten erzwingt Schritte, die es nicht gibt (nach dem Lesen der Anmerkungen
will man oft *nichts* tun). Welche Knöpfe erscheinen, entscheidet allein der Zustand — die
Blöcke behalten dabei ihre Reihenfolge, damit der Dialog nicht bei jedem Zustandswechsel neu
gelesen werden muss.

### 9.2 Fehler des Start-Checks bleiben still

Ein Rechner ohne Netz darf nicht **jeden Morgen** mit einem Fehlerdialog begrüßt werden. Der
stille Check zeigt sich **nur**, wenn es etwas zu holen gibt. Technisch: Im Callback wird
`if (manual)` unterschieden — bei `false` geht der Zustand kommentarlos nach `Idle`.

Der **manuelle** Check ist das Gegenteil: Wer selbst nachsieht, will das Ergebnis sehen,
auch ein Scheitern. Beim Nachbau ist genau diese Unterscheidung der Kern — ein gemeinsamer
Fehlerpfad für beide Wege wäre die naheliegende und falsche Vereinfachung.

⚠️ Die Regel gilt in **beide** Richtungen: Eine Warnung ohne Deckung erzieht den Bediener zu
unnötiger Arbeit. Deshalb steht im Einstellungstext, was der Schalter *tut*, nicht was er
verspricht.

### 9.3 Der Betriebssystem-Hinweis steht FEST, nicht nur im Fehlerfall

Die Pakete sind nicht signiert bzw. nicht notarisiert (Early-Adopter). Die Warnung des
Betriebssystems erscheint erst **nach** dem Download — wer sie dort zum ersten Mal sieht,
hält den Download für kompromittiert. Deshalb steht der Hinweis schon **vor** dem
Herunterladen da, zusammen mit der Gegenaussage, dass QTmux **selbst** Signatur und Prüfsumme
verifiziert. Der Anwender soll wissen: Das Betriebssystem meckert, aber geprüft ist es
trotzdem.

Fällt weg, sobald die Pakete signiert und notarisiert sind — dann aber vollständig, nicht
halb.

### 9.4 Geführte Installation statt stillem Selbsttausch

QTmux ersetzt sich **nicht** selbst und beendet sich **nicht** selbst. Ein Terminal-Manager
trägt laufende Sitzungen; ein ungefragter Neustart reißt sie ab. Der Dialog übergibt an den
Installer und tritt zurück — die Entscheidung, wann die App beendet wird, bleibt beim
Menschen.

🔑 Für den Nachbau in anderen Produkten ist zu prüfen, ob dieses Argument dort **auch** gilt.
Es ist ein Argument aus der **Mechanik** von QTmux (laufende Sessions), nicht aus der Form
des Dialogs. Eine App ohne solchen Zustand darf sich durchaus selbst beenden — dann aber als
bewusste eigene Entscheidung, nicht durch Abschreiben.

### 9.5 Rückstufung erlaubt, aber nie beiläufig

Sie wird **nur auf ausdrückliche Anforderung** angeboten (manueller Check), nie vom stillen
Start-Check — sonst böte eine ältere Veröffentlichung jedem Entwickler-Build **täglich** an,
sich zurückzustufen. Wenn sie angeboten wird, dann mit einem farbigen Warnkasten und ohne
die Möglichkeit, sie zu „überspringen".

### 9.6 Die Paketgröße steht nur dort, wo sie eine Entscheidung stützt

Im Zustand `UpToDate` beschriebe sie ein Paket, das niemand holen wird. Am Live-Bild ist sie
dort als Rauschen aufgefallen und wurde gezielt ausgeblendet — die Bedingung lautet
ausdrücklich `downloadSize > 0 && state !== UpToDate`.

### 9.7 Menschliche Größenangaben, aber ohne falsche Null

`98,4 MB` statt `103181234`. Unterhalb von 1 kB wird die **Byte-Zahl** gezeigt statt eines
gerundeten `0 kB` — das fällt nur bei Test-Fixtures an, sah dort aber wie eine fehlende
Angabe aus.

### 9.8 Ein Enum statt mehrerer Boolescher

Widersprüchliche Kombinationen (`busy` **und** fertig) können gar nicht entstehen. Jede
Sichtbarkeitsregel im Dialog liest **einen** Zustand, nicht drei Flaggen — deshalb ist die
Matrix in Abschnitt 4 überhaupt aufschreibbar.

### 9.9 Fehlende Fähigkeiten werden gesagt, nicht durch tote Knöpfe angedeutet

Linux ohne `$APPIMAGE`: Es gibt nichts zu ersetzen. Statt eines Knopfes, der nichts tut,
steht dort der Grund **und der Dateipfad**. So ist der Fall im Linux-Build überhaupt erst
aufgefallen.

---

## 10 · Fallstricke beim Nachbau in Widgets

1. **Höhe wächst mit dem Inhalt.** Die Anmerkungen sind bis 200 px hoch und darüber rollbar;
   alles andere bestimmt seine Höhe aus dem umbrochenen Text. Eine feste Dialoghöhe bricht
   spätestens bei einer langen Fehlermeldung oder einer Übersetzung mit längeren Wörtern.
2. **Blöcke ausblenden, nicht umsortieren.** Die Reihenfolge ist über alle Zustände gleich.
3. **Zwei Signalfarben sind fest** (`#e5534b` für Warnung/Fehler, dieselbe Farbe auf 14 %
   Fläche für den Downgrade-Kasten) — alles andere leitet sich aus dem Farbschema ab. Beim
   Nachbau bedeutet das: eine Palette, aus der der Dialog seine Farben *bezieht*, statt
   hart gesetzter Werte.
4. **Der Dateipfad wird in der MITTE elidiert.** Am Ende zu kürzen versteckt den Dateinamen,
   und genau der ist die Information.
5. **Esc schließt, Klick außerhalb schließt.** Es gibt keinen Zustand, in dem der Dialog
   festhält — auch nicht während des Downloads.
6. **Der Titel im Zustand `Idle` lautet „Update verfügbar"**, obwohl nichts geprüft wurde.
   Praktisch unsichtbar, weil der Dialog nur über einen Fund oder einen Check aufgeht.
   Nachbauten sollten das **nicht** übernehmen, sondern `Idle` einen eigenen Titel geben.

---

## 11 · Bekannte Schwächen (Ist-Zustand, hier nicht behoben)

1. **Kein Fortschritt in Bytes.** Bei einer sehr langsamen Verbindung sagt `3 %` weniger als
   `1,7 von 57,8 MB`. Bewusst schlicht gehalten; falls der Hub es ergänzt, gehört dann
   **Festbreite** für die Ziffern dazu (7.1).
2. **`Idle`-Titel** — s. 10.6.
3. **Kerntexte englisch.** Die Fehlermeldungen des vendierten `appupdate`-Kerns laufen nicht
   durch `tr()` und erscheinen deshalb in deutscher Oberfläche englisch (Abschnitt 6). Die
   sichtbaren Fälle sind Signatur- und Prüfsummenfehler sowie Netzabbrüche — also genau die
   Momente, in denen der Anwender ohnehin verunsichert ist. **Das ist der lohnendste Punkt
   für den geteilten Hub-Dialog**, weil alle drei Apps denselben Kern verwenden und die
   Übersetzung damit einmal für alle entstünde.
4. **Keine Referenz-Screenshots.** Auf dem Entwicklungsrechner lassen sie sich nicht
   automatisiert erzeugen: Es gibt kein Bedienungshilfen-Recht (keine System-Events,
   keine Bildschirmaufnahme), und der Selbst-Screenshot der App fotografiert das
   Hauptfenster beim Start, nicht einen bedienten Dialog. Ersatz sind die Skizzen in
   Abschnitt 5. Sollen echte Bilder entstehen, braucht es einen bedienten Durchlauf von
   Hand — sinnvollerweise beim nächsten Owner-Durchklick, dann je Zustand ein Bild nach
   `docs/images/update-dialog-<zustand>.png`.
