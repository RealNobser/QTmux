# installer/ — Pakete bauen und ausliefern

> Schicht-Datei: Claude Code lädt sie, wenn in `installer/` gearbeitet wird. Wer ein Release
> baut oder publiziert, liest sie vorher auch von anderswo. Die Absätze stammen wörtlich aus der
> Wurzel-`CLAUDE.md` (ausgelagert 2026-11-19). Dort bleiben die Bau-Kommandos je Plattform, die
> Liste der Versions-Bump-Stellen, die Publish-Mechanik fürs nächste Release und die
> Release-Lehren, die Maschinennamen tragen.

## DMG (macOS)

**DMG:** `installer/build-dmg.sh [version]` — baut `macos-release` (oder `QTMUX_BUILD_DIR`,
wenn aus `macos-release` gerade eine Instanz läuft), `macdeployqt -qmldir=qml`
(self-contained inkl. Plugins/PCBUSB; 🔑 PCBUSB kommt aus `QTMUX_PCBUSB_DIR` oder dem Nachbar-
Checkout `../MacPCAN`, fehlt es, bricht der Release-Bau mit `FAMILY_CODESIGN=require` ab, statt
das CAN-Plugin still wegzulassen), dann **Re-Signatur mit der Familien-Identität**
(`installer/macos/sign-bundle.sh`, aus MacPCAN vendiert, Kontrakt 4 im Sync-Wächter; innen →
außen, kein `--deep` — macdeployqt schreibt rpaths NACH seiner Signatur um → ungültig; Apple
Silicon startet nur signiert), `hdiutil`-DMG → `dist/QTmux-<ver>-macos.dmg`. Seit 2026-10-07
ist die Designated Requirement build-unabhängig (`certificate root = H"015c9a18…"` statt
`cdhash`) — sonst verwarf TCC nach JEDEM Update alle Ordner-Erlaubnisse, und TCC rechnet jeden
Prozess in einer QTmux-Session QTmux zu (Agent-Session 4 h 48 min an einem unsichtbaren
Downloads-Dialog). Ohne Schlüsselbund unter `~/.keys/codesign/` LAUTER Rückfall auf ad-hoc;
**Release-Bau mit `FAMILY_CODESIGN=require`**. Mechanik/Identität/Wiederherstellung:
MacPCAN `docs/codesign.md`. Nicht notarisiert, keine Hardened Runtime
(Early-Adopter): Quarantäne per „Dennoch öffnen“ bzw. `xattr -dr com.apple.quarantine`. macdeployqt-
`ERROR` zu QtVirtualKeyboard/Multimedia/Pdf bricht den Build nicht — **harmlos ist er aber
nur auf einer Maschine ohne Homebrew-Qt** (2026-08-07 an 1.8.1 gemessen, gilt seit
mindestens 1.8.0 unverändert):
> ⚠️ **Ein nicht aufgelöster `@rpath` verschwindet nicht, er wird nur später eingelöst.**
> macdeployqt kopiert `PlugIns/imageformats/libqpdf.dylib` mit, kann dessen `@rpath/QtPdf`
> aber nicht auflösen (genau jener „harmlose" ERROR). Zur **Laufzeit** greift dann der im
> Binary verbliebene `LC_RPATH /opt/homebrew/lib`, und über Homebrews `QtPdf` lädt die
> **komplette zweite Qt-Kette** (QtCore, QtGui, QtNetwork, QtDBus, icu, harfbuzz …). Qt
> meldet das selbst: „Class … is implemented in both … This may cause spurious casting
> failures and mysterious crashes."
> 🔑 **Warum es trotzdem niemandem auffiel:** Das **Hauptbinary** lädt alles über
> `@executable_path/../Frameworks` und **nichts** über `@rpath` — gemessen: 0 Treffer. Der
> Homebrew-RPATH ist für das Hauptbinary also wirkungslos; der Schaden entsteht
> ausschließlich über ein **mitkopiertes, ungenutztes Plugin**. Auf einer Anwendermaschine
> existiert `/opt/homebrew/lib` nicht → das Plugin lädt nicht → alles gut. Betroffen ist
> allein der Entwicklungsrechner, und dort **startet die App trotzdem** (Exit 0, Fenster,
> Screenshot).
> 🔑 **Messgerät:** `env -i HOME=$HOME <bundle>/Contents/MacOS/qtmux --profile <name>
> --mcp-port <frei> --screenshot <png>` und die Ausgabe nach `/opt/homebrew` durchsehen;
> vollständig wird die Kette mit `DYLD_PRINT_LIBRARIES=1` sichtbar. **Ein reines `otool -l`
> genügt nicht** — es zeigt den RPATH, aber nicht, ob ihn jemand einlöst.
> **Abhilfe (nicht umgesetzt, Backlog):** ungenutzte Plugins nach dem `macdeployqt`-Lauf aus
> dem Bundle nehmen (`imageformats/libqpdf`, `platforminputcontexts`, die vkb-/Scene3D-QML-
> Plugins) oder den Homebrew-RPATH per `install_name_tool -delete_rpath` entfernen. Beides
> gehört ins `build-dmg.sh` **vor** die Re-Signatur, sonst ist die Signatur wieder ungültig.

> ⚠️ **`QT_QPA_PLATFORM=offscreen` funktioniert am DMG-Bundle NICHT** — macdeployqt bündelt
> nur die gebrauchten Plattform-Plugins, im Bundle liegt allein `libqcocoa.dylib`. Ein
> erzwungenes offscreen stirbt per `qFatal`, und das sieht wie ein kaputtes Paket aus,
> obwohl es keines ist. QTmux fängt den Fall selbst ab („Offscreen-Plattform-Plugin nicht
> gefunden: Screenshot am sichtbaren Fenster") — deshalb bei jeder Abnahme am **Bundle**
> offscreen weglassen. Der Windows-Weg bündelt `qoffscreen.dll` bereits mit; auf macOS ist
> das nicht nachgezogen (Backlog).

## AppImage (Linux)

**AppImage:** `installer/build-appimage.sh [version]` — `linux-release`-Preset (oder
`QTMUX_BUILD_DIR` von der CI), AppDir (Binary + Plugins nach `usr/bin/plugins`),
`linuxdeploy --plugin qt` (`QML_SOURCES_PATHS`) → `dist/QTmux-<ver>-x86_64.AppImage`.
**CI-Fallen:** Runner ohne FUSE → `APPIMAGE_EXTRACT_AND_RUN=1`; `ARCH=x86_64`;
`qmake` via `QMAKE`/`QT_ROOT_DIR`. Nutzt `installer/qtmux.desktop` + `resources/appicon/`.
AppImage ist **kein CPack-Generator** — bewusst hand-gerollt wie DMG/MSI.
🔒 **Die linuxdeploy-Tools sind familienweit GEPINNT** (seit `d782a5a`, kanonische Quelle
MacPCAN `b74957d` + `docs/SHARED.md` „Gepinnte AppImage-Build-Tools"): datierter
Mirror-Ordner + SHA256-Pflicht statt der rollenden `continuous`-URL; der **Cache wird
mitgeprüft** (`~/.cache/qtmux-appimage/<LD_PIN>`, pro Pin), bewusst **kein**
Upstream-Fallback. Pin anheben: **immer zuerst in MacPCAN**, dann 1:1 hierher spiegeln
(Rezept im Skript-Kommentar).

## Windows: Fernsteuern per SSH

- ⚠️ **Fernsteuern per SSH:** Windows-OpenSSH beendet Kindprozesse beim Sitzungsende — Build
  in einer *offenen* Sitzung laufen lassen, nicht per `Start-Process` detachen (endet sonst
  stumm mit 0-Byte-Logs). PowerShell dort: `&` ist kein Trenner, `-Filter` nimmt nur EINE
  Zeichenkette, Pfade mit Klammern (`… (x86)…`) **nicht** inline quoten (PowerShell wertet
  `(x86)` als Befehl) → `.cmd` lokal erzeugen und per `scp` übertragen. cmake/ninja/ctest
  liegen unter `Common7\IDE\CommonExtensions\Microsoft\CMake\`.

## Versions-Bump: Einzelheiten

Ergänzung zur Liste der Versions-Bump-Stellen in der Wurzel-`CLAUDE.md` (Abschnitt „Konventionen“):

  🔑 **`installer/QTmux.wxs` ist KEINE Bump-Stelle** (stand hier früher fälschlich drin): Die
  Zahl darin steht nur in einem Kommentar-Beispiel, der echte Wert kommt über `-d Version=`.
  ✅ **`build-msi.ps1` hatte eine Vorgabe, die auf `1.2.0` stehengeblieben war** —
  parameterlos gebaut ergäbe das ein MSI mit falscher `ProductVersion`, ohne dass irgendetwas
  rot wird. Seit 2026-08-06 ist `-Version` **Pflichtparameter** (`Mandatory` +
  `ValidatePattern`): Ein Pflichtparameter kann nicht veralten. Kein Aufrufer bricht daran —
  `build-release.ps1` liest die Version aus `CMakeLists.txt` und reicht sie durch.
  📋 Vollständige Analyse als **Vorlage für die anderen fünf Repos**:
  [docs/versionsquellen.md](docs/versionsquellen.md) — dort auch die Kernfalle für die
  workspace-weite Build-ID `<version>+<hash>[-dirty]`: `configure_file` friert den Hash zur
  **Configure**-Zeit ein (ein Commit löst kein Re-Configure aus), er muss zur **Build**-Zeit
  entstehen. Und: Die Build-ID ist eine **Anzeige**, nie eine Vergleichsgröße — in
  `UpdateViewModel::currentVersion` darf nur `1.8.0` ankommen, sonst bricht der
  Manifest-Vergleich.

## Release-Lehren

Wörtlich aus den Release-Absätzen der Wurzel-`CLAUDE.md`; die Erzählung der Releases steht in
[docs/HISTORY.md](../docs/HISTORY.md), weitere Lehren in der Wurzel-`CLAUDE.md`, Abschnitt
„Release-Lehren“.

⚠️ **Eine Ruhe-Messung ist eine Momentaufnahme:** Der EmbyStudio-Tag-Lauf entstand 8 s nach der Messung vor
dem Windows-Fernbau, deren Ansage-Zeile kam gleichzeitig. Der MSI-Bau lief darum 26 statt 4 min parallel zu
deren Windows-Job. Wenn die Gegenseite gerade fertig geworden ist, mit dem Start kurz warten und die
Ansage-Datei unmittelbar vor dem Fernbau erneut lesen.
⚠️ **Selbsttreffer bei Prozess-Proben:** `ps … | grep <muster>` zählt die eigene Befehlszeile mit, sobald
das Muster (oder ein Dateiname, der es enthält) darin steht — vor dem Publish meldete so eine Probe zweimal
einen „fremden Lauf“. Das Muster gehört in eine Skriptdatei, nicht auf die Befehlszeile.

🔴 **Sicherheitsprüfung nach dem Push, vor dem Tag abwarten:** Der Harness prüft einen Push im
Hintergrund und meldete bei 1.9.12 zwei Punkte im OSC-8-Code, als die Commits schon öffentlich waren
(behoben mit `52682b2` vor dem Versions-Commit). Nach dem Code-Push ein paar Minuten warten, bevor
Versions-Commit und Tag folgen.

Davor: **v1.9.10** — Wartungsrelease (2026-11-03, `32a1daa`, Build-ID `1.9.10+32a1daa`; Artefakttabelle
in der Git-Historie dieser Datei). Dauerhaft daraus: 🔴 `v1.9.9` wurde vor der Veröffentlichung verworfen
(kein Update-Server, nie „Latest“; Tag und Draft am 2026-11-04 gelöscht): Am fertigen Artefakt fielen bei
grüner CI zwei Fehler auf, die nur der Inhaltsvergleich gegen 1.9.8 zeigte — AppImage verlangte GLIBC 2.43
(Runner-Sprung) und im frischen DMG-Baum fehlte das CAN-Plugin (beide Riegel im Build- bzw. CI-Abschnitt).
1.9.10 brachte Homebrew-Qt 6.11.2 und OpenSSL 4.0 ins DMG. ⚠️ Ein `build-msi.ps1`-Aufruf aus
`powershell -EncodedCommand` braucht `-ExecutionPolicy Bypass`: ohne ihn endet er sofort ohne Artefakt,
und `$LASTEXITCODE` zeigt noch die 0 des vorigen Befehls.

Davor: **v1.9.6** — Im Finder zeigen + Windows-App-Icon QTMUX-137/138 (2026-09-30, `5f66f37`,
Build-ID `1.9.6+5f66f37`; Artefakttabelle in der Git-Historie dieser Datei). Dauerhaft
daraus: ⚠️ **Fernbau-Falle** — die gehaltene SSH-Session lief als Hintergrund-Task mit 1 h
Limit und wurde mitten im ZIP-Schritt getötet; `Compress-Archive` hing danach verwaist mit
offenem Handle auf einem 1,5-MB-Rumpf-ZIP (erkannt nur an der Größe). Fernbau-Task-Limit
**2 h** und `$ProgressPreference="SilentlyContinue"` für Compress-Archive.
Davor: **v1.9.5** — MCP-Anlage ohne Fokuswechsel QTMUX-135 (2026-09-28, `a98df26`, Build-ID
`1.9.5+a98df26`; Artefakttabelle in der Git-Historie dieser Datei). Dauerhaft daraus:
**hdiutil-UDZO streut bis ~2 MB** bei byte-identischem Bundle (Kontrollversuch 1.9.4:
61 867 279 / 60 752 145 B) — eine DMG-Größe ist nie ein Befund, nur der Datei-/Hash-Vergleich
des gemounteten Inhalts.
Davor: **v1.9.4** — neues App-Icon + Startup-Check-Paar QTMUX-134 (2026-09-28, `3987935`,
Build-ID `1.9.4+3987935`; Artefakttabelle in der Git-Historie dieser Datei, `932e484`).
Dauerhaft daraus: ein AppImage erbt still Bibliotheken des Runner-Images (dort
`usr/lib/libgcrypt.so.20` +4 096 B nach Image-Wechsel `20260823.283.1` → `20260920.314.1`,
linuxdeploy bündelt aus dem System-`/lib`) — Inhalt messen, nicht Größe.

Davor: **v1.9.2** — finaler Stand der Familien-Kampagne (2026-08-18), Manifest
`published: 2026-08-18`, Publish fuhr die MacPCAN-Session. GitHub-Releases — v1.9.1 wurde am
2026-08-18 nachgezogen (⚠️ mit dem **Server**-DMG: das lokale dist/-DMG von 1.9.1 war ein
späterer, nie publizierter Neubau mit anderer Build-ID — vor jedem Release-Nachzug die
Artefakt-Herkunft messen, nicht dem Dateinamen glauben), v1.9.2 mit vier frischen Assets.
Build-ID aller 1.9.2-Artefakte `1.9.2+80405c1`, **je Artefakt einzeln gemessen** (DMG
gemountet · MSI per `msiexec /a` entpackt · ZIP per zipfile · AppImage per
`--appimage-extract`; der Tag `v1.9.2` deckt den Commit, Messkommando `git rev-parse v1.9.2`).
Neu gegenüber 1.9.1: MCP `send_keys` benannte Tasten · `send_text` als Bracketed Paste ·
Restore-Marker toter Prozesse · QTMUX-98-Reaktivierung · Vendor-Syncs auf Hub-Stand.
