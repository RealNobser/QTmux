# QTmux MCP-Schnittstelle

QTmux bringt einen eingebetteten **MCP-Server** (Model Context Protocol) mit, über den ein
externer KI-Agent die Anwendung fernsteuern kann — inklusive der einzelnen Sessions.

## Transport & Sicherheit

- **HTTP / JSON-RPC 2.0** (MCP „Streamable HTTP"), Endpoint: `http://127.0.0.1:7345/mcp`
- Bindet **ausschließlich an `127.0.0.1`** (nur lokale Prozesse) — das ist die Sicherheitsgrenze.
- An/aus über das Menü **Agent-Steuerung → MCP-Server**. Standard: an, Port 7345.

## Tools

| Tool | Argumente | Zweck |
|---|---|---|
| `list_sessions` | – | Alle Sessions (id, title, type, activity, agentId, needsAttention, lastNotification) |
| `create_session` | `type` ("shell"/"serial"/"ssh"), `program?`, `cwd?`, `port?`, `baud?`, `host?`, `user?`, `identity?` | Session anlegen → gibt neue **id** zurück |
| `close_session` | `id` | Session schließen |
| `focus_session` | `id` | Session sichtbar/fokussiert machen |
| `send_text` | `id`, `text`, `enter?` (Standard true) | Text in die Session tippen |
| `read_screen` | `id` | Sichtbaren Bildschirm als Klartext lesen |
| `attach_controller` | `id` | Markiert die Session als steuernde **MCP-Controller**-Session (roter Tab) |
| `set_theme` | `mode` ("system"/"light"/"dark") | App-Design umschalten |
| `subscribe_events` | `sessionId?`, `sources?` (int[]), `kinds?` (string[]) | Agenten-Ereignisse abonnieren (leer = alle Quellen/Arten) |
| `unsubscribe_events` | `sessionId?` | Abo dieser Session aufheben |
| `list_subscriptions` | – | Aktive Abos (`subscriberSessionId`, `sources`, `kinds`) |
| `post_event` | `kind`, `text?`, `sessionId?` | Ereignis dieser Session melden (fertig/Frage/Fehler) |
| `wait_for_events` | `sessionId?`, `afterSeq?`, `timeoutMs?` | **Long-Poll**: blockiert bis ein abonniertes Ereignis vorliegt/Timeout |

`activity`: 1=läuft (grün), 2=wartet, 3=Fehler (rot), 4=geschlossen.
`type`: 0=Shell, 1=SSH, 2=Seriell, 3=App.
`list_sessions` liefert zusätzlich `mcpController` (true = roter Controller-Tab) sowie —
falls die Session bereits ein Agenten-Ereignis erzeugt hat — `lastAgentEventKind`,
`lastAgentEventText`, `lastAgentEventSeq`.

## Inter-Agenten-Benachrichtigung (wer ist fertig / hat eine Frage?)

Ein Agent in Session A meldet „fertig" oder „Frage"; ein Agent in Session B wird
benachrichtigt und erhält **A's Session-ID**, um dort per `send_text`/`read_screen`/
`focus_session` weiterzuarbeiten (Supervisor-/Peer-Muster).

**Ereignis erzeugen** — zwei gleichwertige Wege:
- **Shell-Hook (OSC):** `qtmux-event done|question|error|info "Text"` (aus
  `shell-integration/qtmux.{bash,zsh}`). Ideal für Agenten-Hooks (z. B. Claude Codes
  **Stop**-Hook → `qtmux-event done`, **Notification**-Hook → `qtmux-event question`).
- **MCP:** `post_event {kind, text, sessionId?}` (Quelle = `$QTMUX_SESSION_ID` bzw. der
  Prozess-Heuristik-Fallback).

**Ereignisse empfangen** (der benachrichtigte Agent, selbst MCP-Client):
1. Einmalig `subscribe_events {sessionId, kinds?, sources?}` — ohne Filter alle Ereignisse
   aller *anderen* Sessions (eigene Ereignisse werden nie zugestellt).
2. In einer Schleife `wait_for_events {sessionId, afterSeq, timeoutMs?}` (Long-Poll). Der
   Aufruf kehrt zurück, sobald ein Ereignis vorliegt, sonst nach `timeoutMs` (Default 25 s)
   leer. Antwort: `{events:[{sourceSessionId, kind, text, timestamp, seq}], nextSeq}`.
   `nextSeq` beim nächsten Aufruf als `afterSeq` übergeben → keine Lücken/Doppel.

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

### Ereignis aus einem Agenten-Hook melden (Stop-/Notification-Hook)

**Wichtig:** Aus einem KI-Agenten-**Hook** muss `post_event` (HTTP) verwendet werden, **nicht**
die OSC-Variante `qtmux-event` — der **stdout eines Hooks wird vom Agenten gekapselt** und
erreicht das Terminal nicht, eine OSC-Sequenz käme also nicht bei QTmux an. Der HTTP-Aufruf
geht out-of-band und funktioniert immer. Fertige Helfer: `shell-integration/qtmux-emit.ps1`
(Windows) bzw. ein `curl`-Einzeiler (macOS/Linux). Beispiel Claude Code `settings.json`:

```jsonc
// ~/.claude/settings.json  (Worker-Agent meldet „fertig" bei jedem Stop)
"hooks": {
  "Stop": [ { "hooks": [ { "type": "command",
    "command": "curl -s -X POST http://127.0.0.1:7345/mcp -d \"{\\\"jsonrpc\\\":\\\"2.0\\\",\\\"id\\\":1,\\\"method\\\":\\\"tools/call\\\",\\\"params\\\":{\\\"name\\\":\\\"post_event\\\",\\\"arguments\\\":{\\\"kind\\\":\\\"done\\\",\\\"text\\\":\\\"Aufgabe erledigt\\\",\\\"sessionId\\\":$QTMUX_SESSION_ID}}}}\"" } ] } ]
}
```

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
