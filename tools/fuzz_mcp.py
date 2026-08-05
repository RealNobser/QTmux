#!/usr/bin/env python3
"""Bösartige MCP-Aufrufe gegen eine ISOLIERTE QTmux-Instanz.

Frage: bringt ein illegaler Aufruf den Prozess um? Jede Anfrage wird geschickt,
danach wird geprüft, ob der Prozess noch lebt — so steht am Ende nicht "es hat
nicht geknallt", sondern WELCHER Aufruf ihn umgebracht hätte.

Übernommen aus RAFTNG/tools/fuzz_rest_api.py (dort fand derselbe Ansatz am
2026-08-06 einen echten Killer: {"path": "/dev/zero"} liess RAFTNG in zwei
Sekunden auf 30,4 GB RSS wachsen und beendete den Prozess ohne Crashreport).
Hier auf JSON-RPC/MCP umgebaut.

🔑 WARUM QTMUX BESONDERS: Der MCP-Endpunkt KANN im LAN stehen (QTMUX-127).
Damit ist die Angriffsfläche grösser als bei einer nur lokal erreichbaren API.
Der Lauf geht deshalb ausdrücklich MIT gültigem Token — ein Token schützt nicht
vor einem bösen Pfad, es lässt ihn nur durch einen authentifizierten Kanal.

⚠️ BETRIEBSWARNUNG: NIEMALS gegen eine Instanz laufen lassen, in der jemand
arbeitet. QTmux hält Terminal-Sessions mit laufenden Prozessen; ein getöteter
Server reisst sie mit. Immer eine eigene Instanz auf eigenem Port:

    QTMUX_PROFILE=fuzz QTMUX_MCP_PORT=7399 \\
      ./build/macos-test/qtmux.app/Contents/MacOS/qtmux &
    python3 tools/fuzz_mcp.py 7399 $!

Neue Fälle gehören in CASES. Ein Fall, der "ok" meldet, ist kein Beweis von
Korrektheit — nur davon, dass der Prozess ihn überlebt hat.
"""
import json
import os
import sys
import time
import urllib.error
import urllib.request

PORT = int(sys.argv[1]) if len(sys.argv) > 1 else 7399
PID = int(sys.argv[2]) if len(sys.argv) > 2 else 0
TOKEN = os.environ.get("QTMUX_MCP_TOKEN", "")
URL = f"http://127.0.0.1:{PORT}/"

# Pfade, die sich selbst falsch beschreiben. /dev/zero und /dev/urandom melden
# size() == 0 und liefern trotzdem unendlich; ein FIFO blockiert für immer, OHNE
# Speicher zu fressen — der sieht wie ein Hänger aus, nicht wie ein Leck.
EVIL_PATHS = [
    "/dev/zero", "/dev/urandom", "/dev/null",
    "/proc/kcore",                       # Linux: riesig gemeldet
    "../../../../etc/passwd",            # Traversal
    "/etc/passwd",
    "",                                  # leer
    "/" + "a" * 4000,                    # sehr lang
    "\x00/dev/zero",                     # eingebettetes NUL
]


def alive(pid):
    if not pid:
        return True
    try:
        os.kill(pid, 0)
        return True
    except OSError:
        return False


def rpc(method, params=None, raw=None, timeout=8):
    """(status, kurzer Text); None-Status = Transportfehler."""
    hdrs = {"Content-Type": "application/json"}
    if TOKEN:
        hdrs["Authorization"] = f"Bearer {TOKEN}"
    if raw is not None:
        data = raw if isinstance(raw, bytes) else raw.encode("utf-8", "replace")
    else:
        body = {"jsonrpc": "2.0", "id": 1, "method": method}
        if params is not None:
            body["params"] = params
        data = json.dumps(body).encode()
    req = urllib.request.Request(URL, data=data, method="POST", headers=hdrs)
    try:
        with urllib.request.urlopen(req, timeout=timeout) as r:
            return r.status, r.read(200).decode("utf-8", "replace")
    except urllib.error.HTTPError as e:
        return e.code, e.read(200).decode("utf-8", "replace")
    except Exception as e:
        return None, f"{type(e).__name__}: {e}"


def call(tool, args):
    return ("tools/call", {"name": tool, "arguments": args}, None)


CASES = [
    # --- Protokoll-Ebene ---
    ("json: abgeschnitten",      None, None, '{"jsonrpc":"2.0","id":'),
    ("json: leer",               None, None, ""),
    ("json: nur Array",          None, None, "[1,2,3]"),
    ("json: null",               None, None, "null"),
    ("json: tief verschachtelt", None, None, "[" * 2000 + "]" * 2000),
    ("json: NaN",                None, None, '{"jsonrpc":"2.0","id":NaN}'),
    ("rpc: Methode unbekannt",   "gibtsnicht", {}, None),
    ("rpc: params als String",   "tools/call", "hallo", None),
    ("rpc: params als Array",    "tools/call", [1, 2], None),
    ("rpc: Tool unbekannt",      *call("gibtsnicht", {})),
    ("rpc: name fehlt",          "tools/call", {"arguments": {}}, None),
    ("rpc: arguments als String", "tools/call", {"name": "list_sessions", "arguments": "x"}, None),
    ("body: 4 MB Muell",         None, None, "x" * (4 * 1024 * 1024)),
    ("body: 8 MB (ueber Deckel)", None, None, "y" * (8 * 1024 * 1024)),

    # --- Pfad-artige Argumente: der Kern dieses Laufs ---
    *[(f"cwd: {p[:28]!r}", *call("create_session", {"cwd": p})) for p in EVIL_PATHS],
    *[(f"identity: {p[:24]!r}", *call("create_session",
        {"type": "ssh", "host": "127.0.0.1", "identity": p})) for p in EVIL_PATHS[:4]],
    ("program: /dev/zero",       *call("create_session", {"program": "/dev/zero"})),
    ("program: sehr lang",       *call("create_session", {"program": "x" * 100000})),
    ("program: NUL im Namen",    *call("create_session", {"program": "sh\x00-c"})),

    # --- Zahlenränder ---
    ("id: negativ",              *call("read_screen", {"id": -1})),
    ("id: riesig",               *call("read_screen", {"id": 2**63 - 1})),
    ("id: als String",           *call("read_screen", {"id": "zwei"})),
    ("id: als Objekt",           *call("read_screen", {"id": {}})),
    ("paneId: riesig",           *call("focus_pane", {"paneId": 2**31})),
    ("windowId: negativ",        *call("focus_window", {"windowId": -99})),
    ("port: riesig",             *call("create_session", {"type": "serial", "port": "x", "baud": 2**31})),

    # --- Long-Poll: der einzige Endpunkt, der absichtlich wartet ---
    ("wait: timeoutMs riesig",   *call("wait_for_events", {"timeoutMs": 2**31, "sessionId": 1})),
    ("wait: timeoutMs negativ",  *call("wait_for_events", {"timeoutMs": -5, "sessionId": 1})),
    ("wait: afterSeq negativ",   *call("wait_for_events", {"afterSeq": -1, "sessionId": 1})),
    ("wait: kinds als String",   *call("wait_for_events", {"kinds": "done", "sessionId": 1})),
    ("wait: kinds tief",         *call("wait_for_events", {"kinds": [["a"]], "sessionId": 1})),

    # --- Text-Endpunkte ---
    ("send_text: 8 MB Text",     *call("send_text", {"id": 1, "text": "A" * (8 * 1024 * 1024)})),
    ("send_text: NUL-Bytes",     *call("send_text", {"id": 1, "text": "a\x00b"})),
    ("send_text: enterDelay riesig", *call("send_text", {"id": 1, "text": "x", "enterDelayMs": 2**31})),
    ("queue_text: leer",         *call("queue_text", {"id": 1, "text": ""})),
    ("post_event: kind Unsinn",  *call("post_event", {"kind": "\x00\xff", "text": "x"})),
    ("rename_window: 1 MB Name", *call("rename_window", {"windowId": 1, "name": "N" * (1024 * 1024)})),

    # --- Profile/Plugins ---
    ("connect_profile: unbekannt", *call("connect_profile", {"name": "gibtsnicht"})),
    ("connect_profile: name leer", *call("connect_profile", {"name": ""})),
    ("set_theme: Unsinn",        *call("set_theme", {"mode": "lila"})),
]


def main():
    print(f"Ziel: {URL}   Token: {'ja' if TOKEN else 'nein'}   PID-Wache: {PID or 'aus'}")
    if not alive(PID):
        print("FEHLER: Der Prozess lebt schon vor dem ersten Aufruf nicht.")
        return 2

    tot, schlimm = 0, []
    for name, method, params, raw in CASES:
        tot += 1
        t0 = time.time()
        status, text = rpc(method, params, raw)
        dt = time.time() - t0
        lebt = alive(PID)
        marke = "ok " if lebt else "TOT"
        if not lebt:
            schlimm.append((name, status, text))
        # Ein Aufruf, der ungewöhnlich lange braucht, ist ebenfalls verdächtig:
        # ein FIFO blockiert, ohne Speicher zu fressen.
        lang = "  <-- LANGSAM" if dt > 5 else ""
        print(f"  [{marke}] {name:<34} status={str(status):<6} {dt:5.2f}s{lang}")
        if not lebt:
            break

    print()
    if schlimm:
        print(f"KILLER GEFUNDEN nach {tot} Faellen:")
        for n, s, t in schlimm:
            print(f"  {n}  status={s}  {t[:120]}")
        return 1
    print(f"{tot} Faelle, Prozess lebt nach jedem einzelnen.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
