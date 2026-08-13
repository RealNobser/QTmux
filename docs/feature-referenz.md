# QTmux — Feature-Referenz (Mechanik & teuer erkaufte Lektionen)

> **Wann lesen:** immer dann, wenn an einem der unten beschriebenen Bereiche gearbeitet wird
> — die Abschnitte erklären *warum* etwas so gebaut ist und welche Falle es dort schon gab.
> Ausgelagert am **2026-08-06** aus der `CLAUDE.md`, weil sie in jeder Session vollständig
> geladen wird, dieses Nachschlagewerk aber nur beim Anfassen des jeweiligen Features
> gebraucht wird. **Pflegeregeln unverändert:** je Sachverhalt EINE Stelle, kein
> Status-Changelog (Stände stehen im „Arbeitsstand" der `CLAUDE.md`), ⚠️- und 🔑-Zeilen
> nicht umformulieren — sie sind der eigentliche Wert.

## Feature-Referenz (kompakt, mit Lektionen)

### Rendering (GPU-Glyph-Atlas, QTMUX-6)
`TerminalItem` = `QQuickItem` mit eigenem `QSGMaterial` + RHI-Shadern
(`src/terminal/shaders/glyph.{vert,frag}`, via `qt_add_shaders`); `GlyphAtlas` rastert
zellweise Alpha-Masken (Shelf-Packer, wächst in der Höhe) + **Glyph-Index-Atlas** für
Ligaturen (`glyphByIndex`, `QTextLayout`-Run-Shaping — Atlas durch Glyph-Zahl des Fonts
begrenzt). Farb-Emojis: `tileHasColor()`-Erkennung, Vertex-Alpha als Mono/Farb-Selektor
im Shader. **Damage-Gating:** teurer Inhalt nur bei `m_geomDirty`, Overlay
(Selektion/Cursor/Scrollbalken) billig bei jedem Update.
- 🔑 Custom-Material: Textur in `updateSampledImage` per **`commitTextureOperations`**
  hochladen — sonst Glyphen unsichtbar (`QSGSimpleTextureNode` macht es intern, wir nicht).
- 🔑 **Renderpfad-Tests müssen beweisen, dass der Pfad aktiv ist** (Fallback absichtlich
  brechen oder loggen) — der GPU-Ligatur-Code war einmal toter Code (`useGpu()`-Bedingung
  nicht geändert) und die „Verifikation" lief unbemerkt über den korrekten Fallback.
- Fallback: `gpuRendering=false` / Env `QTMUX_NO_GPU=1` → `QPainter`-Pfad (Run-basiert).
- 🔑 **Keine Glyphe darf über ihre Kachel hinausmalen (QTMUX-97).** `glyph()` zeichnet mit
  hartem `setClipRect(rect)`; passt die **Tinte** trotz korrekter Zellzahl nicht, wird
  proportional eingepasst (gemessen am `tightBoundingRect`, **nicht** am Advance — Emoji-
  Bitmaps haben Seitenränder, ein Advance-Vergleich verkleinerte sonst jedes Doppelzellen-
  Emoji grundlos). Vorher blutete ein Farb-Emoji (**2,15 Zellen breit**) in die
  **Nachbarkachel** des Shelf-Packers; der Überhang blieb dort stehen (spätere Glyphen
  werden nur *darüber*gemalt) und — perfider — `tileHasColor()` stempelte den verunreinigten
  Nachbarn als **Farb-Glyphe** ab, die der Shader dann nicht mehr einfärbt. Symptome daher
  zweierlei: fremde Emoji-Bruchstücke auf beliebigen Zeichen **und einzelne Buchstaben in
  Emoji-Farbe**. Wen es traf, entschied allein die Einfüge-Reihenfolge in den Atlas → wirkte
  zufällig. Die Wurzel lag aber in libvterm (VS-16-Breite, s. o.): Der Atlas-Clip ist das
  Sicherheitsnetz, die Zellbreite der Fix. Messung: Überhang **21 px → 0 px**; live per MCP
  `read_screen` gegengeprüft (114 Spalten: **57** ⚠️ pro Zeile statt 100 in einer).

### Terminal-Verhalten
- **Scrollback** (Cap 10000) in `VtScreen`; Selektion in **absoluten** Inhalts-Zeilen
  (scroll-fest); **Soft-Wrap-Copy** via `sb_pushline4`-Continuation-Flags (eine logische
  Zeile ohne `\n` am weichen Umbruch).
- **Maus-Reporting:** `VtScreen` trackt `VTERM_PROP_MOUSE` (DECSET 1000/1002/1003);
  `TerminalItem` leitet Rad/Klick/Drag an libvterm (X10/SGR-Sequenzen), sonst lokaler
  Scrollback/Selektion; **Shift+Drag** selektiert immer lokal. macOS: Cmd=ControlModifier,
  physisches Ctrl=Meta. libvterm **entprellt** (Tests brauchen press→release-Paare).
  Hover-only-Tracking (1003 ohne Taste) nicht gemeldet.
  🔑 **Nur im Alternate Screen weiterleiten (QTMUX-104).** Bedingung ist
  `appMouseActive() = mouseTracking() != 0 && altScreen()`. Grund: Endet ein Maus-TUI
  **unsauber** (Crash, `kill`, SSH-Abbruch), kommt kein `DECRST` und das Tracking-Flag bleibt
  hängen — die zurückkehrende Shell füllte sich sonst bei jeder Mausbewegung mit SGR-Codes
  (`35;63;49M…`). Vollbild-TUIs (Agenten, vim, htop, less) laufen im Alt-Screen, ihre Maus
  bleibt also intakt; die Shell am Prompt ist im Primary Screen → dort nie weiterleiten.
  🔑 **Voraussetzung: `vterm_screen_enable_altscreen(m_screen, 1)`** im Konstruktor (VOR
  `reset`). Ohne das meldet libvterm **kein** `VTERM_PROP_ALTSCREEN`, und `altScreen()` bliebe
  immer false (Gegenprobe: Test `altScreenTracked` FAIL). Nebeneffekt ist zugleich korrektes
  Terminal-Verhalten: Nach einem TUI kehrt der vorherige Shell-Inhalt zurück (Alt-Screen-Puffer);
  am Scrollback-Dump (QTMUX-81, `serializeAnsiRoundTrip`) ändert sich nichts.
  ⚠️ **Falle beim Testen des Alt-Screen-Inhalts über MCP:** `1049h` blendet um, ein echtes TUI
  **löscht** den Alt-Screen selbst — ein Testskript ohne `\033[2J` zeigt darum Reste des
  Primary, was wie ein fehlender Wechsel aussieht. Mit `clear`/sleep im Alt-Screen sauber
  messbar (dann nur der Alt-Inhalt sichtbar).
  🔑 **Manueller Notausgang (QTMUX-104):** `VtScreen::resetInputModes()` speist DECRST-Sequenzen
  (1000/1002/1003/1006 Maus, 2004 Bracketed Paste, SGR-Reset, Cursor sichtbar) in den EIGENEN
  Parser — `m_mouseTracking` geht über den regulären `cbSetMouse`-Pfad auf 0, **ohne** Bildschirm
  oder Alt-Screen anzutasten. In der GUI „Terminal-Eingabe zurücksetzen" (Ctrl/Cmd+Shift+I,
  Menü, Palette). Für die Fälle, in denen auch der Alt-Screen hängt. Tests
  `altScreenTracked`, `resetInputModesClearsMouse`.
  🔑 **1007 wird dabei bewusst NICHT gelöscht** (QTMUX-128): Ein hängendes Alternate-Scroll-Flag
  richtet keinen Schaden an (das Rad wird nur im Alt-Screen zur Taste), aber es zu löschen würde
  einem *laufenden* Codex das Rad abschalten — der sendet 1007h nie erneut.
- **Mausrad, wenn die App ihren Verlauf selbst zeichnet (QTMUX-128):** QTmux kann nur scrollen,
  was in **seinem** Scrollback liegt, und dorthin gelangen ausschließlich Zeilen, die oben aus
  dem **Primary** Screen herausgeschoben wurden. Zwei Fälle füllen ihn nie: ein Vollbild-TUI im
  Alt-Screen (libvterm schiebt Alt-Screen-Zeilen bewusst nicht hinein) — **und** eine Anwendung,
  die im Primary Screen einen festen Sichtbereich an Ort und Stelle neu zeichnet. Dann ist
  `scrollByLines` ein No-op und das Rad wirkt „festgenagelt" (genau der Anwenderbefund). Regel
  Gui-frei in [src/core/AltScroll.h](src/core/AltScroll.h) (`wheelGoesToApp`, Enum
  `AltScrollMode`), Einstellung `window/altScrollMode` (Eingabe & Zwischenablage, Palette,
  Suchindex), Tastenfolge je Agent in der `AgentRegistry` (`scrollKeysFor`).
  🔑 **Der teuerste Irrtum dieses Tickets — zwei falsche Schlüsse in Folge, beide von einer
  Messung widerlegt:** (1) „Live-Bildschirm == Scrollback ⇒ Alt-Screen" ist **kein** gültiger
  Schluss — ein leerer Scrollback heißt nur, dass nie etwas oben herausgeschoben wurde. Codex
  läuft nachweislich im **Primary** Screen (`altScreen()` = false, im `wheelEvent` protokolliert).
  (2) Codex **bringt `ESC[?1007h` im Binary mit, sendet es aber nicht** — und reagiert auch nicht
  auf das, was 1007 verspricht: einfache Cursor-Tasten, SS3-Form, PageUp/PageDown und End
  bewirken bei ihm **nichts**, es scrollt allein auf **Shift+Pfeil** (`ESC[1;2A`/`B`; am
  laufenden Codex 0.146.0 gemessen, Rundlauf exakt umkehrbar). Eine Umsetzung nach reiner
  Sequenz-Archäologie hätte also fehlerfrei gebaut, getestet und beim Anwender **nichts**
  bewirkt.
  🔑 **Zwei Klauseln tragen die Sicherheit:** Im Primary Screen wird nur delegiert, wenn (a) für
  den erkannten Agenten eine **gemessene** Taste vorliegt und (b) QTmux selbst **keinen**
  Scrollback hat. (a) verhindert Tasten ins Blaue — an einem Shell-Prompt blättern Cursor-Tasten
  die Befehls-Historie durch; (b) gibt dem eigenen Verlauf des Anwenders Vorrang und heilt
  zugleich „Agent beendet, Shell wieder da". Im Alt-Screen wird (b) bewusst **nicht** geprüft:
  Was dort liegt, stammt vom Primary Screen und wäre der falsche Inhalt.
  ⚠️ **Bekannte Grenze:** Der Weg hängt an der Agenten-**Erkennung**, und die prüft nur den
  ersten Token (`cd X; codex` erkennt nichts, s. QTMUX-88) — dann bleibt das Rad tot. Das war
  zugleich die Gegenkontrolle: identische App, unerkannter Agent → keine Wirkung.
- **Kopieren nimmt die Auswahl, nicht den Fokus (QTMUX-105):** Cmd+C/Kopieren läuft über
  `window.copyActiveSelection()` — bevorzugt das aktive Pane, fällt aber auf das erste Pane
  mit `hasSelection` zurück. `activeHasSelection` (treibt `actCopy.enabled` und damit den
  Cmd+C-Shortcut) berücksichtigt **jedes** Pane, nicht nur das aktive; dazu meldet jedes Pane
  in [SplitNode.qml](qml/SplitNode.qml) sein `selectionChanged` ans Fenster. 🔑 **Warum:**
  Der sporadische „Cmd+C kopiert nichts"-Bug entstand, weil der Fokus nach dem Selektieren
  wegwandern kann (anderes Pane, Statusleiste, Flyout, Suchfeld — mit Design 1a/2a mehr
  Fokus-Fänger); `activeTerminal.copy()` fand dann keine Auswahl. `TerminalItem::copy()`
  protokolliert den **Problemfall** (leerer `selectedText()`) leise nach Console.app —
  fängt eine etwaige andere Ursache (reine Whitespace-Auswahl wird zu `""` getrimmt), ohne
  Rauschen im Normalbetrieb. Ohne Bedienungshilfen-Recht nicht per synthetischem Cmd+C
  reproduzierbar → Code-Review + Diagnose.
- **Bildschirm leeren, Verlauf behalten (QTMUX-61):** `VtScreen::clearViewportKeepScrollback()`
  schiebt alles **oberhalb der Cursorzeile** in den Scrollback; die Prompt-Zeile rückt nach
  oben. Umgesetzt als **CSI `<n>` S** (Scroll Up) in den **eigenen** Parser (`inputWrite`) —
  nicht ans PTY: Ein getipptes `clear` verwirft je nach Agent/TUI den Verlauf und landete im
  Eingabefeld eines laufenden Agenten. Danach muss der Cursor per CSI H **selbst** gesetzt
  werden, CSI S bewegt ihn nicht mit (Gegenprobe ohne diese Zeile: Test FAIL).
  Bei Cursor in Zeile 0 passiert nichts — CSI **0** S würde als „rolle um 1" gelesen und die
  Prompt-Zeile schlucken. Kürzel `Ctrl/Cmd+Shift+K` (nicht Ctrl+K — das gehört der Shell),
  Ansicht-Menü, Palette. Test `tst_vtscreen::clearViewportKeepsScrollback`.
  🔑 `screenText()` schneidet **rechte Leerzeichen** ab — ein `startsWith("$ ")` im Test
  scheitert also, obwohl die Zeile korrekt steht; direkt an `cell(0,0)` prüfen.
- **Tasten:** Übersetzungslogik Gui-frei in `src/core/KeyEncoding.cpp` (`encodeKeyBytes`,
  Test `test_keyencoding`); `TerminalItem::encodeKey` delegiert nur. F1–F12 als
  xterm/VT220-Sequenzen (F-Tasten gehören der Shell — keine globalen F-Tasten-Shortcuts);
  **Shift/Alt+Enter → ESC CR** (QTMUX-43: Umbruch einfügen statt absenden in Agenten-TUIs
  wie Claude Code — dieselbe Sequenz, die `/terminal-setup` anderswo auf Shift+Enter legt;
  klassische Shells binden ESC CR nicht, unter ConPTY kann das ESC die Eingabe verwerfen —
  bewusst in Kauf genommen, unmodifiziertes Enter bleibt CR). Copy/Paste macOS Cmd+C/V,
  sonst Ctrl+Shift+C/V; Smart Ctrl+C (Auswahl→Copy, sonst SIGINT). Bracketed Paste +
  Multiline-Warnung; Copy-on-Select + Rechtsklick-Paste optional.
- **Meta-Kodierung Alt+&lt;Zeichen&gt; → `ESC`+Zeichen (QTMUX-84):** `encodeMetaSequence`
  (eigene Funktion, damit sie **plattformunabhängig** testbar bleibt) + Gate
  `metaPrefixEnabled()` = **Windows/Linux, auf macOS aus** (dort erzeugt Option
  Sonderzeichen und physisches Ctrl ist bereits Meta). Damit kommen Claude Codes **Alt+V**
  (Bild aus der Zwischenablage — unter Windows, weil Ctrl+V dort Text-Paste ist) und die
  readline-Kürzel Alt+B/F/D beim Agenten an. Kodiert wird im `default`-Zweig, Enter (QTMUX-43),
  Backspace und die Steuertasten bleiben also unberührt.
  🔑 **Teuerste Falle: AltGr meldet Windows als Ctrl+Alt.** Ohne die Ausnahme
  `if (mods & Qt::ControlModifier) return {}` würden auf deutschen Tastaturen `@` (AltGr+q),
  `€`, `\ ~ | [ ] { }` zerstört — der Fix wäre schlimmer als der Fehler. Am echten Layout
  gegengeprüft (`keybd_event` mit VK_RMENU): `altgr:@` bleibt `@`.
  🔑 Zwei Quellen für das Zeichen: ist `text()` gefüllt (Linux), wird es layout-treu
  übernommen; ist es leer (Windows bei Alt+Buchstabe), wird das Zeichen aus dem **Key-Code**
  gebildet (`Qt::Key_V == 'V'`, ohne Shift kleingeschrieben). Nicht-ASCII-Keycodes ohne
  `text()` werden abgelehnt, statt ein Zeichen zu erfinden.
- **Klickbare Links (QTMUX-39):** `LinkDetector` (Gui-frei) findet **URLs**
  (Scheme-Whitelist http/https/ftp/mailto/file — KI-Output darf keinen beliebigen Handler
  starten) und **existierende Dateipfade** (gegen Session-CWD; die `QFileInfo::exists`-Prüfung
  IST der Fehlalarm-Filter). Unterstreichung + Hand-Cursor schon beim **Hover**, Pane-Pille
  „⌘/Strg-Klick zum Öffnen: <ziel>" (`hoverLinkTarget`); das **Öffnen** bleibt an
  Cmd/Ctrl-Klick (`QDesktopServices::openUrl`) — bewusste Geste gegen versehentliches Öffnen.
  🔑 Erkennung lief zuerst nur bei gehaltenem Modifier (Syscall-Sparen) — ohne sichtbaren
  Hinweis fand der Anwender die Geste nicht. Jetzt Hover, aber **je Zeile gecacht**
  (`m_hoverDetectRow`), nicht je Pixel. Klick läuft **vor** der App-Maus-Weiterleitung.
  Zeilentext aus `absLineText(absRow)` (Spalten↔Zeichen 1:1, solange kein Emoji davor).
  **Rechtsklick auf einen Link** bietet im Kontextmenü zusätzlich „Link kopieren" an
  (Owner-Wunsch 2026-08-09): `TerminalItem` hält das Ziel unmittelbar vor
  `contextMenuRequested` in `contextLinkTarget` fest (`linkTargetAt` — dieselbe
  Span-Logik wie `openLinkAt`), `popupTermContextMenu` übernimmt es einmalig beim
  Öffnen; kopiert wird das **aufgelöste** Ziel (bei Dateipfaden also der absolute Pfad).
  Bewusst nur kopieren, nicht öffnen — das Öffnen bleibt die Cmd/Ctrl-Geste.
  Tests: `tst_linkdetector` + `tst_vtscreen::linkDetectionOnScreenLine`.
  **OSC 8 bewusst NICHT** — s. offene Jira (QTMUX-40).

### PTY-Layer
- `UnixPty`: forkpty, O_NONBLOCK-Master. **⚠️ `write()` ist gepuffert** (`pending` +
  `pendingPos` + Write-`QSocketNotifier`, nur aktiv solange etwas wartet) — der Kernel
  nimmt nur ~1 KB pro `::write()`; ohne Pufferung ging alles darüber **still verloren**
  (QTMUX-28; Regressionstest `tst_pty::largeWriteIsNotTruncated`).
- `terminate()`: Prozessbaum-Kill (SIGHUP→SIGKILL via `descendantPids`); das Abernten
  läuft im **detached Thread** (blockierendes `waitpid` auf schwere node-Bäume fror sonst
  die GUI sekundenlang ein); App-Quit-Sonderpfad `Pty::s_quitting` = synchron ohne
  `waitpid` (damit `nohup`-Nachfahren vor Prozessende sterben).
- **Login-Shell:** `argv[0] = "-zsh"` (optionaler `argv0`-Parameter in `Pty::start`) —
  nur für echte Shells ohne eigene Args; sonst fehlen `~/.zprofile`/Homebrew-PATH.
- `currentWorkingDirectory()`: macOS libproc, Linux `/proc`, Windows PEB — Funktionstest
  bestanden; Details und die prinzipielle PowerShell-Grenze im ConPTY-Abschnitt oben.

### Sessions & UI
- **Persistenz:** Session-Liste (Typ, Serial/SSH-Parameter, CWD) via QSettings;
  `m_shuttingDown`-Guard (sonst leert `shutdownAll` den gespeicherten Zustand),
  `m_restoring`-Guard (Restore erbt kein fremdes CWD, führt keine Login-Scripts aus).
  Neue Shell **erbt das Live-CWD** der aktiven Session (nur Shell-Quellen, explizites
  Verzeichnis hat Vorrang).
- **Wiederhergestellter Verlauf wartet auf die Pane-Breite (QTMUX-130).** Der Scrollback wird
  als ANSI-Strom gesichert (`VtScreen::serializeAnsi`), in dem weiche Umbrüche **bewusst
  ohne CRLF** stehen — das Terminal soll beim Einspielen selbst auf die aktuelle Breite
  umbrechen. Genau daran scheiterte der Restore: `SessionModel` hängt jedes Backend mit den
  geratenen Startwerten **80×24** an, und `Main.qml` lud den Dump unmittelbar danach; die
  echte Breite kommt aber erst über `TerminalItem::applyPendingResize` (Layout + 60 ms
  Entprellung, QTMUX-86). Der Verlauf wurde also bei 80 Spalten **hart in Zellen** umbrochen —
  und `vterm_set_size` reflowt den Scrollback nicht (feste `Cell`-Vektoren), der Umbruch war
  eingefroren. Beim nächsten Beenden wurde er als weiche Fortsetzung gespeichert und erneut
  bei 80 zerhackt: **der Zustand heilte nie von selbst.**
  🔑 **Neu:** `loadHistoryFor` übergibt nur noch (`Session::setPendingHistory`), eingespielt
  wird beim ersten echten `Session::resize()`. Solange puffert die Session auch die
  **Backend-Ausgabe** (`onBackendData`) — sonst stünde das frische Prompt *über* der Historie.
  ⚠️ **Die drei Fälle, die man dabei übersieht:** (1) Ein Pane, das zufällig genau 80×24 trifft,
  bekommt ein `resize` mit unveränderten Werten — der frühe Ausstieg dort muss trotzdem
  freigeben, sonst hängt der Verlauf. (2) Ein Fenster, das seit dem Neustart **nie sichtbar
  war**, wird nie vermessen; dafür trägt die Dump-Datei eine optionale Kopfzeile
  (`core/HistoryDump.h`, `QTMUX-HISTORY 1 cols=<n>`), aus der das Sicherheitsnetz nach 1,5 s
  die **zuletzt bekannte** Breite nimmt statt der geratenen 80. Alte Dumps ohne Kopfzeile
  bleiben gültig (Breite unbekannt = 0). (3) Wer speichert, **solange der Verlauf aussteht**,
  sichert einen leeren Bildschirm und **löscht damit den Dump auf der Platte** — `saveHistory`
  und `saveHistoryFor` steigen bei `hasPendingHistory()` aus.
  📋 Belegt am lebenden Objekt (offscreen-Instanz, 84 Spalten, Zeile aus 100 Zeichen):
  alter Stand `34449de` bricht nach dem Neustart bei **80** um (80 + 26), der neue bei **84**
  (84 + 22) — also genau dort, wo die Anwendung selbst umbrochen hätte. Tests:
  `test_restorehistory` (11 Fälle, inkl. Gegentest `oldOrderWouldWrapAt80`).
- **Gruppen in der Sidebar (QTMUX-42/45, seit QTMUX-83 **Window**-Gruppen):** Frei benannte,
  einklappbare Gruppen mit Kopfzeile + Anzahl; Farbe aus dem Namen gehasht. Zuordnung per
  Rechtsklick, Palette oder MCP (`set_window_group`, `set_session_group` wirkt aufs Window
  der Session). 🔑 Angezeigt über **`ListView.section`** → verlangt **zusammenhängende
  Blöcke**, also sortiert das **Model** um, nicht die View. Drei Fallen (Model-Teil durch
  `tst_sessiongroups` abgesichert): Umgruppieren darf **nicht** über `moveSession` laufen
  (Drag übernimmt bewusst die Gruppe der Nachbarschaft und überschriebe die neue);
  **gruppenlose** Einträge sind KEIN schützenswerter Block (unsichtbare Section — sonst
  springt die erste Zuordnung ans Listenende); `groups()`/`groupSize()` sind Funktionen ohne
  Property → QML braucht den Anker `groupsChanged`/`groupsRevision`, sonst frieren Kopfzeile
  und Kontextmenü ein.
  🔑 **Einzug statt nur Farbe:** Gruppierte Kacheln sind 12 px eingerückt (Form erkennbar,
  nicht nur Farbe), die Farbmarke sitzt in der Einzugsspalte, der rote MCP-Controller-Tab am
  Rand der Kachel — vorher teilten sich beide den Kachelrand und die Marke war per
  `!mcpController` abgeschaltet, also genau an der interessantesten Kachel unsichtbar.
  Eingerückt wird der **Inhalt über Margins**, NICHT die Delegate-Wurzel: ein `x`-Binding
  dort ist wirkungslos (die ListView setzt die Querachse selbst) und schob die Marke aus dem
  `clip:true`-Viewport. Ein inneres `card`-Rechteck trägt Auswahl/Hover.
- **Befehlspalette (Strg/Cmd+K):** Das Such-/Befehlsfeld in der Toolbar ist die zentrale
  Sammelstelle **aller** Funktionen — feste Befehle plus dynamisch je Plugin-Backend, je
  Verbindungsprofil, je Sitzungsgruppe und je offener Session („Wechseln zu: …"). Sie wird
  bei **jedem Öffnen** neu gebaut (`buildCommands()` in `openFor()`), dynamische Einträge
  sind also immer aktuell; Kürzel-Anzeige kommt live aus `Hotkeys.bindings`. 🔑 Neue
  Funktionen gehören HIER hinein, sonst entsteht die Schieflage aus QTMUX-46: Ein Schalter
  lag nur im Einstellungsdialog (`confirmQuit`), Gruppen nur im Rechtsklick — beides in der
  Palette unauffindbar. Faustregel: Was per MCP steuerbar ist, muss auch die Palette können
  (Ausnahme Vault — bewusste Sicherheitsgrenze).
- **Session-Größe wird entprellt (QTMUX-86):** `recomputeGrid()` reicht das Raster **nicht
  sofort** an die Session, sondern über einen 60-ms-Einmal-Timer (`m_resizeTimer` →
  `applyPendingResize`). Grund: Beim Auf-/Abbauen des Pane-Baums (Window-Wechsel, Teilen,
  Zoom) durchläuft ein Pane binnen Millisekunden Zwischenhöhen — **gemessen 418×56 = 2 Zeilen**,
  Millisekunden später 418×278 = 14. Bei 2 Zeilen schiebt libvterm den **ganzen sichtbaren
  Bildschirm in den Scrollback**. Ein TUI holt sich das per SIGWINCH zurück, eine einfache
  Shell (`cmd`/PowerShell) zeichnet **nicht** neu → das Pane bleibt leer, obwohl die Session
  lebt und korrekt gezeichnet wird. Genau daher die Nicht-Determinismus-Erfahrung: es hängt am
  Inhalt, nicht am Zufall. Belegt per A/B am instrumentierten `Session::resize` (identischer
  Detektor): **ohne** Fix 34 angekommene Resizes, davon 2 auf ≤ 5 Zeilen — **mit** Fix 7, davon 0.
  Zusätzlich Gui-frei abgesichert: `gridFor()` ([src/core/TerminalGrid.h](src/core/TerminalGrid.h),
  Test `test_terminalgrid`) liefert bei nicht positiver Größe `valid=false` → ohne belastbare
  Größe wird gar nichts abgeleitet (früher wurde auf 1×1 geklemmt; ein 1-Spalten-Reflow kürzt
  jede Zeile auf ihr erstes Zeichen — im Scrollback als einzelne `H` aus `H:\…>` sichtbar).
  🔑 **Merke:** Nicht die Anzeige war schuld, sondern eine **transiente Layout-Größe, die bis
  ins PTY durchgereicht wurde**. Wer hier etwas ändert, prüft nicht Pixel, sondern die
  **angekommenen** Größen (`Session::resize` protokollieren).
- **Session-ID in der Kachel (QTMUX-44):** Jede Sidebar-Kachel zeigt neben dem Titel klein
  und monospaced `#<id>` — die **stabile** `Session::id()`, also genau die Nummer, mit der
  man die Session per MCP anspricht (`send_text`, `set_session_group` …). Model-Rolle
  `IdRole`/`"sessionId"`; im Delegate `required property int sessionId`. Bewusst NICHT der
  Zeilenindex (der wandert beim Umsortieren/Gruppieren).
- **Fenster darf nicht neben dem Bildschirm starten (2026-07-31):** `window.ensureWindowOnScreen(win)`
  in [qml/Main.qml](qml/Main.qml), gerufen als **erstes** in `Component.onCompleted` und aus
  `PrefsWindow.open()` (über `host.app`, **vor** `show()`). Prüft, ob das Fensterrechteck
  mindestens 120 × 120 px mit *irgendeinem* Bildschirm überlappt; sonst wird die Größe in den
  Bildschirm eingepasst und das Fenster **zentriert**.
  🔑 **Warum** (Diagnose, Messfalle `GetWindowRect` vs. `window.x/y` und das A/B stehen in
  [[app-startet-nicht-fenster-ausserhalb]]): Die Geometrie wird persistiert
  (`window/x|y|width|height`, für den Dialog `ui/prefsX|Y`), der Monitor aber nicht — fehlt er
  beim nächsten Start, läuft QTmux korrekt und ist trotzdem **unsichtbar**. Ein paar Pixel
  Überlappung reichen als Kriterium NICHT (ein Fenster, das mit 5 px am Rand klebt, ist
  genauso unbedienbar) — daher die 120-px-Schwelle.
- **Beenden mit Rückfrage (QTMUX-41):** Dialog listet die offenen Sitzungen auf, bevor
  alles geschlossen wird; abschaltbar (`window/confirmQuit`, **Vorgabe an**; Einstellungen →
  **Allgemein**, Abschnitt „Fenster" — dazu Datei-Menü und Palette, QTMUX-46).
  🔑 Zentraler Wächter ist **`Window.onClosing`** (`close.accepted = false`), NICHT die
  Beenden-Aktion: Seit **Qt 6.5** läuft auch ein Anwendungs-Quit (natives macOS-App-Menü,
  Cmd+Q, `Qt.quit()`) über das Schließen aller Fenster und bricht ab, wenn ein Fenster
  ablehnt — dadurch greift dieselbe Rückfrage auch für Schließkreuz und Alt+F4.
  `quitConfirmed` schaltet die Frage für den bestätigten Durchlauf ab (sonst fragt der
  Wächter beim `close()` aus `onAccepted` erneut).
- **Einstellungsfenster (QTMUX-47):** Nicht-modales `qml/PrefsWindow.qml` (Rail + View) mit
  neun Kategorie-Seiten `qml/prefs/Cat*.qml` auf einem `CatPage`-Gerüst. 🔑 **Brücken-Muster:**
  ein eigenes `Window` sieht die IDs aus `Main.qml` NICHT → `app`/`sessions`/`mcp` und die noch
  modalen Editier-Dialoge werden als `property var` hineingereicht (`host.*`); globale
  Registries (Theme/App/ColorSchemes/Profiles/Hotkeys/Vault/AgentEvents/Plugins) sind
  Context-Properties und überall direkt. Kürzel-Aufnahme inline; `prefs.capturing` deaktiviert
  währenddessen ALLE App-Shortcuts. **Abo-Matrix**: Toggle-Kacheln (TapHandler), KEINE
  CheckBoxen — deren `checked`-Bindung bräche beim Klick und die Kreuzeffekte (leere Liste =
  „alle") ließen Stände veralten. **Suche**: `PrefAnchor` je Sektion (nicht je Grid-Zelle, das
  bräche die GridLayouts) + `host.pendingSetting` blendet ~1,2 s auf.
  🔑 **Fallen:** `MultiEffect` braucht `import QtQuick.Effects` **je Datei**; typografisches
  Schluss-Anführungszeichen in `qsTr` (gerades `"` bricht den String); verschachtelte
  Repeater-Delegates brauchen `pragma ComponentBehavior: Bound` + qualifizierte IDs.
  Headless-Verifikation: das Fenster referenziert alle 9 Cat-Typen → ein defekter Typ bricht
  den App-Start; Seiten einzeln über vorgeseedetes `ui.prefsCategory` instanziierbar.
  🔑 **Vollständigkeit prüfen** (Audit 2026-07-29, Stand: lückenlos): Quelle echter
  Nutzer-Einstellungen ist der `Settings`-Block **`window/*`** in
  [qml/Main.qml](qml/Main.qml) (plus `ui/language`, `ui/themeMode`, `mcp/port` aus C++).
  Seit 2026-07-30 gibt es dort einen **zweiten** Block **`ui/*`** — der hält bewusst nur
  **Ansichtszustand** (Seitenleiste ein-/ausgeklappt, ihre Breite, Statusleiste sichtbar)
  und gehört wie `windows/*` **nicht** in den Dialog. Wer auditiert, liest beide Blöcke und sortiert
  Zustand aus. Jeden Alias von dort gegen
  [qml/prefs/](qml/prefs/) greppen — was fehlt, ist entweder eine Lücke oder bewusst
  **Laufzeitzustand** (`newSessionType`, `collapsedGroups`; die gehören NICHT in den Dialog).
  Alles unter `windows/*` in [WindowModel.cpp](src/viewmodels/WindowModel.cpp) ist ebenfalls
  Zustand, keine Einstellung.
  🔑 **Zeilenformat seit Design 1a, Stufe 5:** eine Einstellung = eine `PrefRow` (Titel +
  Beschreibung links, Control rechts) in einer gerahmten `PrefGroup`; ≤ 3 Optionen als
  `SegmentedControl`, Booleans als `AppSwitch`. Alle vier liegen in [qml/Ui/](qml/Ui/) und
  sind in `CMakeLists.txt` (`QML_FILES`) eingetragen — **eine neue QML-Datei ohne diesen
  Eintrag existiert zur Laufzeit nicht**. Die freistehenden Erklärtexte unter den früheren
  CheckBoxen sind damit weg; **listenartige** Seiten (Hotkeys/Verbindungen/Vault/
  Erweiterungen) bleiben bewusst außen vor.
  🔑 **`SegmentedControl` schreibt `currentIndex` NIE selbst** — es meldet nur
  `activated(index)`, wie `AppComboBox.onActivated`. Ein internes Setzen zerrisse die Bindung
  der Aufrufstelle (`currentIndex: Theme.mode`) beim ersten Klick; danach zeigte der
  Umschalter seinen eigenen Zustand statt den der Einstellung — dieselbe Falle wie bei der
  Abo-Matrix.
  🔑 **`font.pixelSize` ist ein `int`.** Die Anweisung nennt 11,5 px; eine Gleitkommazahl
  scheitert erst zur **Laufzeit** („Invalid property assignment: int expected") und reißt dann
  den GANZEN App-Start mit, weil `PrefsWindow` alle neun Kategorien referenziert. Nach jeder
  QML-Änderung darum einmal starten (`QT_FORCE_STDERR_LOGGING=1`), nicht nur bauen — der Build
  ist dafür blind.
  🔑 **Schrift auf Akzentflächen: `Theme.accentText`** (neu) statt eines weißen Literals — sie
  entscheidet über die **Luminanz des Akzents**, weil ein Schema mit hellem ANSI-Blau sonst
  weiß auf hell zeichnete. Chrome-Farben bleiben damit vollständig schema-abgeleitet.
  🔑 **Rail-Badges dürfen den Kategorienamen nicht verdrängen:** „QTmux Dunkel" ließ in der
  236-px-Rail nur „Erscheinu…" übrig. Das Badge zeigt darum den Schemanamen **ohne den
  eigenen Präfix** („Dunkel") und ist zusätzlich auf 84 px gedeckelt.
- **Zurücksetzen / Export / Import (Design 1a, Stufe 6):** `SettingsIo`
  ([src/viewmodels/SettingsIo.h](src/viewmodels/SettingsIo.h), Context-Property `SettingsIo`)
  plus die zwei Textknöpfe in der Kopfzeile. Export als JSON mit Kopf
  (`format: "qtmux-settings"`), Import erst als **Vorschau** der zu ändernden Schlüssel.
  🔑 **Zentraler Entwurf: eine ALLOWLIST, und Export und Reset arbeiten auf derselben Menge**
  (`patternsFor(<kategorie>)`, Präfix wenn das Muster auf `/` endet). Die Anweisung sagt „alle
  Schlüssel der Domain außer dem Vault" — dagegen sprechen zwei Dinge: (1) eine Blocklist
  müsste bei **jedem** neuen Schlüssel gepflegt werden, und ein vergessener landet in einer
  Datei, die der Anwender weitergibt; (2) unter `windows/*` und `sessions/*` liegt das
  Fenster-/Pane-Layout samt Session-Liste — ein „alles zurücksetzen" darf die Arbeit des
  Anwenders nicht mitnehmen. Der Vault ist doppelt außen vor: er steht ohnehin nicht in
  QSettings, sondern verschlüsselt in `vault.json`. Wächter dagegen: `tst_settingsio`
  (8 Fälle; Gegentest mit `windows/` in der Allowlist → **4 Fehlschläge**, u. a.
  `resetAllKeepsWindowLayout`).
  🔑 **Ein QML-`Settings`-Alias liest seinen Schlüssel NUR beim Aufbau.** Entfernt oder
  überschreibt `SettingsIo` einen Schlüssel, zeigt die laufende App weiter den alten Wert.
  Deshalb `window.applySettingValue(key)` in [qml/Main.qml](qml/Main.qml), von
  `SettingsIo.changed` je Schlüssel gerufen; die **Standardwerte stehen dort, wo die Property
  deklariert ist** (nicht doppelt in C++). Drei Schlüssel kennen ihren Standard nur in C++
  (System-Sprache, System-Design, `QTMUX_MCP_PORT`) → `App.reloadLanguage()`, `Theme.reload()`,
  `mcp.reloadPort()`, alle drei **ohne** erneutes Persistieren (sonst schriebe ein Reset den
  Standard sofort wieder in die Einstellungen). `colorSchemes/*`, `hotkeys/*`, `profiles/*`
  halten C++-Registries → dort neues `reload()` (bei den Schemata mit vollständigem Neuaufbau,
  weil `loadPersisted` importierte Schemata nur **anhängt**).
  ⚠️ **Bewusst nicht bekämpft:** Setzt `applySettingValue` eine Property auf ihren Standard,
  merkt QML-`Settings` die Änderung und schreibt den Standardwert zurück — der Wert ist danach
  korrekt der Standard, der Schlüssel kann aber wieder auftauchen. Das zu verhindern hieße, die
  Settings-Bindung zu umgehen.
  🔑 `FileDialog.selectedFile` ist **kein Namensvorschlag**: ein nicht existierender Pfad
  erzeugt „Cannot set … because it doesn't exist". Stattdessen `defaultSuffix` + `currentFolder`.
- **Agent-Awareness:** OSC 133 (Prompt-Marker → Activity-Ring), OSC 9/777 (Notify),
  OSC 9;4 (Progress-Balken), Bell → Attention-Pulse (blau); MCP-Controller-Tab rot.
  🔑 **Reduzierte Bewegung (QTMUX-47):** `App.reduceMotion` (AppController, beim Start
  ermittelt — macOS CoreFoundation `com.apple.universalaccess/reduceMotion`, Windows
  `SPI_GETCLIENTAREAANIMATION`, sonst false; Env-Override `QTMUX_REDUCE_MOTION` für Tests)
  schaltet die drei Sidebar-Puls-Animationen ab (`running: … && !App.reduceMotion`) → Ring
  in Akzentfarbe und Rahmen statisch statt pulsierend.
- **Umfang der Wiederherstellung ist eine WAHL (QTMUX-99):** `window/restoreSessionMode` =
  `qtmux::RestoreMode` — 0 gar nicht · 1 ohne Verlauf · 2 alles (**Vorgabe**, bisheriges
  Verhalten). Regeln Gui-frei in [src/core/RestoreMode.h](src/core/RestoreMode.h), QML fragt
  ausschließlich über `windows.restoresLayout/restoresHistory/persistsOnQuit` — so wird ein
  defekter Wert an EINER Stelle normalisiert statt an dreien. Erreichbar in Einstellungen →
  **Allgemein** (Abschnitt „Fenster", direkt beim Beenden-Schalter — das eine steuert das
  Ende, das andere den nächsten Start), Datei-Menü, Palette, Suchindex.
  🔑 **Der Kern des Tickets ist NICHT das Nicht-Laden, sondern das Nicht-Speichern.** Bei
  Modus 0 kehrt `persistWindows()` sofort zurück: Sonst schriebe das erste Beenden die eine
  frisch geöffnete Session über den gesamten gespeicherten Stand — ein einmaliges Umstellen
  wäre unwiderruflich. Aus demselben Grund unterbleibt dort auch `pruneHistoryExcept`, sonst
  räumt es die `.ans`-Dumps des eingefrorenen Stands als „verwaist" weg. E2E-belegt: mit
  Wächter ist der gespeicherte Stand nach einem Modus-0-Durchlauf **bitidentisch**
  (gleicher shasum), ohne Wächter bleibt von zwei Windows **eines** übrig und die alten
  Arbeitsverzeichnisse sind weg.
  🔑 **Unbekannte Werte → `Full`, nie `None`** (`restoreModeFromInt`): `None` unterdrückt ja
  zusätzlich das Speichern; ein defekter oder aus einer neueren Version stammender Wert würde
  sonst still den letzten Stand einfrieren und sähe für den Anwender wie Totalverlust aus.
  Tests `tst_windowmodel::restoreModeGatesLayoutHistoryAndPersistence` und
  `unknownRestoreModeFallsBackToFull` (Gegenprobe mit umgedrehter Fallback-Richtung: FAIL).
  🔑 Modus 1 lässt nur `loadHistoryFor` weg — die Dumps bleiben liegen, ein späteres „Alles"
  findet sie wieder vor.
- **Ehrlichkeits-Marker in wiederhergestellten Sessions (`Session::markRestored`):** Nach
  einem Neustart sieht eine `Full`-Session **vollständig lebendig** aus — Titel, Verzeichnis,
  kompletter Verlauf — aber der Prozess dahinter ist **immer** tot (am Neustart gemessen,
  2026-08-13: „PROZESS_LEBT_PID …" stand im Verlauf, nichts lief; das Kind stirbt bei
  SIGTERM wie SIGKILL über den PTY-Master). Deshalb schreibt der Restore eine gedimmt-
  kursive Trennzeile „── QTmux: Sitzung wiederhergestellt …" in den Puffer — **nach** dem
  Schnappschuss (mit dessen mtime als Zeitstempel), im Modus „ohne Verlauf" als erste Zeile
  ohne Zeit; in **beiden** Modi, denn die Shell ist in beiden frisch. Läuft über denselben
  pendingHistory-Rückhalte-Mechanismus wie der Verlauf (QTMUX-130) und steht damit immer
  VOR der ersten frischen Backend-Ausgabe.
  ⚠️ **Der Filter ist die halbe Miete:** Der eingespielte Marker wird beim nächsten Beenden
  Teil des Schnappschusses — ohne `stripRestoreMarkers` stünde nach jedem Neustart ein
  Marker mehr da (beim E2E am zweiten Neustart real gemessen: 17:34 und 17:35 nebeneinander);
  ein Marker, der zur Tapete wird, warnt nicht mehr. Die Filter-Signatur „── QTmux: " ist
  sprachunabhängig (DE/EN identisch) und überlebt einen Sprachwechsel zwischen Neustarts.
  Tests: `tst_restorehistory` (`restoredMarkerAppearsAfterHistory`, `…WithoutHistory`,
  `freshSessionHasNoMarker`, `restoredMarkerDoesNotAccumulate`).
- **Ruhezustand verhindern (QTMUX-89):** `SleepInhibitor` ([src/core/SleepInhibitor.h](src/core/SleepInhibitor.h),
  plattform-gekapselt wie `GlobalHotkey`) — macOS `IOPMAssertionCreateWithName`
  (`PreventUserIdleSystemSleep`, IOKit-Framework), Windows `SetThreadExecutionState`
  (⚠️ **pro Thread**: Setzen und Aufheben aus demselben Thread), **Linux noch Stub**
  (login1/DBus wäre hier nicht lauffähig prüfbar — eine hängende Sperre ist schlimmer als
  keine). Nur **System**schlaf, nie das Display. Schalter `window/preventSleep`,
  **Vorgabe AUS** (Anwender-Vorgabe: ungefragt den Ruhezustand aushebeln ist ein Ärgernis);
  Einstellungen → Allgemein → „Energie" mit Live-Anzeige, ob gerade gesperrt ist, dazu Palette
  und Suchindex. Regel Gui-frei in `shouldPreventSleep` (Test `tst_session::sleepInhibitRule`).
  🔑 **`Activity` allein ist als Auslöser UNBRAUCHBAR** — der Startwert ist `Running`, damit
  der Sidebar-Ring sofort grün ist. Eine Shell **ohne** Shell-Integration meldet nie etwas und
  bliebe für immer „beschäftigt": Der Rechner schliefe nie wieder ein. Deshalb zählt nur, was
  eine Session **selbst gemeldet** hat (`Session::activityReported()`, gesetzt von OSC 133 und
  MCP `set_activity`) — ungemeldet heißt **unbekannt**, nicht „arbeitet". Dieselbe Linie wie
  QTMUX-30/37: QTmux leitet nichts ab. Empirisch aufgefallen: `pmset -g assertions` zeigte die
  Sperre direkt nach dem Start, ohne dass irgendwer arbeitete.
  🔑 **Die erste Meldung ist oft `busy` und ändert den Startwert `Running` gar nicht** →
  `setActivity` feuert kein `activityChanged`, und die Neuberechnung liefe nie an. Deshalb
  `markActivityReported()`, das beim Übergang „ungemeldet → gemeldet" **einmal**
  `activityChanged` auslöst. Symptom vorher: Die Sperre kam erst beim **zweiten** `busy`
  (nach einem Umweg über `waiting`) — sah aus wie ein Wettlauf, war aber ein fehlendes Signal.
  🔑 `Waiting` zählt bewusst **nicht** als Arbeiten: Da wartet der Agent auf einen Menschen,
  und dann darf der Rechner schlafen (Gegenprobe im Test: FAIL, wenn man es mitzählt).
  Abnahme mit `pmset -g assertions` über alle Zustände, inkl. Freigabe beim Beenden.
- **AgentRegistry: Aliase, Kommandonamen, Unterkommando-Vorlagen (QTMUX-88):** Die Liste der
  bekannten CLIs ist **einziger Pflegeort** (nirgends im QML oder README gedoppelt — geprüft).
  `AgentInfo` hat neben `command` jetzt `aliases`, und `AgentInfo::matches()` ist die EINE Stelle,
  die Namen vergleicht (`detect` ruft nur noch sie). Einträge stehen als **designierte
  Initialisierer** (C++20) da — ein neues Feld verschiebt damit keine Werte mehr lautlos.
  🔑 **Der Fehler, um den es ging:** eingetragen war `cursor` — das ist der **Editor**-Starter (wie
  `code` bei VS Code). Der Agent hieß `cursor-agent` und wird laut Dokumentation inzwischen als
  `agent` installiert; beide sind eingetragen, `cursor` **keiner** von beiden (Test hält das
  ausdrücklich fest). Wirkung vorher: `cursor .` machte eine Editor-Session zur „Cursor"-Agenten-
  Session, und der echte Agent wurde nie erkannt.
  ⚠️ Der Alias **`agent` ist generisch** — ein eigenes Skript dieses Namens erbt das Cursor-Etikett;
  bewusst in Kauf genommen, Begründung am Eintrag.
  🔑 **Codex fortsetzt über ein UNTERKOMMANDO** (`codex resume [--last] [SESSION_ID]`, am `--help`
  belegt 2026-07-31) — damit kann Codex als **zweiter** Agent alle drei Modi aus QTMUX-98, inkl.
  Picker. Dafür hat `resumeCommand` einen Wächter: Trägt die Zeile schon ein eigenes Unterkommando,
  darf eine Unterkommando-Vorlage nicht davor rutschen (aus `codex exec "…"` würde sonst
  `codex resume --last exec "…"`). Die Prüfung hängt an der **Form der Vorlage** (erstes Zeichen
  kein `-`), nicht am Agenten — gilt also für jeden künftigen Eintrag dieser Art.
  Jetzt **22 Einträge** (Vorlagen bei allen Nachträgen leer, weil die CLIs hier nicht installiert
  sind — ein ungeprüftes Flag sieht wie ein QTmux-Fehler aus).
  🔑 **Ein Paketname ist kein Kommandoname — die teuerste Lektion der zweiten Runde.** Aus der
  Ticket-Recherche kamen `gemini-cli`/`qwen-code`/`iflow-cli` (npm- bzw. Homebrew-**Paket**namen,
  als Kommando nirgends existent) und drei falsche Kommandonamen: `augment` → **`auggie`**,
  `kiro` → **`kiro-cli`** (Kiro ist die IDE — derselbe Fall wie cursor/cursor-agent),
  `kimi-code-cli` → **`kimi`**. **Prüfweg für jeden künftigen Eintrag:**
  `curl -s https://registry.npmjs.org/<paket>/latest | python3 -c "…d['bin']"` — er nennt das
  tatsächlich installierte Kommando und schlägt damit jede Doku-Seite.
  Wächter: `registryNamesAreUniqueAndDetectable` und `packageNamesAreNotCommandNames` (hält die
  Paketnamen und die bewussten Ausschlüsse `air`/`warp`/`cline` als **nicht** erkennbar fest).
  🔑 Bei einer Namenskorrektur ist die **Positivkontrolle Pflicht** — sonst „behebt" man den
  Fehler auch dadurch, dass gar nichts mehr erkannt wird (per Stub-Agent unter absolutem Pfad
  belegt: `kiro-cli` → „Kiro", `kiro`/`augment` → agentId leer).
- **Agenten überleben den Neustart (QTMUX-85):** Ein Agent läuft **nicht** als `program` —
  er wird in eine Shell **getippt** und in `Session::observeInput` über
  `AgentRegistry::detect` erkannt. Deshalb speichert die Session die erkannte Zeile in
  `m_agentCommand` (vor dem `m_inputLine.clear()`, dort ging sie bisher verloren);
  `sessionConfig()` legt sie als `agentCommand`/`agentId` ins Blatt-`cfg`. Beim Restore baut
  `_createSessionFromCfg` daraus ein **Login-Script** (QTMUX-23) — nicht `program`: Letzteres
  wird direkt exec't und bei argumentloser Angabe als Login-Shell markiert (`argv0 = "-claude"`),
  der Agent liefe ohne Shell-Umgebung und sein `exit` schlösse das Pane.
  Schalter `window/restoreAgents`, **Vorgabe AUS**.
- ⚠️ **ABGESCHALTET seit 2026-08-07 (Owner-Anweisung, Commit `6788a82`).** Das Fortsetzen
  arbeitete nicht zuverlässig und wird überarbeitet; **wiederhergestellt wird weiterhin**, nur
  eben mit frischer Unterhaltung im gespeicherten Arbeitsverzeichnis. Der folgende Absatz
  beschreibt die Mechanik, die **liegen bleibt** — Code, `AgentInfo`-Vorlagen und die sieben
  `agentLaunchCommand`-Tests sind unangetastet, damit die Überarbeitung nicht bei null beginnt.
  🔑 **Der Riegel sitzt an der WIRKUNG, nicht an der Einstellung:** `_createSessionFromCfg`
  (Main.qml) übergibt den Modus fest als `0`, statt `window.resumeAgentMode` zu lesen. Ein
  bereits gespeicherter Wert ≠ 0 wirkt damit nicht mehr — ein reines Umstellen der *Vorgabe*
  hätte genau die Anwender nicht erreicht, die die Funktion benutzt haben, also jene, bei
  denen sie versagte. Die vier Palette-Einträge entfallen, die Prefs-Zeile bleibt **sichtbar,
  aber ausgegraut** (verschwände sie, sähe es aus, als hätte QTmux die Fähigkeit nie gehabt).
  ⚠️ Nicht durch einen Test gedeckt ist der QML-Pfad selbst — `Main.qml` hat keinen Test
  (bekannte Architekturschuld); auf C++-Seite deckt `resumeModeNoneNeverTouches` den jetzt
  einzigen Pfad ab.
- **Unterhaltung fortsetzen ist eine WAHL, kein Schalter (QTMUX-98):** `window/resumeAgentMode`
  = `qtmux::ResumeMode` — 0 gar nicht (Vorgabe) · 1 **jüngste** im Verzeichnis · 2 **Auswahl**
  beim Start · 3 die vom Agenten **gemeldete** Sitzung. Je Modus eine Argument-Vorlage in
  `AgentInfo` (`resumeLastArgs`/`resumePickArgs`/`resumeIdArgs`, Letztere mit Platzhalter
  `{id}`); leer = der Agent kann das nicht → er startet frisch, es wird **nie** auf einen
  anderen Weg ausgewichen. Am `--help` verifiziert (2026-07-29): `--continue` für claude/agy/
  opencode/hermes, per ID `--resume {id}` (claude, hermes), `--conversation {id}` (agy),
  `--session {id}` (opencode); **einen Picker hat nur Claude Code** (`--resume` ohne Wert).
  🔑 **Warum eine Wahl:** `--continue` heißt wörtlich „jüngste Unterhaltung **im Verzeichnis**".
  Wer einen Agenten je Verzeichnis fährt, ist damit exakt bedient; wer mehrere im selben Ordner
  laufen lässt (hier der Normalfall — 5 Panes in RAFTNG, 3 in QTmux), bekäme in **allen**
  dieselbe. E2E-belegt: Modus 1 → beide Panes `--continue`; Modus 3 → `--session
  unterhaltung-EINS` bzw. `-ZWEI`.
  🔑 **Die ID kann QTmux nicht selbst ermitteln — vier Wege gemessen, alle tot:** MCP-Server
  ruft keinen Client (und ein beschäftigter Agent pollt nicht, QTMUX-37); in die PTY tippen
  landet im Eingabefeld der TUI und die Antwort wäre Scraping (gegen QTMUX-30); `lsof` findet
  nichts, weil Claude Code die `.jsonl` nicht offen hält; `ps eww` zeigt nur die **Start**-
  Umgebung, `CLAUDE_CODE_SESSION_ID` wird erst zur Laufzeit gesetzt (nur an Kindprozesse
  vererbt). Deshalb **meldet der Agent** per MCP `set_agent_session` — und muss das nach
  `/resume`/`/clear` **erneut** tun, weil sich die Kennung dabei ändert.
  🔑 **Drei Fallen, jede einzeln erlebt:** (1) Die Startzeile MUSS als `loginScript`-Argument
  von `create*Session` mitgehen — **vor** dem Start. Nachträglich gesetzt kann der Prompt
  schon durch sein, und `armLoginScript` wird erst beim **nächsten** Output scharf; eine
  wartende Shell liefert keinen mehr, der Agent startete nie. (2) `runLoginScript` schreibt
  **direkt ans Backend** und läuft an `observeInput` vorbei → Kennung und Titel muss
  `Session::setRestoredAgent` selbst setzen, sonst steht in der Sidebar weiter „zsh".
  (3) `sessionConfig()` braucht den **Rückfall** auf den vorgemerkten `cfg`-Wert
  (`seedAgentConfig`): Ein einziger Start mit **abgeschaltetem** Schalter schrieb sonst den
  leeren Laufzeitwert zurück und **löschte** den gespeicherten Befehl — ein späteres
  Einschalten fand nichts mehr vor. `resumeCommand` setzt die Argumente **hinter dem
  Kommando-Token** ein (nicht am Ende, sonst bräche eine Subkommando-Form) und ist
  idempotent, sonst sammelt sich `--continue --continue …` über Neustarts an.
  Tests: `tst_agent` (Einfügen/Idempotenz/unbekannt), `tst_session`
  (`agentCommandLineIsRemembered`, `restoredAgentSetsIdentityAndRunsCommand`).
- **Shell-Helfer stecken im Binary (QTMUX-38):** `src/core/ShellIntegration.{h,cpp}` (Gui-frei)
  plus `qt_add_resources(qtmux_core "shell_integration" …)`; `qtmux --install-shell-integration
  [ZIEL]` schreibt sie heraus, nennt den Pfad **und die fertige Hook-Zeile**. Standardziel
  `<GenericDataLocation>/QTmux/shell-integration`.
  🔑 **Warum nicht mitpaketieren** (der Grund, warum es jahrelang liegen blieb): Für DMG und MSI
  ginge das — das **AppImage hat aber gar keinen stabilen Pfad**, es wird bei jedem Start unter
  einem anderen `/tmp/.mount_XXXXXX` gemountet. Ein Hook-Eintrag in einer `settings.json` muss
  Neustarts überleben; Mitpaketieren löst den Linux-Fall also grundsätzlich nicht. Aus dem
  Programm geschrieben gilt **ein** Weg für alle drei Plattformen, und die Dateien passen
  zwangsläufig zur laufenden Version (keine Drift gegenüber einzeln von GitHub Gezogenem).
  🔑 **`GenericDataLocation`, NICHT `AppDataLocation`:** Letzteres trägt den `applicationName`,
  und der bekommt bei `--profile`/`QTMUX_PROFILE` ein Suffix — das Ziel wanderte je Instanz,
  obwohl ein Hook-Eintrag für alle Profile gilt. Test `defaultTargetIsProfileIndependent`.
  🔑 **Windows: GUI-App ohne stdout — und zwei Fallen, beide gemessen.** `qtmux` MUSS
  `WIN32_EXECUTABLE` sein (ConPTY, s. o.) und hat damit keine eigene Ausgabe.
  `runInstallShellIntegration` hängt sich per `AttachConsole(ATTACH_PARENT_PROCESS)` +
  `freopen_s("CONOUT$")` an — **aber nur, wenn `GetStdHandle(STD_OUTPUT_HANDLE)` kein gültiges
  Handle liefert.** Ohne diese Bedingung schreibt `freopen` an einer bestehenden Umleitung
  (`> datei`, PowerShell-Pipe) **vorbei** ins Konsolenfenster: erste Messung am portablen ZIP
  = Dateien korrekt geschrieben, Exit 0, Ausgabe spurlos weg — exakt das „wirkt kaputt", das
  das Ticket vorhergesagt hatte. `SetConsoleOutputCP(CP_UTF8)` nur, wenn es eine Konsole gibt
  (sonst Mojibake bei Umlauten).
  ⚠️ **Bleibt bewusst ungelöst:** Die Shell **wartet nicht** auf ein GUI-Programm. Der Prompt
  ist sofort zurück, die Ausgabe erscheint gleich danach (die Dateien werden vollständig
  geschrieben — nach 3 s gemessen: 11/11). PowerShells `>` schließt seine Zieldatei dabei zu
  früh und bleibt **0 Bytes**; `cmd /c "… > datei"` trägt (763 Bytes gemessen), weil cmd das
  Handle vererbt statt selbst zu lesen. Ein Konsolen-Subsystem ist keine Option, ein zweites
  `qtmux-cli.exe` wäre genau das Extra-Artefakt in allen Paketen, das dieses Ticket vermeiden
  wollte. Steht so in der Doku.
  🔑 Der Befehl läuft **vor** der `QGuiApplication` mit eigener `QCoreApplication` und beendet
  den Prozess selbst — sonst blitzt auf macOS ein Dock-Icon auf und Qt fährt eine GUI-Umgebung
  hoch, die niemand braucht. Eigene Argument-Schleife, weil das Ziel **optional** ist (die
  bestehende Schleife sieht nur `--x <wert>`-Paare).
  Tests: `tst_shellintegration` (8 Fälle) — darunter der Wächter, dass die Ressource aus der
  **statischen** `qtmux_core` überhaupt im Programm landet (ein Linker darf Objektdateien
  verwerfen, auf die niemand verweist), Ausführbar-Bit nur für `.sh`, Idempotenz, und dass eine
  veränderte Datei wieder auf die mitgelieferte Fassung zurückgesetzt wird.
  **Am Artefakt abgenommen, alle drei Pakete** (2026-07-31): DMG gemountet und die App daraus
  gestartet · portables ZIP auf rtzbld01 entpackt · AppImage aus dem CI-Lauf `30641780941` auf
  rtzsvr02 — je 11/11 Dateien. Dazu E2E: das **installierte** `qtmux-emit.sh` stellte einer
  isolierten Instanz ein Ereignis zu (`seq 1`, `sourceSessionId 2`).
  🔑 Das AppImage lässt sich auf rtzsvr02 **nur im Container** starten (`libOpenGL.so.0` fehlt
  auf dem aufgeräumten Host) — die Meldung sieht nach einem kaputten Paket aus, ist aber die
  Umgebung. Mit `sudo /opt/docker/buildenv/buildenv.sh` + `APPIMAGE_EXTRACT_AND_RUN=1` läuft es.
- **AgentEventHub** (Gui-frei, Ringpuffer 256, monotone `seq`): Inter-Agenten-Ereignisse
  `done|question|error|info` via OSC `777;qtmux-event` oder MCP `post_event`; Zustellung
  über MCP-Long-Poll `wait_for_events`. **⚠️ Hook-stdout wird vom Agenten gekapselt** —
  aus KI-Hooks immer `post_event` (HTTP) statt OSC nutzen.
- **Split-Layout je Window (QTMUX-83):** rekursiver JS-Baum **pro Window** — Blatt
  `{paneId, sessionId}` (stabile `Session::id()`, **kein** Row-Index mehr → kein Remap beim
  Umsortieren), Split `{orientation, children}`; QML-Rekursion via Loader —
  **`setSource(url,{props})` VOR dem Laden** (sonst evaluieren Bindungen mit
  `win===undefined` und brechen dauerhaft). `pruneLeaves(pred)` entfernt Blätter gelöschter
  Sessions (sonst teilen sich Panes eine Session und kämpfen um `resize()` → Verzerrung).
  Pane-Reorder: `DragHandler(target:null)` + manueller Szenen-Hit-Test (Qt-`Drag`/`DropArea`
  war fragil). Extern (MCP) erzeugte Sessions werden per `_wrapPending` in ein Window verpackt.
- 🔑 **Niemals ein ListView-Delegat als `DragHandler.target` (QTMUX-100).** Ein ListView
  vergibt die `y` seiner Delegates **selbst** und leitet daraus die Ausdehnung des Inhalts ab.
  Zieht man die **letzte** Kachel nach oben, schrumpft diese Ausdehnung, Flickable korrigiert
  `contentY` ins Negative — und schiebt damit **alle übrigen** Kacheln nach unten. Die
  Korrektur verschiebt die gezogene Kachel erneut gegenüber dem Zeiger → nächste Korrektur:
  eine **Rückkopplung**, die erst endet, wenn nichts mehr im Bild ist. Gemessen (3 Kacheln,
  ohne Gruppen): letzte Kachel `contentY` 0 → −6 → −52 → −100 → −163 → −213 …, erste und
  mittlere Kachel dagegen konstant 0. Daher auch der Anwender-Befund „nur ohne Gruppen": ein
  Section-Header hält die Ausdehnung unten fest. **Richtig ist `target: null` + rein optischer
  Versatz per `transform: Translate { y: … }`** aus `activeTranslation`; die Zielzeile beim
  Loslassen aus Layout-`y` **plus** Versatz. Gilt für Kachel- **und** Gruppen-Header-Drag.
  🔑 **Positivkontrolle ist hier Pflicht**, sonst „behebt" man den Fehler, indem man den Drag
  abschaltet: Der Versatz muss dem Zeiger 1:1 folgen (gemessen: Maus −372 px → `dy −372`) und
  das Loslassen muss umsortieren (`#1 #2 #3` → `#3 #1 #2`).
  Der Versatz ist zusätzlich auf den Inhaltsbereich geklemmt (QTMUX-102, `[-tile.y,
  contentHeight-tile.height-tile.y]`) — sonst zieht man die Kachel aus dem Bild und sieht
  nicht mehr, was man gerade bewegt.
- **ToolTips (QTMUX-101):** [qml/Ui/AppToolTip.qml](qml/Ui/AppToolTip.qml), Verzögerung 600 ms.
  Wie bei `ThemedMenu`/`AppPopupBg` gilt: Popups erben die Window-`palette` **nicht** → Farben
  explizit aus `Theme`, sonst dunkle Schrift auf dunklem Grund. In beiden Designs per
  `--screenshot` abgenommen. Die Sidebar-Kachel zeigt darin vollen Titel, `#Session-ID` und
  Arbeitsverzeichnis — die Kachel elidiert, und bei mehreren Agenten im selben Projekt sind
  die Titel vorne identisch.
  🔑 **`qsTr`-Plurale brauchen Handpflege:** `FinishSourceLanguageTs.cmake` füllt die
  `numerusform` der **Quellsprache** nicht automatisch — die deutschen Pluralformen (z. B.
  `%n Einträge`) sind inzwischen von Hand nachgetragen (beide `.ts` stehen auf
  0 unfinished). Bei neuen Plural-Strings also entweder die Zahl erst ab 2 anzeigen und
  eine feste Form nehmen, oder die deutschen Pluralformen direkt mitpflegen.
- **Einklappbare Seitenleiste + Statusleiste (Design 1a/2a, 2026-07-30):** Zustand in einem
  **zweiten** `Settings`-Block `ui/*` (`sidebarWidth`, `sidebarCollapsed`, `statusBarVisible`).
  Breite [180, 420]; **unter 140 px rastet sie ein** (52 px), Zwischenwerte werden nicht
  gehalten — die gespeicherte Breite bleibt immer im Bereich, sonst endet ein Aufklappen in
  einer unbrauchbar schmalen Liste. Der Splitter ist **neu** (die Leiste war ein festes
  240-px-`Rectangle`; die `SplitView`s gehören den Panes), `DragHandler` mit `target: null`
  wie bei QTMUX-100. Eingeklappt zeigt **dieselbe Kachel** einen zweiten Inhalt (Icon +
  Nummer + Statuspunkt) statt eines zweiten Delegates — sonst müsste die
  Rückkopplungsfalle aus QTMUX-100 zweimal richtig vermieden werden.
  🔑 **Der Chevron ist in BEIDEN Zuständen sichtbar** (2026-07-31): Er war eingeklappt
  ausgeblendet — Begründung damals „kein Platz, Splitter/Kürzel/Ansicht-Menü bleiben als
  Wege". Das trug nicht (Anwender-Befund): Ohne sichtbaren Knopf findet man den Rückweg
  nicht, Kürzel und Menü muss man erst kennen, und der Splitter ist eine unbeschriftete
  Kante. Eingeklappt weicht daher der **Schriftzug** („QTmux" → unsichtbar) und der Chevron
  rückt mittig; die Spitze zeigt, was der Klick tut (`rotation: 90` = links = einklappen,
  `-90` = rechts = ausklappen). Statusleiste als
  `footer` mit Inline-Komponente `StatusField`; die Aggregat-Zähler liegen in
  `SessionModel` (`waitingCount`/`errorCount` + `countersChanged`, Test
  `tst_sessiongroups::statusBarCounters`) — **nicht** in `WindowModel`, das kennt keine
  Sessions.
  🔑 **Rastergröße „80×24" (QTMUX-120) kommt aus der SESSION, nie aus dem `TerminalItem`.**
  `Session` hielt `m_cols`/`m_rows` längst; es fehlte nur die Veröffentlichung als
  `Q_PROPERTY cols/rows` (NOTIFY `sizeChanged`), die Leiste bindet über
  `window.windowGridText()`. Der Grund für die Quelle ist QTMUX-86: Ans Item gebunden zeigt
  die Anzeige die **transienten** Layout-Zwischengrößen (beim Window-Wechsel und beim Teilen
  gemessen 80×2), denn dorthin gelangt die Größe erst nach der 60-ms-Entprellung
  (`applyPendingResize`). Was in `Session` steht, ist genau das, was auch im PTY ankommt.
  Ohne Session blendet sich das Feld aus — eine Größe ohne Terminal wäre eine Erfindung.
  Test `tst_session::gridSizeIsPublishedAndSignalled` (Signal nur bei echter Änderung).
  🔑 **Kein Text ohne `elide` in eine Leiste (QTMUX-121).** Das `Text` im `StatusField` hatte
  weder `elide` noch Breitengrenze; die `Layout.maximumWidth` an der Aufrufstelle begrenzt nur
  das **Item**, der Text darin malte einfach weiter — bei einem langen Arbeitsverzeichnis über
  „x Sessions", Rastergröße und „UTF-8" hinweg. Mit `~` als CWD bleibt das unsichtbar, deshalb
  fiel es erst bei der Bildabnahme auf.
  ⚠️ **Der naheliegende Fix ist eine Bindungsschleife** (selbst hineingelaufen): `width` aus
  `sf.width` zu rechnen schließt einen Kreis, denn die Feldbreite kommt ja aus
  `sfRow.implicitWidth`, also aus der Textbreite. QML löst das mit **0** auf → die ganze
  Statusleiste war leer. Richtig ist eine **eigene** Property (`maxLabelWidth`, 0 = unbegrenzt),
  die nur das eine lange Feld setzt.
  🔑 **Drei QML-Fallen, jede einzeln erlebt:** (1) `Behavior on Layout.preferredWidth` ist
  **ungültig** — auf einer *attached property* erlaubt QML kein `Behavior`; es braucht eine
  eigene Property (hier `animWidth`). (2) Der **Inhalt eines `Popup` entsteht mit dem
  Delegate**, nicht beim Öffnen, und `Date.now()` ist nicht reaktiv → eine gerechnete Dauer
  steht sonst für immer auf „seit 0 s" (am Bild aufgefallen); Abhilfe ist ein `tick`-Anker,
  den ein Timer nur bei offenem Popup hochzählt. (3) Im eingeklappten Zustand muss der
  `AppToolTip` **abgeschaltet** werden, sonst stehen ToolTip und Flyout übereinander.
  🔑 **`Ctrl+B` gehört der Shell** (tmux-Präfix, readline `backward-char`): Der Umschalter
  liegt auf **macOS `Ctrl+B`** (= Cmd+B) und **Windows/Linux `Ctrl+Shift+L`** — dieselbe Linie
  wie `actFind`/`actClearScreen`. Und: `actVault` und `actClearScreen` lagen **beide** auf
  `Ctrl+Shift+K` (in Qt „ambiguous", also feuerte keiner zuverlässig) — „Bildschirm leeren"
  behält die Taste, der Vault hat keine Vorgabe mehr. Wächter dagegen:
  `tst_hotkeys::defaultsAreConflictFree`.
- **Verzeichnis auf der Kachel (2026-07-30):** Zweite, gedimmte Zeile unter dem Titel
  (`tile.dispDir` → `window.prettyDir`, `ElideLeft`, ausgeblendet wenn leer). 🔑 **Warum
  nötig:** Der Kacheltitel kommt **ausschließlich** aus dem OSC-0/2-Titel der Shell — Claude
  Code schreibt dort inzwischen das **Gesprächsthema** (`✳ …`), `cmd.exe` setzt **gar nichts**
  (bleibt ewig „Eingabeaufforderung"). Der Ort war damit nirgends direkt ablesbar; der ToolTip
  aus QTMUX-101 zeigt ihn nur beim Hover und bleibt der Fallback für den **vollen** Pfad.
  `prettyDir` kürzt Home zu `~` und **vergleicht** auf einer normalisierten Kopie
  (`QDir::homePath()` liefert `/`, die Shell unter Windows `\`), **zeigt** aber den
  Originalpfad — sonst stünde dort `C:/Windows/System32`. Der Home-Vergleich geht gegen
  Gleichheit bzw. `home + "/"`, sonst würde `/Users/nrx` als Home `/Users/nr` gelesen.
  ⚠️ Bei **PowerShell**-Sessions **ohne** gesourcte Shell-Integration steht hier dauerhaft das
  Startverzeichnis — nicht die Anzeige ist schuld, sondern `Set-Location` (Begründung im
  ConPTY-Abschnitt). Mit der Integration meldet die Shell ihr Verzeichnis per OSC 7 (QTMUX-108)
  und die Zeile folgt.
- **Git-Branch auf der Kachel (QTMUX-58):** `Session::refreshGitBranch()` führt ihn im
  1500-ms-Takt des `SessionModel` nach (Rollen `gitBranch`/`gitDetached`), die Kachel zeigt ihn
  **vor** dem Verzeichnis (`⎇ main …/projekt`; bei detached `➟ <shortSha>`), der ToolTip
  ungekürzt. 🔑 **Eigene Methode, nicht Teil von `refreshWorkingDirectory()`:** Letzteres
  steigt bei OSC-7-Sessions sofort aus (die Shell meldet ja selbst) — der Branch würde dort nie
  aktualisiert. Und `git checkout` wechselt den Branch, **ohne** dass sich das Verzeichnis
  ändert; die Prüfung darf also nicht am Verzeichnis-Vergleich hängen.
  🔑 **Bei einem Verzeichnis auf einem FREMDEN Rechner bleibt der Branch leer** (QTMUX-108):
  Existiert der gemeldete Pfad hier zufällig auch, läse man den Branch eines **anderen**
  Repositories und hängte ihn an diese Kachel. Test `gitBranchStaysEmptyForRemoteDirectory`
  (Gegentest ohne die Sperre: FAIL).
- **Prompt-Warteschlange (QTMUX-90):** `Session` hält eine `PromptQueue`, stempelt bei jeder
  Backend-Ausgabe eine `QElapsedTimer` (das ist `msSinceLastOutput`) und versucht die Abgabe
  über einen Timer, der **nur läuft, solange etwas ansteht**; abgegeben wird über
  `writeWithEnter` (QTMUX-31 — ein Eintrag landet typischerweise in genau der TUI, die einen
  Block mit `\r` als Einfügen wertet). Erreichbar über Palette („In die Warteschlange
  einreihen …"), `sessions.queueText()` und MCP **`queue_text`**; Zähler als Abzeichen auf der
  Kachel. Ein `static_assert` in [Session.cpp](src/core/Session.cpp) nagelt die
  Zahlen-Spiegelung `ActivityCode` ↔ `Session::Activity` fest — ein Verstoß bricht den
  **Build** (gegengetestet: `Waiting`/`Error` vertauscht → Compile-Fehler).
  ⚠️ **Bekannte Grenze, am laufenden Programm gemessen:** Meldet die Session ihren Zustand
  nicht, entscheidet die **Ruhe im Ausgabestrom** (500 ms) — ein *stiller* Langläufer wie
  `sleep 6` oder ein Build ohne Ausgabe gilt damit als „frei", und der Eintrag geht zu früh
  raus. Das ist bewusst so belassen: Der naheliegende Zusatz „hat die Shell einen
  Kindprozess?" (`ProcessInfo::descendantPids`) würde bei einem laufenden **Agenten-TUI** die
  Warteschlange dauerhaft blockieren — und Agenten sind der Hauptanwendungsfall. Für sie
  greift ohnehin der gemeldete Zweig (OSC 133 / `set_activity`), und sie geben während der
  Arbeit Ausgabe aus. In einer Shell ist der Schaden gering, weil sie die Eingabe puffert.
- **Arbeitsverzeichnis per OSC 7 (QTMUX-108):** `VtScreen` wertet `ESC ] 7 ; file://host/pfad`
  aus (`case 7` im OSC-Fallback — libvterm behandelt nur 0/1/2/52 selbst), dekodiert die
  Prozent-Kodierung und trennt den Host ab; `Session` nimmt die Meldung an, sie hat **Vorrang**
  vor dem gepollten `Pty::currentWorkingDirectory()`, und sobald gemeldet wird, **schweigt das
  Polling** (sonst überschriebe es die genauere Angabe im 1500-ms-Takt).
  🔑 **Ein Pfad von einem FREMDEN Host wird angezeigt, aber nicht als lokales CWD
  weitergereicht.** An `currentWorkingDirectory()` hängen Persistenz und die CWD-Vererbung an
  neue Shells — ein Verzeichnis der Gegenstelle existiert hier womöglich gar nicht. Genau daran
  hängt auch das Gate in `refreshWorkingDirectory()`: Beim entfernten Fall fällt
  `currentWorkingDirectory()` bewusst auf den gepollten Wert zurück, der nicht in die Anzeige
  gespeichert werden darf. **Nur dieser Fall prüft das Gate** — bei einer *lokalen* Meldung
  greift die Vorrangregel ohnehin, der erste Gegentest bestand deshalb fälschlich.
  🔑 **Der Fehler, den kein Unit-Test fand:** `printf '%d' "'$str"` liefert in bash für Bytes
  ≥ 0x80 eine **negative** Zahl (signed char) — das erste Skript schrieb
  `%FFFFFFFFFFFFFFC3` statt `%C3`, also war **jeder Umlaut im Pfad** kaputt. Sichtbar erst im
  realen Skriptlauf; Abhilfe `$((byte & 0xFF))` in `qtmux.bash`/`.zsh`.
  Tests `tst_vtscreen` (3 Fälle) + `tst_session::osc7*` (2, volle Kette PTY→libvterm→Session).
  **Offen:** `pwsh` (nur PS 5.1 belegt), Linux-Lauf, `cmd.exe` (hat keinen Prompt-Hook), und
  die GUI kennzeichnet einen **entfernten** Pfad nicht als solchen.
- **Projekt-Befehle in der Befehlspalette (QTMUX-96):** `ProjectCommands::scan(dir)` (Gui-frei)
  liest `.claude/commands`, `.claude/skills`, `.gemini/commands` (TOML), `.junie/commands`,
  `.agents/skills`; `filterForAgent()` blendet auf den erkannten Agenten ein — **kein** erkannter
  Agent heißt **alles zeigen** (dieselbe Linie wie QTMUX-30: nichts ableiten; der Anwender tippt
  den Agenten oft erst noch, und ein verborgener Befehl sieht aus wie ein fehlender). QML-Brücke
  `App.projectCommands(dir, agentId)`, Absenden über `sessions.sendText(row, text, submit)` →
  `Session::writeWithEnter`. 🔑 **Die Enter-Verzögerung ist hier kein Detail:** Ein Palette-Befehl
  landet typischerweise in einem Agenten-TUI, und dort wird ein `\r` im selben Block als
  Einfügen gewertet (QTMUX-31).
  🔑 **Bei Skills bestimmt das VERZEICHNIS den Befehl**, `name:` im Frontmatter ist nur
  Anzeige-Label (an den echten Bäumen unter `~/.hermes/skills`, `~/.cursor/skills-cursor`
  gegengeprüft). Und die Ticket-Annahme `/db:reset` für verschachtelte `.claude/commands` ist in
  der aktuellen Doku **nicht belegt** (dort nur „File name without extension"); dokumentiert ist
  die Namespace-Regel allein bei Gemini CLI. Verkettet wird trotzdem einheitlich mit `:` — eine
  begründete Entscheidung, keine Messung.
  🔑 **Die neue Funktion legte einen alten Layout-Fehler frei — Befund der Bildabnahme.** Im
  Palette-Delegate hat der **Titel** `Layout.fillWidth: true` (Minimum also 0), die
  Beschreibung (`sub`) hatte **weder `elide` noch Deckel** und bekam ihre volle
  `implicitWidth`: Sie quetschte den **Befehlsnamen auf null**, ausgerechnet bei den ausführlich
  beschriebenen Einträgen. Jahrelang unauffällig, weil `sub` bis dahin nur kurze Tastenkürzel
  trug — die Projekt-Befehle sind die ersten Einträge mit langen Beschreibungen. Fix:
  `elide` + `Layout.maximumWidth: cmdRow.width * 0.55` an der Beschreibung.
  **Merke:** Ein neuer Datentyp in einer bestehenden Liste ist ein Layout-Risiko; kein Test
  findet so etwas, nur der Blick aufs Bild.
- **Umbenennen erhält den Aktivitäts-Indikator (QTMUX-116):** `windowTitle(w)` gab bei gesetztem
  custom `w.name` bisher sofort den Namen zurück und verwarf damit den Session-Titel samt
  führendem Aktivitäts-Indikator (Agenten wie Claude Code setzen `✳` U+2733 im Ruhezustand, einen
  Braille-Spinner U+2800–28FF beim Arbeiten an den Anfang des OSC-Titels). Diese Zeichen kann der
  Anwender im Umbenennen-Dialog nicht tippen. `windowTitle` stellt den Indikator jetzt dem Namen
  **dynamisch** voran — aus dem aktuellen Session-Titel gelesen (`dispTitle` hängt an
  `sessionsRevision`), NICHT statisch in `w.name` gespeichert, damit er dem Wechsel `✳↔Spinner`
  folgt; Hilfsfunktion `activityIndicator()`, Doppelungs-Schutz, wenn der Name schon einen trägt.
  🔑 **Der MCP-`rename_window`-Parameter heißt `name`, nicht `title`** — ein Aufruf mit `title`
  setzt still den leeren Namen (also gar keinen) und sieht dann wie „Rename wirkt nicht" aus.
- **Arbeitsverzeichnis (QTMUX-103):** `windowWorkingDir(w)` liefert das CWD des **aktiven**
  Panes, leer bei seriellen/Plugin-Sessions — daran hängen „Arbeitsverzeichnis öffnen" und
  „Pfad kopieren" ihr `enabled`. Geöffnet wird über `App.openLocalPath` (C++,
  `QUrl::fromLocalFile` + `QDesktopServices`) statt per `"file://" + pfad` in QML: nur so
  werden Leerzeichen kodiert und aus `C:\Pfad` ein gültiges `file:///C:/Pfad`.
  ⚠️ `Session::workingDirectory()` ist ein **Cache**, den `SessionModel` alle **1500 ms**
  auffrischt — direkt nach dem Start ist er noch leer. Wer ihn in einem Test ausliest, misst
  sonst „" und hält die Funktion für kaputt (genau so passiert).
- 🔑 `TerminalItem::setSession` ruft `recomputeGrid` **bedingungslos** — die Regel „ohne
  belastbare Größe nichts ableiten" liegt seit QTMUX-86 in `gridFor()` (s. o.), also an
  EINER Stelle. Vorher stand sie nur in `setSession`, und `geometryChange` hatte sie nicht
  (der Kommentar in [TerminalItem.cpp](src/terminal/TerminalItem.cpp) direkt über dem
  `recomputeGrid()`-Aufruf in `setSession` hält das fest — Zeilennummern veralten, der
  Anker­satz nicht).
- **Backend-Ownership:** Backend gehört NUR dem `unique_ptr` (kein `setParent`);
  stateChanged-Handler nimmt den State aus dem **Signal-Argument** (feuert während der
  Backend-Zerstörung).

### Online-Update (QTMUX-125)

Kern **byte-identisch aus MacPCAN vendiert** (`third_party/updater/update/`, Namespace
`appupdate`, Target **`qtmux_updater`** = STATIC + Qt6::Core/Network). Bewusst **nicht** in
`qtmux_core` — der bleibt Qt6::Core-only. Abgleich `tools/check-updater-sync.sh`
(`--update` zieht nach), Herkunft/Pin in
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
QSettings `update/autoCheck|lastCheck|skippedVersion|baseUrl`) +
[`qml/dialogs/UpdateDialog.qml`](qml/dialogs/UpdateDialog.qml) + Hilfe-Menü + zwei
Palette-Einträge + Einstellungen → Allgemein → „Aktualisierung". Start-Hook in `main.cpp`
(3 s nach dem Start; im Screenshot-Modus nie). Basis-URL `https://nobser.de/updates`,
Produkt `qtmux` → `…/qtmux/manifest.json`.

**Owner-Regeln, die im Code hängen** (jede hat einen Grund, keine ist Geschmack):
- Start-Check **höchstens 1×/Tag, abschaltbar, Fehler bleiben STILL** — ein Rechner ohne
  Netz darf nicht jeden Morgen mit einem Fehlerdialog begrüßen.
- Der Zeitstempel wird **auch nach einem Fehlschlag** geschrieben; sonst wird aus
  „1×/Tag" bei unerreichbarem Server „bei jedem Start".
  📌 **Umzustellen: Zeitstempel VOR den Request** (Koordinator-Entscheid 2026-08-07, dem
  Owner ohne Veto vorgelegt — RAFTNG macht es so). Der Unterschied trifft genau einen Fall:
  eine Antwort, die *nie* kommt (hängender Server, Anwender beendet QTmux vor dem Timeout).
  Dann bleibt der Zeitstempel aus und der nächste Start fragt erneut. Bei einem
  *fehlschlagenden* Request verhalten sich beide Fassungen gleich — wir schreiben ihn auch
  nach Fehlschlag. **Kein eigener Release dafür**: geht als Paar mit der Key-Migration unten
  ins nächste ohnehin anstehende Paket.

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

📌 **Key-Angleichung `update/autoCheck` → `update/auto_check` — NUR mit Migration**
(Koordinator-Entscheid 2026-08-07). RAFTNG nutzt `update/auto_check`, MacPCAN verdrahtet
neu und übernimmt dieselbe Schreibweise; nach der Migration sind es drei von drei.
⚠️ **Ein Angleichen ohne Migration ist keine Kosmetik, sondern ein Übergriff:** Es setzt
jeden Anwender, der den Schalter bewusst ausgeschaltet hat, stillschweigend auf EIN zurück —
der alte Key wird nie mehr gelesen, und niemand merkt es. **Pflichtreihenfolge:** alten Key
lesen, Wert übernehmen, **erst dann** den neuen als führend behandeln.
Geht als Paar mit dem Zeitstempel oben ins nächste Paket — **kein eigener Release**.

🔑 **Zwei Messfallen, beide hier hineingelaufen** — wer den Start-Check nachmisst, verliert
sonst eine halbe Stunde an einem Messgerät, das schweigt:
1. **`--screenshot` löst den Start-Check absichtlich NIE aus** (`if (shotPath.isEmpty())` in
   `main.cpp`). Ein Beleglauf mit dem Screenshot-Schalter misst also garantiert nichts — und
   sieht dabei exakt so aus wie ein abgeschalteter Check.
2. **QSettings ersetzt `/` durch `.`, sobald der Wert in der macOS-plist landet.** Der Key
   heißt im Code `update/lastCheck`, in `defaults read` aber **`update.lastCheck`**. Ein
   `defaults write …  "update/autoCheck"` schreibt einen Schlüssel, den die App **nie liest**
   — der Gegentest lief damit gegen die Vorgabe statt gegen den ausgeschalteten Schalter und
   „bestätigte" fälschlich. Erkennbar war es nur daran, dass Test und Gegentest **dasselbe**
   Ergebnis lieferten.

**Beleg „Default EIN" (2026-08-07, mit scharfem Gegentest):**
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
   Arbeitsstand) · AppImage aus dem **CI-Lauf desselben Commits**
   (`gh run download <id> -n QTmux-AppImage`).
4. **Am Artefakt gegenprüfen, nicht am Build-Log:** je Binary Treffer auf die neue
   Nummer **und 0 Reste der alten**. DMG mounten, ZIP entpacken, AppImage mit
   `--appimage-extract` auspacken (squashfs — ein `grep` aufs AppImage selbst findet nie
   etwas und sähe wie ein Fehler aus).
5. Tag + `gh release create` (voller SHA!), dann
   `UPDATES_SFTP_HOST=… python3 ../MacPCAN/tools/updates/publish.py --product qtmux
   --version <ver> --notes-de … --notes-en … --artifact <key>=<datei>,kind=<art> …
   --upload --verify`. Schlüssel und Zielpfad kennt `publish.py` als Vorgabe
   (`~/.ssh/updates_publish_ed25519`, `public_html/updates`).
6. Gegenprobe: `curl` + `openssl pkeyutl -verify` auf die Live-Bytes und ein manueller
   Check aus der App.

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

### QML-/Theming-Lektionen
- ⚠️ **`Array.isArray(control.model)` ist im Delegate IMMER `false`** — auch wenn das Model
  ein JS-Array ist (2026-08-07 gemessen, Anwenderbefund). Die `model`-Property reicht den
  Wert als **QVariant** durch; beim Auslesen ist er kein JS-Array mehr. In
  [AppComboBox.qml](qml/Ui/AppComboBox.qml) verzweigte der Popup-Delegate darüber und lief
  deshalb **immer** in den `model[textRole]`-Zweig, den es bei einem Array-Model gar nicht
  gibt → `undefined` → **leere Einträge in jeder ComboBox mit `textRole`** (Sprach- und
  Shell-Auswahl). Tückisch: Das **geschlossene Feld blieb korrekt**, weil `currentText`
  einen eigenen Weg nimmt — der Fehler sah nach einem Farb-/Theme-Problem aus.
  🔑 **Regel: `modelData` FRAGEN, nicht den Modelltyp raten.** Es ist bei Array-Models
  gesetzt und bei rollenbasierten Models `undefined`:
  `modelData?.[textRole] ?? model?.[textRole] ?? ""`.
- 🔑 **Ein Sprachwechsel baut die MenuBar NICHT neu auf** (2026-08-07 mit Erzeugungs-/
  Zerstörungs-Hooks gemessen): Es entsteht kein neues `MenuBar`- oder `Menu`-Objekt und
  keines wird zerstört — `applyLanguage()` tauscht die Translator und ruft
  `QQmlApplicationEngine::retranslate()`, das die `qsTr`-Bindings der **bestehenden** Objekte
  neu auswertet. Auch die ins macOS-Programm-Menü promoteten Einträge (Über/Einstellungen/
  Beenden) folgen dabei sofort — die Prefs-Beschreibung behauptete bis dahin das Gegenteil.
  ⚠️ Der **Auslöser** ist Qt-Quick-spezifisch: Bei Widgets gibt es kein `retranslate()`, dort
  müssten alle Texte in `changeEvent(QEvent::LanguageChange)` selbst neu gesetzt werden.
  Ob eine solche Textänderung auch ein bereits promotetes `NSMenuItem` erreicht, ist **nicht
  gemessen** (dafür fehlt hier das Messgerät) — nicht zusagen.
- **Editor-Diagnosen (VSCode/qmlls), 2026-08-04:** Ausgangslage waren **über 2000**
  „Probleme" in den QML-Dateien, geblieben sind **2** (beide echt, s. u.). Drei Ursachen,
  jede mit eigener Abhilfe — die Begründungen stehen in den Dateien selbst
  ([.qmllint.ini](.qmllint.ini), [.vscode/settings.json](.vscode/settings.json)), hier nur
  das, was man ohne Messung nicht wiederfindet:
  1. **Ohne Importpfade kennt qmlls weder `QtQuick` noch das eigene Modul `QTmux`** — jede
     Typreferenz wird zur Fehlermeldung (1092 über 28 Dateien, davon 372 echt). Abhilfe ist
     Qts Schalter `QT_QML_GENERATE_QMLLS_INI`; die erzeugte `.qmlls.ini` enthält absolute
     Pfade und ist **git-ignoriert**. ⚠️ Sie entsteht **erst beim Build** (eigenes Target
     `qtmux_generate_qmlls_ini_file`, weil der Schreiber `$<TARGET_FILE:qtpaths>` braucht) —
     nach einem reinen `--preset`-Lauf fehlt sie und man sucht am falschen Ende. Dazu
     `…_NO_CMAKE_CALLS`: qmlls würde sonst **selbst CMake rufen** — ohne die VS-2022-Umgebung
     (QTMUX-79) und in das Verzeichnis, aus dem die produktive Instanz läuft (LNK1168).
  2. 🔑 **`qmllint` von der Kommandozeile ist NICHT das Messmittel.** Die Erweiterung
     `theqtcompany.qt-qml` lädt sich eine **eigene, neuere** `qmlls.exe` nach
     `%APPDATA%\Code\User\globalStorage\theqtcompany.qt-qml\qmlls\files\` und meldet damit
     Kategorien, die das Qt des Projekts gar nicht kennt (`id-shadows-member`,
     `confusing-expression-statement`, var-Hoisting). Wer nur die CLI prüft, hält die Arbeit
     für fertig, während die IDE weiter rot ist. Gegenprobe **gegen jene Datei**, per
     LSP-Handshake skriptbar (`initialize` → `didOpen` → `publishDiagnostics` zählen).
     ⚠️ Die erste Analyse braucht **30–45 s**; nach 18 s kamen 0 Diagnosen — das sah wie
     „alles behoben" aus.
  3. **`build/` muss aus dem Arbeitsbereich heraus** (`files.exclude`/`search.exclude`):
     dort liegen **1501** `.qml` — je Build-Verzeichnis eine Kopie unserer 28 **plus** Qts
     komplette `QtQuick.Controls.Basic`-Quellen, die `windeployqt` neben die EXE legt.
  🔑 **Nicht jede Meldung ist über eine Kategorie abschaltbar.** Die var-Hoisting-Warnung trägt
  keinen `[kategorie]`-Suffix und ließ sich weder über `VarUsedBeforeDeclaration` noch über
  `CompilerWarnings` stummschalten (beides gemessen) — sie ist deshalb **im Code** behoben
  (17× `var` → `let`). Kontrolle, dass keine Variable ihren Block verlässt: die Zahl der
  unqualifizierten Zugriffe in `Main.qml` ist vorher und nachher **0**.
  **Abgeschaltet** sind nur Kategorien, die hier Idiome treffen (Begründung je Eintrag in der
  Datei): `UnqualifiedAccess` — die Gui-freien Registries sind **Context-Properties**
  (s. Konventionen), ein Werkzeug kann sie prinzipiell nicht kennen —, `Comma` und
  `ConfusingExpressionStatement` (Revisions-Anker `(window.xRevision, …)`) sowie
  `IdShadowsMember`. Wer den Abbau von `Main.qml` angeht, schaltet `UnqualifiedAccess` für
  diese Arbeit wieder ein. **Die 2 verbliebenen sind echt** und bleiben sichtbar:
  `Member "title"/"needsAttention" not found on type "QObject"` in
  [SplitNode.qml](qml/SplitNode.qml) — `TerminalItem::session` ist `QObject*` und `Session`
  bewusst **kein** registrierter QML-Typ; ein Fix wäre ein Signaturwechsel, keine
  Konfigurationszeile.
- Popups/Menüs erben die Window-`palette` NICHT → `ThemedMenu`/`AppPopupBg` mit eigener
  Palette; Menübreite explizit setzen (`window.sizeMenu` → `contentWidth`); Basic-Style-
  Highlight im Hell-Modus braucht eigenen Hintergrund.
- Modale Dialoge: **Enter=OK** braucht In-Dialog-`Shortcut` (fensterweite feuern über
  Modals nicht) UND `TextField.onAccepted` (fokussierte Felder kapern Return via
  ShortcutOverride); Qt-Quick-`Button` im Fokus reagiert nur auf Leertaste. ESC via
  `closePolicy`.
- `header: ToolBar` braucht feste `height` (sonst Kollaps auf 0); verschachtelte
  `RowLayout`-`fillHeight`-Kinder brauchen `maximumHeight`/`fillHeight:false`.
- Icon-Tinting in Delegates: explizite `MultiEffect`-Form (`layer.effect` greift dort
  nicht zuverlässig). Icons: Phosphor-SVGs `qrc:/icons/`, via `icon.source`+`icon.color`.
  🔑 **`brightness: 1.0` VOR `colorization: 1.0` ist Pflicht** — colorize wichtet mit der
  Quell-Luminanz, und die Phosphor-SVGs sind schwarz (≈ 0), also bleibt das Icon sonst dunkel.
  Genau daran hing der Sidebar-Chevron im Dunkel-Design (er benutzte `layer.effect` **ohne**
  brightness). Und: **positive `rotation` dreht im Uhrzeigersinn** (y zeigt nach unten) — aus
  `caret-down` wird „links" bei **+90**, nicht bei −90.
- **Menüs (Design 1a, Stufe 4):** ein Menü enthält **Befehle, keine Zustände**; checkbar sind
  nur die drei Ansichtsumschalter (Seitenleiste, Statusleiste, Broadcast). Was ein Menü
  verlässt, muss in **Einstellungen UND Palette** landen (QTMUX-46) — die Palette bekommt
  dafür je Einstellungs-Kategorie und je Shell einen Eintrag.
  🔑 **`visible: false` an einem `Menu` blendet dessen MenuBar-Eintrag NICHT aus** (am Bild
  belegt: „Fenster" stand auf Windows trotzdem da). Nur ein nicht enthaltenes Menü
  verschwindet → `appMenuBar.removeMenu(macWindowMenu)` in `Component.onCompleted`.
  🔑 **QtQuick.Controls kennt keine Menü-Rollen.** `MenuItem.PreferencesRole` gibt es nur im
  veralteten `Qt.labs.platform`; `QQuickNativeMenuItem` ruft `setRole` nie auf (im Qt-Header
  geprüft). Auf macOS wandert „Einstellungen …" also **nicht** ins App-Menü — Ausblenden im
  Datei-Menü würde es dort schlicht entfernen. Deshalb überall sichtbar.
  🔑 **Ein deaktivierter Menüeintrag sah aus wie ein aktiver** (aufgefallen an „Diese Seite
  zurücksetzen", Stufe 6, gilt aber für JEDEN `enabled: false`-Eintrag der App): das
  contentItem des Basic-Styles färbt den Text **unbedingt** mit `palette.windowText`
  (Qt-Quelltext `Basic/MenuItem.qml`, Z. 48/59) — es gibt keinen Disabled-Zustand zu themen,
  und `palette.disabled.*` an `ThemedMenu` bleibt darum wirkungslos (ausprobiert, am Bild
  widerlegt, wieder entfernt). Lösung: `opacity: enabled ? 1.0 : 0.45` in
  [qml/Ui/ShortcutMenuItem.qml](qml/Ui/ShortcutMenuItem.qml) — dimmt die ganze Zeile inklusive
  Kürzel-Label. A/B am Screenshot belegt.
  🔑 **Standardknöpfe brauchen einen ZWEITEN Translator (QTMUX-117, behoben 2026-07-31).**
  Die Beschriftungen von `standardButtons` kommen nicht aus unseren `.ts`, sondern aus Qts
  eigener Übersetzung im Kontext **`QPlatformTheme`** — vorher hieß der Knopf jedes
  `AppDialog` auch auf Deutsch „Cancel" (per UIA belegt: `Buttons: … | OK | Cancel`).
  `applyLanguage` in [main.cpp](src/app/main.cpp) tauscht deshalb über `swapTranslator`
  **zwei** Translator: `qtmux_<lang>` und `qtbase_<lang>`.
  🔑 **Die `.qm` wird EINGEBETTET, nicht mitgeliefert** (CMakeLists, `qt_add_resources` mit
  `BASE ${QT_TRANSLATIONS_DIR}` aus `qmake -query`). Der naheliegende Weg — jedes der drei
  Deployment-Werkzeuge die Datei kopieren lassen — wären drei Konfigurationen für dieselbe
  Datei (macdeployqt kopiert `translations` **nicht**, windeployqt schon, linuxdeploy je nach
  Plugin), und der Fehler träfe **nur die gepackte App**, nie den Entwickler-Build. Als
  Ressource liegt sie überall unter `:/i18n/`. Fehlt sie in einer Qt-Installation, gibt es
  eine CMake-**Warnung** und `test_i18n` wird gar nicht erst angelegt — ein roter Test wäre
  hier ein Umgebungsproblem, keine Regression. Gegengeprüft: die aqt-Installationen auf
  rtzbld01 (Windows) und im `qtcache` (Linux) führen `qtbase_de.qm` beide.
  🔑 Messfalle: In einer `.qm` stehen die Texte **UTF-16BE**, ein ASCII-`grep -a` auf das
  Binary findet sie also prinzipiell nicht. Ressourcen*namen* legt Qt dagegen unkomprimiert
  als UTF-16BE ab — `"qtbase_de".encode("utf-16-be")` im Binary zählen ist damit der
  belastbare A/B-Beleg für die Einbettung (vorher 0, nachher 1).
- App-Icon: `resources/appicon/` (SVG → icns/ico/png via `generate.sh` + Qt-`svgrender`-
  Mini-Tool, da kein rsvg/inkscape auf den Maschinen).

### macOS-Spezifika
- **Sprache:** Translator + `singletonInstance(App)` VOR `loadFromModule` installieren;
  native App-Menü-Standarditems folgen **AppleLanguages** → in `main.cpp` vor
  `QGuiApplication` per `CFPreferencesSetAppValue` aus `ui/language` setzen (argv-Injektion
  wirkt NICHT); Laufzeit-Wechsel greift fürs native App-Menü erst nach Neustart.
- Native Menüs rendern keine QML-Icons (QTMUX-13, deferred).
- Quake-Modus: Carbon `RegisterEventHotKey` (Ctrl+`), ohne Bedienungshilfen-Rechte;
  Windows/Linux Stub. Session-Nav macOS: `Meta+Tab` (Ctrl+Tab = Cmd+Tab gehört dem OS).
- Einstellungen-Shortcut bewusst String „Ctrl+," statt `StandardKey.Preferences`
  (macOS verschöbe ihn ins App-Menü).

### Verbindungen, Vault, Profile
- **SSH/SFTP über System-Clients im PTY** (Auth/known_hosts „funktionieren einfach";
  SFTP: interaktives `sftp` bis zum `sftp> `-Prompt getrieben, TOFU `accept-new`).
- **SecretsVault:** Pure-Qt-Krypto (PBKDF2-HMAC-SHA512, 210k Iterationen; HMAC-SHA256-
  CTR-Keystream, Encrypt-then-MAC) — bewusste dependency-free-Abwägung, kein AES.
  **Vault-Verwaltung ist NIE über MCP exponiert** (Sicherheitsgrenze); Profile speichern
  nur den **Geheimnis-Namen** (`passwordSecret`), Auflösung intern.
- SSH-Passwort-Auto-Fill: Prompt-Scan auf `password:`, **genau einmal** senden (kein
  Lockout); Login-Scripts einmal am ersten OSC-133-Prompt bzw. Fallback-Timer 800 ms —
  beides NICHT beim Restore.
- Profile: `ConnectionProfileRegistry` (QSettings, Upsert über Name); Registry kennt
  keine Sessions — Starten macht QML (`window.connectProfile`).
- Hotkeys: `HotkeyRegistry` (Gui-frei, nur Overrides persistiert, Multi-Chord);
  während des Aufnahme-Dialogs alle App-Shortcuts deaktivieren.
- Color-Schemes: je ein Schema für Hell und Dunkel; `Theme` leitet ALLE Chrome-Farben
  aus dem aktiven Schema ab; Import iTerm/Xresources/Ghostty.

### Plugin-System (QTMUX-8/9)
- SDK `QTmuxPlugin.h` (IID `com.qtmux.PluginInterface/1.0` — bei inkompatiblen Änderungen
  hochzählen); Plugin linkt `qtmux_core` statisch. `PluginHost`-Suchpfade:
  `QTMUX_PLUGIN_DIR` → `<App>/plugins` → macOS `Contents/PlugIns` → `<AppData>/plugins`.
  Restore überspringt fehlende Plugins still. `qt_add_plugin` ohne Namespace-`CLASS_NAME`.
- **MacPCAN** (`plugins/macpcan/`, nur APPLE): CAN-Bus als Terminal-Backend, zwei Typen
  (`pcan` = Hardware, `pcan-mock` = Demo, bewusst ohne stillen Fallback). Aufbau, PCBUSB-
  Einbindung über `QTMUX_PCBUSB_DIR`, Terminal-UX, Fallen und v1-Backlog stehen in
  [plugins/macpcan/README.md](plugins/macpcan/README.md) — dort pflegen, nicht hier.

### Shells (Windows)
- `ShellRegistry`: cmd/PowerShell/pwsh + **„Eingabeaufforderung (Clink)"** wenn Clink
  installiert (GPL — bewusst nicht gebündelt, nur erkannt; `program` = komplette
  Kommandozeile, `PtyBackend` zerlegt via `splitCommand`). AutoRun-Dedup: ist Clink per
  cmd-AutoRun aktiv, wird der redundante Eintrag ausgeblendet.

### MCP-Server (40 Tools)
`src/server/McpServer.{h,cpp}`, HTTP/JSON-RPC, **Vorgabe** `127.0.0.1:7345`; Tool-Referenz in
`docs/MCP.md`. Kernpunkte:
- **Netzzugang ist eine WAHL, und sie kostet ein Token (QTMUX-127).** Bind-Adresse:
  `QTMUX_MCP_BIND` > Einstellung `mcp/bindAddress` > `127.0.0.1`; Regeln Gui-frei in
  [src/server/McpAccess.h](src/server/McpAccess.h) (Test `test_mcpaccess`, 13 Fälle),
  bedient über Einstellungen → Agenten & MCP (Schalter „Im Netzwerk erreichbar",
  Adressfeld, Token anzeigen/kopieren/neu erzeugen) und die Palette.
  🔑 **Ungültige Adresse fällt auf Loopback zurück, nie auf `Any`** — ein Tippfehler in
  der Einstellung darf den Server nicht ins Netz stellen; kein DNS (blockierte den Start
  und ein Name kann auf eine fremde Adresse zeigen). Gegentest: mit `Any` als Fallback
  fällt `invalidAddressFallsBackToLoopbackWithReason`.
  🔑 **Nicht-Loopback ⇒ Token-Pflicht für ALLE Anfragen**, auch die lokalen: `send_text`
  ist Befehlsausführung unter unserer UID. Ohne Token **startet der Server nicht**
  (`mcp.lastError` + qWarning) statt „unsicher, aber es läuft". Loopback bleibt
  tokenfrei — bestehende lokale Clients laufen unverändert.
  🔑 **Auto-Erzeugung nur, wenn die Öffnung aus der EINSTELLUNG kommt**: dann gibt es
  eine Oberfläche, die das Token anzeigt. Kommt sie aus `QTMUX_MCP_BIND` (Skript, CI),
  bekäme es niemand zu sehen → Startverweigerung ist die ehrlichere Antwort.
  🔑 Vergleich zeitkonstant, leeres erwartetes Token passt **nie**; 401 antwortet **vor**
  dem Ansehen des Rumpfes, dazu ein Deckel von 4 MiB je Request (vorher wuchs der Puffer
  unbegrenzt). `mcp/token` steht bewusst **nicht** in der Export-Allowlist (`SettingsIo`),
  `mcp/bindAddress` schon. Zweite Schicht auf Netzebene: [tools/pf/](tools/pf/) (macOS).
- **Controller-Auto-Erkennung** beim `initialize`: TCP-Port → PID → **Prozess-Vorfahren-
  kette** bis zur Session-Shell-PID (macOS gibt Environments fremder Prozesse nicht mehr
  heraus — daher Hierarchie statt `QTMUX_SESSION_ID`-Lesen); Fallback `attach_controller`.
  🔑 **Nur bei Loopback-Peer** (QTMUX-127): Die Heuristik sucht einen Prozess auf DIESER
  Maschine; bei einer Verbindung aus dem Netz gibt es ihn nicht, und ein lokaler Prozess
  mit zufällig gleichem Quellport würde fälschlich zur Controller-Session erklärt. Aus
  dem Netz gilt darum „unbekannt" (-1) → solche Clients müssen `sessionId` mitgeben.
  Positivkontrolle beim Prüfen ist Pflicht (sonst „behebt" man es durch Abschalten):
  LAN-Aufruf → `mcpController:false`, Aufruf **aus** einer Session → `true`.
- **Die Zugriffseinstellungen sind über MCP bewusst NICHT änderbar** — nur lesbar über
  `get_server_info` (ohne Token-Wert). Ein Fernsteuerungs-Endpunkt, der seine eigene
  Zugriffskontrolle umkonfigurieren kann, hat keine; dieselbe Linie wie beim Vault.
- **Long-Poll `wait_for_events`**: vor `callTool` abgezweigt, Socket bleibt offen
  (`PendingPoll` + QTimer, Default 25 s); `disconnected`-Handler räumt Polls ab.
- **Layout/Profile-Tools (QTMUX-29):** Layout und Windows leben in QML → diese Tools laufen
  über `*Requested`-Signale, deren QML-Handler **synchron** (Direct-Connection) läuft und über
  die **`provideResult`-Brücke** (`bridgedCall`) antwortet; ohne UI → „UI nicht verbunden".
  `list_profiles` liefert nur Flags, `connect_profile` löst Vault-Passwörter **intern**.
- **QTMUX-31 (`send_text`):** Das Enter geht **zeitlich abgesetzt** raus
  (`Session::writeWithEnter`, Tool-Parameter `enterDelayMs`). TUI-Apps
  (belegt mit Claude Code) werten einen in EINEM Rutsch ankommenden Block als
  Einfügevorgang → das `\r` wurde zum Zeilenumbruch im Eingabefeld statt zum Absenden,
  und der Aufruf meldete trotzdem `ok`. Regressionstest bricht bei `enterDelayMs: 0`.
- **Paste-Rahmung für Text-Nutzlasten (`Session::writePasted`):** Lange/mehrzeilige
  `send_text`-Nutzlasten wurden von der Ziel-TUI gestückelt gelesen und
  **teil-abgeschickt** (zeitbasierte Einfüge-Heuristik las ein `\n` an einer
  Chunk-Grenze als Tastendruck; QTmux selbst liefert FIFO-sauber — beide PTY-Wege
  gemessen). Fix: `send_text` (beide Zweige), `SessionModel::sendText` und die
  Warteschlangen-Zustellung rahmen über `startPaste()`/`endPaste()` — libvterm
  sendet die Marker **nur bei aktivem DECSET 2004**, derselbe Weg wie der
  GUI-Paste (`doPaste` nutzt seither ebenfalls `writePasted`). 🔑 Zwei Regeln:
  ein eingebettetes `ESC[201~` wird **entfernt** (Rahmen-Ausbruch — sonst liefe
  der Rest wieder als Tastendrücke; Test `tst_pastewrite` mit Mutationsprobe),
  und der `enterDelayMs`-**Default** skaliert mit der Größe
  (`Session::pasteEnterDelayMs`, 60 ms + 1 ms je 8 Byte, Deckel 2000 ms) als Netz
  für Ziele ohne Modus 2004 — explizite Werte gewinnen unverändert.
  ⚠️ `send_keys` bleibt bewusst **ungerahmt**: Tastensequenzen in Paste-Markern
  wären Inhalt statt Tastendrücke.
- **`send_keys` (benannte Tasten im tmux-Stil):** `send_text` transportiert nur Text —
  rohe Steuerbytes überleben den JSON-/MCP-Transport nicht (kamen als leerer String an),
  und Escape-Formen landeten als Literaltext; damit ließ sich ein TUI-Eingabefeld weder
  leeren (`C-u`) noch steuern (Esc, `C-c`, Pfeile). `send_keys` nimmt eine **Liste**
  benannter Ausdrücke (`["C-u","neuer Text","Enter"]`): Chords (`C-`/`M-`/`S-`, auch
  kombiniert), Sondertasten (Enter, Escape, Tab, BTab, Backspace, Space, Pfeile,
  Home/End, PageUp/PageDown, Delete, Insert, F1–F12; Modifier xterm-konform als
  CSI-Form, `C-Right` → `ESC[1;5C`), alles Unerkannte ist Literaltext (tmux-Verhalten).
  Übersetzung Gui-frei in [KeyEncoding](../src/core/KeyEncoding.cpp)
  (`encodeNamedKey`, Tests in `tst_keyencoding`). 🔑 Drei bewusste Regeln: **erst alles
  übersetzen, dann senden** (ein Tippfehler-Chord bricht ab, statt dass die TUI halbe
  Eingaben sieht — als Literal gesendet stünde `C-uu` wortwörtlich im Feld); ein
  **abschließendes Enter** geht wie bei `send_text` zeitlich abgesetzt raus (QTMUX-31,
  `enterDelayMs`); `S-`/`M-Enter` folgen QTMUX-43 (ESC CR = Umbruch einfügen).
- **QTMUX-30/37 (Ereignis-Kanal — die Quelle ist das Problem, nicht der Kanal):** QTmux
  leitet **nichts** aus Bildschirm/Prozesszustand ab; ein Claude-Code-Worker meldet von sich
  aus nichts (auch keine Bell). Deshalb Ehrlichkeit statt erzwungener Ereignisse:
  `subscribe_events` meldet je Quelle `hasPostedEvents`, `wait_for_events` bricht ohne Abo
  **sofort** ab (statt 25 s Stille) und legt bei Leerlauf einen `hinweis` bei. Worker
  ereignisfähig machen: Stop-Hook auf `shell-integration/qtmux-emit.{sh,ps1}` — **als Skript,
  nicht als curl-Einzeiler** (die dreifache Escape-Verschachtelung scheitert still und sieht
  aus wie „gerade passiert nichts"). Und: `wait_for_events` ist ein **Abholen** — es erreicht
  einen **beschäftigten** Agenten nicht; wecken kann nur dessen Umgebung, am Ende eines
  Hintergrundbefehls → `qtmux-wait.{sh,ps1,cmd}`. 🔑 Vier Fallen, jede erzeugt einen stumm
  nichts meldenden Wächter: `timeoutMs` **unter** dem HTTP-Timeout halten; `nextSeq` **immer**
  fortschreiben (sonst Endlos-Poll über dieselben gefilterten Ereignisse); Gesamt-Deckel auch
  **im laufenden Poll** prüfen (sonst überzieht er um eine Poll-Länge); POSIX-`read` verwirft
  das letzte Element ohne `\n` (`printf '%s'` → leeres `kinds`-Array → serverseitig „kein
  Filter" = alles; nur der **Gegentest** mit einem nicht passenden Ereignis zeigt das).
- **`get_layout`:** liefert `{layout, windowId, activePaneId, sessions}` — der Baum allein
  verschweigt, welche Sessions in **keinem** Pane liegen (laufen, aber unsichtbar).

