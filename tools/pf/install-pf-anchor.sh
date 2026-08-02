#!/bin/bash
# QTmux — pf-Absicherung des MCP-Ports einrichten (macOS, QTMUX-127).
#
#   sudo tools/pf/install-pf-anchor.sh [--port 7345] [--net 192.168.0.0/24]
#   sudo tools/pf/install-pf-anchor.sh --uninstall
#
# Was passiert:
#   1. /usr/local/etc/qtmux/qtmux-mcp.pf.conf  — die Regeln (Anchor-Inhalt)
#   2. /usr/local/etc/qtmux/pf.conf            — Kopie der aktuellen /etc/pf.conf
#      plus zwei Zeilen, die unser Anchor einhängen
#   3. /Library/LaunchDaemons/com.qtmux.pf.plist — lädt (2) beim Systemstart
#   4. pf einschalten und (2) sofort laden
#
# 🔑 Warum eine eigene Hauptkonfiguration statt nur `pfctl -a com.qtmux -f …`:
#    pf wertet die Regeln eines Anchors NUR aus, wenn die geladene Regelmenge es
#    mit `anchor "…"` einhängt. Apples /etc/pf.conf kennt ausschließlich
#    `com.apple/*` — ein frei geladenes Anchor liegt also im Regelwerk, greift
#    aber nie. Das sieht wie eine funktionierende Sperre aus und ist keine.
#    (Gegenprobe im Abschnitt „Prüfen" der README.)
#
# 🔑 Warum eine KOPIE von /etc/pf.conf statt sie zu ändern: macOS-Updates ersetzen
#    /etc/pf.conf kommentarlos. Die Kopie wird bei jedem Lauf neu aus dem aktuellen
#    Original erzeugt — nach einem größeren Systemupdate das Skript einfach erneut
#    aufrufen.
#
# Diese Sperre ersetzt NICHT das Zugriffs-Token des MCP-Servers. Sie begrenzt, WER
# überhaupt anklopfen darf; wer im erlaubten Netz steht, braucht weiterhin ein Token.
set -euo pipefail

PORT=7345
NET="192.168.0.0/24"
UNINSTALL=0
DEST=/usr/local/etc/qtmux
PLIST=/Library/LaunchDaemons/com.qtmux.pf.plist
SRC="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

while [[ $# -gt 0 ]]; do
    case "$1" in
        --port) PORT="$2"; shift 2 ;;
        --net)  NET="$2";  shift 2 ;;
        --uninstall) UNINSTALL=1; shift ;;
        -h|--help) sed -n '2,30p' "$0"; exit 0 ;;
        *) echo "Unbekanntes Argument: $1" >&2; exit 2 ;;
    esac
done

if [[ $EUID -ne 0 ]]; then
    echo "Bitte mit sudo aufrufen: sudo $0 $*" >&2
    exit 1
fi

if [[ $UNINSTALL -eq 1 ]]; then
    echo "== QTmux pf-Absicherung entfernen =="
    launchctl bootout system "$PLIST" 2>/dev/null || true
    rm -f "$PLIST"
    # Zurück auf Apples Regelwerk; pf bleibt an, falls es vorher schon lief.
    pfctl -f /etc/pf.conf 2>/dev/null || true
    pfctl -a com.qtmux -F rules 2>/dev/null || true
    rm -rf "$DEST"
    echo "Entfernt. Aktiver Regelsatz ist wieder /etc/pf.conf."
    exit 0
fi

echo "== QTmux pf-Absicherung einrichten =="
echo "   Port : $PORT"
echo "   Netz : $NET"

mkdir -p "$DEST"

# 1) Anchor-Inhalt aus der Vorlage erzeugen.
sed -e "s|@@PORT@@|$PORT|g" -e "s|@@NET@@|$NET|g" \
    "$SRC/qtmux-mcp.pf.conf" > "$DEST/qtmux-mcp.pf.conf"

# 2) Hauptkonfiguration = aktuelles /etc/pf.conf + unser Anchor.
#    Die anchor-Zeile muss NACH den Apple-Anchors stehen (last match wins), damit
#    unsere block-Regel eine etwaige Apple-pass-Regel überstimmt.
{
    cat /etc/pf.conf
    echo ""
    echo "# --- QTmux (QTMUX-127): MCP-Port absichern ---"
    echo "anchor \"com.qtmux\""
    echo "load anchor \"com.qtmux\" from \"$DEST/qtmux-mcp.pf.conf\""
} > "$DEST/pf.conf"

# 3) Syntax prüfen, BEVOR irgendetwas geladen wird — ein Tippfehler in der
#    Regeldatei ließe pf sonst mit halb geladenem Regelsatz zurück.
if ! pfctl -n -f "$DEST/pf.conf"; then
    echo "FEHLER: Regelsatz ist syntaktisch fehlerhaft, es wurde nichts geladen." >&2
    exit 1
fi

# 4) LaunchDaemon einspielen (idempotent: erst ab-, dann neu laden).
install -m 644 -o root -g wheel "$SRC/com.qtmux.pf.plist" "$PLIST"
launchctl bootout system "$PLIST" 2>/dev/null || true
launchctl bootstrap system "$PLIST"

# 5) Jetzt scharf schalten.
pfctl -E 2>&1 | sed 's/^/   pfctl: /' || true
pfctl -f "$DEST/pf.conf"

echo
echo "Geladen. Prüfen mit:"
echo "   sudo pfctl -s info | head -3"
echo "   sudo pfctl -a com.qtmux -s rules"
echo
echo "Gegenprobe von einem Rechner AUSSERHALB von $NET: Verbindung auf Port $PORT"
echo "muss ins Leere laufen (Timeout), aus $NET muss sie ankommen."
