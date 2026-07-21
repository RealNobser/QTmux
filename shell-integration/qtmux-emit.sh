#!/bin/sh
# QTmux: Agenten-Ereignis aus einem HOOK melden — über das MCP-Tool 'post_event'
# (HTTP/JSON-RPC an 127.0.0.1). Dies ist der ZUVERLÄSSIGE Weg für KI-Agenten-Hooks
# (z. B. Claude Codes Stop-/Notification-Hook): deren stdout wird vom Agenten gekapselt,
# eine OSC-Ausgabe (qtmux-event) käme dort NICHT bei QTmux an — ein HTTP-Aufruf schon.
#
# Aufruf (typisch aus einem Hook):
#     qtmux-emit.sh done      "Aufgabe erledigt"
#     qtmux-emit.sh question  "Brauche eine Entscheidung"
#     qtmux-emit.sh error     "Build fehlgeschlagen"
#
# Unix-Pendant zu qtmux-emit.ps1. Bewusst als SKRIPT und nicht als curl-Einzeiler im
# Hook: die dafür nötige dreifache Escape-Verschachtelung (JSON in JSON in Shell) ist
# eine bekannte Fehlerquelle — ein still nicht feuernder Hook sieht exakt so aus wie
# „es passiert gerade nichts" (QTMUX-30).
#
# Die eigene Session-ID kommt aus $QTMUX_SESSION_ID (injiziert in jede QTmux-Shell und
# vom Agenten samt Hook-Subprozess geerbt). Fehlt sie, ordnet der Server über die
# Prozess-Vorfahrenkette zu. Der Port folgt $QTMUX_MCP_PORT (Vorgabe 7345) — wichtig,
# wenn mehrere QTmux-Instanzen laufen.
KIND="${1:-info}"
TEXT="${2:-}"
PORT="${QTMUX_MCP_PORT:-7345}"

# text JSON-sicher machen (Backslash und Anführungszeichen maskieren).
TEXT_ESC=$(printf '%s' "$TEXT" | sed -e 's/\\/\\\\/g' -e 's/"/\\"/g')

if [ -n "$QTMUX_SESSION_ID" ]; then
    ARGS="{\"kind\":\"$KIND\",\"text\":\"$TEXT_ESC\",\"sessionId\":$QTMUX_SESSION_ID}"
else
    ARGS="{\"kind\":\"$KIND\",\"text\":\"$TEXT_ESC\"}"
fi

# --noproxy: unabhängig von einem System-/Konzern-Proxy, der 127.0.0.1 abfangen könnte.
# Fehler werden geschluckt: ein Hook darf den Agenten nie blockieren oder scheitern lassen.
curl -s --noproxy '*' --max-time 5 \
     -X POST "http://127.0.0.1:$PORT/mcp" \
     -H 'Content-Type: application/json' \
     -d "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"tools/call\",\"params\":{\"name\":\"post_event\",\"arguments\":$ARGS}}" \
     >/dev/null 2>&1

exit 0
