# Owner-Abnahmen — offene Punkte mit Abnahme-Rezept

> Ausgelagert aus `CLAUDE.md` am 2026-08-03 (Pflegeregel „context-schonend"): Diese Liste ist
> **Arbeitsvorrat**, kein dauerhaftes Wissen — sie gehört nicht in jede Session. Die
> **Mechanik** jedes Tickets steht in der Feature-Referenz der `CLAUDE.md`, hier steht nur,
> **wie man es abnimmt**.
>
> **Statuskonvention:** Alle Punkte hier sind umgesetzt und **selbst verifiziert** (Tests,
> E2E, in `main`) — der offene Rest ist die **Owner-Abnahme**. Die wird bewusst **nicht**
> über den Jira-Status geführt; ein Befund bei der Abnahme öffnet das Ticket wieder oder
> erzeugt ein Folgeticket.

**Stand 2026-08-05: 28 Tickets offen.**

## Vorbedingungen für jede Abnahme

⚠️ Abnahmen brauchen eine **frisch gebaute** Instanz. Solange die Produktivinstanz aus einem
Build-Verzeichnis läuft, ist „steht im Repo" **nie** gleich „ist in der App".

🔑 **Rezept für eine Abnahme am laufenden Programm** (seit QTMUX-96 erprobt): isolierte
Instanz starten (`QTMUX_PROFILE=<name> QTMUX_MCP_PORT=<frei>`, Binary aus
`build/macos-release`), per MCP **auf diesem Port** ein vorbereitetes Verzeichnis als Session
anlegen und fokussieren, dann den Owner schauen lassen. Das ersetzt, was hier prinzipiell
fehlt (kein Bedienungshilfen-Recht → keine synthetische Bedienung). Danach aufräumen:
Prozess beenden, `defaults delete com.qtmux.QTmux-<name>`, Testdaten löschen.

QTMUX-86 heilt bereits beschädigte Sessions **nicht** (Inhalt liegt im Scrollback) —
betroffene Sessions neu starten.

## Die Punkte

| Ticket | Was | Abnahme |
|---|---|---|
| **86** | leeres Pane beim Window-Wechsel | mehrfach zwischen Splitscreen und Einzel-Window wechseln, Prompt bleibt stehen |
| **85** | Agenten beim Start wiederherstellen (Vorgabe AUS) | Schalter an, mit echtem `claude` arbeiten, beenden, neu starten |
| **98** | Unterhaltung fortsetzen, 4 Modi (Vorgabe: gar nicht) | vier Modi durchspielen; Modus 3 braucht vorher `set_agent_session` |
| **99** | Umfang der Wiederherstellung, 3 Modi (Vorgabe: alles) | Modus 0 setzen, beenden, neu starten — gespeicherter Stand **unberührt** |
| **100** | Sidebar-Drag ließ die Kacheln weglaufen | Kachel ziehen (auch die letzte), Reihenfolge muss stimmen |
| **101/102/103** | ToolTip · geklemmter Drag · „Arbeitsverzeichnis öffnen"/„Pfad kopieren" | ToolTip in beiden Designs; Kachel bleibt im Bild; Menüpunkte an serieller Session ausgegraut |
| **61** | Bildschirm leeren, Verlauf behalten (`Ctrl/Cmd+Shift+K`) | in einem laufenden Agenten leeren — Prompt oben, Verlauf im Scrollback |
| **89** | Ruhezustand verhindern, solange Agenten arbeiten | Schalter an, Agent arbeiten lassen (macOS `pmset -g assertions`, Windows `powercfg /requests`) |
| **106/109/110/111** | Verzeichnis als zweite Kachelzeile, Seitenleiste + Flyout, Statusleiste (Design 1a/2a, Stufen 1–3) | durchklicken |
| **112** | **sechs Menüs** (Stufe 4) | jedes Menü öffnen; kein Eintrag schaltet mehr eine Einstellung außer Seitenleiste/Statusleiste/Broadcast; alles Verschobene über Einstellungen UND Palette erreichbar |
| **113** | **Einstellungsfenster** (Stufe 5) | Rail-Gruppen, Zeilen mit Beschreibung, Segment-Umschalter, Schalter in Akzentfarbe — in **beiden** Designs |
| **119** | **Fenster neben dem Bildschirm** (`1bb0ba1`) | QTmux auf einem zweiten Monitor platzieren, beenden, Monitor abziehen, starten → Fenster muss zentriert auf dem verbleibenden Bildschirm erscheinen; danach eine normale Position **nicht** verschoben finden |
| **118** | **Chevron klappt auch wieder aus** | Seitenleiste einklappen — oben muss ein `›` stehen, das sie wieder aufklappt; ToolTip und Richtung prüfen, in **beiden** Designs |
| **114** | **Zurücksetzen / Import / Export** (Stufe 6) | „Diese Seite zurücksetzen" auf einer geänderten Seite (Werte springen sofort auf Standard, danach ist der Punkt ausgegraut) · „Alle Einstellungen zurücksetzen …" **abbrechen** und bestätigen · **Export in eine Datei und Import daraus** — das ist der Teil, den hier **kein Automat** prüfen konnte (native Dateidialoge, s. E2E-Fallen): danach muss der geänderte Wert live stehen, und die offenen Fenster/Sessions müssen unberührt sein |
| **117** | **Standardknöpfe auf Deutsch** (`qtbase`-Translator) | einen beliebigen Dialog mit Standardknöpfen öffnen (z. B. „Alle Einstellungen zurücksetzen …" oder die Beenden-Rückfrage): der Knopf muss **„Abbrechen"** heißen, nicht „Cancel" — und in englischer Oberfläche weiterhin „Cancel". 🔑 Der letzte Millimeter ist hier **nicht** automatisierbar: Test und A/B belegen Einbettung, Laden und Kontext, aber kein Automat kann ohne Bedienungshilfen-Recht einen Dialog öffnen und ablesen |
| **108** | **Arbeitsverzeichnis via OSC 7** | Shell-Integration sourcen (`qtmux --install-shell-integration`), dann in **PowerShell** `Set-Location` — die Verzeichniszeile der Kachel muss folgen (ohne Integration bleibt sie stehen, das ist kein Fehler). Bei `ssh` zeigt sie den Pfad der **Gegenstelle** |
| **120** | **Rastergröße in der Statusleiste** | Fenster größer ziehen, Panes teilen, zwischen Windows wechseln: die Anzeige folgt und zeigt **nie** einen Zwischenwert wie 80×2 |
| **121** | **Statusleiste läuft nicht mehr über** | Session in ein tief verschachteltes Verzeichnis legen — der Pfad wird mit „…" gekürzt, die Felder rechts davon bleiben lesbar |
| **58** | **Git-Branch auf der Kachel** | zwei Sessions im **selben** Repo auf **verschiedenen** Branches öffnen — genau der Fall, für den das Ticket existiert. `git checkout` in einer davon: die Kachel folgt binnen ~1,5 s, ohne dass sich das Verzeichnis ändert |
| **90** | **Prompt-Warteschlange** | in einer Session mit **echtem** Agenten einreihen (Palette → „In die Warteschlange einreihen …"), während er arbeitet: Abzeichen zeigt die Anzahl, der Text geht erst nach seiner Fertigmeldung raus. ⚠️ Bei einer gewöhnlichen Shell mit **stillem** Langläufer (`sleep 6`) geht er zu früh raus — bekannte Grenze, Begründung in der Feature-Referenz |
| **125** | **Online-Update** (Hilfe-Menü, Dialog, Einstellung) | Hilfe → „Nach Updates suchen …" auf einem **1.8.0**-Build muss **„QTmux 1.8.0 ist aktuell"** melden — kein Fehler, kein Hänger. Dann Einstellungen → Allgemein → Aktualisierung: Schalter aus, QTmux neu starten → beim Start passiert nichts. Der Server ist scharf, die Meldung „aktuell" ist also der ECHTE Fall. Der **vollständige** Durchlauf ist auf **macOS** am lebenden Objekt belegt (1.7.1-Instanz findet 1.8.0, lädt, mountet das DMG); ⚠️ **Windows und Linux fehlen dort noch** (nur Start-Plan geprüft) |
| **127** | **MCP im Netzwerk erreichbar** (Bind-Adresse, Token, pf) | Einstellungen → Agenten & MCP: „Im Netzwerk erreichbar" **an** — es muss sofort ein Token erscheinen (Anzeigen/Kopieren/Neu erzeugen), die Statusleiste unten rechts muss auf **„MCP LAN :7345" in Amber** wechseln, und ein `curl` von einem anderen Rechner muss **ohne** Kopfzeile 401 und **mit** `Authorization: Bearer <token>` eine Antwort liefern. Danach wieder **aus** → Statusfeld zurück auf „MCP :7345", `curl` von außen läuft ins Leere. Beides in **beiden Designs** ansehen (die Sichtprüfung der Seite ist auf macOS nicht automatisierbar — eigenes `Window`). ⚠️ Getrennt davon die **pf-Regel**: `sudo tools/pf/install-pf-anchor.sh --net 192.168.0.0/24` (braucht ein Passwort, deshalb hier nicht ausgeführt), dann `sudo pfctl -s rules \| grep com.qtmux` — und der einzige echte Beleg ist die Gegenprobe von **außerhalb** des Netzes: **Timeout**, nicht „connection refused" |
| **128** | **Mausrad in Agenten-Oberflächen** | Der eigentliche Anwenderfall: einen **erkannten** Agenten (`codex` als **erster** Befehl der Zeile — `cd x; codex` wird nicht erkannt, dann bleibt das Rad zu Recht tot) arbeiten lassen, bis der Bildschirm voll ist, und **auf ruhigem Bild** eine Rastung vor und wieder zurück drehen: es muss scrollen und **exakt** zum Ausgangsbild zurückkehren. Dann Einstellungen → Eingabe & Zwischenablage → Maus auf **„Immer"** und in `less`/`man` gegenprüfen (in `vim` bewegt es dort den Cursor — bekannt und so dokumentiert). ✅ Die **Optik** der Prefs-Zeile ist in beiden Designs bereits bildlich abgenommen (2026-08-05); offen ist allein das Verhalten am lebenden Agenten |

## Design 1a/2a — was der Durchklick zusätzlich abdecken muss

Über die Zeilen 106–121 hinaus, weil es **keine** eigene Ticketzeile hat:

- die macOS-**nativen Menüs**: Fenster-Menü Minimieren/Zoomen **ohne** Cocoa-Dublette; sechs
  Menüs mit „Einstellungen …" im **Datei**-Menü (macOS-Rollen kennt QtQuick.Controls nicht);
  Kürzel **Cmd+B** Seitenleiste und **Cmd+A** „Alles auswählen" — Letzteres darf das Terminal
  **nicht kapern** und muss `tst_hotkeys::defaultsAreConflictFree` grün halten.
- **Flyout, Statusleiste, Einstellungsfenster und `App.reduceMotion`** in **beiden** Designs.
- Menü-Icons fehlen nativ = QTMUX-13 (deferred) — **kein neuer Fehler**.
- **Export/Import durch die echten Dateidialoge** ist auf keiner Maschine automatisierbar
  (Logik im Unit-Test bewiesen, Dialog-Öffnen per Screenshot) → gehört zwingend hierher.

## Zwei Punkte, die keine Ticket-Zeile sind

- **QTMUX-127, Restarbeit:** die Sichtprüfung der Einstellungsseite in beiden Designs (das
  Prefs-Fenster ist auf macOS mit `--screenshot` prinzipiell nicht greifbar — eigenes
  `Window`) und die **pf-Installation auf dem Zielrechner**: `sudo` verlangt hier ein
  Passwort, das Skript ist fertig und trocken geprüft (`pfctl -n -f` sauber), aber nicht
  geladen. Die Application Firewall ist auf dieser Maschine aus (`State = 0`).
- **Aus der Architektur-Vollanalyse:** `Theme.accentText` statt hartem Weiß (6 Stellen) —
  die Sichtprüfung in beiden Designs steht aus (sichtbar wird der Unterschied erst bei
  Schemata mit hellem ANSI-Blau).

Zum ungeprüften Nebenbefund an QTMUX-100 (Drag auf Zeile 0 bei gescrollter Liste) siehe
den Abschnitt „Arbeitsstand" in der `CLAUDE.md` — dort werden die offenen Fäden geführt.
