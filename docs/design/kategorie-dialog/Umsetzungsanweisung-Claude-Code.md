# Umsetzungsanweisung — Einstellungen als Kategorie-Fenster (QTmux)

> Zum Einfügen in eine Claude-Code-/Entwickler-Session im QTmux-Repo.
> Design-Referenz: `docs/design/kategorie-dialog/Arbeitsanweisung.html`
> (Teil A = Fenster, Teil B = Status-Puls) sowie die Entwürfe 1a / 1b / 1c.
> **Variante:** ⟨1a ruhig · 1b Konsole · 1c Vorschau-Spalte⟩ — vor Start festlegen;
> Struktur und Funktionsumfang sind in allen drei identisch, nur die Ausgestaltung
> unterscheidet sich.

## Auftrag

Ersetze den modalen `settingsDialog` in `qml/Main.qml` durch ein **eigenes,
nicht-modales Fenster** mit Kategorie-Liste links und View rechts. Ziehe
Verbindungsprofile, Secrets-Vault, MCP-Server und Plugins in dieses Fenster ein.
**Keine funktionalen Änderungen an den Einstellungen selbst** — jede Option bleibt
erhalten, wirkt weiterhin sofort und nutzt dieselben Settings-Keys.

Nicht-Ziele: neue Einstellungen, Umbau von `Theme`/`ColorScheme`/`HotkeyRegistry`,
Änderungen am Terminal-Kern, neue Abhängigkeiten.

## Reihenfolge (ein Commit je Schritt, jeder Schritt baut & läuft)

**1 — Gemeinsame UI-Komponenten herausziehen.**
`AppDialog`, `AppComboBox`, `AppPopupBg`, `AppMenuItem`, `ThemedMenu`, `IconToolButton`,
`SectionLabel` aus `Main.qml` in eigene Dateien unter `qml/Ui/` überführen; in
`CMakeLists.txt` bei `qt_add_qml_module(qtmux … QML_FILES …)` eintragen. `Main.qml` nutzt
sie unverändert weiter — rein mechanisch, keine Optikänderung. Danach Debug **und**
Release bauen und visuell gegenprüfen.

**2 — Fenstergerüst `qml/PrefsWindow.qml`.**
Eigenes `Window` (nicht `Dialog`), `flags: Qt.Window`, `title: qsTr("Einstellungen")`,
Start 980×680 (1c: 1060×700), `minimumWidth/Height` 820×560.
*Falle:* Ein separates `Window` erbt die `palette` des `ApplicationWindow` **nicht** →
den kompletten `palette.*`-Block aus `Main.qml` (Zeilen ~17–34) übernehmen, sonst sind
Basic-Controls im Hell-Modus falsch gefärbt. Ebenso: Popups/Menüs im neuen Fenster
brauchen wieder `AppPopupBg` + eigene Palette.
Inhalt: zweispaltiges `RowLayout` — Rail (`Theme.bgSidebar`, 232–250 px, 1 px
`Theme.border`) + View (`Theme.bgMain`, eigenes `Flickable`, Rail scrollt nie mit).
`property string category` + `function open(categoryId, settingKey)`; Persistenz über
`Settings` mit `ui/prefsGeometry` und `ui/prefsCategory`. Esc und ⌘W/Strg+W schließen
nur dieses Fenster. Zweiter Aufruf von `actSettings` hebt das bestehende Fenster nach
vorn (`raise()` + `requestActivate()`), öffnet kein zweites.

**3 — Kategorien 1:1 umziehen.** Je Kategorie eine Datei in `qml/prefs/`:
`CatAllgemein.qml`, `CatErscheinungsbild.qml`, `CatTerminal.qml`, `CatEingabe.qml`,
`CatAgenten.qml`, `CatHotkeys.qml`, `CatVerbindungen.qml`, `CatVault.qml`,
`CatErweiterungen.qml`. Zuordnung der heutigen Optionen exakt nach Tabelle A4 der
Design-Referenz. Der Rail-Eintrag liefert Icon (`qrc:/icons/…`, Tönung über die
explizite `MultiEffect`-Form — `layer.effect` greift in Delegates nicht zuverlässig),
Label und Badge (Vault-Sperrzustand, Anzahl Abos/Profile/Plugins, Schema-Name).
Auswahl `Theme.sidebarSelected`, Hover `Theme.sidebarHover`. **Keine Hex-Literale im
neuen QML-Code** außer den bestehenden Status-Farben aus Teil B.

**4 — Terminal-Vorschau.** Nicht interaktives Vorschaufeld unter den Terminal-Optionen:
aktives Schema als Hintergrund, gewählte Schriftart/-größe, Beispielausgabe mit
ANSI-Farben und einer Ligaturzeile (`!= <= => -->`); reagiert sofort auf jede Änderung.
Umsetzung frei wählbar (vorbefülltes `TerminalItem` ohne PTY oder gestylter
`Text`-Block) — sichtbares Ergebnis ist verbindlich, nicht der Weg.

**5 — Tastenkürzel mit Aufnahme in der Zeile.** `hotkeyCaptureDialog` entfällt; die
Zeile selbst geht in den Aufnahme-Zustand (Akzentrahmen, „Tasten drücken …“, Esc bricht
ab, ⏎ bestätigt, „Leeren“ entfernt). Das bestehende `capturing`-Flag wandert mit und
deaktiviert weiterhin alle globalen `Shortcut`s. Konflikt **inline unter der Zeile**:
„Bereits belegt von: ⟨Aktion⟩“ mit „Zuweisen“ / „Abbrechen“. Gruppen: Sitzungen · Panes ·
Verbindungen · Ansicht & App; „Standard“ nur bei Abweichung vom Default.

**6 — Agenten-Matrix + MCP.** Zeile = Empfänger, Spalte = Quelle (`#id`, Tooltip mit
Titel + CWD), Diagonale gesperrt. Mapping auf die bestehende API: leere Quell-Liste
bedeutet weiterhin „alle anderen“; wird eine einzelne Zelle abgewählt, wird die explizite
Quell-Liste geschrieben (`AgentEvents.subscribe(id, sources, kinds)`). Arten-Chips je
Zeile (fertig/Frage/Fehler, leere Liste = alle). Darunter MCP: an/aus, Port, Hinweis
„nur 127.0.0.1 · Vault nicht über MCP erreichbar“. Leerzustand „Keine Sessions geöffnet.“
bleibt.

**7 — Suche mit Sprung.** Suchfeld in der Rail (1a/1b) bzw. im Fensterkopf (1c),
⌘F/Strg+F fokussiert, Esc leert. Jede Einstellung erhält ID + Stichwortfeld
(z. B. `terminal.ligatures` → „FiraCode, Ligaturen, Glyph“); durchsucht werden Label,
Erklärtext, Stichworte und Aktionsnamen der Kürzel. Treffer-Auswahl wechselt die
Kategorie, scrollt die Zeile in den Blick und hebt sie **einmalig ~1,2 s** mit
`Theme.sidebarSelected` hervor (kein Dauerblinken). Die Command-Palette (⌘K) bleibt
unverändert und springt für ihre Einstellungs-Einträge nach `prefs.open(…)`.

**8 — Alte Dialoge entfernen.** `settingsDialog`, `connectionsDialog`, `vaultDialog`,
`hotkeyCaptureDialog` aus `Main.qml` löschen; `actSettings`, `actConnections`,
`actVault`, `actMcpToggle`, Toolbar-Knöpfe und Palette-Einträge auf
`prefs.open(kategorie)` umbiegen. Als modale Dialoge **bleiben**: Profil anlegen/ändern,
Secret anlegen/ändern, Master-Passwort ändern, Datei-/Ordner-Dialoge, Paste-Warnung,
Beenden-Rückfrage.

**9 — Status-Puls (Teil B) festschreiben.** Verhalten bleibt wie heute
(Opazität 1.0 → 0.3 in 600 ms und zurück, endlos, `alwaysRunToEnd`, Reset auf 1.0 beim
Ende; Ring **und** 2 px Kachelrahmen synchron; Fortschritt unbestimmt = 700 ms je
Richtung). **Neu:** bei aktivem „Bewegung reduzieren“ des Systems nicht animieren,
sondern Ring dauerhaft in Akzentfarbe und Rahmen statisch zeichnen. Im
Einstellungsfenster wird **nichts** animiert außer dem Vorschau-Cursor und dem einmaligen
Aufblenden nach einem Suchtreffer; keine Toasts/Banner für Agenten-Ereignisse — die
Sidebar bleibt die einzige Meldestelle. Das Fenster öffnet beim ersten Mal versetzt,
sodass die Sidebar sichtbar bleibt.

## Konventionen (gelten für alle Schritte)

- Deutsche Kommentare, Code-Referenzen als Markdown-Links, committen/pushen nur auf
  Auftrag, Commit-Trailer wie gewohnt.
- Alle sichtbaren Strings mit `qsTr()`; danach
  `cmake --build … --target update_translations`, `i18n/qtmux_en.ts` pflegen
  (DE wird automatisch finalisiert). Übersetzungen übernommener Texte wiederverwenden,
  Formulierungen nicht ändern.
- Neue QML-Dateien **immer** in `qt_add_qml_module(… QML_FILES …)` eintragen.
- Gui-freie Singletons bleiben Context-Properties in `main.cpp` — nicht in die URI
  „QTmux“ registrieren.
- Jeder Build-Zyklus: Debug **und** Release (`*-release`-Presets).
- `header: ToolBar` braucht feste `height`; verschachtelte `RowLayout`-Kinder mit
  `fillHeight` brauchen `maximumHeight`/`fillHeight: false`.

## Fertig, wenn

1. Jede heutige Einstellung ist im neuen Fenster erreichbar und wirkt sofort.
2. Das Fenster ist nicht modal: Terminals laufen weiter, Sessions lassen sich im
   Hauptfenster wechseln, der Status-Puls bleibt sichtbar.
3. Design- und Schema-Wechsel färben beide Fenster ohne Neustart, Hell **und** Dunkel
   geprüft.
4. Kürzel-Aufnahme inkl. Konfliktfall funktioniert; währenddessen feuert kein globaler
   Shortcut.
5. Suche findet jede Einstellung über Label *und* Stichwort und hebt sie nach dem Sprung
   hervor.
6. Fenstergrößen 820×560 und maximiert ohne Überlappung; Rail scrollt nicht mit.
7. `tst_hotkeys` und `tst_profiles` um die neuen Einstiegspunkte ergänzt, alle Tests
   grün, Release-Build sauber.
8. `docs/`-Eintrag und Jira-Ticket aktualisiert; README nur anpassen, falls sich der
   beschriebene Bedienweg ändert (DE **und** EN).
