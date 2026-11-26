# QTmux — Historie

> Erzählung aus der `CLAUDE.md`: Release-Verläufe, abgeschlossene Tickets, der alte
> Wiedereinstieg. Den Zustand führt die `CLAUDE.md`; was hier steht, ist Verlauf und wird nicht
> nachgeführt. Die Absätze sind wörtlich übernommen. Wo eine Zeile fehlt, steht sie als Lehre an
> anderer Stelle; der Hinweis steht dann in Klammern dabei.

## QTMUX-162 — MCP-Revision 2026-07-28 und MCP-Tasks (2026-11-26, noch nicht ausgeliefert)

**Inhalt:** Der MCP-Server spricht neben dem alten `initialize`-Weg die Revision 2026-07-28
(zustandslos, `server/discover`, Fehler mit HTTP-Status und Code nach Spezifikation) und die
Tasks-Erweiterung. Neue Werkzeuge `await_events`, `await_message` (nur neue Fassung) und
`message_status` (beide). `send_message` nimmt zuerst einen lebenden Posteingangs-Task, mit
60-s-Puffer zwischen zwei Tasks. HTTP: 405 auf `GET /mcp`, POST nur auf `/mcp` und `/`
(Owner-Entscheid 2026-11-26), Origin-Prüfung, Kopfteil-Grenzen.

**Gemessen vor der Änderung (Stand 1.10.1, eigene Testinstanz, Claude Code 2.1.332):** Nach
dem Rückfall auf `initialize` fragte der Client einmal je Sekunde per `GET /mcp`
(`Accept: text/event-stream`) an, 90 GETs von 23:13:12 bis 23:14:41, bis `/exit`.

**Unabhängige Durchsicht** (lesend, Opus) fand vor dem ersten Commit: unbegrenzten Speicher
durch Nachrichtenbelege (gemessen: +321 MB RSS nach 20 abgelehnten Nachrichten zu 3,9 MB),
Nachrichten in einem Posteingang ohne Abholer, `tasks/cancel` als falschen Zustellbeleg, zwei
Long-Polls auf einer Verbindung, Überholen bei vollem Puffer, Verdrängen eines gebundenen
Posteingangs aus dem Netz, unbekanntes Werkzeug ohne `-32602`, Pflicht-Kopfzeilen bei
Benachrichtigungen, absolute Zielform und Entwurfsfassungen auf dem alten Weg. Je ein Test
war am Stand davor rot und ist jetzt grün. Ein Befund (abgelehntes Anlegen nimmt den alten
Posteingang weg) ist widerlegt: Der ersetzte Task wird beim Ersetzen frei.

**Teststand:** 73 auf macOS (vorher 70): `test_taskstore`, `test_mcphttp`, `test_procowner`.

## Release v1.10.1 (2026-11-26)

**Inhalt:**
- Update-Kern auf Hub-Stand MacPCAN 0.8.1 (`8d060c3`).
- macOS-Ziel 13.0 vor `project()`; `LSMinimumSystemVersion` 13.0 (bisher leer).
- Die Selbst-Ersetzen-Helfer tragen die Build-ID.
- Eingehängte Update-Images werden beim Start aufgeräumt (Hub).
- Szene 15 läuft wieder überall (QTMUX-161, Revert der drei Aussetz-Commits).
- 🔑 **Das DMG bündelt das offizielle Qt 6.10.3 (Version der CI) statt Homebrew-Qt 6.11.2.**
  Homebrew-Qt trägt `minos 14.0`; mit ihm wäre das Paket trotz Ziel 13.0 erst ab macOS 14
  gelaufen. Owner-Entscheid: eigenes Qt, Paket echt ab macOS 13, weiter nur Apple Silicon.
  `build-dmg.sh` nimmt Qt nur noch aus `QTMUX_QT_MAC` (Riegel in [installer/CLAUDE.md](../installer/CLAUDE.md)).

**Commits:** Tag `v1.10.1` (annotiert) auf `6693d65` („Release 1.10.1“). Alle vier Artefakte
stammen aus diesem Commit, je aus einem frischen Klon eines Bundles.

| Artefakt | Bytes | SHA256 | Build-ID |
|---|---|---|---|
| `QTmux-1.10.1-macos.dmg` | 55 257 600 | `2a3c252d517dda0df2c6eb07dd2135a145623f14b9e22738bfadb9e57de2a6d1` | `1.10.1+6693d65` |
| `QTmux-1.10.1-win64.msi` | 32 267 483 | `2542b720daa47965d13018e23e760713d39e011d74ac9aed6e545e98afbc1b5e` | `1.10.1+6693d65` |
| `QTmux-1.10.1-win64-portable.zip` | 39 684 887 | `44b06f2962827814dcd535e1841baca32cab3c2da6a4f958be49a00808200a50` | `1.10.1+6693d65` |
| `QTmux-1.10.1-x86_64.AppImage` | 45 255 160 | `aa8accbee7326ef11732e60fa7e46c8fcea8ae78e7030206534c4d1f73beb91c` | `1.10.1+6693d65` |

**CI:**
- Messlauf 45534833674 (`workflow_dispatch`, `e8786bb`); main-Läufe 45714242909 (`e8786bb`)
  und 45717209779 (`6693d65`).
- Je Job: macOS 70, Windows 65, Linux 64, 0 Skips. Szene 15 lief auf dem macOS-Runner (kein
  `QSKIP` mehr im Baum).
- Tag-Lauf 45718291677: Versuch 1 mit macOS und Linux grün; Windows hing im Schritt „Tests
  (Windows, ohne test_pty)“ ab 23:12:23Z. Abbruch angefordert 23:45:29Z, Job beendet
  (cancelled) 23:50:29Z, also rund 38 min bis zum Abbruch. GitHub hat für diesen Job **keinen
  Log** gespeichert. **Die Ursache des Hängers ist UNBEKANNT.** Versuch 2 (`rerun --failed`):
  Windows 65/65 grün, danach `linux-release` grün.

**Wege:**
- DMG am Mac, `FAMILY_CODESIGN=require`, Qt aus `QTMUX_QT_MAC`.
- MSI und ZIP auf dem Windows-Bau-Rechner als `claude`, mit einer Kopie des Wrappers.
- AppImage aus dem Tag-Lauf (Draft-Asset), per `gh release download` geholt.

**Abnahme:**
- DMG: `dmg_minos.py` OK (App und `update-swap` `minos 13.0`, nur arm64); 104/104 Mach-O
  familien-signiert, 0 ad-hoc; DR unverändert (`certificate root`); `update-swap build
  1.10.1+6693d65`. HTTPS-Abruf des Live-Manifests NUR mit dem Qt des Bundles: securetransport,
  HTTP 200, keine Qt-Bibliothek von außerhalb geladen.
- MSI: Build-ID in `qtmux.exe` und `update-helper.exe`; CRT zehn DLLs wie 1.10.0; 1424 Dateien
  wie 1.10.0.
- AppImage: Build-ID im entpackten `usr/bin/qtmux`; nur `libqxcb`, glibc höchstens 2.38; Start
  unter xcb mit xvfb (Selbsttest rc 0, Hauptfenster geladen).
- Größen über Dateilisten erklärt:
  - DMG +6 491 136 B: das Qt der CI ist universal, das Bundle bleibt unausgedünnt (arm64-Ausdünnen
    spart ~75 MB, Kandidat); Homebrew-Bibliotheken fallen weg.
  - MSI −4 096 B, ZIP +315 B.
  - AppImage ±0: das squashfs ist auf 4 KiB aufgefüllt; Dateiliste und Einzelgrößen gleich, im
    Inhalt unterscheiden sich nur `usr/bin/qtmux` und das Echo-Plugin.

**Live:**
- serial `1795655722`, Publish 2026-11-26 02:16.
- Index 12 Produkte, geändert nur qtmux; Signatur mit dem Client-Schlüssel gültig, Gegenprobe
  ungültig; DMG, MSI und AppImage von live geladen und byte-gleich.
- Die Notes, im Dialog des 1.10.0-Clients gerendert (de/en): nichts abgeschnitten.
- Pflichtprobe aus dem Updater von 1.10.0: vor dem Publish rc 20 (Positivkontrolle Replay
  rc 11), danach rc 0 mit dem macOS-Paket (55 257 600 B, `cmp`-gleich); Replay danach rc 11.
- GitHub-Release `v1.10.1` mit vier Assets, je zurückgeladen und `cmp`-gleich, Latest.

**Ungemessen:** der Start auf einem echten macOS 13 (belegt sind nur `minos` und plist) und das
echte Selbst-Ersetzen von 1.10.0 auf 1.10.1 am Rechner.

## Release v1.10.0 (2026-11-21)

**Inhalt:** gemeinsamer Update-Ablauf (Vertrag V2) mit QML-Oberfläche, Selbst-Ersetzen ab dem
Update nach 1.10.0, „wirkt sofort“ auf der Seite „Updates“ (QTMUX-160; Folgeticket QTMUX-161).

**Commits:** Tag `v1.10.0` (annotiert) auf `4959c8a` („Release 1.10.0“). Danach `287dc22`: Fix
in `installer/build-msi.ps1`, COM-Aufruf `OpenDatabase` mit `[string]$msi`. Ohne die Umwandlung
scheiterte er mit `DISP_E_TYPEMISMATCH`, und die eigene Paketprüfung löschte beim ersten
Windows-Lauf das MSI. Die App-Quellen sind gleich. DMG und AppImage tragen `1.10.0+4959c8a`,
MSI und ZIP `1.10.0+287dc22` (Entscheid: den gepushten Tag nicht verlegen).

| Artefakt | Bytes | SHA256 | Build-ID |
|---|---|---|---|
| `QTmux-1.10.0-macos.dmg` | 48 766 464 | `9bda8791af3ab4f3d4d517e795d1f78f3c22ba47ef5205c1443c8b5a2208c9f8` | `1.10.0+4959c8a` |
| `QTmux-1.10.0-win64.msi` | 32 271 579 | `05ccd930b7f968ada0265c7b258bf65bbe7e18a179fd674cff3630be2ced04c8` | `1.10.0+287dc22` |
| `QTmux-1.10.0-win64-portable.zip` | 39 684 572 | `dab3c9283cca0dbaf18203f9c11aa741af5c440f0a7ca657ea3938b125887a39` | `1.10.0+287dc22` |
| `QTmux-1.10.0-x86_64.AppImage` | 45 255 160 | `149568b712d1a64caaa985b26771f443a32671a1fa19130b94884c2de752c2f8` | `1.10.0+4959c8a` |

**CI:**
- Messläufe 44880347502 (`4959c8a`) und 44882879935 (`287dc22`);
- main-Läufe 44881268948 und 44883838609;
- Tag-Lauf 44882170471 (Matrix 3/3, `linux-release`).
- Je Job: Linux 68/68, Windows 67/67, macOS 69 mit zwei Skips (Szene 15, siehe unten).

**Wege:**
- DMG am Mac aus einem frischen Worktree auf dem Tag, `FAMILY_CODESIGN=require`.
- MSI und ZIP auf dem Windows-Bau-Rechner als `claude` aus einem frischen Klon auf `287dc22`,
  mit einer Kopie des Wrappers, deren `cd` geändert ist.
- AppImage aus dem Tag-Lauf (Draft-Asset).
- 🔑 Der dokumentierte Klon des Wrappers wurde zuletzt am 2026-11-03 für 1.9.10 benutzt (Reflog,
  letztes MSI dort 1.9.10). Woher die MSIs 1.9.11 bis 1.9.15 kamen, steht in dieser Datei nicht.
  Darum baut 1.10.0 aus einem frischen Klon. Die Wrapper-Stelle in der `CLAUDE.md` ist damit ein
  veralteter Anker; die Korrektur kommt auf Owner-Wort.

**Abnahme:**
- DMG: DR byte-gleich zu 1.9.15; `update-swap` mit `com.qtmux.app.update-swap`; 169/169 Mach-O
  familien-signiert; `CFBundleVersion` 1.10.0.
- MSI: Paketprüfung grün; `SetARPINSTALLLOCATION` nach `CostFinalize`; CRT zehn DLLs gleich 1.9.15;
  `update-helper.exe` mit MZ.
- AppImage: nur `libqxcb`, glibc höchstens 2.38, Start unter xcb mit xvfb.
- Größen über Dateilisten erklärt: DMG −11 468 645 B trotz +828 985 B Inhalt (`diskutil image`
  statt `hdiutil`), MSI +246 320, AppImage +266 240, ZIP +301 755.

**Gemessen, nicht geändert:** App und `update-swap` sind **nur arm64**, `minos 27.2`,
`LSMinimumSystemVersion` leer. 1.9.15 ist genauso; ein universales QTmux-DMG gab es nie, obwohl
der Manifest-Schlüssel `macos-universal` heißt (Owner-Frage).

**Szene 15 auf dem GitHub-macOS-Runner:**
- Belegt ist, dass die Verbindung auf `localhost` in 15 s nicht beim Server auf 127.0.0.1
  ankommt; die Auflösung ist `[::1, 127.0.0.1]`.
- In Lauf 44876744240 galt dort ein `connect` auf einen geschlossenen Port als verbunden. In
  44880347502 meldete der Client am geschlossenen Port und bei `localhost` „verbunden: nein“.
  Der Runner verhält sich von Lauf zu Lauf verschieden. Die frühere Lesung „der Runner nimmt
  Verbindungen zu geschlossenen Ports an“ gilt nicht allgemein.
- Die Probe zählt die Ankunft am eigenen Server und trägt beide Fälle.
- Rücknahme beim nächsten Vendoring (QTMUX-161); der Wächter `test_scene15_skip_retired`
  erzwingt sie.

**Live:**
- serial `1795249486`, Publish 2026-11-21 09:25.
- Signatur gegen die Client-Bytes gültig, Gegenprobe ungültig; drei Artefakte per Cache-Bust
  `cmp`-gleich; Index 12 Produkte, geändert nur qtmux.
- Neuer Client: 1.9.15 → verfügbar, 1.10.0 → aktuell, Replay und falsches Produkt abgelehnt.
- Pflichtprobe aus dem Updater von 1.9.15: vor dem Publish rc 20 (Positivkontrolle Replay
  rc 11), danach rc 0 mit dem macOS-Paket (48 766 464 B, `cmp`-gleich).
- Der 1.9.15-Client zeigt die Notes einmal roh (`TextArea` ohne `textFormat`).
- GitHub-Release `v1.10.0` mit vier Assets, je zurückgeladen und `cmp`-gleich, Latest.

## 2026-11-19 — CLAUDE.md-Diät

Die `CLAUDE.md` hatte 117 195 Zeichen und lag zusammen mit der Workspace-Datei über der Grenze,
ab der Claude Code beim Start warnt. Schicht-Wissen ging in Schicht-`CLAUDE.md`-Dateien, die
folgenden Absätze hierher.

### Releases v1.9.15 bis v1.9.0 (aus „Arbeitsstand & Wiedereinstieg“)

**Ausgeliefert: v1.9.15 — Wartungsrelease (2026-11-17, „RELEASE FREI QTMUX“ nach Hub 0.7.0 und RAFTNG 0.11.0).**
Tag `v1.9.15` (annotiert) auf dem Bau-Commit `2000c70`, Bump 15/15 Zeilen. Inhalt gegenüber 1.9.14:
Update-Härtung V1 (QTMUX-159, `539ae88`). Alle vier Artefakte aus `2000c70`, Build-ID `1.9.15+2000c70` je
Artefakt EINZELN gemessen (ASCII + UTF-16 je 1×, kein `-dirty`/`+unknown`, 0 Reste von `1.9.14`/`78086dd`/
`33aab7d`/`539ae88`):

| Artefakt | Bytes | SHA256 |
|---|---|---|
| `QTmux-1.9.15-macos.dmg` | 60 235 109 | `48deb85aa63bb3929a842a6fed65fbac43113bcebcf96b540b7caf96b60d94e4` |
| `QTmux-1.9.15-win64.msi` | 32 025 259 | `864ee0e880c068feddd9b440afdb09e0108ca7ea1b26d09e34a5bfc5a98f21cb` |
| `QTmux-1.9.15-win64-portable.zip` | 39 382 817 | `606dda3ad9c8026d664dac8bd0ae14df4e5fe3e337dc8f0ef3ab22bc8149be66` |
| `QTmux-1.9.15-x86_64.AppImage` | 44 988 920 | `13ab7137e5b63755d4e11fe227efab7bb0909bd05e6906a50296b9133fe69f02` |

CI: main-Lauf `43967061676`, Tag-Lauf `43968245072` (4/4), je macOS und Linux 54/54, Windows 52/52;
`test_updatehardening`, `test_settingsio`, `test_updater`, `test_updateviewmodel` in jedem Job als Passed
gelesen. DMG: DR `certificate root`, 168/168 Mach-O, Plugin + PCBUSB enthalten. Live-Manifest mit
`serial 1794873078`, Signatur gegen die Client-Bytes gültig, beide Gegentests fallen, drei Artefakte
`cmp`-gleich, Index-Diff = qtmux `1.9.14`→`1.9.15` plus Top-Level `updated` (Publish nach Mitternacht).
Abnahme mit dem neuen Client (eigenes Profil, stille Startprüfung, gemerkter Stand in den QSettings): nimmt
das Live-Manifest über HTTPS und `file://` an; ein gültig signiertes RAFTNG-Manifest unter `/qtmux/`, ein
höherer gemerkter Stand und ein Manifest ohne `serial` lassen den Stand unverändert. Benutzerdoku dual
(on-prem v32, Cloud v31), QTMUX-159 dual Done mit Auslieferungs-Kommentar.
🔑 **Größen gegen 1.9.14 am Inhalt erklärt:** Dateilisten in allen vier Paketen gleich, größer ist allein das
Hauptbinary (`qtmux` +17 280, `qtmux.exe` +14 336, `usr/bin/qtmux` +16 384; die Plugins linken den Updater
nicht). DMG −134 877 B (hdiutil-Streuung), MSI +8 192, ZIP +6 295, AppImage +8 192.


Davor: **v1.9.14** — Wartungsrelease (2026-11-15, `78086dd`, Build-ID `1.9.14+78086dd`, Vorrang der Eingabe +
`send_message`, QTMUX-157; Artefakttabelle in der Git-Historie dieser Datei).

Davor: **v1.9.13** — Wartungsrelease (2026-11-15, `1531102`, Build-ID `1.9.13+1531102`, Rückfrage vor dem
Öffnen ausführbarer Dateien, QTMUX-151 §8.4; Artefakttabelle in der Git-Historie dieser Datei).

Davor: **v1.9.12** — Wartungsrelease (2026-11-15, `d3a7979`, Build-ID `1.9.12+d3a7979`, QTMUX-40/154/155;
Artefakttabelle in der Git-Historie dieser Datei). Dauerhaft daraus:
(Die beiden Lehren aus 1.9.12 stehen in [installer/CLAUDE.md](../installer/CLAUDE.md),
„Release-Lehren“, und in [.github/CLAUDE.md](../.github/CLAUDE.md).)


Davor: **v1.9.11** — Wartungsrelease (2026-11-05, `b1137f4`, Build-ID `1.9.11+b1137f4`, QTMUX-150…152;
Artefakttabelle in der Git-Historie dieser Datei). Dauerhaft daraus: Die Plugins linken `qtmux_core`
statisch und wachsen deshalb ohne eigene Quelländerung mit, sobald `VtScreen` neuen Code mitzieht.

(Der Absatz zu v1.9.10 steht als Release-Lehre in [installer/CLAUDE.md](../installer/CLAUDE.md).)


Davor: **v1.9.8 — Wartungsrelease (2026-10-10, letztes Release der Familien-Runde
MacPCAN 0.6.0 → RAFTNG 0.10.1 → QTmux, Orchestrator-Auftrag mit Owner-Freigabe).** Tag
`v1.9.8` (annotiert) auf dem Bau-Commit `58931b0` (Bump aller sechs manuellen Stellen,
15/15 Zeilen). Inhalt gegenüber 1.9.7: **QTMUX-141** (`5f4f9c6`), **QTMUX-142** (`1b01b4d`),
**QTMUX-143** (`ff46008`), **QTMUX-144** (`b0bb26a`). Vendoring: Hub `origin/main`
`538a392` (sauber), Wächter 4/4 byte-identisch (17/8/5/1 Dateien) → kein Nachzug, Pin in
`UPSTREAM.md` unverändert. **Alle vier Artefakte aus `58931b0`, Build-ID `1.9.8+58931b0` je
Artefakt EINZELN gemessen** (ASCII + UTF-16 je 1×, kein `-dirty`, kein `+unknown`, 0 Reste
von `1.9.7`/`961d262`; liegen unter `dist/`):

| Artefakt | Bytes | SHA256 |
|---|---|---|
| `QTmux-1.9.8-macos.dmg` | 62 202 753 | `c460e0d8409e2930d6dfbc1530e0eafd69edbc0b0a1182cbd2c0f73326bd83fb` |
| `QTmux-1.9.8-win64.msi` | 31 935 146 | `afb594ca26f3842336e19182831664fdedd398c81dbc2e28359f343dc87505e8` |
| `QTmux-1.9.8-win64-portable.zip` | 39 286 932 | `e71e7ef4ebc5513e19ece2b4b405239251af5d8d1b9e4a7c0f3cb08af0c5780f` |
| `QTmux-1.9.8-x86_64.AppImage` | 44 874 232 | `985ff8b831e4c798c6a2de37b310b7cd0d489407cfe30354b1c76720902b5055` |

**Kern am fertigen Artefakt belegt:** (1) gemountetes DMG (`FAMILY_CODESIGN=require`):
DR `certificate root = H"015c9a18…"`, `codesign --verify --deep --strict` grün, **168/168
Mach-O** mit `Authority=RealNobser Code Signing` + derselben DR (gezählt per `codesign -dvv`),
47/47 Frameworks — dieselbe DR noch einmal am vom Update-Server **heruntergeladenen** DMG.
(2) MSI: Riegel im Bau-Log `QTmuxStartMenu ok (icon from target)` + `msi-shortcut-icon-check:
OK: 1 shortcut(s), none uses the MSI Icon table.`, 1619-Smoke OK (1423 Dateien); Icon-Stream
`qtmux.ico` = Repo-`resources/appicon/qtmux.ico` bytegleich (30 147 B, Kopf `00000100`), 7/7
ICO-Bilder in `qtmux.exe`; CRT per Zählung im entpackten MSI 10 DLLs wie 1.9.7 (⚠️ QTmux hat
**keinen** harten CRT-Riegel im `build-msi.ps1` wie RAFTNG RAF-181 — nur `$ErrorActionPreference
= "Stop"`; Folgekandidat). (3) App aus dem gemounteten DMG (Profil `rel198`, Port 7352,
`env -i`, Objekt-Bindung per `get_server_info` vor jedem Aufruf): `echo PROBE198_$((6*7))`
→ `42`; `--screenshot` mit `QT_QPA_PLATFORM=cocoa` in beiden Designs, Statuspunkt pixelgenau
Hell `#078737` / Dunkel `#46D369` (= QTMUX-142-Sollwerte).
🔑 **Größen gegenüber 1.9.7 — am Inhalt erklärt:** DMG **−1 080 673 B**, gemounteter Inhalt
**+16 800 B** (2006 = 2006 Dateien; nur `qtmux` größer, 167 Mach-O gleich groß mit neuem
Signaturblock, `Info.plist`) → hdiutil-Streuung · MSI **±0 B** (Sektorrundung; entpackt 1422 =
1422, nur `qtmux.exe` +1 536) · ZIP **+803 B** (1456 = 1456, nur `qtmux.exe` +1 536; ZIP-Exe =
MSI-Exe) · AppImage **±0 B** (1672 = 1672; gleich groß, aber anders: `usr/bin/qtmux`,
Echo-Plugin (64 B, keine Quelländerung) und `usr/lib/libpng16.so.16` (gleiche Version 1.6.43,
Neubau aus dem Runner-Image `20261004.327.1` — Klasse wie libgcrypt bei 1.9.4)).
CI: main-Lauf `38079074472` 3/3 grün (macOS/Linux 37/37, Windows 35/35 — ohne `test_pty` und
`test_check_updater_sync`; `test_icons`/`test_statuscolors` je `Passed`, `test_check_updater_sync`
auf macOS/Linux `Passed`), Tag-Lauf `38079568827` 4/4 grün (Draft mit AppImage, sha256 =
Joblog = GitHub-Digest = Download). Publish in der Owner-Permission-Form ohne Harness-Block
(Manifest im 2. Versuch bytegleich); eigene Abnahme: `index.json`-Diff = genau qtmux
`1.9.7`→`1.9.8`, 12 Produkte, `updated` blieb `2026-10-10`; Manifest-Signatur gegen die
Client-Bytes aus `UpdateKeys.hpp` gültig, 1-Byte-Gegentest fällt; drei Artefakte per Cache-Bust
zurückgeladen, sha256 = Manifest, `cmp`-gleich. GitHub-Release `v1.9.8` (Target voller SHA,
vier Assets je Rückladung `cmp`-gleich, Latest). Benutzerdoku dual (on-prem v26, Cloud v25,
Absatztext beidseitig gleich zurückgelesen): Stand-Absatz. Release-Notes mit Neu-Anheften-
Hinweis; den macOS-Satz „keine erneute Ordnerabfrage" bewusst NICHT, weil ungemessen.

(Die Absätze zu v1.9.7 bis v1.9.2 stehen als Release-Lehren in der `CLAUDE.md` und in
[installer/CLAUDE.md](../installer/CLAUDE.md). Ebenso dort: „AppImage lokal messbar“ aus dem
Absatz zu v1.9.8.)

Davor: v1.9.1 (einzige Änderung ggü. 1.9.0: Windows-Updater-Fix msiexec 1619 aus dem Hub,
Pin in `third_party/updater/UPSTREAM.md`) · v1.9.0 (Meilenstein, voller Update-Zyklus am
lebenden Objekt, macOS-Zweig); Auslieferungs-Sollwerte in
[docs/update-regressionsliste.md](docs/update-regressionsliste.md). Jira dual synchron bis
**QTMUX-158** (2026-11-15 beidseitig als höchste Nummer gelesen; 158 Folgepunkte zu `send_message`, offen; 157 Vorrang der Eingabe + `send_message`, beidseitig Done am Ticket gelesen; 156 Erlaubnisliste für die Rückfrage, offen; QTMUX-151 trägt den Nachtrag zu 1.9.13 und bleibt Done; davor QTMUX-40 OSC 8, 154 umbrochene Links, 155 Kopieren unter ConPTY, 2026-11-15 beidseitig Done am Ticket gelesen; davor QTMUX-150…152 Auswahl, Links, Netzpfade, 2026-11-05 beidseitig Done am Ticket gelesen, 153 Resize-Befund bewusst offen; davor QTMUX-149 ConPTY-Fixes aus dem Clink-Befund, 2026-11-03 beidseitig Done am Ticket gelesen; davor QTMUX-148 alte `.part`-Reste eines harten Endes räumt der nächste Download ab, Hub `4a645a4` vendiert, + TMPDIR-Messung macOS-CI, 2026-10-16 dual angelegt; davor QTMUX-147 Download ersetzt den Zielnamen statt hineinzuschreiben, Hub `f2d9ac7` vendiert, 2026-10-15 dual angelegt; davor QTMUX-146 Update-Dialog ohne Rechtsklick-Rat + Hub-`quarantineState`, 2026-10-14 beidseitig Done am Ticket gelesen; davor QTMUX-145 deutsche Oberfläche siezt + Wächter, Proxy-Art-Auswahl, 2026-10-14 beidseitig Done am Ticket gelesen; davor QTMUX-144 check-updater-sync ohne grünen Nachweis ohne Messung, 2026-10-10, beidseitig Done am Ticket gelesen; davor QTMUX-143 Startmenü-Verknüpfung ohne MSI-Icon-Tabelle, 2026-10-10; davor QTMUX-142 Statusfarben als Theme-Rollen, 2026-10-10; davor QTMUX-141 Icon „Im Finder zeigen" + Wächter, 2026-10-08 dual angelegt; davor 139
Stillstand, 140 macOS-Signatur — am 2026-10-07 beidseitig Done am Ticket gelesen; davor QTMUX-138 „Im Finder/Explorer zeigen",
2026-09-30, beidseitig Done am Ticket gelesen; davor
QTMUX-137 Windows-App-Icon 2026-09-29; seit QTMUX-136 nicht neu gezählt — rechnerisch 112/26). Davor **110 Done / 26 offen**
(2026-09-28 nach QTMUX-136 exakt gezählt, beide paginiert über alle 136 Tickets; nach QTMUX-135
waren es 109/26, bis QTMUX-134 108/26 — Cloud per paginierter `search/jql`; ⚠️ Clouds
`search/approximate-count` meldete zur selben Zeit 107/27, der Index hinkt einer frischen
Transition hinterher. Vorher 107/26 bis QTMUX-133, gemessen im Jira-Audit 2026-09-06; QTMUX-64 im Projektabschluss geschlossen — durch QTMUX-81 abgedeckt —,
QTMUX-60 mit Vermerk bewusst offen; QTMUX-2 on-prem fehlte bis zum Audit der
Abschlusskommentar vom 30.07., am 2026-09-06 nachgezogen).

### Teststände (aus „Arbeitsstand & Wiedereinstieg“)

Den heutigen Stand führt die `CLAUDE.md`, Abschnitt „Teststände“; dort stehen auch die Zeilen zu
den Plattformen und der `test_session`-Befund aus dem Lauf zu 1.9.7, die hier fehlen.

**Teststände:** **54** Tests auf macOS/Linux, **53** auf Windows seit QTMUX-159 (Update-Härtung V1,
2026-11-16, unreleased: `updatehardening` kam hinzu; lokal 54/54 in frischem Debug- **und** Release-Baum
`build/wuh`, `build/wuhr`; die CI lässt unter Windows 52 laufen). Davor **53** und **52** seit QTMUX-157 (1.9.14, 2026-11-15,
`ownerdraft`, `inputpriority`, `agentinbox` und `mcpmessage` kamen hinzu; die CI lässt unter Windows 51
laufen, also ohne `test_pty`; main-Lauf `43758653045` und Tag-Lauf `43759524567` je 53/53/51). Davor 49 und
48 seit der Rückfrage (1.9.13, `executabletarget` und `execopendialog`). Davor 47 und 46 (s. Dateitabelle; per
`ctest -N` am 2026-11-04 gezählt — `screenlinks`, `terminalselection`, `terminalitem` kamen mit
dem Auftrag Links/Auswahl hinzu; davor am 2026-11-03 44 — `conptyreplay` kam mit dem Clink-Auftrag hinzu, lokal 44/44
in frisch angelegtem Debug- **und** Release-Baum (`build/wclink`, `build/wclinkr`); davor
am 2026-10-13 43 — `combowidth` + fünf `I18n.*` kamen mit QTMUX-145 hinzu,
lokal 43/43 in frisch angelegtem Debug- **und** Release-Baum, CI-Lauf `38486777601` macOS/Linux
43/43, Windows 41/41; davor am 2026-10-10 37 —
`check_updater_sync` kam mit QTMUX-144 hinzu und entsteht nur mit bash, also nicht auf
Windows; davor `statuscolors` mit QTMUX-142, `icons` mit QTMUX-141; lokal vor dem
1.9.8-Bump 37/37 in `build/macos-release`). macOS
(Fortsetzung „lässt `test_pty` mitlaufen; …“ in der `CLAUDE.md`.)

Grün-Messung: main-Lauf `43743829277` und Tag-Lauf `43744430216` (1.9.13, 2026-11-15) macOS/Linux
49/49, Windows 47/47 (dort zusätzlich ohne `test_check_updater_sync`), Tag-Lauf 4/4 grün. Davor main-Lauf
`43729246269` und Tag-Lauf `43729247628` (1.9.12) macOS/Linux 47/47, Windows 45/45. Fallzahlen der
betroffenen Programme, lokal gezählt (die CI druckt sie nicht): `test_screenlinks` 86, `test_vtscreen` 33,
`test_conptyreplay` 26, `test_terminalselection` 40, `test_terminalitem` 28. Davor main-Lauf
`42036454172` und Tag-Lauf `42037643668` (1.9.11) mit denselben Bilanzen. Davor
main-Lauf `38079074472` und Tag-Lauf `38079568827` (1.9.8) macOS/Linux 37/37, Windows 35/35; CI-Tag-Lauf `37687181937` (1.9.7) macOS/Linux 34/34, Windows 33/33;

### Wiedereinstieg vor der Diät (aus „Nächster Schritt“)

Die offenen Punkte daraus führt die `CLAUDE.md` als Liste weiter. Wörtlich in der `CLAUDE.md`
geblieben sind „Offen aus 1.9.7“, „Offen aus 1.9.6“, die Regel zum eigenen Commit-Hash, die
Publish-Mechanik und der nächste eigene Punkt QTMUX-94.

🔧 **2026-11-16: Update-Härtung Vertrag V1 (QTMUX-159), unreleased, Familienauftrag der
Orchestrator-Session.** Vendierter Kern auf Hub `2b560a8` (MAC-108, Pin in
`third_party/updater/UPSTREAM.md`): Produkt/Kanal gebunden, `serial` gegen Replay, Download auf
`size` begrenzt. QTmux speichert `update/last_serial/qtmux` (nicht exportiert, nicht
zurückgesetzt) und zeigt Ablehnungen mit eigener Überschrift. Mechanik, Wächter und
Mutationsproben: Feature-Referenz „Online-Update“, Absatz „Vertrag V1“. Release nur nach
Freigabe durch den Orchestrator, nach Hub und RAFTNG. Offen: Die QML-Überschrift
(`failureTitle`) ist im Test nur am ViewModel belegt, nicht am gerenderten Dialog; nach dem
Release prüft der neue Client sein Live-Manifest (trägt dann `serial`).

🔧 **2026-11-15: QTMUX-40, 154 und 155 ausgeliefert mit 1.9.12, Owner-Abnahme steht aus.** Umbrochene
Links: OSC 8 (libvterm-Patch 5), ein am Rand abgeschnittener Text-Link ist keiner und sagt warum, Schalter
`window/announceHyperlinks` (Vorgabe aus). Ein `file:`-Ziel aus OSC 8 öffnet nie, Format- und Steuerzeichen
im Ziel sind abgelehnt. Kopieren unter ConPTY: `VtScreen::joinsPreviousLine`, Leerraum am Rand ist kein
weicher Umbruch. Mechanik und Grenzen: Feature-Referenz „Klickbare Links“ und „Fortsetzungs-Marken unter
ConPTY“, Rezepte: [docs/owner-abnahmen.md](docs/owner-abnahmen.md) (Zeilen 40 / 154 und 155).
**Owner-Entscheide:** 2026-11-14 („1: Folge deiner Empfehlung / 2: so lassen / 3: ok / 4: beauftragen“):
Schalter bleibt mit Vorgabe AUS, Codeblöcke bleiben unklickbar, die Randregel bleibt. 2026-11-15 („C: B +
kein file: aus OSC 8“): die Härtung oben. **Weiter offen:** (1) Die Kopier-Regel ist eine Abwägung, einzeln
nicht entschieden, ausgeliefert ist „so lassen“. (2) Ein neueres ConPTY mitliefern, das die Ausgabe
durchreicht (Abhilfe an der Wurzel, eine Abhängigkeit mehr, nicht gemessen). (3) Die Rückfrage bei Strg-Klick
auf ausführbare Dateien kam erst mit 1.9.13, s. nächster Absatz. Am echten Fenster ist nichts davon gesehen, Claude Code
lief unter Windows nicht. An QTMUX-153 ändert dieser Stand nichts: Die Dopplung dort kommt von Claude Codes
klassischer Darstellung, die falschen Marken tragen nicht dazu bei.

🔧 **2026-11-15: Vorrang der Eingabe des Menschen + `send_message` (QTMUX-157), ausgeliefert mit
1.9.14, Owner-Abnahme steht aus.** Owner-Befund: Worker hängten ihre Meldungen an halb
getippte Sätze des Owners. Jetzt wartet die Warteschlange, solange der Mensch getippt und nicht
abgeschickt hat, `send_text`/`send_keys` lehnen ab (`force` überstimmt), und `send_message` stellt
Claude-Sitzungen über ihren Sitzungs-Socket zu, ohne die Eingabezeile. Mechanik und Grenzen:
Feature-Referenz (zwei Absätze „QTMUX-157“) und [docs/MCP.md](docs/MCP.md); Rezept:
[docs/owner-abnahmen.md](docs/owner-abnahmen.md) Zeile 157. 32 Mutationen rot (macOS). **Owner-Entscheide
2026-11-15 („1–5 wie empfohlen“):** kein Zeitablauf für den Merker; `crossSessionInbound` bleibt ungesetzt.
Offen beim Owner: SessionStart-Hook `qtmux-inbox.sh` in seine Einstellungen, sobald 1.9.14 bei ihm läuft
(ohne ihn nimmt `send_message` die Warteschlange), und die Abnahme am echten Fenster (QTmux kann keine
Tasten des Menschen nachstellen). Offen als QTMUX-158: Windows (Named Pipe mit Anmeldung, dort nur der
Warteschlangen-Weg), Zustellbeleg, Empfänger mit freien Rechten. 🔴 Das Rahmenformat des
Sockets ist undokumentiert und an Claude Code 2.1.323 belegt — nach einem Claude-Code-Update die Probe
(c) aus der Abnahme fahren.

🔧 **2026-11-15: Rückfrage vor dem Öffnen ausführbarer Dateien (QTMUX-151 §8.4), ausgeliefert mit 1.9.13,
Owner-Abnahme steht aus.** Owner-Wort „Ich würde sagen, JA!“. Ein Datei-Link aus dem Text, dessen Ziel das
System ausführen würde, öffnet erst nach einer Rückfrage; Vorgabe Abbrechen, kein „nicht mehr fragen“, die
Zeigen-Geste, URLs und Verzeichnisse fragen nicht. Mechanik, Listen und Grenzen: Feature-Referenz
„Klickbare Links“ (Absatz „Rückfrage“), Rezept: [docs/owner-abnahmen.md](docs/owner-abnahmen.md) Zeile 151.
**Sie sollte in 1.9.12 und kam zu spät:** Ich hatte die erste Antwort des Auftraggebers aus der eigenen
Warteschlange gelesen, bevor sie zugestellt war; sie wurde danach durch eine Fassung mit diesem zweiten
Entscheid ersetzt. 🔑 Eine Nachricht in der Warteschlange ist nicht gesendet, erst nach der Zustellung
handeln, und vor jedem Schritt nach außen (Push, Tag, Publish) die Warteschlange erneut lesen.
**Owner-Entscheid 2026-11-15 („1 ja / 2–4 so“):** Die Verbotsliste geht so raus; unter Windows fragen auch
Dateien ohne Endung; auf Laufwerken ohne Unix-Rechte fragt unter macOS und Linux jede Datei. Die Umkehrung
(nur bekannte, harmlose Typen ohne Frage öffnen, oder das System fragen) ist **QTMUX-156** und nicht
umgesetzt. Am echten Fenster ist der Dialog nie gesehen, belegt sind die Tests auf allen drei Plattformen.

🔧 **2026-11-05: QTMUX-150…152 ausgeliefert mit 1.9.11, Owner-Abnahme steht aus.** Auswahl markiert nie
mehr fremden Text; Link-Gesten treffen über Emoji und weiche Umbrüche und melden jedes Ergebnis in der
Pille; Netzpfade lösen keinen Zugriff aus (Mechanik: Feature-Referenz „Auswahl“, „Klickbare Links“; Rezepte:
[docs/owner-abnahmen.md](docs/owner-abnahmen.md)). Das zeitweise Versagen von Strg-Klick ist unter Windows
nicht am echten Desktop reproduziert, die Pille macht es jetzt unterscheidbar. Offen als Nebenbefund: Die
Scrollback-Suche nutzt weiter die Spalten-Zeichen-Gleichsetzung (`absLineText`) und setzt ihre Treffer
hinter Emoji daneben; `m_matches` rutschen auch an der Scrollback-Grenze. Die zwei damals offenen Owner-Entscheide aus
QTMUX-151 sind entschieden: OSC 8 ist mit 1.9.12 ausgeliefert, die Rückfrage bei Strg-Klick auf
ausführbare Dateien mit 1.9.13 (s. oben).

🔧 **2026-11-04: Owner-Befund „Windows + Clink + Resize gibt immer noch Artefakte“, QTMUX-153 offen.**
Ursache gemessen: Claude Codes **klassischer** Renderer, nicht ConPTY, nicht Clink, nicht der Reflow
(Feature-Referenz, „Doppelter Inhalt mit Claude Code“). Kein Code-Fix in QTmux, nur Fälle in
`test_conptyreplay` und Doku, beides mit 1.9.11 ausgeliefert. Empfehlung: `/tui fullscreen` beim Owner
messen; die Alternative wäre ein Textvergleich, der Scrollback löscht, im Prototyp nicht zuverlässig. Ein
neuer Verlauf ist dabei Pflicht, der alte trägt die Dopplung als Inhalt.

🔧 **2026-11-03: QTMUX-149 ausgeliefert mit 1.9.10, Owner-Test unter Windows steht aus.** ConPTY-Reflow
bei Größenänderung + Verlauf vor dem ConPTY-Start-`ESC[2J` in den Scrollback (Mechanik in der
Feature-Referenz, „ConPTY malt nach jeder Größenänderung neu“ und QTMUX-130-Absatz). libvterm-Reflow
braucht Patch 4 (s. Abschnitt libvterm). Am echten Windows-Fenster bisher nicht gesehen, belegt sind
die Bytes im Replay. Offen: Symptom „Auto-Copy“: QTmux verhält sich korrekt, ConPTY liefert mit und
ohne Clink dieselben Bytes; Kandidat ist Claude Codes Renderer-Wahl (Vollbild/klassisch, s.
`docs/e2e-fallen.md`), für cmd+Clink nicht am echten Programm belegt — Einzeltest beim Owner:
`set CLAUDE_CODE_NO_FLICKER=1`, dann `claude`.

✅ **2026-10-16: QTMUX-148 — alte `.part`-Reste räumt der nächste Download ab, unreleased
(Familien-Auftrag „0124“, Owner-Freigabe).** Hub-Updater auf `4a645a4` (MAC-99) vendiert,
nur `UpdateChecker.cpp` geändert, Wächter 19/8/5/1, `ls-tree`-Vergleich blob-gleich 19/19,
UPSTREAM.md-Pin + Abgleich. Mechanik, Riegel, Wächter und Mutationsproben: Feature-Referenz
„Online-Update“ (Absatz „Reste eines harten Endes“) — 2 neue Fälle in `tst_updater`, 1 in
`tst_updateviewmodel`, alle Plattformen, gegen den vorherigen vendierten Stand alle rot.
Frische Bäume `build/w148` (Debug) + `build/w148r` (Release) je 43/43. **TMPDIR-Umleitung der
macOS-CI bewusst NICHT übernommen** (Begründung und Belegzeilen im CI-Abschnitt). Gepusht
f0af19a..26af17f (11:13:54) und ..9bdae8a (11:22:36, nur Belegzeile mit Namen), Messung der
vier Repos davor angesagt; CI `38879028974` und `38880025712` je macOS 43/43, Linux 43/43,
Windows 41/41 (je Job: Checkout des Commits, `test_updater` + `test_updateviewmodel` Passed —
die drei neuen Funktionen haben keinen Skip-Pfad, ctest druckt die QtTest-Zeilen bei Erfolg
nicht). Jira QTMUX-148 dual Done am Ticket gelesen.

✅ **2026-10-15: QTMUX-147 — Download ersetzt den Zielnamen statt hineinzuschreiben,
unreleased (Familien-Auftrag „0.12.3“, Owner-Freigabe).** Hub-Updater auf `f2d9ac7` (MAC-95)
vendiert, Wächter 19/8/5/1, `ls-tree`-Vergleich blob-gleich 19/19, UPSTREAM.md-Pin +
Abgleich. Mechanik, Messung und Wächter: Feature-Referenz „Online-Update“ (Absatz
„ERSETZT den Zielnamen“) — 4 neue Fälle in `tst_updater` (alle Plattformen, Symlink nicht
unter Windows), 1 in `tst_updateviewmodel` (macOS); gegen den vorherigen vendierten Stand
alle fünf rot. Kein bestehender Test hielt das alte Überschreiben fest (keiner legte eine
Datei an den Zielnamen). Frische Bäume `build/w147` (Debug) + `build/w147r` (Release) je
43/43. Gepusht bb09375..e396055 (21:40:43, Messung der vier Repos davor angesagt); CI
`38795855466` Linux 43/43, macOS 43/43, Windows 41/41 (je Job: Checkout e396055,
`UpdateChecker.cpp` gebaut, `test_updater` + `test_updateviewmodel` Passed); Jira QTMUX-147
dual Done am Ticket gelesen. ⚠️ Offen: ob Windows eine offen gehaltene Datei ersetzt oder
ablehnt, steht NICHT im CI-Log (ctest druckt die `qInfo`-Zeile von
`aFileHeldOpenGetsAnAnswerNeverAHang` bei Erfolg nicht) — beide Antworten erfüllen die Zusage,
gemessen ist nur „keine Hänger, kein Rest“. 🔑 `git push` kennt kein `--ff-only` (druckt nur
die Hilfe, pusht nichts); ein normaler Push ohne Force verweigert Nicht-Fast-Forwards ohnehin.

✅ **2026-10-14: QTMUX-146 gepusht, unreleased (Orchestrator-Auftrag, Owner-Freigabe).** Der
Update-Dialog riet unter macOS zu „Rechtsklick → „Öffnen““ — seit macOS 15 wirkungslos, und
der Update-Download trägt gemessen keine Quarantäne (Messung + Wächter: Feature-Referenz
„Online-Update“). Neu: `UpdateViewModel::installHint` / `qtmux::updatehint` sagt je Plattform,
was „Installieren …“ tut, „Dennoch öffnen“ nur bei gemessener Quarantäne der geladenen Datei;
Wächter in `tst_updateviewmodel` (6 neue Fälle) mit sechs Mutationsproben rot. Hub-Updater
auf `aa73f21` (MAC-94) vendiert: neu `Quarantine.{hpp,cpp}`, `appupdate::quarantineState`
trägt die Messung (`Unreadable` → keine Abhilfe, aber `qWarning`); Wächter 19/8/5/1,
UPSTREAM.md-Pin + Abgleich. Frische Bäume `build/w146b` (Debug) + `build/w146r2` (Release)
je 43/43. Gepusht 0931170..d1ad0d3 (10:39:39, Messung der vier Repos davor angesagt); CI
`38536691210` macOS 43/43, Windows 41/41, Linux 43/43 (je Job: Checkout d1ad0d3,
`Quarantine.cpp` gebaut, `test_updateviewmodel` Passed); Jira QTMUX-146 dual Done am Ticket
gelesen. Bilder vorher/nachher
liegen außerhalb des Repos (Pfad in der Meldung an den Orchestrator). Der frühere Benutzerdoku-Satz
zum selbständigen Einblenden des Hinweises wurde mit 1.9.10 ersetzt (2026-11-05 dual nachgemessen: 0 Treffer).

Davor: Stand **2026-10-13: QTMUX-145** (deutsche Oberfläche siezt + Wächter; Proxy-Art-Auswahl
fiel zur Caret-Pille zusammen → `AppComboBox` misst den längsten Eintrag + `test_combowidth`)
gepusht und **unreleased**; CI-Lauf `38486777601` grün (Windows 41/41, macOS/Linux 43/43, die
sechs neuen Tests je Job als `Passed` gelesen), Jira QTMUX-145 dual Done am Ticket gelesen.
`git log --oneline origin/main..HEAD` muss leer sein. Kandidat (nicht beauftragt):
`AppComboBox` hat keinen eigenen Disabled-Zustand — in einer deaktivierten Zeile bleibt ihr
Text voll kontrastreich, während die Textfelder daneben ausgegraut sind. ⚠️ **`build/macos-test` und `build/macos-release` sind
verschmutzt** (2026-10-13 gemessen): inkrementell gebaut stürzen `test_windowmodel`/
`test_sessiongroups` (+ `test_mcpfocus` in Debug) mit SIGSEGV, derselbe Arbeitsbaum und der
Ausgangsstand `ead87a3` frisch gebaut sind grün — vor der nächsten Messung dort neu anlegen
(Verdacht, ungemessen: Homebrew-Qt-Revision 6.11.2_1 vom 2026-10-10 nach dem letzten Vollbau).
Davor (2026-10-10, nach Release 1.9.8) lag seit `v1.9.8` nur Doku auf `main` (Messkommando
`git log --oneline v1.9.8..origin/main`). Die vier
Tickets QTMUX-141…144 sind mit 1.9.8 ausgeliefert (Mechanik je in der Feature-Referenz bzw.
im Abschnitt „Vendoring-Wächter"). Folgekandidat aus dem 1.9.8-Release: harter CRT-Riegel in
`installer/build-msi.ps1` nach RAFTNG-Vorlage RAF-181 (Ticket nur auf Zuruf).
Release-Endstand im „Ausgeliefert"-Absatz oben; Rolle: **Standby**, Arbeit kommt per
Owner-Zuruf (Individual-Entwicklung).

**Rolle derzeit: Standby-Worker des Orchestrators** (Session 1 im Workspace; Rückmeldungen
als `MELDUNG QTMUX [FERTIG|FRAGE|ABBRUCH]: …` per `queue_text`). Zuletzt erledigt
(2026-10-10): **Release 1.9.8 komplett** (QTMUX-141…144, letztes Release der Runde nach
MacPCAN 0.6.0 / RAFTNG 0.10.1). Davor
(2026-10-07): **Release 1.9.7 komplett** (QTMUX-139/140, erstes Release mit Familien-Signatur).
Davor (2026-09-30): **Release 1.9.6 komplett** (QTMUX-137/138, erstes Release nach dem
QTMUX-136-Rezept). Davor (2026-09-28): **Release 1.9.5** (QTMUX-135) und am selben Tag
**Release 1.9.4** inkl. QTMUX-134 (beides im „Ausgeliefert"-Absatz).
Davor (2026-09-04): **Release 1.9.3 komplett** — Bump, Tag, CI, vier Artefakte, Publish mit
eigener Abnahme, GitHub-Release, Confluence dual (alles im „Ausgeliefert"-Absatz). Davor
(2026-08-18): **Familien-Schlussrelease komplett** — v1.9.1-Release nachgezogen, v1.9.2
gebaut/publiziert/getaggt, Jira-Abschluss (64 zu, 60 vermerkt), Benutzerdoku beider
Confluence-Instanzen auf 1.9.2 (alles im „Ausgeliefert"-Absatz). Davor
(2026-08-17/18): Betroffenheitsmessung der Vendoring-Kontrakte gegen Hub `f0e22f9` —
0 Treffer, Wächter 3/3 „byte-identisch" (die Hub-Commits liegen in `src/ota/`,
`src/deviceupdate/`, `app/control/` — außerhalb aller drei Kontrakte, KEIN Nachzug nötig;
die UPSTREAM.md-Pins bleiben korrekt) · `retention-days: 7` am einzigen
upload-artifact-Step der CI (Wirkung belegt: frisches Artefakt expires exakt +7 Tage —
Hintergrund: die 0,5-GB-Actions-Quota war am 2026-08-17 voll, QTmux mit ~7,2 GB aus
164 Läufen × 90-Tage-Default der Haupttäter; Owner hat alt aufgeräumt).

✅ **Startup-Check-Paar mit 1.9.4 umgesetzt** (Koordinator-Entscheid 2026-08-07): Key
`update/auto_check` **mit Migration** vom alten `update/autoCheck` (Start **und**
Import alter Exportdateien) und Drossel-Zeitstempel **vor** dem Request — Mechanik und
Wächter in der Feature-Referenz „Online-Update".
Der Vertrag selbst war in QTmux bereits vollständig erfüllt — es wurde **nichts** nachgebaut.

### Weitere verschobene Absätze

Erledigte Tickets aus „Offene Jira“ der `CLAUDE.md`:

**122/123** (OSC-52-Zwischenablage · Hinweis bei App-Maus) sind seit `162f079` in `main`
und mit 1.8.1 **ausgeliefert** — Mechanik in der Feature-Referenz; Jira-Status dual
**Done** (am 2026-09-06 per REST in beiden Systemen gemessen). ·
**40** (OSC-8-Hyperlinks — mit 1.9.12 ausgeliefert, über eine Kennung im libvterm-Stift statt
Span-Tracking; Ticket dual Done) ·

Aus „Jira-Statuskonvention & Lektionen“:

**Bewusst NICHT als Ticket angelegt:** Qt 6.10.3 lokal + Preset-Reihenfolge — steht
vollständig im Windows-Preset-Kasten oben; ein sofort geschlossenes Ticket ohne eigenen
Inhalt wäre nur Rauschen im Board.

### Zuletzt abgeschlossen (Mechanik in der Feature-Referenz, Verlauf in Git)

QTMUX-124 bis **131** (u. a. Online-Update, MCP im LAN, Proxy, „Link kopieren") sowie der
familienweite **AppImage-Tool-Pin** (`d782a5a`) — alle selbst verifiziert und in `main`;
die dauerhaften Lektionen stehen jeweils im zuständigen Fachabschnitt, nicht hier.

### Online-Update bis 1.9.15 (eigener Automat, aus der Feature-Referenz)
Wörtlich die Fassung aus `docs/feature-referenz.md` bis zum Umstieg auf den gemeinsamen Ablauf (K2, 1.10.0). Die Mechanik dieser Absätze liegt seither im Hub; die Wächter-Namen sind die damaligen.


Kern **byte-identisch aus MacPCAN vendiert** (`third_party/updater/update/`, Namespace
`appupdate`, Target **`qtmux_updater`** = STATIC + Qt6::Core/Network). Bewusst **nicht** in
`qtmux_core` — der bleibt Qt6::Core-only. Abgleich `tools/check-updater-sync.sh`
(`--update` zieht nach; Exit-Codes, Positivkontrolle und relative `MACPCAN_DIR` im
CLAUDE.md-Abschnitt „Vendoring-Wächter", QTMUX-144), Herkunft/Pin in
[third_party/updater/UPSTREAM.md](third_party/updater/UPSTREAM.md).
🔑 **Die Kopie liegt in einem Verzeichnis namens `update`**, weil sich der Kern selbst mit
dem Präfix `update/` inkludiert; flach vendiert müsste man jede `#include`-Zeile ändern und
gäbe die Byte-Identität — und damit den Sync-Wächter — auf.
⚠️ **Einbahnstraße:** nie lokal editieren. Erlebt und bewährt: Der Windows-Build fand einen
Fehler IM KERN (s. u.); der Fix ging nach MacPCAN, wurde dort gepusht und kam per
`--update` zurück.

📐 **Der Dialog ist als Design-Spezifikation ausgeschrieben:**
[docs/update-dialog-spec.md](update-dialog-spec.md) — Zustände und Übergänge, Layout und
Maße, Elementmatrix je Zustand, **alle** Texte in DE und EN, Verhalten (Skip, Abbrechen je
Phase, Fortschritt, Übergabe an den Installer) und ein Abschnitt „bewusste Entscheidungen".
Anlass: Der Owner hat ihn zum **Vorbild für alle drei Desktop-Apps** erklärt (2026-08-07);
der geteilte Widgets-Dialog entsteht danach im Hub. Wer hier etwas ändert, ändert damit die
Vorlage — **die Spec mitziehen**, sonst driften Beschreibung und Sache auseinander.

**App-Seite:** [`UpdateViewModel`](src/viewmodels/UpdateViewModel.h) (Context-Property
`Updates`, Zustandsautomat Idle/Checking/UpToDate/Available/Downloading/Ready/Failed,
QSettings `update/auto_check|lastCheck|skippedVersion|baseUrl`; bis 1.9.3 `update/autoCheck`) +
[`qml/dialogs/UpdateDialog.qml`](qml/dialogs/UpdateDialog.qml) + Hilfe-Menü + zwei
Palette-Einträge + Einstellungen → Allgemein → „Aktualisierung". Start-Hook in `main.cpp`
(3 s nach dem Start; im Screenshot-Modus nie). Basis-URL `https://nobser.de/updates`,
Produkt `qtmux` → `…/qtmux/manifest.json`.

**Owner-Regeln, die im Code hängen** (jede hat einen Grund, keine ist Geschmack):
- Start-Check **höchstens 1×/Tag, abschaltbar, Fehler bleiben STILL** — ein Rechner ohne
  Netz darf nicht jeden Morgen mit einem Fehlerdialog begrüßen.
- Der Zeitstempel wird **auch nach einem Fehlschlag** geschrieben; sonst wird aus
  „1×/Tag" bei unerreichbarem Server „bei jedem Start".
  ✅ **Seit 1.9.4 steht der Zeitstempel VOR dem Request** (Koordinator-Entscheid
  2026-08-07, RAFTNG macht es so), nicht mehr im Callback. Der Unterschied trifft genau
  einen Fall: eine Antwort, die *nie* kommt (hängender Server, Anwender beendet QTmux vor
  dem Timeout) — früher blieb der Stempel dann aus und der nächste Start fragte erneut.
  Wächter `tst_updateviewmodel::throttleStampIsWrittenBeforeTheRequest` (Server nimmt an,
  antwortet nie; Mutationsprobe „Stempel zurück in den Callback" macht ihn rot).

**Vertrags-Abgleich „Beim-Start-Update-Check" (Owner-Vorgabe 2026-08-07, workspace-weit für
alle drei Desktop-Apps):** In QTmux war der Vertrag **bereits vollständig erfüllt** — es gab
nichts zu bauen. Punkt für Punkt gemessen, nicht aus dem Code geschlossen:

| Vertragspunkt | Stand in QTmux |
|---|---|
| Einstellung, Default EIN | „Beim Start automatisch nach Updates suchen", Einstellungen → **Allgemein → Aktualisierung** (`CatAllgemein.qml`, `objectName: swUpdateAutoCheck`); `value(kKeyAutoCheck, **true**)` |
| Start nie blockieren | `QTimer::singleShot(**3000**, …)` in `main.cpp`, nach Layout/Session-Wiederherstellung |
| Still, sichtbar nur bei neuerer Version | Callback: `if (manual) setError/Failed; **else setState(Idle)**` |
| Kein Netz ⇒ lautlos | derselbe Zweig — der stille Pfad kennt keinen Fehlerzustand |
| Menüweg unverändert | `checkNow()` → `startCheck(manual=**true**)`, eigener Pfad, unberührt |
| Strings in beiden `.ts` | **0 unfinished** in `qtmux_de.ts` und `qtmux_en.ts`; EN: „Check for updates automatically at startup" |
| Settings-Text mit Deckung | beschreibt, was er **tut** („Höchstens einmal am Tag und still: Gibt es nichts Neues oder ist der Server nicht erreichbar, passiert gar nichts") — keine Zusage „hält aktuell" |
| Proxy: nie Auth-Dialog beim stillen Check | `answerProxyChallenge` prüft `m_manual`; Wächter `tst_updateviewmodel::silentStartupCheckNeverAsksForProxyCredentials` |

✅ **Key-Angleichung `update/autoCheck` → `update/auto_check` — seit 1.9.4, MIT Migration**
(Koordinator-Entscheid 2026-08-07; RAFTNG und MacPCAN nutzen dieselbe Schreibweise).
⚠️ **Ein Angleichen ohne Migration wäre ein Übergriff gewesen:** Es hätte jeden Anwender,
der den Schalter bewusst ausgeschaltet hat, stillschweigend auf EIN zurückgesetzt.
Mechanik: `UpdateViewModel::migrateSettings()` läuft im Konstruktor — alten Key lesen,
Wert übernehmen, **erst dann** gilt der neue; steht der neue schon, gewinnt er; der alte
wird in beiden Fällen entfernt. **Zweite Tür:** Exportdateien bis 1.9.3 tragen den alten
Key — `SettingsIo::readFile()` übersetzt ihn, sonst fiele ein exportiertes AUS als
„fremder Schlüssel" durch die Allowlist. Wächter: `legacyOffSwitchSurvivesTheKeyMigration`,
`keyMigrationKeepsOnAndPrefersTheNewKey`, `tst_settingsio::importTranslatesLegacyAutoCheckKey`
(je per Mutationsprobe rot gesehen). Die zwei früher blinden Negativprüfungen des
Start-Checks (`qWait(200)` + `count()==0`) prüfen seither **fristfrei** auf
`stateChanged`: ein Start setzt `Checking` synchron im Aufruf.

🔑 **Zwei Messfallen, beide hier hineingelaufen** — wer den Start-Check nachmisst, verliert
sonst eine halbe Stunde an einem Messgerät, das schweigt:
1. **`--screenshot` löst den Start-Check absichtlich NIE aus** (`if (shotPath.isEmpty())` in
   `main.cpp`). Ein Beleglauf mit dem Screenshot-Schalter misst also garantiert nichts — und
   sieht dabei exakt so aus wie ein abgeschalteter Check.
2. **QSettings ersetzt `/` durch `.`, sobald der Wert in der macOS-plist landet.** Der Key
   heißt im Code `update/lastCheck`, in `defaults read` aber **`update.lastCheck`**. Ein
   `defaults write …  "update/auto_check"` schreibt einen Schlüssel, den die App **nie liest**
   — der Gegentest lief damit gegen die Vorgabe statt gegen den ausgeschalteten Schalter und
   „bestätigte" fälschlich. Erkennbar war es nur daran, dass Test und Gegentest **dasselbe**
   Ergebnis lieferten.

**Beleg „Default EIN" (2026-08-07, mit scharfem Gegentest; damals noch unter dem alten
Key — ab 1.9.4 heißt er in der plist `update.auto_check`, ein gesetztes `update.autoCheck`
wird beim Start migriert und entfernt):**
```bash
# A: frische Config, Key NICHT gesetzt  -> Vorgabe muss greifen
env -i HOME=$HOME ./build/macos-release/qtmux.app/Contents/MacOS/qtmux \
    --profile belegA --mcp-port 7353 &     # 14 s laufen lassen, dann TERM
defaults read com.qtmux.QTmux-belegA | grep update
#   -> "update.lastCheck" = "2026-08-07T21:18:36";     Pruefung LIEF

# C: Gegentest, Schalter ausdruecklich AUS (Punkt-Schreibweise!)
defaults write com.qtmux.QTmux-belegC "update.autoCheck" -bool false
env -i HOME=$HOME … --profile belegC --mcp-port 7355 &
defaults read com.qtmux.QTmux-belegC | grep update
#   -> nur "update.autoCheck" = 0;   KEIN lastCheck  ->  Pruefung lief NICHT
```
- **„Version überspringen" bindet nur den stillen Check.** Von Hand sieht man sie weiter —
  sonst wäre ein Fehlklick unwiderruflich.
- **Downgrade** erlaubt (Owner), aber nur auf ausdrückliche Anforderung und mit Warnung;
  der Start-Check bietet ihn nie an, sonst fragte er jeden Entwickler-Build täglich.
- **Kein Silent-Self-Replace:** QTmux startet den Installer und bleibt stehen.

🔑 **Linux ohne `$APPIMAGE` hat keinen Start-Plan** — die AppImage-„Installation" IST die
Selbstersetzung von `$APPIMAGE`; läuft QTmux aus einem Distributionspaket oder einem
Entwickler-Build, gibt es nichts zu ersetzen. Der Dialog blendet „Installieren …" dann aus
und nennt stattdessen den Pfad der geprüften Datei (`canLaunchInstaller()`). Aufgefallen ist
das erst am **Linux-Build** — auf macOS/Windows gibt es den Fall nicht.

🔑 **Der Update-Download trägt KEINE Quarantäne — darum kein Gatekeeper-Rat im Dialog
(QTMUX-146, gemessen 2026-10-14).** Bis 1.9.8 riet der Dialog fest zu „Rechtsklick →
„Öffnen““. Das war doppelt falsch: Seit macOS 15 gibt der Rechtsklick nichts mehr frei (nur
Systemeinstellungen › Datenschutz & Sicherheit › „Dennoch öffnen“), und auf diesem Weg gibt es
nichts freizugeben. Download-Weg: `UpdateViewModel::download()` →
`appupdate::UpdateChecker::downloadArtifact` (vendiert, schreibt per `QFile`), kein eigener
Weg in C++/QML. Gemessen mit einem Messprogramm auf genau diesem Code:

| Fall | Quelle | geladene Datei |
|---|---|---|
| `file://`, Quarantäne an der Quelle gesetzt | `com.apple.quarantine` + `com.apple.provenance` | nur `com.apple.provenance` |
| HTTPS vom Produktions-Update-Server (1.9.8-DMG, sha256 = Manifest) | — | nur `com.apple.provenance` |
| App per `ditto` aus dem eingehängten DMG kopiert | — | 0 von 2538 Einträgen mit Quarantäne |
| **Positivkontrolle:** dasselbe DMG mit gesetzter Quarantäne, eingehängt, App per `ditto` kopiert | Quarantäne | **2538 von 2538** mit Quarantäne |

⚠️ **Im eingehängten Volume selbst ist `xattr -r` blind** — es zählte 0 auch beim
quarantänierten DMG (die Markierung hängt am Mount, nicht an den Dateien). Tragfähig ist nur
die Zählung NACH dem Kopieren. Am lebenden Objekt bestätigt: alle sieben DMGs, die der
Updater einer echten Installation über die Versionen 1.8.1…1.9.8 geladen hatte, tragen keine
Quarantäne, die installierte App ebenso wenig. Die Info.plist hat kein
`LSFileQuarantineEnabled`, das Bundle keine Sandbox-Entitlements — sonst setzte macOS die
Markierung an jede geschriebene Datei. Der Finder-Zug in „Programme“ selbst ist nicht
automatisierbar (kein Bedienungshilfen-Recht); `ditto` ist das Ersatzmaß.
Deshalb sagt der Hinweis jetzt, was „Installieren …“ je Plattform tut (`open <dmg>` ·
`msiexec /i` eines `perMachine`-MSI, also UAC mit unbekanntem Herausgeber · Selbstersetzung
des AppImage mit `.old` daneben), und nennt „Dennoch öffnen“ nur, wenn die geladene Datei die
Quarantäne an der Datei gemessen trägt (`updatehint::afterDownload`). Gemessen wird über die
Hub-Mechanik `appupdate::quarantineState` (vendiert, `Quarantine.hpp`, MAC-94): nur `Present`
gibt die Abhilfe. ⚠️ **`Unreadable` ist kein „nein“** (Datei weg, keine Rechte, fremdes
Dateisystem): Der Hinweis bleibt dann beim Satz, was der Knopf tut — er behauptet also nichts
über die Quarantäne —, und QTmux protokolliert den Grund per `qWarning`. Realistisch ist eine
Datei, die nach dem Download verschwand; dann scheitert „Installieren …“ ohnehin sichtbar.
Windows: `QFile` schreibt
keinen `Zone.Identifier`, SmartScreen bewertet nur Dateien mit dieser Markierung — abgeleitet,
nicht unter Windows gemessen. Wächter in `tst_updateviewmodel`: kein Rechtsklick-Rat in den
lebenden Katalogeinträgen beider Sprachen, alle übersetzt; Abhilfe genau bei `Present`
(echtes xattr auf macOS, Hub-Probe `setQuarantineProbeForTests` mit allen vier Zuständen auf
jedem Runner); `Unreadable` ohne Abhilfe, aber mit Warnung (auch echt: verschwundene Datei);
die Messung selbst läuft bei jedem macOS-Lauf mit (Quelle quarantäniert, Download `Absent` —
zusätzlich per direktem `getxattr` als unabhängigem Messgerät). Mutationsproben, alle rot:
alter Satz zurück · Quarantäne immer wahr · Abhilfe nie angehängt · alles außer `Absent`
zählt als Quarantäne · `Unreadable` still · Hub-Messung ignoriert.

🔑 **Der Download ERSETZT den Zielnamen, er schreibt nicht hinein (QTMUX-147, Hub-Stand
MAC-95, gemessen 2026-10-15).** Die Tabelle oben galt bis dahin nur für einen **freien**
Namen: Der Downloader öffnete `<Download-Ordner>/<Name aus der URL>` mit
`WriteOnly|Truncate`. Lag dort schon eine Datei gleichen Namens — derselbe Installer, früher
per Browser geladen —, behielt sie Inode **und** `com.apple.quarantine`; der Dialog riet dann
zu Recht zu „Dennoch öffnen“. Ein Symlink an der Stelle wurde bis in sein Ziel beschrieben,
ein SHA-Fehler zerstörte eine ältere Datei gleichen Namens, und wer die alte Datei offen
hatte, las plötzlich die neuen Bytes. Seitdem landen die Bytes in `<Name>.XXXXXX.part`
daneben (`QFile` mit `NewOnly`, kein `QTemporaryFile` — dessen `close()` hält unter Windows
das Handle offen, und `MoveFileExW` scheitert dann an jedem Download), der SHA-256 wird von
der Platte gelesen, und erst dann ersetzt `rename()` bzw. `MoveFileExW(REPLACE_EXISTING)`
den **Namen**. Ein Link wird ersetzt, nie durchschrieben; ein Fehler (auch ein abgelehntes
Ersetzen, unter Windows bei einer offen gehaltenen Datei möglich) lässt die alte Datei
unberührt und keinen `*.part`-Rest. `QSaveFile` wäre falsch: es löst einen Symlink am
Zielnamen auf und ersetzt dessen ZIEL.
Wächter: `tst_updater` auf allen drei Plattformen — vorhandener Name wird ersetzt (Inhalt,
unter POSIX neues Inode; unter Windows ist das der einzige Test, der `MoveFileExW` auf einen
besetzten Namen fährt), SHA-Fehler lässt die ältere Datei stehen, eine offen gehaltene Datei
bekommt immer eine Antwort (Ersetzen oder Fehler mit Grund, protokolliert per `qInfo`),
Symlink wird ersetzt (nicht unter Windows: `QFile::link` legt dort eine `.lnk` an), nach
jedem Ausgang kein `*.part`. `tst_updateviewmodel` auf macOS: markierte Bestandsdatei im
Download-Ordner → Download trägt keine Marke (direkt per `getxattr`), neues Inode,
`quarantineState` = `Absent`, Hinweis ohne Abhilfe. Positivkontrolle: dieselben fünf Tests
gegen den vorherigen vendierten Stand sind alle rot (Marke geerbt, Inode gleich, Linkziel mit
Installer-Bytes überschrieben, ältere Datei nach SHA-Fehler leer, Haltender liest neue Bytes).

🔑 **Reste eines harten Endes räumt der nächste Download ab (QTMUX-148, Hub-Stand MAC-99,
gemessen 2026-10-16).** „Kein `*.part`-Rest“ oben gilt für jeden Ausgang, den die App
**erlebt**. Stirbt sie zwischen dem Anlegen der Zwischendatei und dem Umbenennen (SIGKILL,
Absturz, Strom weg), bleibt `<Name>.XXXXXX.part` im Ordner `QTmux-Updates` liegen — ein
Update-Paket wiegt 30–60 MB. Der nächste Download **desselben Namens** in **denselben**
Ordner löscht solche Reste, und nur, was nachweislich einer ist: genau das Muster
`<Name>.<6 Zeichen [A-Za-z0-9]>.part` · eine gewöhnliche Datei (kein Symlink, kein Ordner;
unter POSIX zweiter Riegel `O_NOFOLLOW`) · zuletzt vor mehr als **10 min** geschrieben
(deckt nur den Augenblick zwischen Anlegen und Sperren bei einer zweiten Instanz) · **nicht
belegt**: ein lebender Download hält unter POSIX ein `flock()` auf seiner Zwischendatei
(vom Anlegen bis zum Umbenennen, der Kernel gibt es mit dem Prozess frei — wie auch immer er
endet), unter Windows öffnet Qt ohne `FILE_SHARE_DELETE`, das Löschen scheitert und die Datei
bleibt mit Warnung. Das Alter allein beweist nichts: der Downloader kennt keine Frist, ein
hängender Download einer zweiten Instanz hält eine **alte** Zwischendatei. Jede Löschung steht
per `qWarning` im Log (`update: removed download remnant <Pfad> (<Bytes> bytes, last written
<UTC>)`). Ein Rest eines **anderen** Pakets (andere Version) bleibt bis zu einem Download
genau dieses Namens liegen.
Wächter, alle drei Plattformen: `tst_updater::aRemnantOfAnEarlierDownloadIsRemovedAndLogged`
(Rest weg + genau eine Logzeile; anderer Name, Beinahe-Treffer, junge Datei und — POSIX — ein
Link mit Musternamen bleiben, Linkziel unberührt) · `aPartHeldByALivingDownloadStays`
(belegter alter Rest bleibt, nach Freigabe räumt der nächste Download ihn ab) ·
`tst_updateviewmodel::aRemnantInTheUpdateFolderIsClearedByTheNextDownload` (Weg über das
ViewModel in den echten Update-Ordner). Positivkontrolle: alle drei rot gegen den vorherigen
vendierten Stand. Mutationsproben am vendierten Kern (danach per `--update` zurück), alle rot:
Belegungsriegel aus · Altersfrist aus · Muster gelockert · **beide** Symlink-Riegel aus (einer
allein bleibt grün — der zweite fängt ihn, so gewollt).

🔑 **Vertrag V1: Produktbindung, Seriennummer, Größengrenze (QTMUX-159, Hub-Stand MAC-108,
2026-11-16).** Eine gültige Signatur beweist nur, dass der Herausgeber das Manifest geschrieben
hat — alle Produkte tragen denselben Schlüssel. Seit dem Hub-Stand prüft der vendierte Kern
nach der Signatur, in dieser Reihenfolge: (1) `product` = `qtmux` und `channel` = `stable`,
ein fehlendes Feld ist eine Abweichung, nie ein Platzhalter (`ErrorKind::WrongProduct`);
(2) `serial` (UTC-Sekunden der Manifest-Erzeugung, vom Hub-`publish.py` geschrieben) gegen das
höchste bisher angenommene: kleiner, oder fehlend nachdem schon eines angenommen war, ist ein
Replay (`ErrorKind::Replay`); solange noch keines angenommen war, gilt jedes signierte Manifest
des richtigen Produkts (Übergang, so sehen alle Manifeste vor V1 aus). (3) Beim Download bricht
der Kern ab, sobald mehr als `size` Bytes angekommen sind, und weniger als `size` am Ende ist
ebenso ein Fehler (`ErrorKind::Size`); die Teildatei wird gelöscht, eine ältere Datei gleichen
Namens bleibt. **QTmux' Anteil** ist die Bedeutung: `UpdateViewModel::rebuildChecker` gibt dem
Kern den Kanal und einen `SerialStore` über die QSettings, Schlüssel
`update/last_serial/qtmux` (Hub-Schreibweise `policy::lastSerialSettingsKey`, als Zahl; ein
unlesbarer Wert oder einer über uint32 zählt als 0). Geschrieben wird erst nach bestandener
Signatur-, Produkt- und Replay-Prüfung, auch beim stillen Check und ohne Installation. Der
Schlüssel steht **nicht** in der Export-Allowlist: Er ist Sicherheitszustand der Installation,
ein Import könnte ihn zurückdrehen (Replay würde angenommen) oder hochdrehen (jedes echte
Update würde abgelehnt), und „Alles zurücksetzen“ lässt ihn stehen. Ein Testprofil
(`QTMUX_PROFILE`) hat seinen eigenen. Die manuelle Prüfung zeigt Replay und fremdes Manifest
mit eigener Überschrift (`failureTitle`) und eigenem Satz, der Grund des Kerns steht darunter;
die stille Prüfung bleibt still und endet in `Idle`, nie in `UpToDate` — ein Replay sieht so nie
wie „kein Update“ aus. Ein `Size`-Fehler bekommt beim Download einen eigenen Satz.
⚠️ Ohne `setSerialStore` gäbe es Produktbindung, aber **keinen** Replay-Schutz, und der Kern
sagte das nur per `qWarning` („checked without a serial store“) — `test_updatehardening` macht
diese Warnung in jedem Fall rot.
Wächter: `test_updatehardening` (signiert zur Laufzeit mit einem Wegwerf-Schlüssel, die Test-Naht
`UpdateViewModel::setUpdatePublicKey` ist kein `Q_INVOKABLE`): fremdes Produkt (T-4), falscher
und fehlender Kanal, älteres und fehlendes `serial`, Übergang ohne `serial`, Speichern ohne
Installation, Rückzug mit neuem `serial`, gefälschte Signatur speichert nichts, unlesbarer
Wert = 0, `serial` je Produkt, `size`±1 über `file://` **und** HTTP (T-20), genaue Größe lädt;
dazu `tst_settingsio::lastSerialIsNeitherExportedNorResetNorImported`. Mutationsproben an der
Verdrahtung 12/12 rot, am vendierten Kern (Produktprüfung, beide Replay-Zweige, Ober- und
Untergrenze; danach Wächter `check-updater-sync` Exit 0) 5/5 rot.
⚠️ Nebenbefund im Kern: Nach dem Abbruch an der Obergrenze liest der `finished`-Zweig die
abgebrochene Antwort noch einmal und Qt meldet `QIODevice::read (…): device not open` als
Warnung — harmlos, aber Rauschen im Log; Behebung gehört in den Hub.

🔑 **Signierte Fixtures brauchen `-text` in `.gitattributes`.** Auf der Windows-Maschine
(`core.autocrlf=true`) machte git aus 935 Byte `manifest.json` 966 Byte — 31 eingefügte CR.
Die Ed25519-Signatur steht über die **exakten** Bytes, also fiel `test_updateviewmodel` mit
6 von 11 Fällen und sah dabei nach einem Fehler im Update-Code aus. Gilt für jedes signierte
Artefakt im Ökosystem.

🔑 **`busy()` darf im Abschluss-Callback nicht mehr wahr sein.** Der Kern hielt seine
`QPointer` auf die Reply bis zur nächsten Event-Loop-Runde (nur `deleteLater()`), also
meldete `busy()` „ja", während der Aufrufer schon „fertig" hörte. Wer daraus die nächste
Anfrage startet — Check → Download, also genau die GUI, weil erst dieser Callback den
Dialog aufgehen lässt — bekam `a request is already running`: **Der Dialog ging auf und
sein erster Knopf tat nichts.** Fix in MacPCAN `59a9e35` (`finishActive()`).
🔑 **Die Lehre daneben ist wertvoller als der Fix:** Das sah zwei Läufe lang wie ein
**Flake** aus (einmal rtzbld01, einmal CI-Windows) und verschwand beim Wiederholen. Es
trat nur über **HTTP** auf, weil `file://` anders verschränkt. **Ein sporadischer
Fehlschlag, der nur auf EINEM Transportweg auftritt, ist ein Timing-Fehler, kein
Rauschen.** Sichtbar wurde er erst, nachdem die Windows-Testbinaries auf das
Konsolen-Subsystem umgestellt waren — vorher meldete die CI nur „***Failed" ohne Fall.

🔑 **Auf Windows muss ein Datei-Handle VOR dem Löschen zu sein.** Bei SHA-Mismatch löscht der
Kern den Download — mit noch offenem Read-Back-Handle ist das dort ein stilles No-op: Der
Aufrufer bekam „file deleted", der beschädigte Installer blieb in Downloads liegen. Unter
POSIX unsichtbar (offene Dateien lassen sich entlinken). Fix in MacPCAN `d0ed07b`.

🔑 **`file://` prüft den echten Transportweg NICHT.** Zwei Dinge gehen daran vorbei: die
Cache-Bust-Abfrage `?ts=<epoch>` (der Kern hängt sie nur an http(s) an — an einem
Datei-URL zerstörte sie die Pfadauflösung) und ein Download, der in Häppchen ankommt und
darum überhaupt Fortschritt meldet. Deshalb hat `tst_updateviewmodel` einen eigenen
In-Process-HTTP-Server; genau dort ist der `busy()`-Fehler oben aufgeschlagen.

**Proxy (QTMUX-129):** Der Mechanismus kommt aus `appupdate` (MacPCAN, MAC-36) und wird
vendiert; GUI und Persistenz liegen bei uns — Sitzungsspeicher
[ProxyCredentials.h](src/core/ProxyCredentials.h), Settings `update/proxy*`,
[ProxyAuthDialog.qml](qml/dialogs/ProxyAuthDialog.qml).
🔑 **Warum der Dialog nichts zurückgibt:** `proxyAuthenticationRequired` ist **synchron**,
der `QAuthenticator*` gilt nur im Slot — ein QML-Dialog antwortet asynchron. Also fragt die
Lib einen **Lieferanten**, der nur aus dem Speicher antwortet und nie blockiert; ist er leer,
endet die Anfrage sauber, QML fragt den Menschen, und `provideProxyCredentials()` legt ab und
**wiederholt** den unterbrochenen Vorgang.
🔑 **Die Ein-Versuch-Regel MUSS bei uns sitzen** (an MacPCAN `0934eff` gelesen): `appupdate`
reicht `previousAttemptFailed` nur durch und erzwingt **keine** Obergrenze. Mehrere
Fehlversuche sperren in einer AD-Umgebung das **Domänen-Konto** — dieselbe Lektion wie beim
SSH-Passwort-Auto-Fill. Kein Keychain (drei Implementierungen, Linux endete wie beim
`SleepInhibitor` als Stub), nicht der Vault (er startet **gesperrt**, der stille Start-Check
dürfte also nach dem Master-Passwort fragen).
🔑 **Der stille Start-Check fragt NIE nach dem Proxy-Passwort** (`answerProxyChallenge` prüft
`m_manual`): Ein ungefragt aufspringendes Passwortfenster drei Sekunden nach dem Start ist
genau das, was die Regel „Start-Check bleibt still" verbietet — und es käme, wenn der
Anwender auf sein Terminal wartet. Gefragt wird beim Check von Hand und beim Download.
Wächter `tst_updateviewmodel::silentStartupCheckNeverAsksForProxyCredentials`.

🔑 **Regressionsliste für den Update-Weg:**
[docs/update-regressionsliste.md](docs/update-regressionsliste.md) — fünf Minuten an einem
1.8.0-Build (Manifest-Abruf, Signatur **mit Gegentest**, Versionsvergleich, „ist aktuell").
Gedacht als schneller Gegencheck **nach jedem Re-Vendoring**, weil die kommende
Proxy-Unterstützung auf **beide** Netzwege wirkt (Manifest **und** Artefakt-Download) und
QTmux der Konsument ist, bei dem eine Regression zuerst auffällt. Alle Sollwerte sind
gemessen, die Befehle laufen wörtlich.

**Nächste Version veröffentlichen — das Rezept** (einmal komplett gefahren für 1.8.0):
1. Bump an den Stellen aus den Konventionen; `qtmux_version.h` danach **gegenlesen**.
2. Alle drei Plattformen bauen + testen, committen, pushen, CI abwarten.
3. Installer: `installer/build-dmg.sh <ver>` lokal · auf rtzbld01
   `C:\Tools\qtmux-build\build_msi.cmd <ver>` (Version ist **Argument** — s. Falle im
   Arbeitsstand) · AppImage aus dem **Draft-Release des Tags** — nach dem Tag-Push hängt
   der CI-Job `linux-release` es dort an (QTMUX-136):
   `gh release download v<ver> -p 'QTmux-*-x86_64.AppImage' -D dist/`. (Bis 1.9.5 war es
   ein Actions-Artefakt; die Quota dafür ist account-weit und lief am 2026-09-28 voll.)
4. **Am Artefakt gegenprüfen, nicht am Build-Log:** je Binary Treffer auf die neue
   Nummer **und 0 Reste der alten**. DMG mounten, ZIP entpacken, AppImage mit
   `--appimage-extract` auspacken (squashfs — ein `grep` aufs AppImage selbst findet nie
   etwas und sähe wie ein Fehler aus).
5. DMG/MSI/ZIP per `gh release upload v<ver> …` an **dasselbe** Draft (kein
   `gh release create` — das Draft legt der Tag-Lauf an), dann
   `UPDATES_SFTP_HOST=… python3 ../MacPCAN/tools/updates/publish.py --product qtmux
   --version <ver> --notes-de … --notes-en … --artifact <key>=<datei>,kind=<art> …
   --upload --verify`. Schlüssel und Zielpfad kennt `publish.py` als Vorgabe
   (`~/.ssh/updates_publish_ed25519`, `public_html/updates`).
6. Gegenprobe: `curl` + `openssl pkeyutl -verify` auf die Live-Bytes und ein manueller
   Check aus der App. Erst danach das GitHub-Release veröffentlichen:
   `gh release edit v<ver> --draft=false`.

🔑 **Der Zyklus-Nachweis braucht eine ÄLTERE Instanz MIT Feature** — dafür ein
`git worktree` auf den Vor-Bump-Commit, dort ein temporäres Gerüst in `main.cpp`, das
`checkForUpdates` → `download()` → `launchInstaller()` durchruft und mitloggt. Der
Hauptbaum bleibt sauber, das Gerüst wird danach mit dem Worktree entfernt.
🔑 **Beleg für „kein stiller Selbsttausch":** SHA-256 **und** mtime des laufenden
Binaries vor und nach `launchInstaller()` vergleichen — beides unverändert.

**Tests:** `test_updater` (13 Fälle, Kern; Fixtures zur Laufzeit erzeugt und mit dem
mitkompilierten Monocypher signiert, dazu ein **RFC-8032-Vektor** als Gegenprobe gegen eine
nur zu sich selbst passende Krypto) und `test_updateviewmodel` (11 Fälle, App-Seite gegen
committete, mit dem **Produktionsschlüssel** signierte Fixtures unter
`tests/fixtures/update/`). 🔑 Letzteres ist zugleich der Interop-Nachweis der
ausgelieferten App (openssl signiert, das vendierte Monocypher verifiziert) — ein
Schlüsselwechsel macht den Test rot, und genau das ist der gewollte Alarm.
