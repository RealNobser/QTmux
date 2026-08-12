#!/usr/bin/env bash
# Prueft, ob die aus MacPCAN vendierten Baeume noch byte-identisch zur
# kanonischen Quelle sind. Zwei Kontrakte:
#   1. third_party/updater/update/  <->  MacPCAN/src/update/   (QTMUX-125, Paket E1)
#   2. plugins/macpcan/vendor/      <->  MacPCAN/src/          (Auswahl, s. unten)
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
# ⚠️ Einbahnstrasse: Vendierte Baeume werden NIE lokal editiert. Aenderungen
# gehoeren in den Hub und kommen von dort per --update zurueck;
# QTmux-spezifisches liegt in src/viewmodels/UpdateViewModel.* und im QML
# bzw. beim CAN-Plugin in plugins/macpcan/{MacPcanPlugin.cpp,CanText.h}.

set -u

here="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
vendored="$here/third_party/updater/update"
macpcan_root="${MACPCAN_DIR:-$here/../MacPCAN}"
upstream="$macpcan_root/src/update"

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

# --- Kontrakt-Waechter: bleibt der Kern in sich geschlossen? -----------------
#
# 🔑 Warum das hier steht (2026-08-06, vor dem AP8-Umbau im Hub): Der
# Datei-Abgleich oben bemerkt zwar JEDE Aenderung an `src/update/` — die
# Dateiliste kommt aus BEIDEN Baeumen, neue und geloeschte Dateien fallen also
# auf. Was er NICHT bemerkt, ist eine neue Abhaengigkeit NACH AUSSEN: Bekommt
# der Kern eine Zeile wie `#include "specs/DbcDecoder.hpp"`, meldet der Abgleich
# nur "ABWEICHUNG", man zieht nach — und bekommt danach einen Compile-Fehler
# "file not found", der nicht sagt, dass der VENDORING-KONTRAKT verletzt wurde.
#
# QTmux vendiert ausschliesslich `src/update/`. Alles, was der Kern darueber
# hinaus inkludiert, muesste QTmux mitvendieren und liegt damit ausserhalb des
# vereinbarten Umfangs. Dieser Waechter benennt genau das, statt es dem Compiler
# zu ueberlassen.
fremd=0
while IFS= read -r inc; do
    [ -z "$inc" ] && continue
    # Erlaubt: alles unter update/… und die im selben Baum liegenden Dateien
    # (Monocypher inkludiert sich flach, deshalb der Existenztest).
    case "$inc" in
        update/*) continue ;;
    esac
    if [ -e "$vendored/$inc" ] || find "$vendored" -name "$(basename "$inc")" -print -quit | grep -q .; then
        continue
    fi
    echo "  FREMD-INCLUDE (ausserhalb des Vendorings): $inc"
    fremd=1
done <<EOF
$(grep -rhoE '#include[[:space:]]+"[^"]+"' "$vendored" 2>/dev/null | sed 's/.*"\(.*\)"/\1/' | sort -u)
EOF

if [ "$fremd" = "1" ]; then
    echo "check-updater-sync: Der Kern greift ausserhalb von src/update/ zu."
    echo "  QTmux vendiert NUR src/update/ — solche Abhaengigkeiten muessen im Hub"
    echo "  aufgeloest oder der Vendoring-Umfang muss mit MacPCAN neu vereinbart werden."
    [ "$mode" != "update" ] && exit 1
fi

# --- Zweiter Kontrakt: plugins/macpcan/vendor/ <-> MacPCAN/src/ --------------
#
# Anders als beim Updater vendiert QTmux hier KEIN ganzes Verzeichnis, sondern
# eine AUSWAHL: MacPCAN/src/ enthaelt weit mehr (gui/, specs/, TraceBuffer,
# SocketCanDevice, …), das QTmux nichts angeht. Die Dateiliste kommt darum aus
# dem VENDOR-Baum — jede vendierte Datei muss byte-identisch zu ihrem
# Gegenstueck unter MacPCAN/src/<pfad> sein. Eine im Hub GELOESCHTE Datei
# faellt so weiter auf ("upstream geloescht?"); eine im Hub NEUE, vom Kern
# gebrauchte Datei faellt auf, sobald eine vendierte sie inkludiert
# (Fremd-Include-Pruefung unten). Bewusste Ausnahmen gibt es derzeit KEINE;
# kaemen welche, gehoeren sie hier als kommentierte Liste hinein, nicht als
# stilles Auslassen.
#
# 🔑 Warum der Waechter dieses Verzeichnis erst seit 2026-08-12 kennt: Vorher
# pruefte er NUR den Updater — plugins/macpcan/vendor/ lief seit dem Anlegen
# ungemessen und war mit 7 von 8 Dateien vom Hub weggedriftet (Alt-Stand M9).
mp_vendored="$here/plugins/macpcan/vendor"
mp_upstream="$macpcan_root/src"

mp_drift=0
while IFS= read -r f; do
    [ -z "$f" ] && continue
    a="$mp_upstream/$f"
    b="$mp_vendored/$f"
    if [ ! -f "$a" ]; then
        echo "  MACPCAN NUR VENDIERT (upstream geloescht?): $f"; mp_drift=1; continue
    fi
    ha="$(shasum -a 256 "$a" | cut -d' ' -f1)"
    hb="$(shasum -a 256 "$b" | cut -d' ' -f1)"
    if [ "$ha" != "$hb" ]; then
        echo "  MACPCAN ABWEICHUNG:                         $f"
        echo "      upstream $ha"
        echo "      vendiert $hb"
        mp_drift=1
        [ "$mode" = "update" ] && cp "$a" "$b"
    fi
done <<EOF
$(cd "$mp_vendored" && find . -type f | sed 's|^\./||' | sort)
EOF

# Fremd-Include-Pruefung fuer den Vendor-Baum: Jeder mit Anfuehrungszeichen
# inkludierte Pfad muss im Vendor-Baum selbst liegen (core/…, drivers/…).
# Bekaeme eine vendierte Datei z. B. `#include "specs/DbcDecoder.hpp"`, waere
# das eine Kontrakt-Erweiterung, die hier benannt gehoert — nicht erst als
# "file not found" beim Kompilieren.
mp_fremd=0
while IFS= read -r inc; do
    [ -z "$inc" ] && continue
    if [ -e "$mp_vendored/$inc" ]; then
        continue
    fi
    echo "  MACPCAN FREMD-INCLUDE (ausserhalb des Vendorings): $inc"
    mp_fremd=1
done <<EOF
$(grep -rhoE '#include[[:space:]]+"[^"]+"' "$mp_vendored" 2>/dev/null | sed 's/.*"\(.*\)"/\1/' | sort -u)
EOF

if [ "$mp_fremd" = "1" ]; then
    echo "check-updater-sync: Der macpcan-Vendor-Baum greift ausserhalb seiner Auswahl zu."
    echo "  Entweder die Datei mitvendieren (Auswahl bewusst erweitern) oder die"
    echo "  Abhaengigkeit im Hub aufloesen."
    [ "$mode" != "update" ] && exit 1
fi

if [ "$mode" = "update" ]; then
    if [ "$drift" = "1" ] || [ "$mp_drift" = "1" ]; then
        echo "check-updater-sync: Dateien uebernommen. UPSTREAM.md-Commit nachziehen:"
        ( cd "$macpcan_root" && git rev-parse HEAD 2>/dev/null )
        exit 0
    fi
    echo "check-updater-sync: nichts zu tun — bereits identisch."
    exit 0
fi

if [ "$drift" = "1" ] || [ "$mp_drift" = "1" ]; then
    echo "check-updater-sync: DRIFT gegenueber MacPCAN. Beheben mit:"
    echo "  tools/check-updater-sync.sh --update"
    exit 1
fi

echo "check-updater-sync: byte-identisch zu $upstream"
echo "check-updater-sync: byte-identisch zu $mp_upstream (macpcan-Vendor-Auswahl)"
exit 0
