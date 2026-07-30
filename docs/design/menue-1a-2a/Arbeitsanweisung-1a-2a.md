> ⚠️ **Temporär eingecheckte Kopie — nach Stufe 7 wieder entfernen.**
> Original und SSoT ist die Datei `Arbeitsanweisung-1a-2a.md` im Claude-Design-Projekt
> „Menü-Struktur und Design-Auffrischung" (`ab66e9b5-053b-4e81-9e4a-c45752fd42d1`, lesbar über
> das `DesignSync`-Werkzeug, `get_file`). Diese Kopie liegt hier, damit die **macOS-Session**
> die Anweisung ohne Zugriff auf das Design-Projekt hat (Arbeitsteilung vom 2026-07-30:
> Windows baut, macOS verifiziert auf macOS + Linux und macht die GUI-Abnahme).
> Stand der Umsetzung: **Stufen 1–6 fertig**, Stufe 7 offen — Fortschritt und die bewussten
> Abweichungen stehen in der [CLAUDE.md](../../../CLAUDE.md), nicht hier.
> Bei Divergenz gilt das Original im Design-Projekt.

---

# Arbeitsanweisung — QTmux: Menü-Neuordnung (1a) + einklappbare Seitenleiste (2a)

Grundlage: Design-Vorschläge `1a` und `2a` in `QTmux Menue und Konfiguration.dc.html`.
Ziel: aufgeräumte Menüstruktur, Statusleiste, Einstellungsfenster mit Gruppen-Rail,
Seitenleiste auf Icon-Breite (52 px) einklappbar. **Kein** Umbau des Grundlayouts
(Panels links / Terminals rechts bleibt), **kein** neues Farbsystem.

Randbedingungen:
- Qt 6, Qt Quick Controls **Basic**, keine Blur-/Schatten-/Layer-Effekte über das
  bestehende `MultiEffect`-Icon-Tinting hinaus.
- Chrome-Farben weiter ausschließlich über `Theme.*` (aus dem aktiven Farbschema
  abgeleitet, `src/viewmodels/Theme.cpp`) — keine Literal-Farben in QML, Ausnahme sind
  die bereits vorhandenen Statusfarben (`#46d369`, `#f5c451`, `#e5534b`, `#5a5d6a`).
- macOS rendert die `MenuBar` nativ, Windows/Linux in-window. Beide müssen denselben
  Action-Satz zeigen; nur die Platzierung von „Einstellungen …" und ein „Fenster"-Menü
  dürfen sich unterscheiden.
- Jede neue benutzersichtbare Zeichenkette in `qsTr()`; danach `i18n/qtmux_de.ts`
  und `qtmux_en.ts` aktualisieren.
- Alle bestehenden Tests müssen grün bleiben (`ctest --test-dir build/<preset>`).

---

## Teil A — Menüstruktur (1a)

### A1. Zielstruktur: 6 Menüs

| Menü | Einträge (Reihenfolge) |
|---|---|
| **Datei** | Neues Fenster · Neue Session · *Neu ▸* (SSH, Seriell, Plugin-Backends) · — · Verbindungen … · Secrets-Vault … · — · Einstellungen … (`actSettings`, nur Windows/Linux) · Beenden |
| **Bearbeiten** | Kopieren · Einfügen · Alles auswählen · — · Im Terminal suchen … · Befehlspalette |
| **Ansicht** | *Teilen ▸* (Nebeneinander, Untereinander) · Pane zoomen · Pane schließen · Nächstes/Vorheriges Pane · — · *Zoom ▸* (Vergrößern, Verkleinern, Zurücksetzen) · — · Bildschirm leeren · Eingabe zurücksetzen · — · **Seitenleiste** (Umschalter, neu) · **Statusleiste anzeigen** (Umschalter, neu) |
| **Session** | Nächste · Vorherige · — · Umbenennen … · *Gruppe ▸* · — · Broadcast-Eingabe · — · Session schließen |
| **Agent** | Neue Agent-Session … · Agent-Ereignisse … · — · MCP-Server starten/stoppen (`actMcpToggle`) · — · Agenten-Einstellungen … (öffnet `prefs.open("agenten")`) |
| **Hilfe** | Dokumentation · Tastenkürzel-Übersicht · Über QTmux |

### A2. Was aus den Menüs verschwindet

Ersatzlos aus `menuBar` entfernen — die Funktion bleibt, nur der Ort ändert sich:

| Heutiger Eintrag | Neuer Ort |
|---|---|
| Datei ▸ Standard-Shell ▸ | Einstellungen › Terminal (bestehendes `CatTerminal`) + „+"-Splitbutton wie bisher |
| Datei ▸ Vor dem Beenden nachfragen | Einstellungen › Allgemein |
| Datei ▸ Sessions beim Start wiederherstellen ▸ | Einstellungen › Allgemein |
| Bearbeiten ▸ Auswahl automatisch kopieren / Rechtsklick fügt ein / Vor mehrzeiligem Einfügen warnen | Einstellungen › Eingabe |
| Ansicht ▸ Einstellungen … | Datei (bzw. macOS-App-Menü) |
| Ansicht ▸ Design: Wie System / Hell / Dunkel | Einstellungen › Allgemein + Statusleisten-Feld (Klick = Umschalten) |
| Top-Menü **Sprache** | Einstellungen › Allgemein (Menü entfällt komplett) |
| Top-Menü **Agent-Steuerung** | Agent ▸ MCP-Server + Statusleisten-Feld (Menü entfällt komplett) |
| Agent ▸ Agenten wiederherstellen / Unterhaltung fortsetzen ▸ | Einstellungen › Agenten & MCP |

Regel für künftige Einträge: **Ein Menü enthält Befehle, keine Zustände.** Checkbare
Einträge sind nur noch dort erlaubt, wo sie eine reine Ansichtseigenschaft des Fensters
umschalten (Seitenleiste, Statusleiste, Broadcast-Eingabe).

### A3. macOS-Koexistenz

- `actSettings` bekommt in `Main.qml` die Rolle `MenuItem.role: MenuItem.PreferencesRole`
  (bzw. `Action`-Äquivalent), damit macOS es ins App-Menü zieht; der Eintrag im
  Datei-Menü wird auf macOS mit `visible: Qt.platform.os !== "osx"` ausgeblendet.
- `actAbout` → `AboutRole`, `actQuit` → `QuitRole`.
- Auf macOS zusätzlich ein Standard-Menü **Fenster** (Minimieren, Zoomen, Alle nach
  vorne) — `visible: Qt.platform.os === "osx"`.
- Keine Duplikate: alle Einträge kommen weiterhin aus demselben `Action`-Satz, damit
  Kürzelanzeige (`ShortcutMenuItem`) und Command-Palette synchron bleiben.

### A4. Command-Palette

`buildCommands()` in `Main.qml` um die verschobenen Einträge ergänzen, damit nichts
unerreichbar wird: „Design umschalten", „Sprache …", „Sessions beim Start
wiederherstellen …", „Seitenleiste ein-/ausklappen", „Statusleiste ein-/ausblenden",
„Einstellungen: <Kategorie> …" (ein Eintrag pro Kategorie, öffnet `prefs.open(id)`).

---

## Teil B — Statusleiste (neu)

Neues `footer:` am `ApplicationWindow` in `Main.qml`, Höhe **26 px**,
`color: Theme.bgSidebar`, 1 px Oberkante `Theme.border`, Schrift 11 px in
`window.terminalFontFamily` (Mono), Farbe `Theme.textDim`.

Felder von links nach rechts:

1. Aktive Session: Statuspunkt (7 px, Farbe wie Status-Ring) · `#<sessionId>` · Programm · Arbeitsverzeichnis (elidiert). Klick → Fokus ins aktive Pane.
2. Zusammenfassung: `qsTr("%1 Sessions · %2 wartet · %3 Fehler")` — aus `windows`/`sessions` aggregiert, Zähler nur anzeigen, wenn > 0.
3. Encoding + Grid (`80×24`) der aktiven Session.
4. *Rechts:* **MCP** (`mcp.listening` → `Theme.accent`, Text `MCP :7345`; Klick startet/stoppt, Rechtsklick → `prefs.open("agenten")`).
5. **Vault** (`Vault.unlocked`; Klick entsperrt bzw. `prefs.open("vault")`).
6. **Broadcast** (`window.broadcastInput`; Klick toggelt).
7. **Design/Schema** (`Theme.dark ? "Dunkel" : "Hell"` + `ColorSchemes.current`; Klick = `Theme.toggle()`, Rechtsklick → `prefs.open("erscheinungsbild")`).

Jedes Feld ist ein klickbares `Item` mit `HoverHandler` (Hover = `Theme.sidebarHover`,
Radius 4) und `AppToolTip`, der die Aktion benennt. Sichtbarkeit persistieren unter
`ui/statusBarVisible` (Standard: an), Umschalter in *Ansicht*.

---

## Teil C — Einstellungsfenster (1a)

Datei `qml/PrefsWindow.qml` — Struktur bleibt (Rail links, Loader rechts), folgende
Änderungen:

### C1. Kopfzeile
Höhe 60 → **56 px**, Suchfeld von Pille (Radius 17) auf **Radius 6, Höhe 30, Breite 320**,
zusätzlich rechts zwei Textbuttons: **„Zurücksetzen"** und **„Import / Export"** (s. C4).

### C2. Rail mit Gruppen
Die neun Kategorien behalten IDs und Inhalte, bekommen aber drei Gruppen-Überschriften
(11 px, `Theme.textDim`, Versalien, nicht anklickbar):

- **Arbeitsplatz** — `allgemein`, `erscheinungsbild`
- **Terminal** — `terminal` (Label neu: „Darstellung & Shell"), `eingabe` („Eingabe & Zwischenablage"), `hotkeys`
- **Agenten & Geräte** — `agenten`, `verbindungen`, `vault`, `erweiterungen`

Kachelhöhe 40 → **36 px**, Radius 10 → 8, Rail-Breite 232 → **236 px**. Badges bleiben
unverändert (`badgeFor()`). Tastatur-Navigation (`selectDelta`) muss die Überschriften
überspringen.

### C3. Einheitliches Zeilenformat auf den Seiten
`qml/prefs/CatPage.qml` um eine wiederverwendbare Zeile erweitern (z. B. `PrefRow.qml`):
links Titel (13 px, `Theme.textBright`) + Beschreibung (11.5 px, `Theme.textDim`,
`wrapMode`), rechts das Control, vertikal zentriert, Zeilenpadding 13 × 16 px,
Zeilen in einer Gruppe in einem 1-px-`Theme.border`-Rahmen mit Radius 10.
Alle `Cat*.qml` auf diese Zeile umstellen — dadurch verschwinden die heutigen
freistehenden Erklärtexte unterhalb der CheckBoxen.

Für Mehrfachauswahl mit ≤ 3 Optionen (Design, Restore-Modus) statt `AppComboBox` einen
**Segment-Umschalter** (Row aus `Button`s, gemeinsamer Rahmen, aktives Segment
`Theme.accent`) — neue Komponente `qml/Ui/SegmentedControl.qml`.
Boolesche Werte als Schalter (`Switch`, `Theme.accent`) statt `CheckBox`.

### C4. Neu: Zurücksetzen, Import/Export
- „Zurücksetzen" → Menü mit *Diese Seite zurücksetzen* / *Alle Einstellungen zurücksetzen*; Letzteres mit Bestätigungsdialog (`AppDialog`). Setzt die betroffenen `QSettings`-Schlüssel zurück und lässt die Registries neu laden.
- „Import / Export" → `FileDialog`, Format JSON: alle `QSettings`-Schlüssel der Domain außer dem Vault (Secrets werden **nie** exportiert; Profile ohne aufgelöste Passwörter). Import zeigt vorab eine Liste der zu ändernden Schlüssel.
- Implementierung in einem neuen `SettingsIo`-Objekt unter `src/viewmodels/`, in QML als Context-Property; dazu ein Unit-Test `tests/tst_settingsio.cpp` (Round-Trip Export → Reset → Import).

### C5. Aufgeräumte Zuordnung
`CatAllgemein` erhält die Abschnitte **Sprache & Design**, **Start & Beenden**
(Restore-Modus, Beenden-Rückfrage, Quake), **Energie** — also genau die Schalter, die
aus Datei/Ansicht/Sprache herausfallen. `CatEingabe` erhält die drei Clipboard-Schalter
aus *Bearbeiten*. `CatAgenten` erhält Restore-Agenten + Resume-Modus aus *Agent*.

---

## Teil D — Einklappbare Seitenleiste (2a)

### D1. Verhalten
- Breite frei ziehbar (bestehender Splitter), Bereich **180 – 420 px**.
- Beim Ziehen **unter 140 px** rastet die Leiste in den eingeklappten Zustand (52 px);
  Ziehen über 140 px klappt wieder auf. Zwischenwerte werden nicht gehalten.
- Umschalten zusätzlich über: Chevron-Button im Kopf der Leiste, Doppelklick auf den
  Splitter, `Ctrl+B` / `⌘B` (neue Action `actToggleSidebar`, in `HotkeyRegistry`
  registrieren) und *Ansicht ▸ Seitenleiste*.
- Persistenz pro Fenster: `ui/sidebarWidth` (int) und `ui/sidebarCollapsed` (bool).
- Übergang: `Behavior on Layout.preferredWidth { NumberAnimation { duration: 120; easing.type: Easing.OutCubic } }`, abgeschaltet bei `App.reduceMotion`.

### D2. Eingeklappte Darstellung (52 px)
Dieselbe `ListView` und dasselbe `windows`-Model, zweites Delegate (Auswahl per
`sidebarCollapsed`):

- Kachel **40 × 44 px**, Radius 8; ausgewählt `Theme.sidebarSelected`, Hover `Theme.sidebarHover`.
- **Backend-Icon** 18 px, mittig, getönt wie heute (`MultiEffect`, `brightness: 1.0`,
  `colorization: 1.0`): Agent → `robot`, lokale Shell → `terminal-window`,
  SSH → `plugs`, Seriell → `usb`, Plugin-Backend → `plugs`.
- **Statuspunkt** 8 px oben rechts, 1.5 px Rand in der Kachelfarbe, Farben wie der
  heutige `statusRing` (Attention = `Theme.accent`, 1 = grün, 2 = gelb, 3 = rot,
  4 = grau). Attention-Puls (`SequentialAnimation on opacity`) läuft unverändert weiter,
  jetzt auf dem Punkt.
- **Session-Nummer** darunter, 9 px, `window.terminalFontFamily`, `Theme.textDim`
  (aktive Kachel heller).
- **Gruppenfarbe** als 3 px breiter Balken am linken Kachelrand (Radius 2), Höhe = Kachelhöhe − 16.
- **MCP-Controller-Marke** (heute roter Tab) bleibt: 3 px Balken rechts statt links.
- **Gruppen-Sections** werden zu 1-px-Trennern (`Theme.border`, Breite 28, zentriert);
  eingeklappte Gruppen bleiben eingeklappt, ihre Kacheln also unsichtbar.
- Kopf der Leiste: 26 × 26 „Q"-Marke; Fuß: „+"-Button (neue Session, gleiche Aktion wie
  der Toolbar-Splitbutton).
- Drag-Reorder muss auch schmal funktionieren — vorhandene `DragHandler`-Logik
  (Translate-Transform, **nicht** `target`) unverändert übernehmen.

### D3. Hover-Flyout
Bei Hover auf eine Kachel (Verzögerung 350 ms) öffnet rechts neben der Leiste ein
`Popup` (Breite 250, `AppPopupBg`) mit: Statuspunkt + Titel + `#id`, Arbeitsverzeichnis,
Pane-Anzahl, Zustandstext („wartet auf Eingabe · seit 40 s"), Gruppenname.
Inhalt entspricht dem heutigen `AppToolTip` der Kachel — dieser wird im eingeklappten
Zustand durch das Flyout ersetzt, im aufgeklappten bleibt er.
Klick aktiviert das Window, Rechtsklick öffnet unverändert `windowMenu`.

### D4. Zugänglichkeit / Fallback
- Jede Kachel behält `Accessible.name` = voller Titel + `#id` + Zustand.
- Kein Informationsverlust nur über Farbe: Zustand steht zusätzlich im Flyout-Text und
  aggregiert in der Statusleiste.
- **Kein** Fortschritts-Ring (OSC 9;4) im eingeklappten Zustand — bewusst verworfen,
  da nicht alle Agenten OSC 9;4 senden und die Ringform vom gewohnten Statuspunkt
  abweicht. Fortschritt bleibt wie bisher nur im aufgeklappten Zustand / im Terminal.

---

## Teil E — Reihenfolge, Tests, Abnahme

Empfohlene Commit-Folge (jeder Schritt für sich lauffähig und getestet):

1. `actToggleSidebar` + Persistenz + Splitter-Einrasten (ohne neues Delegate).
2. Eingeklapptes Delegate + Flyout (Teil D2/D3).
3. Statusleiste inkl. `ui/statusBarVisible` (Teil B).
4. Menü-Neuordnung + macOS-Rollen + Palette-Ergänzungen (Teil A).
5. Einstellungsfenster: Rail-Gruppen, `PrefRow`, `SegmentedControl` (C1–C3, C5).
6. `SettingsIo` mit Reset/Import/Export (C4).
7. Übersetzungen `de`/`en` finalisieren, README-Screenshots ersetzen.

Tests:
- `tests/tst_hotkeys.cpp` um `actToggleSidebar` erweitern (Default, Konflikte).
- `tests/tst_windowmodel.cpp`: Aggregat-Zähler für die Statusleiste (Sessions gesamt / wartend / Fehler).
- Neu `tests/tst_settingsio.cpp` (s. C4); Vault-Schlüssel dürfen im Export **nicht** vorkommen.
- Manuell auf macOS prüfen: natives Menü zeigt Einstellungen im App-Menü, kein doppelter Eintrag, Fenster-Menü vorhanden.

Abnahmekriterien:
- Kein Menüeintrag schaltet mehr eine Einstellung um, außer Seitenleiste, Statusleiste, Broadcast-Eingabe.
- Jede vorher im Menü erreichbare Funktion ist über Einstellungen **und** Command-Palette erreichbar.
- Eingeklappte Leiste: Status, Gruppe, Typ und Session-Nummer jeder Session ohne Aufklappen ablesbar.
- Zustand von Leiste, Statusleiste und Fensterposition überlebt einen Neustart.
- Keine neuen Warnungen im QML-Log, keine Literal-Chrome-Farben, `ctest` grün auf allen drei Plattformen.
