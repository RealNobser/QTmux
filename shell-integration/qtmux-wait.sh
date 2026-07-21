#!/bin/sh
# QTmux: auf ein Agenten-Ereignis WARTEN — als Hintergrundprozess, der endet, sobald
# etwas vorliegt. Gegenstück zu qtmux-emit.sh (senden).
#
# Warum das nötig ist: 'wait_for_events' ist ein Abholen (Long-Poll). Ein Ereignis
# erreicht einen Empfänger nur, WÄHREND der in diesem Aufruf wartet. Ein KI-Agent tut
# das praktisch nie: er arbeitet einen Zug ab und ruft Werkzeuge nur, wenn er sich
# dafür entscheidet — und in einen laufenden Zug kann ein MCP-Server nicht hineinreichen.
# Ein Controller, der baut, verpasst deshalb jede Meldung, obwohl der Kanal intakt ist.
# Der Ausweg ist die einzige Stelle, an der ein Agent von außen geweckt werden kann:
# das ENDE eines Hintergrundbefehls. Dieses Skript blockiert also stellvertretend und
# beendet sich beim ersten Treffer — aus dem Abholen wird ein Zustellen.
#
# Aufruf (im Hintergrund starten!):
#     qtmux-wait.sh                                 # alle Quellen, ab jetzt
#     qtmux-wait.sh --after 45                      # lückenlos ab Cursor weiterwarten
#     qtmux-wait.sh --sessions 2,3 --kinds done,question
#     qtmux-wait.sh --max-wait 600                  # Gesamt-Deckel in Sekunden
#
# Ausgabe bei Treffer: eine Kopfzeile 'QTMUX EVENT seq=<n>' plus das Ereignis als JSON,
# Exit 0. Ohne Treffer nach dem Deckel: 'QTMUX TIMEOUT seq=<n>', ebenfalls Exit 0 —
# der Wächter soll nicht als Fehler aussehen, nur weil nichts passiert ist. Exit 2 nur
# bei einem echten Aufsatzfehler (keine Session-ID, Server nicht erreichbar).
#
# Das seq= der Abschlusszeile ist der Cursor für den NÄCHSTEN Wächter: mit --after
# fortgesetzt, entsteht keine Lücke zwischen zwei Wartephasen.
#
# Bewusst POSIX-sh und ohne jq — wie qtmux-emit.sh, damit der Helfer überall läuft.

PORT="${QTMUX_MCP_PORT:-7345}"
URL="http://127.0.0.1:$PORT/mcp"
AFTER=""            # leer = „ab jetzt" (Server-Vorgabe)
SESSIONS=""
KINDS=""
MAX_WAIT=3000       # ~50 min Deckel gegen vergessene Wächter

# Der Server deckelt timeoutMs bei 55 s. Das curl-Timeout MUSS darüber liegen, sonst
# schneidet curl den Long-Poll ab, BEVOR der Server antwortet — der Wächter verlöre
# genau die Ereignisse, auf die er wartet. Deshalb hier abgeleitet statt doppelt gepflegt.
TIMEOUT_MS=45000
CURL_MAX=$(( TIMEOUT_MS / 1000 + 10 ))

while [ $# -gt 0 ]; do
    case "$1" in
        --after)    AFTER="$2";    shift 2 ;;
        --sessions) SESSIONS="$2"; shift 2 ;;
        --kinds)    KINDS="$2";    shift 2 ;;
        --max-wait) MAX_WAIT="$2"; shift 2 ;;
        -h|--help)  sed -n '2,30p' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
        *) echo "qtmux-wait: unbekannte Option '$1'" >&2; exit 2 ;;
    esac
done

SID="$QTMUX_SESSION_ID"
if [ -z "$SID" ]; then
    echo "qtmux-wait: \$QTMUX_SESSION_ID fehlt — in einer QTmux-Shell starten," >&2
    echo "            oder die eigene Session-ID über list_sessions ermitteln." >&2
    exit 2
fi

call() {   # call <toolname> <arguments-json>
    curl -s --noproxy '*' --max-time "$2" \
         -X POST "$URL" -H 'Content-Type: application/json' \
         -d "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"tools/call\",\"params\":{\"name\":\"$1\",\"arguments\":$3}}" \
         2>/dev/null
}

# Kommaliste -> JSON-Array; $2 = 'q' für Zeichenketten (kinds), sonst Zahlen (sources).
# Das '\n' im printf ist Pflicht: ohne abschließendes Zeilenende verwirft 'read' das
# letzte (bei einem einzigen Wert: das einzige) Element — die Liste käme leer heraus,
# und ein leeres Abo bedeutet beim Server „kein Filter", also ALLES. Genau so wachte
# der Wächter im Test bei einem 'info' auf, obwohl nur 'done' abonniert schien.
as_array() {
    printf '['
    printf '%s\n' "$1" | tr ',' '\n' | while IFS= read -r item; do
        [ -z "$item" ] && continue
        if [ "$2" = q ]; then printf '"%s",' "$item"; else printf '%s,' "$item"; fi
    done | sed 's/,$//'
    printf ']'
}

# --- Abo sicherstellen -------------------------------------------------------------
# wait_for_events OHNE Abo antwortet sofort mit einem Fehler — ein Wächter würde dann in
# einer heißen Schleife den Server hämmern statt zu warten. Filter werden serverseitig
# ausgewertet; ein Abo je Subscriber-Session, ein neues ERSETZT das alte. Ohne --sessions/
# --kinds fassen wir ein vorhandenes Abo deshalb nicht an.
SUBS=$(call list_subscriptions 5 '{}')
if [ -z "$SUBS" ]; then
    echo "qtmux-wait: kein Kontakt zum MCP-Server auf $URL" >&2
    exit 2
fi

HAS_SUB=no
printf '%s' "$SUBS" | grep -q "subscriberSessionId\\\\\":$SID\\b" && HAS_SUB=yes

if [ -n "$SESSIONS" ] || [ -n "$KINDS" ] || [ "$HAS_SUB" = no ]; then
    SUB_ARGS="{\"sessionId\":$SID"
    [ -n "$SESSIONS" ] && SUB_ARGS="$SUB_ARGS,\"sources\":$(as_array "$SESSIONS" n)"
    [ -n "$KINDS" ]    && SUB_ARGS="$SUB_ARGS,\"kinds\":$(as_array "$KINDS" q)"
    SUB_ARGS="$SUB_ARGS}"
    call subscribe_events 5 "$SUB_ARGS" >/dev/null
fi

# --- warten ------------------------------------------------------------------------
DEADLINE=$(( $(date +%s) + MAX_WAIT ))

while [ "$(date +%s)" -lt "$DEADLINE" ]; do
    # Den Poll auf die Restzeit kürzen, sonst überzieht der Deckel um bis zu eine
    # volle Poll-Länge (--max-wait 5 wartete so 45 s statt 5).
    REMAIN=$(( DEADLINE - $(date +%s) ))
    POLL_MS=$TIMEOUT_MS
    [ $(( REMAIN * 1000 )) -lt "$POLL_MS" ] && POLL_MS=$(( REMAIN * 1000 ))
    [ "$POLL_MS" -lt 1000 ] && POLL_MS=1000       # Server-Untergrenze
    POLL_CURL=$(( POLL_MS / 1000 + 10 ))

    ARGS="{\"sessionId\":$SID,\"timeoutMs\":$POLL_MS"
    [ -n "$AFTER" ] && ARGS="$ARGS,\"afterSeq\":$AFTER"
    ARGS="$ARGS}"

    RESP=$(call wait_for_events "$POLL_CURL" "$ARGS")
    [ -z "$RESP" ] && continue          # curl-Abbruch/Netzhänger: einfach neu pollen

    # Der Cursor wird IMMER fortgeschrieben — auch wenn nichts Passendes dabei war.
    # Sonst pollt der Wächter endlos über dieselben (herausgefilterten) Ereignisse.
    NEXT=$(printf '%s' "$RESP" | sed -n 's/.*nextSeq\\":\([0-9]*\).*/\1/p')
    [ -n "$NEXT" ] && AFTER="$NEXT"

    # Echte Fehler (kein Abo, unbekannte Session) kommen SOFORT zurück. Ohne diese
    # Prüfung würde daraus eine heiße Schleife statt eines wartenden Wächters.
    if printf '%s' "$RESP" | grep -q 'error\\":'; then
        echo "qtmux-wait: Server meldet einen Fehler:" >&2
        printf '%s\n' "$RESP" >&2
        exit 2
    fi

    # Serverseitig gefiltert: was hier ankommt, passt zum Abo — kein eigenes Sieben nötig.
    if ! printf '%s' "$RESP" | grep -q 'events\\":\[\]'; then
        echo "QTMUX EVENT seq=${AFTER:-0}"
        printf '%s' "$RESP" \
            | sed -e 's/.*"text":"//' -e 's/","type":"text.*//' \
                  -e 's/\\"/"/g' -e 's/\\\\/\\/g'
        echo
        exit 0
    fi
done

echo "QTMUX TIMEOUT seq=${AFTER:-0} — kein Ereignis innerhalb von ${MAX_WAIT}s"
exit 0
