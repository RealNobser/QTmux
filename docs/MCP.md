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
| `create_session` | `type` ("shell"/"serial"), `program?`, `cwd?`, `port?`, `baud?` | Session anlegen → gibt neue **id** zurück |
| `close_session` | `id` | Session schließen |
| `focus_session` | `id` | Session sichtbar/fokussiert machen |
| `send_text` | `id`, `text`, `enter?` (Standard true) | Text in die Session tippen |
| `read_screen` | `id` | Sichtbaren Bildschirm als Klartext lesen |
| `set_theme` | `mode` ("system"/"light"/"dark") | App-Design umschalten |

`activity`: 1=läuft (grün), 2=wartet, 3=Fehler (rot), 4=geschlossen.
`type`: 0=Shell, 1=SSH, 2=Seriell, 3=App.

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
