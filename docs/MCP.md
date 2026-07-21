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
| `list_sessions` | – | Alle Sessions (id, title, type, activity, agentId, needsAttention, lastNotification) |
| `create_session` | `type` ("shell"/"serial"/"ssh"), `program?`, `cwd?`, `port?`, `baud?`, `host?`, `user?`, `identity?` | Session anlegen → gibt neue **id** zurück |
| `close_session` | `id` | Session schließen |
| `focus_session` | `id` | Session sichtbar/fokussiert machen |
| `send_text` | `id`, `text`, `enter?` (Standard true), `enterDelayMs?` (Standard 60) | Text in die Session tippen; Enter geht **kurz danach** raus (s. u.) |
| `read_screen` | `id` | Sichtbaren Bildschirm als Klartext lesen |
| `attach_controller` | `id` | Markiert die Session als steuernde **MCP-Controller**-Session (roter Tab) |
| `set_theme` | `mode` ("system"/"light"/"dark") | App-Design umschalten |
| `list_shells` | – | Verfügbare Shells (`{program, name}`) für `create_session type=shell` |
| `list_serial_ports` | – | Verfügbare serielle Ports für `create_session type=serial` |
| `list_plugins` | – | Backend-Typen geladener Plugins (`{pluginId, typeId, name, description}`) |
| `subscribe_events` | `sessionId?`, `sources?` (int[]), `kinds?` (string[]) | Agenten-Ereignisse abonnieren (leer = alle Quellen/Arten) |
| `unsubscribe_events` | `sessionId?` | Abo dieser Session aufheben |
| `list_subscriptions` | – | Aktive Abos (`subscriberSessionId`, `sources`, `kinds`) |
| `post_event` | `kind`, `text?`, `sessionId?` | Ereignis dieser Session melden (fertig/Frage/Fehler) |
| `wait_for_events` | `sessionId?`, `afterSeq?`, `timeoutMs?` | **Long-Poll**: blockiert bis ein abonniertes Ereignis vorliegt/Timeout |
| `get_layout` | – | `{layout, activePaneId, sessions}` — Baum **plus** Pane-Zuordnung aller Sessions (s. u.) |
| `split_pane` | `orientation` ("h"/"v") | Aktives Pane teilen (neue Shell-Session im neuen Pane, wird aktiv) → neue **Session-id** |
| `close_pane` | `paneId?` | Pane **mitsamt Session** schließen (GUI-Semantik); ohne `paneId` das aktive Pane |
| `assign_session` | `id`, `paneId?` | Session in ein Pane laden (ohne `paneId` ins aktive — wie ein Sidebar-Klick) |
| `list_profiles` | – | Gespeicherte Verbindungsprofile; **ohne Geheimniswerte** (nur `hasPasswordSecret`/`hasLoginScript`-Flags) |
| `connect_profile` | `name` | Profil verbinden — ein Vault-Passwort wird **intern** aufgelöst (nie über MCP ausgegeben) → neue **Session-id** |

`activity`: 1=läuft (grün), 2=wartet, 3=Fehler (rot), 4=geschlossen.
`type`: 0=Shell, 1=SSH, 2=Seriell, 3=App.
`list_sessions` liefert zusätzlich `mcpController` (true = roter Controller-Tab) sowie —
falls die Session bereits ein Agenten-Ereignis erzeugt hat — `lastAgentEventKind`,
`lastAgentEventText`, `lastAgentEventSeq`.

## Sessions steuern: zwei Fallen, die Erfolg melden

### `send_text` — das Enter geht abgesetzt raus (QTMUX-31)

TUI-Anwendungen (belegt mit Claude Code) werten einen Byteblock, der **in einem Rutsch**
ankommt, als Einfügevorgang. Ein darin enthaltenes `\r` wird dann zum Zeilenumbruch *im
Eingabefeld* statt zum Absenden: bei kurzem Text (`/clear`) unauffällig, ab etwa
Feldbreite blieb die Arbeitsanweisung stumm stehen — und der Aufruf meldete `ok`.
Deshalb schreibt QTmux erst den Text und schickt das Enter **60 ms später** als eigenen
Tastendruck hinterher. Bei besonders trägen Oberflächen `enterDelayMs` erhöhen;
`enterDelayMs: 0` stellt das alte Verhalten (alles in einem Block) wieder her.

### `get_layout` — Baum **und** unsichtbare Sessions (QTMUX-33)

Der Baum allein beantwortet die Frage eines Controllers nicht: Sessions, die in *keinem*
Pane liegen, laufen weiter, sind aber nicht zu sehen. Die Antwort umfasst daher:

```jsonc
{
  "layout": { "orientation": "h", "children": [ {"paneId":1,"sessionId":5,"active":false},
                                                {"paneId":2,"sessionId":6,"active":true} ] },
  "activePaneId": 2,
  "sessions": [ {"sessionId":5,"title":"Zsh","paneId":1,"visible":true,"active":false},
                {"sessionId":6,"title":"claude","paneId":2,"visible":true,"active":true},
                {"sessionId":7,"title":"Zsh","paneId":null,"visible":false,"active":false} ]
}
```

`paneId: null` / `visible: false` heißt: läuft, ist aber gerade nicht sichtbar (nur in der
Seitenleiste). Ist das Fenster ungeteilt, besteht `layout` erwartungsgemäß aus einem
einzigen Blatt — das ist kein Fehler.

### Parameternamen: Eingabe ist immer `id` (QTMUX-32)

Alle sitzungsbezogenen Werkzeuge erwarten **`id`** — auch wenn die *Antwortfelder* von
`list_sessions` und `get_layout` `sessionId` bzw. `paneId` heißen. Wer die Antwortnamen
übernimmt, bekommt jetzt Klartext (`Parameter 'id' fehlt (übergeben wurde 'sessionId')`)
statt der irreführenden Meldung „Unbekannte ID.". Ist die ID vorhanden, aber unbekannt,
nennt die Antwort die tatsächlich vorhandenen IDs.

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
  die mitgelieferten `shell-integration/qtmux-emit.{sh,ps1}`. **Nur dieser Weg trägt in
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
Zustellen. Dafür liegt **`shell-integration/qtmux-wait.sh`** bei (Windows:
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

### Worker ereignisfähig machen (Stop-/Notification-Hook)

**Wichtig:** Aus einem KI-Agenten-**Hook** muss `post_event` (HTTP) verwendet werden, **nicht**
die OSC-Variante `qtmux-event` — der **stdout eines Hooks wird vom Agenten gekapselt** und
erreicht das Terminal nicht, eine OSC-Sequenz käme also nicht bei QTmux an. Der HTTP-Aufruf
geht out-of-band und funktioniert immer.

Dafür liegen fertige Helfer bei: **`shell-integration/qtmux-emit.sh`** (macOS/Linux) und
**`qtmux-emit.ps1`** (Windows). Beide lesen `$QTMUX_SESSION_ID` (steckt in jeder
QTmux-Shell und wird an den Agenten samt Hook-Subprozess vererbt) sowie `$QTMUX_MCP_PORT`.

```jsonc
// ~/.claude/settings.json  (Worker-Agent meldet „fertig" bei jedem Stop)
"hooks": {
  "Stop": [ { "hooks": [ { "type": "command",
    "command": "/Pfad/zu/QTmux/shell-integration/qtmux-emit.sh done \"Aufgabe erledigt\"" } ] } ],
  "Notification": [ { "hooks": [ { "type": "command",
    "command": "/Pfad/zu/QTmux/shell-integration/qtmux-emit.sh question \"Rückfrage offen\"" } ] } ]
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
