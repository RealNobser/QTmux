# QTmux — E2E-/Test-Fallen (alle Plattformen)

> **Wann lesen:** vor **jeder** Verifikation, die über `ctest` hinausgeht — MCP-E2E,
> `--screenshot`, GUI-Automatisierung, Messungen an einer laufenden Instanz. Fast jede Zeile
> hier steht für eine Fehldiagnose, die einmal Stunden gekostet hat; mehrere beschreiben
> Messmittel, die **falsch-positiv** melden (der Gegentest bestand, obwohl nichts gebaut war).
> Ausgelagert am **2026-08-06** aus der `CLAUDE.md` — sie wird in jeder Session geladen,
> diese Fallen braucht man nur beim Messen.

## E2E-/Test-Fallen (alle Plattformen)

- 🔑 **Eine Gegenprobe, die nur die gewünschte Richtung misst, beweist nichts — prüfe BEIDES:
  ist das Neue da UND ist das Alte weg?** (2026-08-07, an einem Prefs-Text erlebt; RAFTNG hat
  die Regel danach übernommen.) Der neue Text allein belegt nicht, dass die alte Stelle
  verschwunden ist — eine **zweite, unbemerkte Fundstelle** sieht dann aus wie ein Erfolg.
  Bei uns steht dieselbe Regel schon für Versions-Bumps („neue Nummer **und** 0 Reste der
  alten"), sie gilt aber allgemein: für Umbenennungen, Allowlists, Konstanten, jede Migration.
  ⚠️ **Der zweite Teil derselben Geschichte, und er ist der gefährlichere:** Die Suche lieferte
  für UTF-8 **zweimal `False`** — für den neuen wie für den alten Satz. Ein Messgerät, das auf
  beide Fragen dasselbe antwortet, beantwortet **keine davon**; „Änderung nicht angekommen"
  wäre hier eine Diagnose des Messgeräts gewesen. Ursache: **QML-Texte liegen kompiliert im
  qmlcache**, nicht als UTF-8 im Binary — mit UTF-16 zeigte sich sofort „neu da, alt restlos
  weg". Verwandt und aus derselben Woche: **rcc legt Ressourcen*namen* als UTF-16 ab**, weshalb
  `strings <binary> | grep qtbase_de.qm` nichts findet, obwohl die Datei eingebettet ist.
  **Merke: Bei jedem negativen Binary-Befund erst das Messgerät prüfen** — beide Kodierungen,
  und eine Kontrollzeichenkette, von der man WEISS, dass sie drinsteht.
- ⚠️ **`tools\vsdev-build.cmd` baut standardmäßig NUR `qtmux` — Tests brauchen `all`**
  (`tools\vsdev-build.cmd windows all`). Steht im Kopf des Skripts, ist trotzdem passiert
  (2026-07-31): `ctest` lief danach gegen ein **altes** Testbinary, und zwar in beide
  Richtungen — erst meldete es „grün" für Tests, die es noch nicht kannte, dann bestand der
  **Gegentest** mit absichtlich kaputtem Code. Genau dieses „der Gegentest besteht" ist das
  Alarmsignal. Erste Messung darum immer die **mtime des Testbinaries**
  (`(Get-Item build\windows\test_<x>.exe).LastWriteTime`), nicht die ctest-Zusammenfassung.
- 🔑 **QtTest-Binaries schreiben hier nichts auf die Konsole** (`qt_add_executable` macht sie
  auf Windows zu GUI-Programmen) — `./test_x.exe | grep FAIL` liefert **leere** Ausgabe bei
  Exit 3. Ergebnisse mit `-o <datei>,txt` in eine Datei schreiben und die lesen; nur so sieht
  man, **welche** Fälle fielen. (PowerShell verschluckt zusätzlich die Ausgabe, wenn der
  Exit-Code ≠ 0 ist.)
  🔑 **In der CI kann man `-o` nicht nachschieben** — dort meldet `--output-on-failure` nur
  „***Failed" ohne den Fall (genau so bei `test_updateviewmodel` erlebt). Abhilfe für Tests,
  die **keine Prozesse starten**: `set_target_properties(<t> PROPERTIES WIN32_EXECUTABLE FALSE)`
  (seit 2026-08-02 für `test_updater`/`test_updateviewmodel`). Für `test_pty`/`test_session`/
  `test_sessiongroups` gilt das NICHT — die brauchen das GUI-Subsystem wegen der
  ConPTY-Konsolenvererbung.
- **Nach einem Rebuild `open qtmux.app` NICHT auf eine laufende Instanz** — `open`
  aktiviert nur; das alte Binary antwortet dann (z. B. „Unbekanntes Tool"). Erst beenden,
  dann starten.
- ⚠️ **„Fehlt die Funktion, oder nur das Binary?" — erst datieren, dann suchen.** Meldet
  jemand, eine gebaute Funktion sei in der GUI nicht auffindbar, ist die **erste** Messung das
  Alter des laufenden Programms: `ps -o pid,lstart,command -p <pid>` liefert den Pfad,
  `ls -la` darauf die mtime des Binaries — liegt sie **vor** dem Commit, ist die Frage
  beantwortet, ohne eine Zeile Quellcode zu lesen. Beweiskraft gibt der **Gegentest am
  Artefakt**: Dialogtexte sind als Klartext im Binary auffindbar
  (`grep -ac "<Text aus dem Dialog>" …/MacOS/qtmux`) — alt **0**, neu **>0**.
  🔑 `strings | grep` versagt dabei: QML wird per qmlcachegen eingebettet, `strings` fand 0
  Treffer in **beiden** Binaries und hätte den Fehlschluss „auch der neue Build hat es nicht"
  gestützt. `grep -a` direkt auf die Datei trennt sauber. Passiert am 2026-07-29 mit den
  Agenten-Schaltern aus QTMUX-85/98 (Quelle korrekt, Instanz vom Vortag).
- macOS-GUI-E2E: CGEvent-Tool braucht Pause zwischen MouseDown/Up (sonst nur Hover);
  App-Sprache über das App-Menü umstellen, nicht `defaults write` (cfprefsd-Cache);
  Details [[qtmux-gui-test-macos]].
- **Ohne Bedienungshilfen-Recht testen (macOS):** System Events/`osascript`/CGEvent scheitern
  hart (`-1719`, `AXIsProcessTrusted()`=false). Zwei Wege gehen trotzdem:
  (a) **Beenden** per `NSRunningApplication(processIdentifier:)?.terminate()` — dasselbe
  Apple-Event wie Cmd+Q, aber **PID-genau** (`tell application` ginge über die Bundle-ID und
  träfe die produktive Instanz); Beweiskraft nur mit **Gegentest** (mit Rückfrage: Prozess
  lebt; ohne: er endet); Einstellungen vorher per `defaults write` in die Profil-Domain
  (`com.qtmux.QTmux-<profil>`) — QSettings schreibt `/` als `.`.
  (b) **Maus-Gesten** per synthetischem `QMouseEvent` in den eigenen Prozess (s. QTMUX-100
  weiter unten) — braucht ebenfalls kein Recht.
- ⚠️ Ein temporärer Test-Hook kann selbst der Fehler sein: `Dialog.accept()` direkt in
  `onOpened` wird verschluckt (Popup ist mitten im Öffnen) und sah exakt aus wie ein
  kaputter Bestätigen-Pfad. Erst ein Timer (~400 ms) zeigte die Kette vollständig.
- Windows-E2E: Foreground nur zuverlässig mit `AttachThreadInput`; ein Alt-Stoß vor ESC
  schaltet den Qt-Menümodus (ESC schließt dann nur den). Menüs via UIA-`InvokePattern`
  öffnen. Synthetische Tasten erst nach Warteschleife aufs `MainWindowHandle`.
  ⚠️ **Anhängen an den Ziel-Thread allein genügt nicht**, wenn eine andere App den
  Vordergrund hält (hier VS Code): `AttachThreadInput` zusätzlich an den Thread des
  **aktuellen Vordergrundfensters** + `SystemParametersInfo(SPI_SETFOREGROUNDLOCKTIMEOUT,0)`,
  sonst schlägt `SetForegroundWindow` still fehl und die Tasten landen in der IDE.
  Vordergrund **prüfen** (`GetForegroundWindow()`), nicht annehmen — und den PID des
  Vordergrundfensters mitloggen, das benennt den Dieb sofort.
  🔑 **Fokus holen und Bild ziehen trennen.** Der Alt-Stoß schaltet den Qt-Menümodus und
  schließt damit ein gerade geöffnetes Popup: `tests/release-visual-check.ps1` fotografierte
  deshalb korrekte Fenster **ohne Menü** — sah wie „Menü öffnet nicht" aus, war die eigene
  Fokus-Routine. Seit 2026-07-30 hat das Skript `FocusWindow` + `GrabWindow` getrennt, läuft
  gegen eine **isolierte** Instanz (`-QtmuxProfile visualcheck`, Port 7346), beendet sie am
  Ende und killt nur Prozesse **des eigenen EXE-Pfads** (vorher `Get-Process qtmux |
  Stop-Process` — das hätte die Arbeitsinstanz mitgerissen).
  🔑 Die Werkzeug-Fallen dieser Maschine — Defender beendet `SendKeys`-Skripte, PS 5.1 liest
  UTF-8 als CP1252, ohne bedienten Desktop **keine** synthetische Eingabe und **kein**
  Bildschirm-Grab, `GetWindowTextW` braucht `CharSet.Unicode` — stehen vollständig in
  [[gui-testskripte-windows-fallen]]. Was trägt: **UIA-`InvokePattern`** (braucht keinen
  Eingabefokus) und **`PrintWindow`** mit `PW_RENDERFULLCONTENT`.
- **Das EINSTELLUNGSFENSTER fotografieren (macOS):** Es ist ein eigenes `Window` und mit
  `--screenshot` prinzipiell nicht greifbar. Was trägt, ist ein **temporärer** QML-Hook in
  [qml/Main.qml](qml/Main.qml) — Timer → `prefs.open("<kategorie>")`, zweiter Timer →
  `prefs.contentItem.children[0].grabToImage(…)` → `Qt.quit()`; der Pfad kommt aus
  `Theme.dark`, also liefern zwei Läufe mit `defaults write com.qtmux.QTmux-<profil>
  ui.themeMode -int 1|2` beide Designs. Danach Hook entfernen und neu bauen.
  ⚠️ `timeout` gibt es auf macOS nicht (Exit 127) — solche Läufe in den Hintergrund geben.
- **Das EINSTELLUNGSFENSTER fotografieren (Windows):** `--screenshot` greift nur das
  **Root**-Fenster, das Prefs-Fenster ist ein eigenes `Window` und fehlt darin. Weg, der
  trägt: Kategorie **vorher** in die QSettings-Domain schreiben
  (`HKCU:\Software\QTmux\QTmux-<profil>\ui\prefsCategory`), Instanz starten, per UIA
  „Datei" → „Einstellungen …" öffnen, dann per `EnumWindows` **nach Titel** suchen (in der
  UIA-Kinderliste des Desktops taucht es nicht auf) und mit `PrintWindow` greifen.
  Tastatur-Navigation als Steuerweg ist untauglich: die Rail hat nach dem Öffnen keinen Fokus,
  alle Bilder wurden identisch. 🔑 **Menü-Popups gehen mit demselben Weg**: Ein Qt-Menü ist hier
  **kein** eigenes Fenster, sondern ein Item **im** Prefs-Fenster; zur Sicherheit alle
  sichtbaren Fenster des PID einzeln greifen.
- ⚠️ **Native Dateidialoge sind hier nicht automatisierbar** (Folge des fehlenden bedienten
  Desktops): Das Fenster öffnet und ist per Titel auffindbar („Einstellungen exportieren"),
  aber UIA-`ValuePattern.SetValue` auf das Dateinamenfeld läuft in einen Timeout (0x80131505)
  und blockiert bei Wiederholung minutenlang; Einfügen per Zwischenablage + Enter kommt nicht
  an. Solche Pfade **nicht erzwingen**: Logik im Unit-Test beweisen, Dialog-Öffnen per
  Screenshot, Rest auf die Owner-Abnahme.
- ⚠️ **Umleiten von stdout/stderr beendet die App** (nicht nur die Sessions, Stufe-6-Erfahrung):
  `-RedirectStandardError/-Output` reißt die ConPTY-Anbindung → die einzige Session stirbt →
  das letzte Pane/Window schließt → QTmux beendet sich (QTMUX-87, gewolltes Verhalten); die
  leere Sidebar davor sieht wie ein Regressionsbug aus — genau so schon fehlinterpretiert. Für
  QML-Warnungen darum ein **eigener, kurzer Lauf** mit Umleitung: alle Bindungen von `Main.qml`
  **und** `PrefsWindow.qml` werden beim Start ausgewertet (das Einstellungsfenster ist eine
  Instanz in `Main.qml`, nur unsichtbar) — die Warnungen stehen also im Log, bevor die App
  sich beendet. Der eigentliche Interaktionslauf dann **ohne** Umleitung.
- ⚠️ **`--screenshot` NICHT aus dem Bash-Werkzeug starten.** Von dort entsteht kein PNG und
  der Exit-Code führt in die Irre (die GUI-App hängt nicht am Pipeline-Status). Richtig ist
  PowerShell mit `Start-Process … -PassThru -Wait` und danach `$pr.ExitCode` +
  `Test-Path` — so belegt: 46 kB PNG, Exit 0 (2026-07-30, vier Fehlversuche vorher).
- ⚠️ **Offscreen rendert `MultiEffect` NICHT** (2026-07-31, an der Seitenleiste erlebt).
  Alle per `MultiEffect` eingefärbten Icons — Sidebar-Chevron, Delegate-Icons — fehlen im
  `--screenshot`-Bild **ersatzlos und ohne Warnung**. Toolbar-Icons erscheinen trotzdem,
  weil sie über `icon.source`/`icon.color` laufen; das Bild sieht damit völlig plausibel
  aus. Wer daraus „das Element fehlt" schließt, diagnostiziert sein Messmittel — aufgefallen
  nur, weil der Chevron auch im **ausgeklappten** Zustand fehlte, wo er nachweislich seit
  Wochen funktioniert. **Abhilfe:** `QT_QPA_PLATFORM=cocoa` vor den Aufruf setzen —
  [main.cpp](src/app/main.cpp) überschreibt eine **bereits gesetzte** Variable nicht, der
  Flag greift dann am sichtbaren Fenster (derselbe Weg wie bei den README-Bildern). Nur so
  ist eine `MultiEffect`-Änderung abnehmbar.
- ⚠️ **Detektor-Blindheit (QTMUX-86, teuerste Fehldiagnose):** Um „geht Inhalt verloren?" zu
  messen, lief `read_screen` mit **`scrollback: true`** — der Inhalt war aber genau *dorthin*
  verschoben worden. Der Detektor fand die Marken also wieder und meldete „nur ein
  Rendering-Fehler, keine Daten weg". Erst der Blick auf den **Live**-Bildschirm (ohne
  Scrollback) zeigte: 0 Zeichen. **Regel:** Beim Suchen eines Verlusts das Messfenster genau
  so eng wählen wie die Behauptung — sonst beweist man die eigene Vermutung.
- **Maus-Gesten ohne Bedienungshilfen-Recht testen (QTMUX-100):** CGEvent scheitert hier
  (`AXIsProcessTrusted()` = false), aber **synthetische `QMouseEvent` in den eigenen Prozess**
  brauchen kein Recht: temporäres `Q_INVOKABLE dbgMouse(art, x, y)` auf dem AppController →
  `QGuiApplication::sendEvent(window, &ev)` mit press/move/release, dazu ein QML-Timer, der die
  Positionen protokolliert. Damit läuft der **echte** DragHandler gegen das **echte** Modell.
  🔑 Zwei Fallen dabei: (1) Ereignisse **realistisch schnell** schicken (16 ms/Schritt) — bei
  400 ms sieht Flickable keine Geschwindigkeit und verhält sich anders. (2) Ausgabe in eine
  **Datei** leiten, nicht in eine Pipe: wird der Prozess abgeschossen, geht der Pipe-Puffer
  verloren und es sieht aus, als hätte die App nichts gemeldet.
- ⚠️ **Ein Nachbau ist kein Beweis — er kann am Original vorbeigehen (QTMUX-100).** Für den
  Sidebar-Drag stand ein QML-Minimalnachbau, der eine Drift zeigte; die Ursache dort war aber
  die **Zielzeile 0 bei gescrollter Liste**, nicht der eigentliche Fehler. Erst die Messung an
  der echten App mit echten Ereignissen zeigte den wahren Auslöser (letzte Kachel, `contentY`).
  Der Nachbau hatte also *eine* Drift reproduziert und zu einer falschen Diagnose verleitet.
  **Regel:** Am Nachbau darf man Hypothesen bilden, entschieden wird am Original — und die
  Reproduktion muss die **konkret gemeldeten** Bedingungen treffen (hier: 3 Kacheln, keine
  Gruppen, bis an den Rand). Mit 5 Kacheln und einer mittleren Kachel war nichts zu sehen.
- ⚠️ **Instrumentierung ohne Aufrufstelle beweist nichts.** Ein Log meldete 98 abgefangene
  Nullgrößen — daraus schloss ich auf die Ursache. Mit mitgeloggter Aufrufstelle kamen **alle**
  aus einem Aufruf, den *dieselbe Änderung* neu eingeführt hatte; im alten Code gab es sie
  nicht. **Regel:** Jede Diagnose-Zeile trägt die Herkunft, sonst misst man den eigenen Fix.
- ⚠️ **Pixel-Prüfungen sind nur bei entsperrtem Bildschirm gültig.** Ein Lauf hat den
  Windows-**Sperrbildschirm** fotografiert und über alle Runden identische „Tinte"-Werte
  gemeldet, die wie ein Befund aussahen. Screen-Grabs immer gegen das erwartete Fenster
  gegenprüfen (Fenster-Handle + PID des Vordergrundfensters mitloggen).
- ⚠️ **Marker-Kollision = falsch-positiver E2E-Beweis.** Wird eine Marke per Befehl in die
  Session eingerichtet (`Set-PSReadLineKeyHandler … Insert("META_OK")`), steht sie durch das
  **Echo der Befehlszeile** schon auf dem Bildschirm — `read_screen` findet sie, obwohl nie
  eine Taste ankam. Marke im Befehl **zusammensetzen** (`'MET'+'A_OK'`), damit nur die
  tatsächliche Einfügung sie als Ganzes erzeugt. Aufgefallen nur, weil der Gegentest gegen
  das alte Binary „bestanden" meldete.
  ⚠️ Synthetische **Mausrad**-Ereignisse (`mouse_event WHEEL`) nimmt Qt erst nach einer
  **echten Cursorbewegung** an (Hover-Enter) und nur im Vordergrund — sonst verpuffen sie
  spurlos und man hält ein nicht scrollendes Flickable für ein Layout-Problem.
- ⚠️ **`--screenshot` auf Windows: erst Absturz, dann Kästchen — beides behoben (2026-07-30).**
  Warum `QT_QPA_PLATFORM=offscreen` dort zweifach untauglich ist (fehlendes Plugin → stummer
  `qFatal`-Absturz; mit Plugin → keine Fonts, jede Glyphe ein Kästchen bei Exit 0), steht in
  [[offscreen-plattform-windows-fonts]]. **Umsetzung hier:** `main.cpp` setzt unter Windows
  bewusst **kein** offscreen, sondern greift das **sichtbare** Fenster (TCC ist ein
  macOS-Grund); auf macOS/Linux bleibt offscreen, aber nur wenn das Plugin **vorhanden** ist
  (`offscreenPluginAvailable`), sonst derselbe Ausweichweg statt `qFatal`. Das Plugin wird
  trotzdem mitgeliefert (CMake-Post-Build **und** `build-msi.ps1` — das Paket staged separat,
  eine Stelle allein genügt nicht), am **paketierten** Binary gegengeprüft. Eingegrenzt wurde
  es per A/B: **`QTMUX_NO_GPU=1` allein läuft stabil**, der Anwenderfall `gpuRendering=false`
  war nie betroffen.
- **Laufende Instanz fotografieren (Windows):** `--screenshot` startet immer einen **neuen**
  Prozess. Wer eine **laufende** Instanz abbilden will, nimmt `PrintWindow` mit
  **`PW_RENDERFULLCONTENT` (2)** auf `MainWindowHandle` — braucht **keinen Vordergrund** und
  stört damit die Arbeit des Owners nicht (dem `keybd_event`-Weg vorzuziehen).
- ⚠️ **PowerShell-Testskripte: `$args` ist eine automatische Variable.** Ein Parameter dieses
  Namens (`function Mcp($name, $args)`) wird verschluckt — die MCP-Aufrufe gingen **ohne
  Argumente** hinaus. Symptom: `cwd` schien ignoriert, `send_text` tat nichts, `read_screen`
  antwortete „Parameter 'id' fehlt". Sah wie drei Fehler in der App aus, war einer im Skript.
  Dieselbe Klasse wie `$pid`/`$Profile` — Parameternamen in PowerShell-Harnessen präfixen.
- ⚠️ **`wait_for_events` ohne `afterSeq` zeigt NUR künftige Ereignisse.** Der Long-Poll steigt
  beim aktuellen Stand ein; ein soeben gesendetes Ereignis fehlt dann in der Antwort, und das
  sieht exakt aus wie „der Ereignisweg ist kaputt". Verräter ist `nextSeq` — steht es über 0,
  liegen Ereignisse vor. Für eine **Messung** darum `afterSeq: 0` übergeben. (Beim
  QTMUX-38-Nachweis erst als Fehlschlag gelesen.) Zweite Falle daneben: `subscribe_events`
  braucht die `sessionId` als **Argument**; ohne sie antwortet es „Keine Subscriber-Session",
  auch wenn `attach_controller` vorher „ok" meldete.
- MCP-E2E ist der Standard-Verifikationsweg gegen die echte GUI (create_session/send_text/
  read_screen, `scrollback:true` für Historie) — gegen eine **isolierte Testinstanz**
  (s. Build-Abschnitt macOS), nie gegen eine, in der jemand arbeitet. Ergebnisse möglichst
  am **Zustand** messen statt am Screenshot (z. B. Palette-Befehl ausführen → `list_sessions`
  prüfen); rein visuelle Änderungen brauchen `--screenshot`/Screenshot + Anwender-Abnahme.
  ⚠️ **Den MCP-Port VOR dem Messen auf Eigentümerschaft prüfen** (`lsof -nP -iTCP:<port>
  -sTCP:LISTEN`): Läuft dort schon eine fremde Testinstanz (paralleler Worker!), bindet die
  eigene still **nicht** — jede Antwort kommt dann von der fremden Instanz und sieht völlig
  plausibel aus. Genau so ging 2026-07-28 ein kompletter Messdurchlauf an die falsche App.
  🔑 **Am 2026-07-29 erneut hineingelaufen — der häufigste Fall ist die eigene Leiche.**
  Nicht ein fremder Worker, sondern eine **vergessene Instanz aus einem früheren Lauf
  derselben Sitzung** hielt den Port; `kill -TERM` hatte sie zuvor nicht erwischt. Symptom:
  MCP meldete 12 Zeilen mit den erwarteten Marken, während dieselbe Session in C++ nur den
  Prompt hatte. **Regel:** Die Prüfung als PID-**Vergleich** in das Testskript einbauen
  (`lsof`-PID gegen `$!`) und bei Ungleichheit abbrechen — die bloße „ist der Port belegt?"-
  Frage genügt nicht, denn belegt ist er ja, nur vom Falschen. Und der Widerspruch zweier
  Messwege ist das Alarmsignal: Zwei Quellen für dieselbe Session dürfen nie verschiedene
  Inhalte melden.
  ⚠️ **Persistenz nach dem Beenden mit `defaults read <domain>` lesen, NICHT mit `plutil` auf
  der `.plist`** — cfprefsd hält die Datei zurück; die Datei zeigte „gar keine `windows`-
  Schlüssel", während `defaults read` den korrekt geschriebenen Stand lieferte.
- **Agenten-Neustart prüfen (QTMUX-85):** Ein **Stub-Agent** ist der saubere Messfühler —
  eine ausführbare Datei, die wie ein Agent aus der Registry heißt (`opencode`, `qwen`) und
  `pid`, `pwd` und ihre Argumente ausgibt. 🔑 Sie über den **absoluten Pfad** aufrufen, nicht
  über `PATH`: Die Login-Shell baut `PATH` per `path_helper`/`.zshrc` um und stellt den echten
  Agenten davor — sonst startet man versehentlich das Original (passiert). 🔑 Die **PID im
  Marker** ist entscheidend: Der wiederhergestellte Scrollback enthält die *alte* Startzeile,
  ein bloßes „Marker gefunden" beweist also nichts; erst eine **neue** PID belegt einen echten
  Neustart. 🔑 Und `detect` prüft den **ersten** Token — `cd /tmp && opencode` erkennt nichts.
- **Doku-Wächter `test_doc_duplicates`** (QTMUX-34): findet doppelte Überschriften, wie
  sie beim Kompaktieren entstehen (Block eingefügt statt ersetzt → zwei gleichnamige
  Abschnitte mit widersprüchlichem Inhalt; in RAFTNG genau so passiert). Verglichen wird
  der Überschriften-**Pfad**, damit das zweisprachige README keinen Fehlalarm auslöst.
  `file(STRINGS)` braucht dort **`ENCODING UTF-8`** — sonst verschluckt CMake bei Zeilen
  mit Emoji den Zeilenanfang, die `##`-Marke geht verloren und der Pfad verrutscht.
  🔑 **Und es braucht den `REGEX`-Filter — der Wächter war bis 2026-08-06 fast blind.**
  Wird die Datei **vollständig** eingelesen (gleich ob `file(STRINGS)` oder `file(READ)` +
  Splitten), zerlegen einzelne **Prosa**-Zeilen die CMake-Liste und alles dahinter geht
  verloren: Zeilen mit Backslash-Sequenzen (`\033[2J`) und mit Variablen-Referenzen
  (`${command:cmake.activeBuildPresetName}`) im Fließtext. Gemessen: von der `CLAUDE.md`
  kamen **141 von 779** Zeilen an — **7 von 27** Überschriften; von
  `docs/feature-referenz.md` **62 von 1044** (4 von 13). `README.md` und `docs/MCP.md`
  waren zufällig vollständig, **deshalb fiel es jahrelang nicht auf**. Der Test meldete
  grün, ohne die Datei je gesehen zu haben.
  ⚠️ **Aufgefallen ist es nur, weil ein Gegentest NICHT fiel** — ein künstlich eingebautes
  Duplikat blieb unbemerkt. Genau dafür ist die Gegentest-Pflicht da: Ein grüner Wächter
  ohne Gegenprobe belegt nichts über die Doku, sondern nur, dass der Prozess startete.
  **Prüfmaß nach jeder Änderung am Skript:** Erkennungsrate je Datei gegen
  `grep -c '^#\+ '` — sie muss **100 %** sein.
- **CMake-Skripttests (`cmake -P`) laufen ohne Policies** — ohne `cmake_minimum_required`
  im Skript steht CMP0057 auf OLD und `IN_LIST` ist dann kein Operator, sondern ein
  Fehler. Lokal unsichtbar, wenn die eigene CMake neuer ist als die des CI-Runners
  (so brach `test_doc_duplicates` nur den Linux-Job, macOS/Windows waren grün).
  In Skripttests daher `cmake_minimum_required` setzen **und** policy-unabhängige
  Befehle bevorzugen (`list(FIND)` statt `IN_LIST`). Neue CMake-Versionen kennen das
  OLD-Verhalten alter Policies teils nicht mehr → der CI-Zustand ist lokal nicht
  nachstellbar; dann die Ursache strukturell ausschließen statt sie zu reproduzieren.
- Claude-CLI-Fallen (Agenten-Demos): `--settings` braucht eine DATEI; `--allowedTools`
  ist variadisch → Prompt via stdin.
