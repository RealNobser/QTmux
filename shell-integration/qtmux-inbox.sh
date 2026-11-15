#!/bin/sh
# QTmux: den NACHRICHTEN-EINGANG dieser Agenten-Sitzung melden — über das MCP-Tool
# 'set_agent_inbox' (HTTP/JSON-RPC an 127.0.0.1). Danach erreicht 'send_message' die
# Sitzung OHNE ihre Eingabezeile: Die Nachricht erscheint als eigene Zeile im Verlauf,
# auch während der Mensch dort tippt oder eine Rückfrage offen ist.
#
# Gedacht als SessionStart-Hook von Claude Code:
#     "SessionStart": [{"hooks": [{"type": "command", "command": "/pfad/zu/qtmux-inbox.sh"}]}]
#
# Woher die Angaben kommen:
#   $CLAUDE_CODE_MESSAGING_SOCKET   Socket der Sitzung — Claude Code gibt ihn jedem Hook mit
#   $QTMUX_SESSION_ID               eigene Session-ID — QTmux gibt sie jeder seiner Shells mit
#   $AI_AGENT                       trägt die Fassung (claude-code_2-1-323_…), optional
# Der Port folgt $QTMUX_MCP_PORT (Vorgabe 7345), wie bei qtmux-emit.sh.
#
# Läuft der Agent NICHT in einer QTmux-Session oder hat er keinen Eingang (ältere Fassung,
# Funktion abgeschaltet), tut das Skript nichts und endet mit 0: Ein Hook darf den Agenten
# nie blockieren oder scheitern lassen. Der Token der Sitzung
# ($CLAUDE_CODE_MESSAGING_TOKEN) wird bewusst NICHT gelesen und nicht übertragen.
#
# Nur macOS/Linux. Unter Windows ist der Eingang eine Named Pipe mit Anmeldepflicht;
# dafür gibt es noch keinen Weg — send_message nimmt dort die Warteschlange.
#
# Bewusst POSIX-sh und ohne jq — wie qtmux-emit.sh, damit der Helfer überall läuft.
PORT="${QTMUX_MCP_PORT:-7345}"
SOCK="${CLAUDE_CODE_MESSAGING_SOCKET:-}"

[ -n "$QTMUX_SESSION_ID" ] || exit 0
[ -n "$SOCK" ] || exit 0
# Nur Ziffern als Session-ID und kein Zeichen im Pfad, das das JSON aufbräche.
case "$QTMUX_SESSION_ID" in *[!0-9]*) exit 0 ;; esac
case "$SOCK" in *'"'*|*'\'*) exit 0 ;; esac

# Fassung des Agenten, soweit er sie dem Hook mitgibt (gemessen an 2.1.323:
# AI_AGENT=claude-code_2-1-323_harness). Nur eine Auskunft für die Antwort von
# send_message — fehlt sie, wird trotzdem gemeldet.
VERSION=""
case "${AI_AGENT:-}" in
    claude-code_*)
        VERSION="${AI_AGENT#claude-code_}"
        VERSION="${VERSION%%_*}"
        VERSION=$(printf '%s' "$VERSION" | tr '-' '.')
        ;;
esac
case "$VERSION" in *[!0-9.]*|"") VERSION="" ;; esac

ARGS="{\"kind\":\"claude-code\",\"address\":\"$SOCK\",\"version\":\"$VERSION\",\"sessionId\":$QTMUX_SESSION_ID}"

curl -s --noproxy '*' --max-time 5 \
     -X POST "http://127.0.0.1:$PORT/mcp" \
     -H 'Content-Type: application/json' \
     -d "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"tools/call\",\"params\":{\"name\":\"set_agent_inbox\",\"arguments\":$ARGS}}" \
     >/dev/null 2>&1

exit 0
