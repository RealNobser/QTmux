#!/usr/bin/env bash
# Prueft, ob third_party/updater/update/ noch byte-identisch zur kanonischen
# Quelle MacPCAN/src/update/ ist (QTMUX-125, Paket E1).
#
# 🔑 Warum ein Skript und kein ctest: Die Pruefung braucht einen MacPCAN-Checkout
# neben QTmux. Auf CI-Runnern und Build-Maschinen gibt es den nicht — ein Test,
# der dort rot wird, meldete ein Umgebungsproblem als Regression (dieselbe
# Ueberlegung wie bei test_i18n und den fehlenden qtbase_*.qm). Ohne Quelle
# beendet sich das Skript darum mit Exit 0 und einer Meldung.
#
# Verwendung:
#   tools/check-updater-sync.sh              # sucht ../MacPCAN
#   MACPCAN_DIR=/pfad tools/check-updater-sync.sh
#   tools/check-updater-sync.sh --update     # zieht Aenderungen HERUEBER (Einbahnstrasse!)
#
# ⚠️ Einbahnstrasse: third_party/updater/ wird NIE lokal editiert. Aenderungen
# gehoeren nach MacPCAN/src/update/ und kommen von dort per --update zurueck;
# QTmux-spezifisches liegt in src/viewmodels/UpdateViewModel.* und im QML.

set -u

here="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
vendored="$here/third_party/updater/update"
upstream="${MACPCAN_DIR:-$here/../MacPCAN}/src/update"

if [ ! -d "$upstream" ]; then
    echo "check-updater-sync: keine MacPCAN-Quelle unter '$upstream' — uebersprungen."
    echo "  (MACPCAN_DIR setzen, wenn der Checkout woanders liegt.)"
    exit 0
fi

mode="check"
[ "${1:-}" = "--update" ] && mode="update"

# Dateiliste aus BEIDEN Baeumen, damit auch eine NEUE Datei upstream auffaellt
# (ein reiner Vergleich ueber die vendierten Dateien saehe sie nie).
files="$( { cd "$vendored" && find . -type f; cd "$upstream" && find . -type f; } \
          | sed 's|^\./||' | sort -u )"

drift=0
while IFS= read -r f; do
    [ -z "$f" ] && continue
    a="$upstream/$f"
    b="$vendored/$f"
    if [ ! -f "$a" ]; then
        echo "  NUR VENDIERT (upstream geloescht?): $f"; drift=1; continue
    fi
    if [ ! -f "$b" ]; then
        echo "  FEHLT VENDIERT (upstream neu):      $f"; drift=1
        [ "$mode" = "update" ] && mkdir -p "$(dirname "$b")" && cp "$a" "$b"
        continue
    fi
    ha="$(shasum -a 256 "$a" | cut -d' ' -f1)"
    hb="$(shasum -a 256 "$b" | cut -d' ' -f1)"
    if [ "$ha" != "$hb" ]; then
        echo "  ABWEICHUNG:                         $f"
        echo "      upstream $ha"
        echo "      vendiert $hb"
        drift=1
        [ "$mode" = "update" ] && cp "$a" "$b"
    fi
done <<EOF
$files
EOF

if [ "$mode" = "update" ]; then
    if [ "$drift" = "1" ]; then
        echo "check-updater-sync: Dateien uebernommen. UPSTREAM.md-Commit nachziehen:"
        ( cd "$(dirname "$upstream")/.." && git rev-parse HEAD 2>/dev/null )
        exit 0
    fi
    echo "check-updater-sync: nichts zu tun — bereits identisch."
    exit 0
fi

if [ "$drift" = "1" ]; then
    echo "check-updater-sync: DRIFT gegenueber MacPCAN. Beheben mit:"
    echo "  tools/check-updater-sync.sh --update"
    exit 1
fi

echo "check-updater-sync: byte-identisch zu $upstream"
exit 0
