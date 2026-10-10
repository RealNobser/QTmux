#!/usr/bin/env bash
# Test fuer tools/check-updater-sync.sh — prueft das SKRIPT, nicht den Hub.
#
# Baut je Fall eine Attrappe aus zwei Nachbarbaeumen (<tmp>/ws/QTmux mit einer
# Kopie des Waechters unter tools/ und <tmp>/ws/MacPCAN) und erwartet je Fall
# einen bestimmten Exit-Code plus eine Kennzeile. Braucht keinen echten Hub und
# laeuft darum auch in der CI (macOS/Linux; Windows-ctest kennt kein bash).
#
# 🔑 Warum es diesen Test gibt (2026-10-10): Mit RELATIVEM MACPCAN_DIR meldete
# der Waechter "byte-identisch" (Exit 0), obwohl ein `cd` scheiterte und eine
# im Hub neue Datei nie gesehen wurde. Die Faelle "relativ + upstream neu",
# "Pfad existiert nicht", "leerer Vendor-Baum" und "unbekanntes Argument" sind
# genau die, die der alte Waechter gruen meldete — Mutationsprobe: diesen Test
# gegen `git show <alt>:tools/check-updater-sync.sh` fahren, sie muessen rot
# werden.
#
# Verwendung: tests/test_check_updater_sync.sh <pfad/zu/check-updater-sync.sh>

set -u
unset CDPATH

waechter="${1:?Pfad zu check-updater-sync.sh fehlt}"
[ -f "$waechter" ] || { echo "FEHLER: kein Skript unter $waechter"; exit 2; }

root="$(mktemp -d "${TMPDIR:-/tmp}/tst_check_updater_sync.XXXXXX")" || exit 2
trap 'chmod -R u+rwX "$root" 2>/dev/null; rm -rf "$root"' EXIT

fehler=0
faelle=0

# Legt die Attrappe frisch an: identische Baeume in allen vier Kontrakten.
attrappe() {
    rm -rf "$root/ws" && mkdir -p "$root/ws" || exit 2
    local q="$root/ws/QTmux" h="$root/ws/MacPCAN" d
    mkdir -p "$q/tools" && cp "$waechter" "$q/tools/check-updater-sync.sh" || exit 2

    # Kontrakt 1: Updater-Kern (mit Include nach update/… und flachem Include)
    for d in "$q/third_party/updater/update" "$h/src/update"; do
        mkdir -p "$d/ed25519"
        printf '#include "update/Checker.hpp"\nint manifest;\n' > "$d/Manifest.cpp"
        printf 'struct Checker {};\n' > "$d/Checker.hpp"
        printf '#include "monocypher.h"\n' > "$d/ed25519/verify.c"
        printf '/* mono */\n' > "$d/ed25519/monocypher.h"
    done
    # Kontrakt 2: Vendor-AUSWAHL — der Hub hat mehr, als vendiert ist
    for d in "$q/plugins/macpcan/vendor" "$h/src"; do
        mkdir -p "$d/core" "$d/drivers"
        printf '#include "core/Frame.hpp"\n' > "$d/drivers/Driver.cpp"
        printf 'struct Frame {};\n' > "$d/core/Frame.hpp"
    done
    mkdir -p "$h/src/gui" && printf 'gui\n' > "$h/src/gui/Fenster.cpp"
    # Kontrakt 3: explizite Liste
    for d in "$q/installer" "$h/platform/windows"; do
        mkdir -p "$d/smoke"
        for f in msiexec-path-smoke.ps1 msi-shortcut-icon-check.ps1 \
                 smoke/MacPCAN-PathSmoke.msi smoke/marker.txt smoke/msi-path-smoke.wxs; do
            printf 'inhalt %s\n' "$f" > "$d/$f"
        done
    done
    printf 'QTmux-eigen\n' > "$q/installer/build-msi.ps1"
    # Kontrakt 4: Einzeldatei
    for d in "$q/installer/macos" "$h/platform/macos"; do
        mkdir -p "$d" && printf '#!/bin/sh\necho sign\n' > "$d/sign-bundle.sh"
    done
}

# fall <name> <erwarteter Exit> <Muster, das in der Ausgabe stehen MUSS>
#      <Muster, das NICHT darin stehen darf, oder ""> -- <Befehl…>
# Der Befehl laeuft in einer Subshell; sein cwd setzt er selbst.
fall() {
    local name="$1" soll="$2" muss="$3" nie="$4"; shift 5
    faelle=$((faelle + 1))
    local out ist
    out="$( "$@" 2>&1 )"; ist=$?
    local ok=1
    [ "$ist" = "$soll" ] || ok=0
    printf '%s\n' "$out" | grep -q -- "$muss" || ok=0
    if [ -n "$nie" ] && printf '%s\n' "$out" | grep -q -- "$nie"; then ok=0; fi
    if [ "$ok" = 1 ]; then
        echo "PASS  $name (exit $ist)"
    else
        echo "FAIL  $name: exit $ist, erwartet $soll; muss '$muss'${nie:+, nie '$nie'}"
        printf '%s\n' "$out" | sed 's/^/        | /'
        fehler=$((fehler + 1))
    fi
}

lauf_in() {  # lauf_in <cwd> [VAR=wert …] -- <args…>
    local cwd="$1"; shift
    local envs=()
    while [ "$#" -gt 0 ] && [ "$1" != "--" ]; do envs+=("$1"); shift; done
    shift
    ( cd "$cwd" && env ${envs[@]+"${envs[@]}"} bash "$root/ws/QTmux/tools/check-updater-sync.sh" "$@" )
}

Q() { echo "$root/ws/QTmux"; }

# 1. Identische Baeume, Standardort ../MacPCAN -> gruen, mit Zaehlung
attrappe
fall "identisch, Standardort" 0 "byte-identisch .*(4 Dateien verglichen)" "WERKZEUGFEHLER" -- \
    lauf_in "$(Q)" --

# 2. Relativer MACPCAN_DIR aus der Repo-Wurzel -> gruen
fall "identisch, relativ aus Repo-Wurzel" 0 "macOS-Signatur, 1 Datei verglichen" "No such file" -- \
    lauf_in "$(Q)" MACPCAN_DIR=../MacPCAN --

# 3. Relativer MACPCAN_DIR aus einem ANDEREN Verzeichnis, mit Koeder-CDPATH
mkdir -p "$root/koeder/MacPCAN/src/update"
fall "identisch, relativ aus fremdem cwd (CDPATH-Koeder)" 0 "msiexec-Pfad-Smoke, 5 Dateien verglichen" "koeder" -- \
    lauf_in "$root/ws" MACPCAN_DIR=MacPCAN CDPATH="$root/koeder" --

# 4. DER Befund: relativ + im Hub NEUE Datei -> Drift (alter Waechter: gruen)
attrappe
printf 'neu\n' > "$root/ws/MacPCAN/src/update/Neu.cpp"
fall "relativ + upstream neue Datei -> Drift" 1 "FEHLT VENDIERT (upstream neu): *Neu.cpp" "byte-identisch" -- \
    lauf_in "$(Q)" MACPCAN_DIR=../MacPCAN --

# 5. Nicht existierender ausdruecklicher Pfad -> Fehler, nie gruen
attrappe
fall "MACPCAN_DIR existiert nicht -> Exit 2" 2 "WERKZEUGFEHLER" "byte-identisch" -- \
    lauf_in "$(Q)" MACPCAN_DIR=../GibtsNicht --
fall "MACPCAN_DIR ohne Quellbaum -> Exit 2" 2 "keinen MacPCAN-Quellbaum" "byte-identisch" -- \
    lauf_in "$(Q)" MACPCAN_DIR=tools --

# 6. Kein Hub am Standardort -> Banner, Exit 0 (Owner-Entscheid), nie "identisch"
rm -rf "$root/ws/MacPCAN"
fall "Standardort ohne Hub -> Banner" 0 "NICHTS GEPRUEFT" "byte-identisch" -- \
    lauf_in "$(Q)" --

# 7. Je Kontrakt eine verfaelschte Datei -> Abweichung
for ziel in third_party/updater/update/Checker.hpp plugins/macpcan/vendor/core/Frame.hpp \
            installer/smoke/marker.txt installer/macos/sign-bundle.sh; do
    attrappe
    printf 'verfaelscht\n' >> "$root/ws/QTmux/$ziel"
    fall "verfaelscht: $ziel" 1 "ABWEICHUNG" "byte-identisch" -- \
        lauf_in "$(Q)" --
done

# 8. Leerer Vendor-Baum (Kontrakt 2) -> Positivkontrolle schlaegt an
attrappe
rm -rf "$root/ws/QTmux/plugins/macpcan/vendor" && mkdir -p "$root/ws/QTmux/plugins/macpcan/vendor"
fall "leerer Vendor-Baum -> 0 verglichen = Exit 2" 2 "Kontrakt 2: 0 Dateien verglichen" "byte-identisch" -- \
    lauf_in "$(Q)" --

# 9. Fehlender Vendor-Baum (Kontrakt 1) -> Werkzeugfehler
attrappe
rm -rf "$root/ws/QTmux/third_party/updater/update"
fall "fehlender Updater-Baum -> Exit 2" 2 "Verzeichnis fehlt" "byte-identisch" -- \
    lauf_in "$(Q)" --

# 10. Unlesbare Datei -> Werkzeugfehler, nicht "gleich" (als root sinnlos)
if [ "$(id -u)" != 0 ]; then
    attrappe
    chmod 000 "$root/ws/MacPCAN/src/update/Checker.hpp"
    fall "unlesbare Datei -> Exit 2" 2 "Vergleich gescheitert" "byte-identisch" -- \
        lauf_in "$(Q)" --
    chmod 644 "$root/ws/MacPCAN/src/update/Checker.hpp"
fi

# 11. Fremd-Include im Kern -> Befund
attrappe
for d in "$root/ws/QTmux/third_party/updater/update" "$root/ws/MacPCAN/src/update"; do
    printf '#include "specs/DbcDecoder.hpp"\n' >> "$d/Manifest.cpp"
done
fall "Fremd-Include -> Exit 1" 1 "FREMD-INCLUDE (ausserhalb des Vendorings): specs/DbcDecoder.hpp" "byte-identisch" -- \
    lauf_in "$(Q)" --

# 12. Unbekanntes Argument (Tippfehler) -> nicht still pruefen
attrappe
fall "Tippfehler --updte -> Exit 2" 2 "unbekannte Argumente" "byte-identisch" -- \
    lauf_in "$(Q)" -- --updte

# 13. --update zieht nach, danach ist der Check gruen
attrappe
printf 'neu\n' > "$root/ws/MacPCAN/src/update/Neu.cpp"
printf 'verfaelscht\n' >> "$root/ws/QTmux/installer/macos/sign-bundle.sh"
fall "--update uebernimmt" 0 "Dateien uebernommen" "" -- \
    lauf_in "$(Q)" MACPCAN_DIR=../MacPCAN -- --update
fall "nach --update identisch" 0 "src/update (5 Dateien verglichen)" "" -- \
    lauf_in "$(Q)" --

echo "----"
echo "$((faelle - fehler))/$faelle Faelle bestanden"
[ "$fehler" = 0 ]
