# QTmux MCP-Schnittstelle

QTmux bringt einen eingebetteten **MCP-Server** (Model Context Protocol) mit, über den ein
externer KI-Agent die Anwendung fernsteuern kann — inklusive der einzelnen Sessions.

## Transport & Sicherheit

- **HTTP / JSON-RPC 2.0** (MCP „Streamable HTTP"), Endpoint: `http://127.0.0.1:7345/mcp`
- Bindet **ausschließlich an `127.0.0.1`** (nur lokale Prozesse) — das ist die Sicherheitsgrenze.
- An/aus über das Menü **Agent-Steuerung → MCP-Server**. Standard: an, Port 7345.

### Port und zweite Instanz

Der Port ist konfigurierbar: **`QTMUX_MCP_PORT`** (Umgebungsvariable) vor der Einstellung
`mcp/port`, sonst 7345. Zusammen mit **`QTMUX_PROFILE`** — das hängt einen Suffix an den
App-Namen und trennt damit die gesamte Einstellungs-Domain (Session-Liste, Profile,
Hotkeys, Vault) — lässt sich eine **zweite Instanz zum Testen** starten, ohne der
produktiven in die Quere zu kommen:

```bash
QTMUX_PROFILE=test QTMUX_MCP_PORT=7346 ./qtmux.app/Contents/MacOS/qtmux
```

> ⚠️ Wer die MCP-Schicht testet, während eine **produktive** Instanz Arbeitssitzungen
> steuert: Deren Terminal-Sessions überleben einen Neustart der Anwendung **nicht**.
> Vor jedem Neustart oder Rebuild prüfen, was auf dem Port hört (`lsof -nP -iTCP:7345
> -sTCP:LISTEN`) — und **nicht in das Build-Verzeichnis bauen, aus dem die laufende
> Instanz gestartet wurde** (das Überschreiben des Binaries reißt sie mit).

## Tools

| Tool | Argumente | Zweck |
|---|---|---|
| `list_sessions` | – | Alle Sessions (id, title, type, activity, agentId, needsAttention, lastNotification, workingDir, progress*) |
| `create_session` | `type` ("shell"/"serial"/"ssh"/"plugin"), `program?`, `cwd?`, `port?`, `baud?`, `host?`, `user?`, `identity?`, `pluginId?`, `typeId?`, `loginScript?` | Session in einem **neuen Window** (Tab) anlegen → gibt neue **id** zurück (Pane *im* aktiven Window: `split_pane`) |
| `close_session` | `id` | Session schließen |
| `set_session_group` | `id`, `group?` | Das **Window** dieser Session einer Sidebar-Gruppe zuordnen; leerer/fehlender `group` nimmt es heraus (Window-Modell) |
| `set_window_group` | `windowId`, `group?` | Ein **Window** (Tab) direkt einer Sidebar-Gruppe zuordnen (leer = ohne Gruppe) |
| `rename_group` | `from`, `to?` | Bestehende **Window-Gruppe** umbenennen (alle Mitglieder) bzw. **auflösen** (leeres `to`) |
| `move_group` | `name`, `direction` | Ganze **Window-Gruppe** als Block in der Sidebar verschieben (`direction`: `up`/`down`) |
| `focus_session` | `id` | **Window** aktivieren, in dem die Session als Pane liegt (Window-Modell) |
| `send_text` | `id`, `text`, `enter?` (Standard true), `enterDelayMs?` (Standard 60), `broadcast?` | Text in die Session tippen; Enter geht **kurz danach** raus (s. u.). Mit `broadcast:true` an **alle** Sessions (`id` entfällt) |
| `read_screen` | `id`, `scrollback?` | Sichtbaren Bildschirm als Klartext lesen; mit `scrollback:true` zusätzlich die Historie davor |
| `attach_controller` | `id` | Markiert die Session als steuernde **MCP-Controller**-Session (roter Tab) |
| `set_theme` | `mode` ("system"/"light"/"dark") | App-Design umschalten |
| `list_shells` | – | Verfügbare Shells (`{program, name}`) für `create_session type=shell` |
| `list_serial_ports` | – | Verfügbare serielle Ports für `create_session type=serial` |
| `list_plugins` | – | Backend-Typen geladener Plugins (`{pluginId, typeId, name, description}`) |
| `subscribe_events` | `sessionId?`, `sources?` (int[]), `kinds?` (string[]) | Agenten-Ereignisse abonnieren (leer = alle Quellen/Arten) |
| `unsubscribe_events` | `sessionId?` | Abo dieser Session aufheben |
| `list_subscriptions` | – | Aktive Abos (`subscriberSessionId`, `sources`, `kinds`) |
| `post_event` | `kind`, `text?`, `sessionId?` | Ereignis dieser Session melden (fertig/Frage/Fehler/Info); **question/error** lassen die Kachel pulsen |
| `needs_attention` | `text?`, `sessionId?` | Explizit Aufmerksamkeit anfordern (Kachel pulst) — für „blockiert/brauche Mensch" |
| `clear_attention` | `sessionId?` | Aufmerksamkeits-Markierung wieder löschen |
| `set_activity` | `state`, `sessionId?` | Dauer-Zustand für den Sidebar-Ring: `idle`(dim)/`busy`(grün)/`waiting`(amber)/`error`(rot) |
| `set_agent_session` | `ref`, `sessionId?` | Eigene Unterhaltungs-Kennung melden, damit QTmux dich beim nächsten Start damit fortsetzen kann (s. u.) |
| `wait_for_events` | `sessionId?`, `afterSeq?`, `timeoutMs?` | **Long-Poll**: blockiert bis ein abonniertes Ereignis vorliegt/Timeout |
| `get_layout` | `windowId?` | `{layout, windowId, activePaneId, sessions}` — Baum des **aktiven** (oder per `windowId` gewählten) Windows plus Pane-Zuordnung (s. u.) |
| `split_pane` | `orientation` ("h"/"v") | Aktives Pane **im aktiven Window** teilen (neue Shell im neuen Pane, wird aktiv) → neue **Session-id** |
| `close_pane` | `paneId?` | Pane **mitsamt Session** schließen (GUI-Semantik); ohne `paneId` das aktive Pane |
| `focus_pane` | `paneId` | Bestehendes Pane **aktiv** setzen (reiner Fokuswechsel, ohne die Session zu ändern) |
| `zoom_pane` | `paneId?` | Pane maximieren („zoomen"); ohne/`-1` = Zoom aufheben |
| `list_windows` | – | Alle **Windows** (Tabs): `{windowId, title, group, paneCount, active, sessionIds}` |
| `focus_window` | `windowId` | Window aktivieren (ganzes Layout umschalten) |
| `new_window` | – | Neues Window (Tab) mit einer Shell → gibt neue **Session-id** zurück |
| `rename_window` | `windowId`, `name?` | Window umbenennen (leerer `name` = automatischer Titel) |
| `close_window` | `windowId` | Window **samt aller** seiner Sessions/Panes schließen |
| `assign_session` | `id`, `paneId?` | **VERALTET** (Window-Modell): kein „Session in Pane laden" mehr → nutze `focus_session`/`focus_window` |
| `list_profiles` | – | Gespeicherte Verbindungsprofile; **ohne Geheimniswerte** (nur `hasPasswordSecret`/`hasLoginScript`-Flags) |
| `connect_profile` | `name` | Profil verbinden — ein Vault-Passwort wird **intern** aufgelöst (nie über MCP ausgegeben) → neue **Session-id** |

`activity` (Sidebar-Ring): 0=untätig (dim), 1=läuft/beschäftigt (grün), 2=wartet (amber),
3=Fehler (rot), 4=geschlossen (grau). Setzbar vom Agenten über `set_activity`; bei Shells
mit OSC-133-Integration automatisch (Prompt=untätig, Kommando=läuft, Exit≠0=Fehler).
`type`: 0=Shell, 1=SSH, 2=Seriell, 3=App.
`list_sessions` liefert zusätzlich `mcpController` (true = roter Controller-Tab), `group`
(Sidebar-Gruppe, leer = ohne) sowie — falls die Session bereits ein Agenten-Ereignis
erzeugt hat — `lastAgentEventKind`, `lastAgentEventText`, `lastAgentEventSeq`.

### `set_session_group` — zusammengehörige Worker sichtbar machen (QTMUX-42)

Wer mehrere Worker parallel fahren lässt, sieht in der Sidebar sonst nur eine flache
Liste gleich aussehender Shells. Eine Gruppe fasst die Sessions einer Aufgabe zusammen:
Sie stehen unter einer benannten, einklappbaren Kopfzeile beieinander, sind **eingerückt**
und tragen links eine gemeinsame Farbmarke (QTMUX-45 — der Einzug macht die Zuordnung
unabhängig von der Farbe erkennbar und bleibt auch dann sichtbar, wenn die Session als
MCP-Controller den roten Tab trägt). Der Controller kann seine Worker also beim Anlegen
sofort einsortieren:

```jsonc
{"name": "set_session_group", "arguments": {"id": 7, "group": "Migration Auth"}}
{"name": "set_session_group", "arguments": {"id": 8, "group": "Migration Auth"}}
{"name": "set_session_group", "arguments": {"id": 7, "group": ""}}   // wieder heraus
```

Der Gruppenname ist frei wählbar; er wird mit der Sitzungsliste **persistiert** und
überlebt einen Neustart. Das Zuordnen **sortiert die Sidebar um** (die Mitglieder einer
Gruppe müssen zusammenhängen) — eine Session kann dadurch ihre Zeile wechseln, ihre
`id` bleibt.

### `set_agent_session` — deine Unterhaltung über einen Neustart retten (QTMUX-98)

QTmux kann deine Sitzungen wiederherstellen: Es merkt sich die getippte Agenten-Kommandozeile
je Pane und setzt sie beim nächsten Start erneut ab. **Welche Unterhaltung** dabei fortgesetzt
wird, hängt an einer Einstellung des Anwenders — und der genaueste Weg braucht dich:

```jsonc
{"name":"set_agent_session","arguments":{"ref":"<deine Unterhaltungs-ID>"}}
```

QTmux legt `ref` an deinem Pane ab, persistiert sie und startet dich beim nächsten Mal mit
genau dieser Unterhaltung (z. B. `claude --resume <ref>`, `opencode --session <ref>`).

**Warum du das melden musst und QTmux es nicht selbst herausfindet** — alle vier naheliegenden
Wege wurden gemessen und scheitern: Ein Server kann seinen Client nicht anrufen, und ein
beschäftigter Agent pollt nicht; in die PTY zu tippen landet in deinem Eingabefeld und die
Antwort wäre Bildschirm-Raterei; deine Verlaufsdatei hältst du nicht offen (`lsof` findet
nichts); und deine Session-Variable setzt du erst zur **Laufzeit**, sie ist von außen am
Prozess nicht sichtbar. QTmux leitet deshalb nichts ab — du meldest, genau wie bei den
Ereignissen.

⚠️ **Nach jedem Wechsel erneut melden.** `/resume` und `/clear` ändern die Kennung mitten im
Betrieb; ohne neue Meldung zeigt QTmux auf die alte Unterhaltung. Am besten beim Start und
nach jedem Wechsel. Ein leerer `ref` löscht die Zuordnung.

Ohne Meldung ist das kein Fehler: Der Anwender kann stattdessen „jüngste Unterhaltung im
Verzeichnis" oder „Auswahl beim Start" wählen — dann startest du eben ohne diese Angabe.

## Sessions steuern: zwei Fallen, die Erfolg melden

### `send_text` — das Enter geht abgesetzt raus (QTMUX-31)

TUI-Anwendungen (belegt mit Claude Code) werten einen Byteblock, der **in einem Rutsch**
ankommt, als Einfügevorgang. Ein darin enthaltenes `\r` wird dann zum Zeilenumbruch *im
Eingabefeld* statt zum Absenden: bei kurzem Text (`/clear`) unauffällig, ab etwa
Feldbreite blieb die Arbeitsanweisung stumm stehen — und der Aufruf meldete `ok`.
Deshalb schreibt QTmux erst den Text und schickt das Enter **60 ms später** als eigenen
Tastendruck hinterher. Bei besonders trägen Oberflächen `enterDelayMs` erhöhen;
`enterDelayMs: 0` stellt das alte Verhalten (alles in einem Block) wieder her.

### Windows (Tabs) — das Bedienmodell (QTMUX-83)

Die Sidebar listet **Windows** (Tabs); **jedes Window hat sein eigenes Split-Layout**.
`create_session`/`new_window` öffnen ein neues Window, `split_pane` erzeugt ein Pane *im*
aktiven Window, `focus_window`/`focus_session` schalten das ganze Layout um. Sessions
bleiben stabil per **Session-`id`** adressierbar (`send_text`/`read_screen`/…), egal in
welchem Window sie liegen. `list_windows` gibt die Übersicht, `get_layout` den Baum eines
Windows (Standard: das aktive; `windowId` wählt ein anderes).

### `get_layout` — Baum **und** unsichtbare Sessions (QTMUX-33)

Der Baum allein beantwortet die Frage eines Controllers nicht: Sessions in **anderen**
Windows laufen weiter, sind im abgefragten Window aber nicht zu sehen. Die Antwort umfasst
daher zusätzlich die `windowId` und die Sitzungsübersicht:

```jsonc
{
  "layout": { "orientation": "h", "children": [ {"paneId":1,"sessionId":5,"active":false},
                                                {"paneId":2,"sessionId":6,"active":true} ] },
  "windowId": 2,
  "activePaneId": 2,
  "sessions": [ {"sessionId":5,"title":"Zsh","windowId":2,"paneId":1,"visible":true,"active":false},
                {"sessionId":6,"title":"claude","windowId":2,"paneId":2,"visible":true,"active":true},
                {"sessionId":7,"title":"Zsh","windowId":3,"paneId":null,"visible":false,"active":false} ]
}
```

`paneId: null` / `visible: false` heißt: läuft in einem **anderen** Window (`windowId`).
Ist das Window ungeteilt, besteht `layout` erwartungsgemäß aus einem einzigen Blatt — das
ist kein Fehler.

### Parameternamen: Eingabe ist immer `id` (QTMUX-32)

Alle sitzungsbezogenen Werkzeuge erwarten **`id`** — auch wenn die *Antwortfelder* von
`get_layout` `sessionId` bzw. `paneId` heißen. Wer die Antwortnamen übernimmt, bekommt
Klartext (`Parameter 'id' fehlt (übergeben wurde 'sessionId')`) statt der irreführenden
Meldung „Unbekannte ID.". Ist die ID vorhanden, aber unbekannt, nennt die Antwort die
tatsächlich vorhandenen IDs.

⚠️ **Antwortformen sind nicht einheitlich** — beim Schreiben eines Test-Harness zweimal
hineingelaufen (2026-07-31): `list_sessions` liefert ein **flaches Array**, dessen Feld
**`id`** heißt (nicht `sessionId` — das ist der Name im `get_layout`-Baum und der Name
eines *Eingabe*-Parameters der Controller-Werkzeuge). `create_session` antwortet mit der
**nackten Zahl**, nicht mit einem JSON-Objekt. Und: Werkzeug-Fehler kommen nach
MCP-Konvention als `result.isError` zurück, **nicht** als JSON-RPC-`error` — ein Skript,
das nur `error` prüft, läuft stumm weiter und misst nichts (hier: alle `send_text`-Aufrufe
liefen mit `id = 0` ins Leere, und die Messung zeigte plausible, aber leere Ergebnisse).

## Inter-Agenten-Benachrichtigung (wer ist fertig / hat eine Frage?)

> **Das Wichtigste zuerst (QTMUX-30):** Der Ereignis-Kanal transportiert **nur, was eine
> Quell-Session selbst meldet.** QTmux leitet **nichts** aus Bildschirminhalt oder
> Prozesszustand ab — ein Claude-Code-Worker, in dem kein Hook eingerichtet ist, erzeugt
> also nie ein Ereignis, egal wie viele Aufgaben er abschließt. Wer seinen Arbeitsablauf
> aufs Aufwachen ausrichtet, wartet sonst vergeblich. `subscribe_events` meldet deshalb je
> Quelle, ob sie bisher **je** ein Ereignis gesendet hat; steht `sourcesWithEventsSoFar`
> auf `0`, erst die Quellseite einrichten (nächster Abschnitt).
>
> **Das ist nur die halbe Bedingung.** Die andere: Der Empfänger muss im Moment der
> Meldung auch zuhören. Ein arbeitender KI-Agent tut das nicht — siehe
> [Empfangen als KI-Agent](#empfangen-als-ki-agent-der-wichtigste-abschnitt).

Ein Agent in Session A meldet „fertig" oder „Frage"; ein Agent in Session B wird
benachrichtigt und erhält **A's Session-ID**, um dort per `send_text`/`read_screen`/
`focus_session` weiterzuarbeiten (Supervisor-/Peer-Muster).

**Ereignis erzeugen** — zwei Wege, die sich NICHT ersetzen:
- **Aus einem Agenten-Hook: `post_event` (HTTP)** — `post_event {kind, text, sessionId?}`
  (Quelle = `$QTMUX_SESSION_ID` bzw. der Prozess-Heuristik-Fallback), am bequemsten über
  die mitgelieferten `qtmux-emit.{sh,ps1}` (Beschaffung: `--install-shell-integration`, s. u.). **Nur dieser Weg trägt in
  Hooks** (Begründung unten: der stdout eines Hooks erreicht das Terminal nicht).
- **Aus der interaktiven Shell: OSC** — `qtmux-event done|question|error|info "Text"`
  (aus `shell-integration/qtmux.{bash,zsh,ps1}`). Für Tools, die in ihr eigenes TTY
  schreiben; **nicht** für Agenten-Hooks geeignet.

**Ereignisse empfangen** (der benachrichtigte Agent, selbst MCP-Client):
1. Einmalig `subscribe_events {sessionId, kinds?, sources?}` — ohne Filter alle Ereignisse
   aller *anderen* Sessions (eigene Ereignisse werden nie zugestellt).
2. In einer Schleife `wait_for_events {sessionId, afterSeq, timeoutMs?}` (Long-Poll). Der
   Aufruf kehrt zurück, sobald ein Ereignis vorliegt, sonst nach `timeoutMs` (Default 25 s)
   leer. Antwort: `{events:[{sourceSessionId, kind, text, timestamp, seq}], nextSeq}`.
   `nextSeq` beim nächsten Aufruf als `afterSeq` übergeben → keine Lücken/Doppel.

> **Diese Schleife setzt einen Client voraus, der auch wirklich schleift** — ein Skript,
> einen Daemon. Ein KI-Agent ist das nicht; für ihn gilt der Abschnitt
> [Empfangen als KI-Agent](#empfangen-als-ki-agent-der-wichtigste-abschnitt).

**Der erste `afterSeq`: `list_sessions` → `lastAgentEventSeq`.** Ohne `afterSeq` wartet
`wait_for_events` **nur ab jetzt** und verschweigt damit alles, was zwischen zwei Abfragen
anfiel — also genau die Ereignisse, die man verpasst hat, während man beschäftigt war.
`list_sessions` nennt je Session `lastAgentEventSeq`; der höchste dieser Werte ist der
Stand, den man bereits gesehen hat, und damit der saubere Einstiegs-Cursor. Danach immer
mit dem `nextSeq` der letzten Antwort weiterpollen. (Ein `nextSeq: 0` heißt **nicht**
„Kanal kaputt", sondern „bislang kein Ereignis im Puffer".)

```bash
U=http://127.0.0.1:7345/mcp
SID=$QTMUX_SESSION_ID    # eigene Session

# abonnieren: alle „fertig"/„Frage"-Ereignisse der anderen Sessions
curl -s -X POST $U -d "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"tools/call\",
  \"params\":{\"name\":\"subscribe_events\",\"arguments\":{\"sessionId\":$SID,
  \"kinds\":[\"done\",\"question\"]}}}"

# warten (blockiert bis Ereignis/Timeout); liefert sourceSessionId zum Weiterarbeiten
curl -s -X POST $U -d "{\"jsonrpc\":\"2.0\",\"id\":2,\"method\":\"tools/call\",
  \"params\":{\"name\":\"wait_for_events\",\"arguments\":{\"sessionId\":$SID,\"afterSeq\":0}}}"
```

Hinweise: Abos sind **laufzeit-flüchtig** (Session-IDs sind nur zur Laufzeit eindeutig);
`afterSeq` ist ein laufzeit-relativer Cursor und nicht über App-Neustarts hinweg gültig.

### Empfangen als KI-Agent (der wichtigste Abschnitt)

> **Auch mit eingerichtetem Stop-Hook erreicht eine Meldung einen *beschäftigten*
> Controller nicht.** `wait_for_events` ist ein **Abholen**; es wirkt nur, *während* der
> Empfänger darin wartet. Ein Agent, der arbeitet, wartet nicht — er führt einen Zug aus
> und ruft Werkzeuge nur, wenn er sich dafür entscheidet. In einen laufenden Zug kann ein
> MCP-Server nicht hineinreichen. Der eingerichtete Hook ist also nur die **halbe**
> Bedingung; die andere Hälfte ist, dass jemand zuhört.

Gemessen am laufenden System (2026-07-21): Zwei Worker meldeten `done`, die Ereignisse
lagen mit Sequenz 32 und 41 im Server — der Controller bemerkte über eine halbe Stunde
nichts davon, weil er in dieser Zeit baute. Kein Fehler im Kanal, sondern ein falsches
Modell vom Empfangen. Und der Fehlerfall sieht aus wie „gerade passiert nichts" — dieselbe
Klasse stiller Fehlfunktion wie bei QTMUX-30.

**Die Lösung ist ein Hintergrundprozess**, der stellvertretend wartet und **endet**, sobald
etwas vorliegt: Das Ende eines Hintergrundbefehls ist die eine Stelle, an der die
Agenten-Umgebung einen arbeitenden Agenten von außen weckt. Damit wird aus dem Abholen ein
Zustellen. Dafür liegt **`qtmux-wait.sh`** bei (Windows:
`qtmux-wait.ps1` / `qtmux-wait.cmd`) — das Gegenstück zu `qtmux-emit.*`:

```bash
# im HINTERGRUND starten, dann normal weiterarbeiten
qtmux-wait.sh --sessions 2,3 --kinds done,question &
```

Endet mit `QTMUX EVENT seq=<n>` plus dem Ereignis als JSON, sonst nach dem Deckel
(`--max-wait`, Vorgabe ~50 min) mit `QTMUX TIMEOUT seq=<n>`, damit ein vergessener Wächter
nicht ewig läuft. Das `seq=` der Abschlusszeile ist der Cursor für den nächsten Wächter:

```bash
qtmux-wait.sh --after 45 &      # lückenlos dort weiter, wo der letzte aufhörte
```

Das Skript legt bei Bedarf selbst ein Abo an (ohne Abo antwortet `wait_for_events` sofort
mit einem Fehler — ein selbstgebauter Wächter würde daraus eine heiße Schleife machen).
Mit `--sessions`/`--kinds` **ersetzt** es das Abo der Session, ohne Filter lässt es ein
vorhandenes unangetastet.

> **Nimm auch hier das Skript, nicht einen `curl`-Einzeiler.** Drei Fallstricke stecken
> darin, jeder einzelne sorgt für einen Wächter, der stumm nichts meldet:
> `timeoutMs` **muss unter** dem HTTP-Timeout liegen (sonst schneidet der Client den
> Long-Poll ab, bevor der Server antwortet); `nextSeq` muss **immer** fortgeschrieben
> werden, auch wenn nichts Passendes dabei war (sonst pollt der Wächter endlos über
> dieselben herausgefilterten Ereignisse); und ohne Gesamt-Deckel überlebt ein vergessener
> Wächter die Sitzung.

### Die Helfer-Skripte beschaffen (`--install-shell-integration`)

Die Skripte stecken **im Programm** und werden auf Wunsch an einen stabilen Ort geschrieben:

```bash
qtmux --install-shell-integration            # Standardziel, s. u.
qtmux --install-shell-integration /mein/ort  # eigenes Verzeichnis
```

Der Befehl nennt anschließend den Pfad **und die fertige Hook-Zeile zum Kopieren**. Er startet
keine Oberfläche. Standardziel:

| Plattform | Ziel |
|---|---|
| macOS | `~/Library/Application Support/QTmux/shell-integration` |
| Linux | `~/.local/share/QTmux/shell-integration` |
| Windows | `%LOCALAPPDATA%\QTmux\shell-integration` |

> **Warum nicht einfach mitpaketieren?** Für DMG und MSI ginge das — für das **AppImage
> nicht**: Es wird bei jedem Start unter einem anderen `/tmp/.mount_XXXXXX` gemountet, ein
> dorthin zeigender Hook-Eintrag überlebt also keinen Neustart. Aus dem Programm heraus
> geschrieben, liegen die Dateien überall an einem Ort, der bleibt — und passen zwangsläufig
> zur laufenden Version.

**Windows:** QTmux ist zwingend eine GUI-Anwendung (sonst erben die Terminal-Sitzungen die
Konsole) und hat von sich aus kein stdout. Der Befehl hängt sich deshalb an die Konsole des
Aufrufers, sofern dort keine Umleitung vorliegt. Zwei Folgen, beide gemessen: Die Shell
**wartet nicht** auf das Prozessende — der Prompt ist sofort zurück, die Ausgabe erscheint
gleich danach, die Dateien werden vollständig geschrieben. Und PowerShells `>` schließt seine
Zieldatei zu früh (bleibt leer); zum Mitschneiden `cmd /c "… > ausgabe.txt 2>&1"` verwenden,
eine Pipe (`| Out-String`) funktioniert ebenfalls.

Im Repo liegen dieselben Dateien unter `shell-integration/` — für Entwickler ändert sich nichts.

### Worker ereignisfähig machen (Stop-/Notification-Hook)

**Wichtig:** Aus einem KI-Agenten-**Hook** muss `post_event` (HTTP) verwendet werden, **nicht**
die OSC-Variante `qtmux-event` — der **stdout eines Hooks wird vom Agenten gekapselt** und
erreicht das Terminal nicht, eine OSC-Sequenz käme also nicht bei QTmux an. Der HTTP-Aufruf
geht out-of-band und funktioniert immer.

Dafür liegen fertige Helfer bei: **`qtmux-emit.sh`** (macOS/Linux) und
**`qtmux-emit.ps1`** (Windows). Beide lesen `$QTMUX_SESSION_ID` (steckt in jeder
QTmux-Shell und wird an den Agenten samt Hook-Subprozess vererbt) sowie `$QTMUX_MCP_PORT`.

```jsonc
// ~/.claude/settings.json  (Worker-Agent meldet „fertig" bei jedem Stop)
"hooks": {
  "Stop": [ { "hooks": [ { "type": "command",
    "command": "~/Library/Application Support/QTmux/shell-integration/qtmux-emit.sh done \"Aufgabe erledigt\"" } ] } ],
  "Notification": [ { "hooks": [ { "type": "command",
    "command": "~/Library/Application Support/QTmux/shell-integration/qtmux-emit.sh question \"Rückfrage offen\"" } ] } ]
}
```

> **Nimm das Skript, nicht einen `curl`-Einzeiler im Hook.** Der Aufruf braucht sonst eine
> dreifach verschachtelte Maskierung (JSON im JSON in der Shell); geht dabei etwas schief,
> feuert der Hook **still** nicht — und das sieht für den Controller exakt so aus wie
> „gerade passiert nichts". Genau diese Sorte Stille steckte hinter QTMUX-30.

**Prüfen, ob es trägt:** nach dem Einrichten `subscribe_events` aufrufen — sobald der
Worker einmal fertig war, steht seine Quelle dort mit `hasPostedEvents: true`.

Die OSC-Variante (`qtmux-event` aus `shell-integration/qtmux.{bash,zsh,ps1}`,
`qtmux-event.cmd`) bleibt für die **interaktive Shell** und Tools, die in ihr eigenes TTY
schreiben.

## Controller-Session markieren (roter Tab)

Startet man **in** einer QTmux-Shell einen Agenten, der sich per MCP verbindet, um die
*anderen* Sessions zu steuern, bekommt diese Session in der Sidebar einen **roten Tab**.

**Automatisch (Standard):** Beim MCP-`initialize` ermittelt QTmux den verbindenden
Client-Prozess (über den TCP-Port → PID) und ordnet ihn anhand seiner **Prozess-Vorfahrenkette**
genau der Session zu, in deren Shell er läuft (Client → … → Shell-PID). Es ist **kein** Setup
im Agenten nötig — der Connector allein genügt. (macOS/Linux; das Lesen fremder Umgebungen ist
auf aktuellem macOS gesperrt, daher die Zuordnung über die Prozesshierarchie.)

**Manuell (Fallback):** Jede Shell-Session erhält die Umgebungsvariable **`QTMUX_SESSION_ID`**.
Ein Agent kann sich auch explizit anmelden:

```bash
# innerhalb der Agenten-Session:
curl -s -X POST http://127.0.0.1:7345/mcp -d \
  "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"tools/call\",
    \"params\":{\"name\":\"attach_controller\",\"arguments\":{\"id\":$QTMUX_SESSION_ID}}}"
```

Die Markierung gilt für die **Lebensdauer der Session** (nicht persistiert).

## Prozess-Cleanup beim Beenden

Beim Schließen der Anwendung werden alle Sessions sauber beendet: QTmux erfasst je Session
den **gesamten Prozessbaum** (Shell + Kinder, z. B. ein laufender Agent) und beendet ihn
(SIGHUP, dann SIGKILL). So bleiben keine verwaisten Prozesse zurück — auch nicht solche, die
das PTY-Hangup ignorieren (`nohup` o. ä.).

## Beispiel (curl)

```bash
U=http://127.0.0.1:7345/mcp

# Handshake
curl -s -X POST $U -d '{"jsonrpc":"2.0","id":1,"method":"initialize","params":{}}'

# Sessions auflisten
curl -s -X POST $U -d '{"jsonrpc":"2.0","id":2,"method":"tools/call",
  "params":{"name":"list_sessions","arguments":{}}}'

# In Session 1 ein Kommando ausführen
curl -s -X POST $U -d '{"jsonrpc":"2.0","id":3,"method":"tools/call",
  "params":{"name":"send_text","arguments":{"id":1,"text":"ls -la","enter":true}}}'

# Ergebnis vom Schirm lesen
curl -s -X POST $U -d '{"jsonrpc":"2.0","id":4,"method":"tools/call",
  "params":{"name":"read_screen","arguments":{"id":1}}}'
```

## Anbindung an einen MCP-Client

Clients mit „Streamable HTTP"-Transport verbinden sich direkt auf die URL oben.
Für stdio-basierte MCP-Hosts (z. B. manche Desktop-Clients) lässt sich später ein
schlanker stdio↔HTTP-Proxy ergänzen.

## Implementierung

- `src/server/McpServer.{h,cpp}` — QTcpServer + HTTP/JSON-RPC-Parser + Tool-Dispatch.
- Läuft im GUI-Thread; Session-Aufrufe sind dadurch thread-sicher.
- Entkoppelt von Theme/Fokus über Signale (`focusRequested`, `setThemeRequested`),
  die in `qml/Main.qml` mit `window.currentRow` bzw. `Theme.mode` verbunden werden.
