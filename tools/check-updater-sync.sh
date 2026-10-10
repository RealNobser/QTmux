#!/usr/bin/env bash
# Prueft, ob die aus MacPCAN vendierten Baeume noch byte-identisch zur
# kanonischen Quelle sind. Vier Kontrakte:
#   1. third_party/updater/update/  <->  MacPCAN/src/update/   (QTMUX-125, Paket E1)
#   2. plugins/macpcan/vendor/      <->  MacPCAN/src/          (Auswahl, s. unten)
#   3. installer/{msiexec-path-smoke.ps1,msi-shortcut-icon-check.ps1,smoke/}
#      <-> MacPCAN/platform/windows/ (msiexec-Pfad-Smoke + Verknuepfungs-
#      Symbol-Riegel, explizite Liste, s. installer/smoke/UPSTREAM.md)
#   4. installer/macos/sign-bundle.sh <-> MacPCAN/platform/macos/sign-bundle.sh
#      (macOS-Signatur mit Familien-Identitaet, seit 2026-10-07)
#
# 🔑 Warum ein Skript und kein ctest: Die Pruefung braucht einen MacPCAN-Checkout
# neben QTmux. Auf CI-Runnern und Build-Maschinen gibt es den nicht — ein Test,
# der dort rot wird, meldete ein Umgebungsproblem als Regression (dieselbe
# Ueberlegung wie bei test_i18n und den fehlenden qtbase_*.qm). Ohne Quelle
# beendet sich das Skript darum mit Exit 0 — aber mit einem unuebersehbaren
# Banner, dass NICHTS geprueft wurde (ein Skip darf nicht wie ein OK aussehen).
# Das gilt NUR fuer den Standardort ../MacPCAN: Wer MACPCAN_DIR ausdruecklich
# setzt, will eine Messung — ein falscher Pfad ist dann ein Fehler (Exit 2).
# Getestet wird das SKRIPT selbst mit Attrappen-Baeumen
# (tests/test_check_updater_sync.sh, ctest `test_check_updater_sync`) — das
# braucht keinen Hub und laeuft darum auch in der CI.
#
# 🔑 Warum der Waechter bewusst NICHT in der CI haengt (Owner-Entscheid,
# 2026-08-12): QTmux ist das einzige oeffentliche Repo der Familie und baut auf
# github-hosted Runnern; MacPCAN ist privat. Ein Hub-Checkout in der CI braeuchte
# ein Zugangstoken fuer ein privates Repo im Secret-Store eines OEFFENTLICHEN
# Repos — abgelehnt. Ohne Hub kann die CI prinzipiell nie pruefen; ein
# CI-Schritt, der immer uebersprungen wird, waere selbst ein gruenes Versprechen
# ohne Deckung. Der Waechter ist ein LOKALES Werkzeug (Entwicklermaschine mit
# Hub daneben) und laeuft automatisch im Release-Weg: installer/build-dmg.sh
# ruft ihn vor dem Build auf — Drift bricht dort den Release-Build ab.
#
# Verwendung:
#   tools/check-updater-sync.sh              # sucht ../MacPCAN (neben dem Repo)
#   MACPCAN_DIR=/pfad tools/check-updater-sync.sh
#   tools/check-updater-sync.sh --update     # zieht Aenderungen HERUEBER (Einbahnstrasse!)
#
# Ein RELATIVER MACPCAN_DIR gilt relativ zum AUFRUFVERZEICHNIS — wie bei jedem
# Unix-Werkzeug, und so hat ihn die Existenzpruefung schon immer gelesen; nicht
# relativ zu tools/ oder zum Repo (`MACPCAN_DIR=../MacPCAN` aus der Repo-Wurzel
# meint den Hub daneben, nicht QTmux/MacPCAN). Er wird als Erstes in einen
# absoluten Pfad uebersetzt; alles danach arbeitet nur mit absoluten Pfaden.
#
# Exit-Codes — jeder Fall hat seinen eigenen, ein Werkzeugfehler ist NIE gruen:
#   0  alle vier Kontrakte gemessen und byte-identisch (je Kontrakt >= 1 Datei
#      verglichen) — oder: Hub am Standardort fehlt (Banner "NICHTS GEPRUEFT")
#   1  Befund: Drift oder Fremd-Include
#   2  Werkzeugfehler: MACPCAN_DIR gesetzt, aber unbrauchbar; Verzeichnis oder
#      Datei nicht lesbar; Vergleich/Uebernahme gescheitert; ein Kontrakt mit
#      0 verglichenen Dateien; unbekanntes Argument
#
# 🔑 Warum Exit 2 existiert (2026-10-10): Bis dahin entstand die Dateiliste von
# Kontrakt 1 als `{ cd vendiert && find; cd upstream && find; }`. Das zweite
# `cd` loeste einen RELATIVEN MACPCAN_DIR gegen das vendierte Verzeichnis auf
# (das erste `cd` hatte schon gewechselt), scheiterte, und der Fehler ging im
# `;` unter: Die upstream-Haelfte der Liste fehlte, eine im Hub NEUE Datei
# wurde nie gesehen — und das Skript meldete "byte-identisch", Exit 0. Ein
# Werkzeugfehler war zur Entwarnung geworden. Seitdem: keine Liste und kein
# Vergleich mehr aus einer Befehlssubstitution, deren Fehler verloren geht,
# und `cmp` statt zweier Hashes (die LEEREN Hashes eines unlesbaren Paars
# waren "gleich").
#
# ⚠️ Einbahnstrasse: Vendierte Baeume werden NIE lokal editiert. Aenderungen
# gehoeren in den Hub und kommen von dort per --update zurueck;
# QTmux-spezifisches liegt in src/viewmodels/UpdateViewModel.* und im QML
# bzw. beim CAN-Plugin in plugins/macpcan/{MacPcanPlugin.cpp,CanText.h}.

set -u
# Ein gesetzter CDPATH liesse `cd relativ` in einem fremden Verzeichnis landen.
unset CDPATH

werkzeugfehler() {
    echo "check-updater-sync: WERKZEUGFEHLER: $*" >&2
    echo "  Dieser Lauf ist KEIN Nachweis von Synchronitaet (Exit 2)." >&2
    exit 2
}

mode="check"
case "$#:${1:-}" in
    0:) ;;
    1:--update) mode="update" ;;
    *) werkzeugfehler "unbekannte Argumente: $* (erlaubt: keins oder --update)" ;;
esac

here="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd -P)" \
    || werkzeugfehler "Repo-Verzeichnis nicht bestimmbar"
vendored="$here/third_party/updater/update"

if [ -n "${MACPCAN_DIR:-}" ]; then
    # Ausdruecklich gesetzt: absolut machen (relativ = gegen das Aufruf-
    # verzeichnis), und jeder Fehlschlag ist laut.
    macpcan_root="$(cd -- "$MACPCAN_DIR" && pwd -P)" \
        || werkzeugfehler "MACPCAN_DIR='$MACPCAN_DIR' ist kein erreichbares Verzeichnis (Aufrufverzeichnis: $(pwd))"
    [ -d "$macpcan_root/src/update" ] \
        || werkzeugfehler "MACPCAN_DIR='$MACPCAN_DIR' ($macpcan_root) enthaelt keinen MacPCAN-Quellbaum (erwartet: src/update/)"
else
    macpcan_root="$here/../MacPCAN"
    if [ ! -d "$macpcan_root/src/update" ]; then
        echo "########################################################################"
        echo "##  check-updater-sync: NICHTS GEPRUEFT — MacPCAN-Hub nicht gefunden  ##"
        echo "########################################################################"
        echo "Unter '$macpcan_root' liegt kein MacPCAN-Quellbaum (erwartet: src/update/)."
        if [ -d "$macpcan_root" ]; then
            echo "  Das Verzeichnis existiert, enthaelt aber keine Quellen — liegt dort"
            echo "  wirklich der Hub? (~/Projects/GitHub/MacPCAN z. B. enthaelt NUR build/.)"
        fi
        echo "Alle VIER Vendoring-Kontrakte bleiben damit UNGEPRUEFT:"
        echo "  1. third_party/updater/update/               (Updater-Kern)"
        echo "  2. plugins/macpcan/vendor/                   (CAN-Plugin-Auswahl)"
        echo "  3. installer/msiexec-path-smoke.ps1 + smoke/ (1619-Riegel)"
        echo "     + installer/msi-shortcut-icon-check.ps1    (Verknuepfungs-Symbol)"
        echo "  4. installer/macos/sign-bundle.sh            (macOS-Signatur)"
        echo "Dieser Lauf ist KEIN Nachweis von Synchronitaet. Auf einer Maschine mit"
        echo "Hub-Checkout wiederholen oder MACPCAN_DIR=/pfad/zum/Hub setzen."
        exit 0
    fi
    macpcan_root="$(cd -- "$macpcan_root" && pwd -P)" \
        || werkzeugfehler "'$here/../MacPCAN' nicht aufloesbar"
fi
upstream="$macpcan_root/src/update"

tmp="$(mktemp -d "${TMPDIR:-/tmp}/check-updater-sync.XXXXXX")" \
    || werkzeugfehler "mktemp gescheitert"
trap 'rm -rf "$tmp"' EXIT

# Schreibt die Dateien unter $1 (relativ, sortiert) nach $2. Die Subshell
# schreibt per Umleitung in eine Datei — ihr Exit-Code bleibt erhalten, ein
# gescheitertes `cd` oder `find` beendet den Lauf.
dateiliste() {
    [ -d "$1" ] || werkzeugfehler "Verzeichnis fehlt: $1"
    ( cd -- "$1" && find . -type f ) > "$tmp/find.raw" \
        || werkzeugfehler "Dateiliste nicht lesbar: $1"
    sed 's|^\./||' "$tmp/find.raw" | sort > "$2"
}

# Vergleicht zwei Dateien. 0 = gleich, 1 = verschieden; alles andere (Datei
# nicht lesbar, ...) ist ein Werkzeugfehler — nie "gleich".
gleich() {
    cmp -s -- "$1" "$2"
    case $? in
        0) return 0 ;;
        1) return 1 ;;
        *) werkzeugfehler "Vergleich gescheitert: $1 <-> $2" ;;
    esac
}

hash_of() { shasum -a 256 "$1" 2>/dev/null | cut -d' ' -f1; }

uebernehmen() {  # $1 Quelle, $2 Ziel, $3 optional "-p"
    mkdir -p "$(dirname "$2")" && cp ${3:-} "$1" "$2" \
        || werkzeugfehler "Uebernahme gescheitert: $1 -> $2"
}

# Sammelt die `#include "…"`-Pfade unter $1 nach $2. grep-Exit 1 heisst
# "keine Treffer" (zulaessig), 2 heisst Fehler.
includes() {
    grep -rhoE '#include[[:space:]]+"[^"]+"' "$1" > "$tmp/inc.raw"
    [ $? -le 1 ] || werkzeugfehler "Include-Suche gescheitert: $1"
    sed 's/.*"\(.*\)"/\1/' "$tmp/inc.raw" | sort -u > "$2"
}

# --- Erster Kontrakt: third_party/updater/update/ <-> MacPCAN/src/update/ ----
#
# Dateiliste aus BEIDEN Baeumen, damit auch eine NEUE Datei upstream auffaellt
# (ein reiner Vergleich ueber die vendierten Dateien saehe sie nie).
dateiliste "$vendored" "$tmp/k1.vendiert"
dateiliste "$upstream" "$tmp/k1.upstream"
sort -u "$tmp/k1.vendiert" "$tmp/k1.upstream" > "$tmp/k1.alle" \
    || werkzeugfehler "Dateilisten nicht zusammenfuehrbar"

drift=0
n1=0
while IFS= read -r f; do
    [ -z "$f" ] && continue
    a="$upstream/$f"
    b="$vendored/$f"
    if [ ! -f "$a" ]; then
        echo "  NUR VENDIERT (upstream geloescht?): $f"; drift=1; continue
    fi
    if [ ! -f "$b" ]; then
        echo "  FEHLT VENDIERT (upstream neu):      $f"; drift=1
        [ "$mode" = "update" ] && uebernehmen "$a" "$b"
        continue
    fi
    n1=$((n1 + 1))
    if ! gleich "$a" "$b"; then
        echo "  ABWEICHUNG:                         $f"
        echo "      upstream $(hash_of "$a")"
        echo "      vendiert $(hash_of "$b")"
        drift=1
        [ "$mode" = "update" ] && uebernehmen "$a" "$b"
    fi
done < "$tmp/k1.alle"

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
includes "$vendored" "$tmp/k1.inc"
fremd=0
while IFS= read -r inc; do
    [ -z "$inc" ] && continue
    # Erlaubt: alles unter update/… und die im selben Baum liegenden Dateien
    # (Monocypher inkludiert sich flach, deshalb der Namensabgleich gegen die
    # vendierte Dateiliste).
    case "$inc" in
        update/*) continue ;;
    esac
    if [ -e "$vendored/$inc" ] \
       || awk -F/ -v n="$(basename "$inc")" '$NF == n { f = 1 } END { exit !f }' "$tmp/k1.vendiert"; then
        continue
    fi
    echo "  FREMD-INCLUDE (ausserhalb des Vendorings): $inc"
    fremd=1
done < "$tmp/k1.inc"

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
dateiliste "$mp_vendored" "$tmp/k2.vendiert"

mp_drift=0
n2=0
while IFS= read -r f; do
    [ -z "$f" ] && continue
    a="$mp_upstream/$f"
    b="$mp_vendored/$f"
    if [ ! -f "$a" ]; then
        echo "  MACPCAN NUR VENDIERT (upstream geloescht?): $f"; mp_drift=1; continue
    fi
    n2=$((n2 + 1))
    if ! gleich "$a" "$b"; then
        echo "  MACPCAN ABWEICHUNG:                         $f"
        echo "      upstream $(hash_of "$a")"
        echo "      vendiert $(hash_of "$b")"
        mp_drift=1
        [ "$mode" = "update" ] && uebernehmen "$a" "$b"
    fi
done < "$tmp/k2.vendiert"

# Fremd-Include-Pruefung fuer den Vendor-Baum: Jeder mit Anfuehrungszeichen
# inkludierte Pfad muss im Vendor-Baum selbst liegen (core/…, drivers/…).
# Bekaeme eine vendierte Datei z. B. `#include "specs/DbcDecoder.hpp"`, waere
# das eine Kontrakt-Erweiterung, die hier benannt gehoert — nicht erst als
# "file not found" beim Kompilieren.
includes "$mp_vendored" "$tmp/k2.inc"
mp_fremd=0
while IFS= read -r inc; do
    [ -z "$inc" ] && continue
    if [ -e "$mp_vendored/$inc" ]; then
        continue
    fi
    echo "  MACPCAN FREMD-INCLUDE (ausserhalb des Vendorings): $inc"
    mp_fremd=1
done < "$tmp/k2.inc"

if [ "$mp_fremd" = "1" ]; then
    echo "check-updater-sync: Der macpcan-Vendor-Baum greift ausserhalb seiner Auswahl zu."
    echo "  Entweder die Datei mitvendieren (Auswahl bewusst erweitern) oder die"
    echo "  Abhaengigkeit im Hub aufloesen."
    [ "$mode" != "update" ] && exit 1
fi

# --- Dritter Kontrakt: msiexec-Pfad-Smoke <-> MacPCAN/platform/windows/ ------
#
# Der 1619-Riegel in installer/build-msi.ps1 (Skript + MSI-Fixture) ist byte-
# identisch aus MacPCAN uebernommen — Hintergrund und Ablage-Begruendung stehen
# in installer/smoke/UPSTREAM.md. Anders als bei den zwei Kontrakten oben ist
# die Liste hier EXPLIZIT: installer/ enthaelt ueberwiegend QTmux-Eigenes
# (build-msi.ps1, QTmux.wxs, …), und auch installer/smoke/UPSTREAM.md ist
# QTmux-eigen — eine Verzeichnis-Wanderung wie oben meldete lauter
# Fehlalarme. Kommt upstream eine Datei zur Smoke-Familie hinzu, faellt das
# beim naechsten Nachziehen im Hub auf, nicht hier; die Liste dann erweitern.
sm_upstream="$macpcan_root/platform/windows"
cat > "$tmp/k3.liste" <<'EOF'
msiexec-path-smoke.ps1
msi-shortcut-icon-check.ps1
smoke/MacPCAN-PathSmoke.msi
smoke/marker.txt
smoke/msi-path-smoke.wxs
EOF

sm_drift=0
n3=0
while IFS= read -r f; do
    [ -z "$f" ] && continue
    a="$sm_upstream/$f"
    b="$here/installer/$f"
    if [ ! -f "$a" ]; then
        echo "  SMOKE NUR VENDIERT (upstream geloescht?): $f"; sm_drift=1; continue
    fi
    if [ ! -f "$b" ]; then
        echo "  SMOKE FEHLT VENDIERT:                     $f"; sm_drift=1
        [ "$mode" = "update" ] && uebernehmen "$a" "$b"
        continue
    fi
    n3=$((n3 + 1))
    if ! gleich "$a" "$b"; then
        echo "  SMOKE ABWEICHUNG:                         $f"
        echo "      upstream $(hash_of "$a")"
        echo "      vendiert $(hash_of "$b")"
        sm_drift=1
        [ "$mode" = "update" ] && uebernehmen "$a" "$b"
    fi
done < "$tmp/k3.liste"

# --- Vierter Kontrakt: macOS-Signatur <-> MacPCAN/platform/macos/ -----------
#
# installer/build-dmg.sh signiert das Bundle mit der Familien-Identitaet ueber
# installer/macos/sign-bundle.sh — byte-identisch zu MacPCANs
# platform/macos/sign-bundle.sh (Hub liefert die Mechanik, seit 2026-10-07;
# Doku MacPCAN/docs/codesign.md). Eine einzelne Datei, darum eine explizite
# Zuordnung statt einer Verzeichnis-Wanderung.
sg_drift=0
n4=0
sg_a="$macpcan_root/platform/macos/sign-bundle.sh"
sg_b="$here/installer/macos/sign-bundle.sh"
if [ ! -f "$sg_a" ]; then
    echo "  SIGN NUR VENDIERT (upstream geloescht?): installer/macos/sign-bundle.sh"; sg_drift=1
elif [ ! -f "$sg_b" ]; then
    echo "  SIGN FEHLT VENDIERT:                     installer/macos/sign-bundle.sh"; sg_drift=1
    [ "$mode" = "update" ] && uebernehmen "$sg_a" "$sg_b" -p
else
    n4=1
    if ! gleich "$sg_a" "$sg_b"; then
        echo "  SIGN ABWEICHUNG:                         installer/macos/sign-bundle.sh"
        echo "      upstream $(hash_of "$sg_a")"
        echo "      vendiert $(hash_of "$sg_b")"
        sg_drift=1
        [ "$mode" = "update" ] && uebernehmen "$sg_a" "$sg_b" -p
    fi
fi

if [ "$mode" = "update" ]; then
    if [ "$drift" = "1" ] || [ "$mp_drift" = "1" ] || [ "$sm_drift" = "1" ] || [ "$sg_drift" = "1" ]; then
        echo "check-updater-sync: Dateien uebernommen. UPSTREAM.md-Commit nachziehen:"
        ( cd "$macpcan_root" && git rev-parse HEAD 2>/dev/null ) \
            || echo "  (Hub-Commit nicht lesbar — in $macpcan_root von Hand nachsehen)"
        exit 0
    fi
    echo "check-updater-sync: nichts zu tun — bereits identisch."
    exit 0
fi

if [ "$drift" = "1" ] || [ "$mp_drift" = "1" ] || [ "$sm_drift" = "1" ] || [ "$sg_drift" = "1" ]; then
    echo "check-updater-sync: DRIFT gegenueber MacPCAN. Beheben mit:"
    echo "  tools/check-updater-sync.sh --update"
    exit 1
fi

# Positivkontrolle: "byte-identisch" nur, wenn je Kontrakt wirklich Dateien
# gelesen und verglichen wurden. Ein leerer Baum hiesse sonst "gruen, weil
# nichts zu vergleichen war".
for k in "1:$n1" "2:$n2" "3:$n3" "4:$n4"; do
    [ "${k#*:}" -gt 0 ] \
        || werkzeugfehler "Kontrakt ${k%%:*}: 0 Dateien verglichen — der Waechter hat nichts gemessen"
done

echo "check-updater-sync: byte-identisch zu $upstream ($n1 Dateien verglichen)"
echo "check-updater-sync: byte-identisch zu $mp_upstream (macpcan-Vendor-Auswahl, $n2 Dateien verglichen)"
echo "check-updater-sync: byte-identisch zu $sm_upstream (msiexec-Pfad-Smoke, $n3 Dateien verglichen)"
echo "check-updater-sync: byte-identisch zu $sg_a (macOS-Signatur, $n4 Datei verglichen)"
exit 0
