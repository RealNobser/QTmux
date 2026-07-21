# qtmux — Fehlerbericht aus dem MCP-Controller-Betrieb

> **Erledigt am 2026-07-21** (QTMUX-30 bis QTMUX-34). Dieser Bericht bleibt als
> Belegquelle im Repo: Er ist die erste Rückmeldung aus einem *echten* Mehragenten-
> Betrieb, und die Punkte 1–3 waren allesamt Fehler, die **Erfolg meldeten** — genau
> die Sorte, die eine Testmatrix nicht findet. Was daraus wurde:
>
> | Punkt | Ergebnis |
> |---|---|
> | 1 `wait_for_events` | Kanal war intakt, es fehlte die **Quelle**. Vermutung des Berichts bestätigt (Worker melden nichts von sich aus, auch keine Bell/OSC 9). Antwort: ehrliche Rückmeldung (`hasPostedEvents`, `sourcesWithEventsSoFar`, `hinweis`), sofortiger Abbruch ohne Abo, mitgeliefertes Hook-Skript `shell-integration/qtmux-emit.sh` — kein erzwungener Ereignisstrom. |
> | 2 `send_text` | Behoben: Enter geht 60 ms abgesetzt raus (`enterDelayMs`). Regressionstest bricht bei `0`. |
> | 3 „Unbekannte ID." | Behoben: Klartext bei falschem Parameternamen, sonst Nennung der vorhandenen IDs. |
> | 4 `get_layout` | Umgesetzt: `{layout, activePaneId, sessions}` inkl. unsichtbarer Sessions. |
> | 5 CLAUDE.md | Gegenprobe ohne Fund; Dauerwächter `test_doc_duplicates` ergänzt. |
>
> Verifiziert gegen eine **separate** Instanz (`QTMUX_PROFILE=test QTMUX_MCP_PORT=7346`)
> mit echten Claude-Code-Workern — der MCP-Port ist dafür konfigurierbar geworden.

**Server:** QTmux 1.3.1, `streamable-http`, `http://localhost:7345`
**Client:** Claude Code v2.1.215 (Opus 4.8) als MCP-Controller in Session 7
**Aufbau:** Session 7 = Controller (`mcpController: true`), Sessions 8 und 9 =
zwei Claude-Code-Worker im Auto-Mode, alle im selben Arbeitsverzeichnis
**Datum:** 2026-07-21

Der Betrieb funktioniert im Kern gut — Sessions auflisten, Bildschirm lesen und
Text senden tragen einen echten Mehragenten-Arbeitsablauf. Die folgenden vier
Punkte sind das, worüber ich gestolpert bin, nach Schwere geordnet.

---

## 1. `wait_for_events` liefert nie Ereignisse (blockierend für den Nutzen)

**Erwartet:** Nach `subscribe_events` melden die beobachteten Sessions
`done`/`question`/`error`/`info`, sodass der Controller aufwacht, statt zu
pollen. Genau dafür ist der Kanal laut Werkzeugbeschreibung da.

**Beobachtet:** Es kommt nie etwas an. `nextSeq` bleibt auf `0`.

**Ablauf zum Nachstellen:**

1. `subscribe_events` mit `{sessionId: 7, sources: [8, 9]}` → Antwort `ok`
2. Den beiden Worker-Sessions per `send_text` eine Aufgabe geben
3. Die Worker arbeiten mehrere Minuten und schließen ihre Aufgaben ab
   (sichtbar per `read_screen`: fertiger Bericht, Eingabeaufforderung wieder frei)
4. `wait_for_events` mit `{sessionId: 7, timeoutMs: 55000}`

**Ergebnis:** `{"events": [], "nextSeq": 0}` nach voller Wartezeit — obwohl in
diesem Zeitraum beide Worker je eine Aufgabe beendet haben.

**Auswirkung:** Der Controller muss die Bildschirme reihum abfragen. Das
funktioniert, kostet aber deutlich mehr Aufrufe, und der Zeitpunkt „ist fertig"
lässt sich nur raten. Für längere Läufe (hier: 5 bis 11 Minuten je Aufgabe) ist
das der spürbarste Reibungspunkt.

**Vermutung, ungeprüft:** Möglicherweise müssen die Worker-Sessions ihre
Ereignisse aktiv melden (etwa über `post_event` oder einen Haken auf
Worker-Seite), und normale Claude-Code-Sessions tun das von sich aus nicht.
Falls das so gedacht ist, wäre ein Hinweis in der Beschreibung von
`subscribe_events` hilfreich — sie liest sich heute so, als würde der Server
die Ereignisse selbst erzeugen („Abonniert Agenten-Ereignisse … Ohne Filter
werden alle Ereignisse aller anderen Sessions empfangen"). Ein Controller
richtet seinen ganzen Arbeitsablauf danach aus und merkt erst nach der ersten
vollen Zeitüberschreitung, dass nichts kommt.

**Wunsch, falls es am Aufbau liegt:** `subscribe_events` könnte zurückmelden,
wie viele der genannten Quell-Sessions überhaupt ereignisfähig sind — dann wäre
der Fall sofort erkennbar statt erst nach 55 Sekunden Stille.

---

## 2. `send_text` schickt bei langem Text kein Enter ab

**Erwartet:** `send_text` mit `enter` (Voreinstellung `true`) sendet den Text
und schließt mit Enter ab.

**Beobachtet:** Bei kurzem Text (`/clear`) funktioniert es. Bei längerem Text —
hier etwa 300 Zeichen, der im Eingabefeld auf drei Zeilen umbricht — **landet
der Text im Eingabefeld, wird aber nicht abgeschickt.** Der Bildschirm zeigt
ihn am Prompt stehen, die Session bleibt untätig.

**Umgehung, die zuverlässig funktioniert:** ein zweiter Aufruf mit leerem Text
und `enter: true`:

```
send_text {id: 8, text: "<langer Text>"}      → Text steht im Feld, nichts passiert
send_text {id: 8, text: "", enter: true}      → jetzt wird abgeschickt
```

**Auswirkung:** Ohne die Umgehung bleibt eine Arbeitsanweisung stumm im
Eingabefeld stehen. Im Auto-Mode ist das besonders unangenehm: der Controller
denkt, er habe delegiert, und der Worker steht still. Der Fehler ist außerdem
schwer zu bemerken, weil der Aufruf `ok` zurückgibt.

**Vermutung:** Das Enter wird vermutlich zu früh geschickt oder geht im
Zeilenumbruch der Eingabezeile unter — als würde die Oberfläche den Umbruch
selbst noch verarbeiten, wenn der Tastendruck ankommt.

---

## 3. Die Fehlermeldung „Unbekannte ID." führt in die Irre

Alle sitzungsbezogenen Werkzeuge nehmen den Parameter schlicht `id`. Ich hatte
zuerst `sessionId` beziehungsweise `paneId` benutzt — die Namen, die in
`list_sessions` und `get_layout` in den **Antworten** stehen.

Die Antwort war jedes Mal:

```
Unbekannte ID.
```

Das liest sich wie „die Session 8 gibt es nicht", also habe ich an der falschen
Stelle gesucht (Layout geprüft, andere IDs probiert, Session als nicht
angehängt vermutet). Tatsächlich fehlte nur der erwartete Parametername.

**Vorschlag:** Wenn der Pflichtparameter `id` gar nicht vorhanden ist,
entsprechend antworten — etwa „Parameter `id` fehlt (übergeben wurde:
`sessionId`)". Der Unterschied zwischen „ID unbekannt" und „Parameter fehlt"
spart die ganze falsche Fährte.

Am Rande: dass die Antwortfelder `sessionId`/`paneId` heißen, die Eingabe aber
`id`, lädt zu genau diesem Fehler ein.

---

## 4. `get_layout` zeigt nur das aktive Pane (Wunsch, kein Fehler)

`get_layout` lieferte `{"paneId": 5, "sessionId": 7, "active": true}` — nur das
eigene Pane. Sessions 8 und 9 tauchen dort nicht auf, obwohl `list_sessions`
sie kennt.

Für einen Controller wäre eine Übersicht nützlich: welche Session liegt in
welchem Pane, was ist sichtbar, was läuft im Hintergrund. Sonst muss man aus
`list_sessions` und `get_layout` selbst zusammensetzen — und sieht nicht, ob
der Nutzer eine der Worker-Sessions gerade vor Augen hat.

---

## Was gut lief (zur Einordnung)

Damit der Bericht nicht schiefliegt: `list_sessions`, `read_screen` und
`send_text` haben über zweieinhalb Stunden zuverlässig getragen. Zwei Worker
parallel zu führen — jedem eine Arbeitsanweisung geben, Fortschritt am
Bildschirm mitlesen, mitten in der Arbeit eine Präzisierung nachschieben (die
prompt berücksichtigt wurde) — funktioniert mit diesen drei Werkzeugen bereits
sehr gut. `read_screen` mit `lines` ist dabei besonders praktisch, weil man die
Ausgabemenge steuern kann.

Der Ereignis-Kanal wäre das fehlende Stück, um vom Abfragen zum Aufwachen zu
kommen.
