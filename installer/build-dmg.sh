#!/usr/bin/env bash
# Baut einen verteilbaren macOS-Installer (DMG) für QTmux.
#
# Ablauf:
#   1. Release bauen (Preset macos-release — DASSELBE wie für die normale
#      Release-Konvention; kein separates Build-Verzeichnis). Ausnahme:
#      QTMUX_BUILD_DIR lenkt den Build um, falls aus build/macos-release gerade
#      eine Instanz läuft (s. u.).
#   2. Das .app-Bundle nach dist/stage-mac stagen und mit `macdeployqt`
#      self-contained machen: Qt-Frameworks, QML-Module und Plugins (inkl.
#      unseres Echo-Plugins in Contents/PlugIns) werden hineinkopiert und die
#      rpaths umgeschrieben. -qmldir=qml lässt macdeployqt die genutzten
#      QML-Importe (QtQuick, Controls, …) auflösen.
#   3. Ein „Drag-to-Applications"-DMG per diskutil image bauen
#      (installer/macos/dmg-image.sh, kein Zusatztool nötig):
#      Inhalt = QTmux.app + Symlink auf /Applications.
#
# Ergebnis: dist/QTmux-<Version>-macos.dmg  (dist/ ist git-ignoriert).
#
# Apple-Signierung/Notarisierung: bewusst NICHT (Early-Adopter-Build, wie das
# unsignierte Windows-MSI); signiert wird mit der selbst ausgestellten
# Familien-Identität (MacPCAN docs/codesign.md). Folge: Ein per Browser geladenes DMG trägt die
# Gatekeeper-Quarantäne — Nutzer öffnen das Programm beim ersten Mal über Systemeinstellungen ›
# Datenschutz & Sicherheit › „Dennoch öffnen“ (seit macOS 15 hilft Rechtsklick → „Öffnen“ nicht
# mehr) bzw. entfernen das Quarantäne-Attribut (`xattr -dr com.apple.quarantine
# /Applications/QTmux.app`). Der eingebaute Updater lädt ohne Quarantäne (QTMUX-146).
set -euo pipefail

VERSION="${1:-1.10.0}"
REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PRESET="macos-release"
# Ausweich-Build-Verzeichnis wie beim AppImage-Skript (QTMUX_BUILD_DIR). Nötig, wenn
# aus build/macos-release gerade eine QTmux-Instanz LÄUFT: Der Release-Build würde
# deren Binary im Betrieb überschreiben und den Prozess mitsamt allen Terminal-
# Sessions abreißen. Dann hierhin ausweichen, statt die laufende Arbeit zu verlieren:
#   QTMUX_BUILD_DIR=build/macos-dmg installer/build-dmg.sh 1.10.0
# Pflicht: QTMUX_QT_MAC=<Qt-Prefix> (offizielles Qt in der CI-Version, s. u.).
BUILD_DIR="${QTMUX_BUILD_DIR:-$REPO/build/$PRESET}"
[[ "$BUILD_DIR" = /* ]] || BUILD_DIR="$REPO/$BUILD_DIR"
APP_SRC="$BUILD_DIR/qtmux.app"
STAGE="$REPO/dist/stage-mac"
APP="$STAGE/QTmux.app"           # Anzeigename im Finder/Applications
DMG="$REPO/dist/QTmux-$VERSION-macos.dmg"

# Das Qt des Pakets: der offizielle Bau in der Version der CI (env.QT_VERSION in
# .github/workflows/ci.yml, Familienregel „Qt 6.10.3 überall“), mit den Modulen,
# die CMakeLists.txt verlangt (u. a. SerialPort, ShaderTools). Der Ort kommt NUR
# über QTMUX_QT_MAC (Prefix mit lib/, bin/macdeployqt), nie aus dem Repo.
# 🔑 Bis 1.10.0 bündelte das Skript Homebrew-Qt: 6.11 statt 6.10.3 und minos 14.0
# — die macOS-Untergrenze des Pakets stand damit still über dem Ziel 13.0, und
# CMake fand Qt über die Homebrew-Pfade des Presets. Darum drei Riegel, bevor
# gebaut wird: Version = CI-Version, minos von QtCore ≤ Ziel, und nach dem
# Konfigurieren muss Qt6_DIR im Bauordner unter diesem Prefix liegen (ein
# Bauordner von früher behielte sonst still das alte Qt).
QT_PREFIX="${QTMUX_QT_MAC:-}"
[[ -n "$QT_PREFIX" && -d "$QT_PREFIX/lib" ]] \
    || { echo "FEHLER: QTMUX_QT_MAC zeigt auf kein Qt (Prefix mit lib/ und bin/macdeployqt setzen)." >&2; exit 1; }
QT_PREFIX="$(cd "$QT_PREFIX" && pwd -P)"
MACDEPLOYQT="$QT_PREFIX/bin/macdeployqt"
[[ -x "$MACDEPLOYQT" ]] || { echo "FEHLER: $MACDEPLOYQT fehlt." >&2; exit 1; }
QT_WANT="$(sed -n 's/^[[:space:]]*QT_VERSION:[[:space:]]*\([0-9.]*\).*/\1/p' "$REPO/.github/workflows/ci.yml" | head -1)"
# Je nach Qt steht die Version in der …Impl.cmake oder direkt in der …Version.cmake;
# nur vorhandene Dateien lesen (ein sed-Fehler bräche unter pipefail STILL ab).
QT_HAVE=""
for f in "$QT_PREFIX/lib/cmake/Qt6Core/Qt6CoreConfigVersionImpl.cmake" \
         "$QT_PREFIX/lib/cmake/Qt6Core/Qt6CoreConfigVersion.cmake"; do
    if [[ -z "$QT_HAVE" && -f "$f" ]]; then
        QT_HAVE="$(sed -n 's/^set(PACKAGE_VERSION "\([0-9.]*\)").*/\1/p' "$f" | head -1)"
    fi
done
[[ -n "$QT_WANT" && "$QT_HAVE" == "$QT_WANT" ]] \
    || { echo "FEHLER: Qt unter $QT_PREFIX ist '$QT_HAVE', erwartet die CI-Version '$QT_WANT'." >&2; exit 1; }
MIN_SOURCE="$(sed -n 's/^[[:space:]]*set(CMAKE_OSX_DEPLOYMENT_TARGET "\([0-9.]*\)" CACHE.*/\1/p' "$REPO/CMakeLists.txt" | head -1)"
QT_MINOS="$(vtool -show-build "$QT_PREFIX/lib/QtCore.framework/Versions/A/QtCore" 2>/dev/null | awk '$1=="minos"{print $2}' | sort -t. -k1,1n -k2,2n | tail -1 || true)"
[[ -n "$MIN_SOURCE" && -n "$QT_MINOS" ]] \
    || { echo "FEHLER: Untergrenze nicht lesbar (Quelle '$MIN_SOURCE', QtCore '$QT_MINOS')." >&2; exit 1; }
[[ "$(printf '%s\n%s\n' "$QT_MINOS" "$MIN_SOURCE" | sort -t. -k1,1n -k2,2n | tail -1)" == "$MIN_SOURCE" ]] \
    || { echo "FEHLER: QtCore unter $QT_PREFIX verlangt macOS $QT_MINOS, das Ziel ist $MIN_SOURCE." >&2; exit 1; }
echo "    Qt $QT_HAVE (minos $QT_MINOS) aus QTMUX_QT_MAC"

# Vendoring-Wächter VOR dem Release-Build: Das DMG entsteht ausschließlich auf
# der Entwicklermaschine — der einzige Ort, an dem der MacPCAN-Hub daneben liegt,
# und der Moment, in dem Drift am teuersten ist (das Artefakt geht raus). Drift
# (Wächter-Exit 1) und jeder Werkzeugfehler (Exit 2, z. B. ein gesetzter, aber
# falscher MACPCAN_DIR — QTMUX-144) brechen hier ab (set -e). Fehlt der Hub am
# Standardort (fremde Maschine), läuft der Build weiter — der Wächter meldet
# dann unübersehbar, dass NICHTS geprüft wurde. In der CI hängt der Wächter
# bewusst nicht (öffentliches Repo, kein Token für den privaten Hub — Begründung
# im Wächter-Kopf).
echo "==> 1/4  Vendoring-Sync gegen MacPCAN prüfen"
"$REPO/tools/check-updater-sync.sh"

echo "==> 2/4  Release bauen ($PRESET -> $BUILD_DIR)"
cmake --preset "$PRESET" -B "$BUILD_DIR" -DCMAKE_PREFIX_PATH="$QT_PREFIX" >/dev/null
QT_DIR_USED="$(sed -n 's/^Qt6_DIR:[A-Z]*=//p' "$BUILD_DIR/CMakeCache.txt" | head -1)"
[[ "$QT_DIR_USED" == "$QT_PREFIX"/* ]] \
    || { echo "FEHLER: Der Bauordner nutzt Qt aus '$QT_DIR_USED', nicht aus $QT_PREFIX (frischen QTMUX_BUILD_DIR nehmen)." >&2; exit 1; }
cmake --build "$BUILD_DIR"

echo "==> 3/4  Bundle stagen + macdeployqt"
rm -rf "$STAGE"; mkdir -p "$STAGE"
cp -R "$APP_SRC" "$APP"
# -qmldir: QML-Importe auflösen; macdeployqt bündelt Qt-Frameworks + Plugins
# (auch unser Contents/PlugIns/libqtmux_echo_plugin.dylib wird mit-prozessiert).
# Die ERROR-Zeilen über QtVirtualKeyboard/QtMultimedia/QtPdf sind harmlos: das sind
# optionale QML-Module, die QtQuick.Controls referenziert, die wir aber nicht nutzen
# und die hier nicht installiert sind — der Lauf bündelt die benötigten Module trotzdem.
"$MACDEPLOYQT" "$APP" -qmldir="$REPO/qml" || true

# Ein Release ohne CAN-Plugin entsteht sonst STILL: Fehlt PCBUSB, überspringt CMake das Plugin
# ohne Fehler, und alles bleibt grün (beim 1.9.9-Bau in einem frischen Baum passiert, aufgefallen
# nur am Dateilisten-Vergleich gegen 1.9.8). Nur für Release-Bauten; ohne `require` bleibt der
# Bau ohne PCBUSB erlaubt (öffentliches Repo, PCBUSB ist nicht enthalten).
if [[ "${FAMILY_CODESIGN:-}" == "require" ]]; then
    for f in "$APP/Contents/PlugIns/libqtmux_macpcan_plugin.dylib" \
             "$APP/Contents/Frameworks/libPCBUSB.0.13.dylib"; do
        [[ -f "$f" ]] || { echo "FEHLER: Release-Bau ohne CAN-Plugin: $f fehlt (QTMUX_PCBUSB_DIR setzen)." >&2; exit 1; }
    done
fi

# macdeployqt schreibt rpaths NACH seiner internen Ad-hoc-Signatur um → die Signatur
# einzelner mitkopierter dylibs (z. B. libbrotlicommon) wird ungültig, und auf Apple
# Silicon startet ein Bundle ohne gültige Signatur nicht. Daher das gesamte Bundle neu
# signieren — innen → außen mit der Familien-Identität (stabile Designated Requirement:
# macOS-Datenschutz-Erlaubnisse überleben Updates; ad-hoc verlor sie bei JEDEM Update, und
# TCC rechnet jeden Prozess in einer QTmux-Session QTmux zu). Fehlt der Schlüsselbund,
# fällt das Skript LAUT auf ad-hoc zurück; Release-Bau mit FAMILY_CODESIGN=require.
# installer/macos/sign-bundle.sh ist aus MacPCAN vendiert (Kontrakt 4 im Sync-Wächter).
# KEINE Notarisierung, keine Hardened Runtime (Shells/PTYs laufen wie bisher).
echo "    Signatur (Familien-Identität, sonst ad-hoc mit Warnung) …"
"$REPO/installer/macos/sign-bundle.sh" "$APP"

echo "==> 4/4  DMG bauen (diskutil image, Drag-to-Applications)"
mkdir -p "$REPO/dist"
rm -f "$DMG"
DMG_ROOT="$(mktemp -d)"
cp -R "$APP" "$DMG_ROOT/QTmux.app"
ln -s /Applications "$DMG_ROOT/Applications"
# ⚠️ Das Image entsteht über installer/macos/dmg-image.sh, nie direkt über
# hdiutil (2026-11-20, gemessen unter macOS 27.2.1). Erstens meldet sich
# `hdiutil create` bei jedem Aufruf als „deprecated“. Zweitens kann das eigene
# Zwischen-Volume beim Aushängen einmalig belegt sein (EBUSY); hdiutil brach den
# Bau dann ohne Wiederholung ab. dmg-image.sh wiederholt nur diesen einen Fehler
# (diskutil „error code 156“), höchstens dreimal mit 15 s Pause. Jeder andere
# Fehler bricht sofort ab und nennt den Text des Werkzeugs; jeder Fall hat einen
# eigenen Exit-Code. Bei gleichem Inhalt ist das Image kleiner als mit hdiutil:
# diskutil packt stärker (etwa zlib-Stufe 5), und die hdiutil-Datei trug rund
# 7,8 MB außerhalb ihrer Chunks.
# Das Skript ist aus MacPCAN vendiert (Kontrakt 5 im Sync-Wächter).
"$REPO/installer/macos/dmg-image.sh" create "$DMG_ROOT" "QTmux $VERSION" "$DMG"
rm -rf "$DMG_ROOT"
# Ab hier gilt: Ein DMG, das nicht alle Prüfungen am eingehängten Image bestanden
# hat, bleibt nicht in dist/ liegen, auch wenn das Skript unterwegs abbricht (etwa
# weil das Aushängen scheitert; set -e beendet dann vor jedem eigenen rm).
DMG_CHECKED=""
trap '[[ -n "$DMG_CHECKED" ]] || rm -f "$DMG"' EXIT

# Die App muss als QTmux.app in der Wurzel des Volumes liegen, neben dem Link
# auf /Applications. diskutil legt den INHALT der Quelle ab; wer ihm die App
# selbst gibt, bekommt Contents/ in der Wurzel und ein DMG ohne startbare App,
# bei Exit 0 (2026-11-20 gemessen; dmg-image.sh lehnt eine solche Quelle schon
# ab). Diese Prüfung am eingehängten Image fängt denselben Fehler, falls der
# Aufruf je anders zusammengesetzt wird, und nennt ihn beim Namen. Eingehängt
# wird schreibgeschützt und verborgen; der leere Einhängeordner wird nur per
# rmdir entfernt, nie per rm -rf (er könnte noch eingehängt sein).
echo "    Prüfung am eingehängten Image …"
DMG_MP="$(mktemp -d)"
"$REPO/installer/macos/dmg-image.sh" attach "$DMG" "$DMG_MP"
if [[ ! -f "$DMG_MP/QTmux.app/Contents/Info.plist" || ! -L "$DMG_MP/Applications" ]]; then
    echo "FEHLER: Im DMG liegt keine QTmux.app mit Link auf /Applications in der Wurzel." >&2
    ls -A "$DMG_MP" >&2
    "$REPO/installer/macos/dmg-image.sh" eject "$DMG_MP" && rmdir "$DMG_MP"
    # Ein rotes DMG bleibt nicht in dist/ liegen, wo ein späterer Publish es fände.
    rm -f "$DMG"
    exit 1
fi
echo "    QTmux.app und Link auf /Applications liegen in der Wurzel."
# Am selben eingehängten Image: Die ausgelieferte App trägt den Tauschhelfer des
# Selbst-Ersetzens (Contents/Helpers/update-swap) und NICHT den Szenen-Baustein
# der Bildläufe (derselbe Wächter wie test_release_has_no_showcase, mit der festen
# Prüfdatei als Gegenprobe; das Bildziel qtmux-bilder liegt in keinem Paket).
DMG_FAIL=""
[[ -x "$DMG_MP/QTmux.app/Contents/Helpers/update-swap" ]] \
    || DMG_FAIL="Im DMG fehlt QTmux.app/Contents/Helpers/update-swap."
if [[ -z "$DMG_FAIL" ]] && ! cmake -DAPP="$DMG_MP/QTmux.app/Contents/MacOS/qtmux" \
        -DPROBE="$REPO/tests/fixtures/showcase-probe/probe.bin" \
        -P "$REPO/tests/CheckNoShowcase.cmake"; then
    DMG_FAIL="Die App im DMG enthält den Szenen-Baustein oder --update-picture (s. o.)."
fi
# macOS-Untergrenze am ausgelieferten Inhalt (tools/ci/dmg_minos.py, aus dem Hub
# vendiert, Kontrakt 6): jedes eigene Mach-O (MacOS/, Helpers/, die zwei eigenen
# Plugins) trägt genau die Untergrenze und NUR arm64 (Owner 2026-11-24: QTmux bleibt
# bei Apple Silicon), jedes fremde (Qt, PCBUSB) verlangt höchstens sie, und die
# Info.plist verspricht dieselbe. Der Wert kommt aus dem Bauordner und muss der
# Quelle in CMakeLists.txt gleichen; ein Bauordner mit altem Cache-Eintrag ergäbe
# sonst ein Release mit falscher Untergrenze, das gegen sich selbst grün misst.
MIN_TARGET="$(sed -n 's/^CMAKE_OSX_DEPLOYMENT_TARGET:[A-Z]*=//p' "$BUILD_DIR/CMakeCache.txt" | head -1)"
if [[ -z "$DMG_FAIL" ]]; then
    if [[ -z "$MIN_SOURCE" || "$MIN_SOURCE" != "$MIN_TARGET" ]]; then
        DMG_FAIL="Untergrenze im Bauordner '$MIN_TARGET' ≠ Quelle in CMakeLists.txt '$MIN_SOURCE'."
    else
        # Eigene Plugins ausdrücklich als „eigen“ melden (sonst gälten sie als fremd:
        # ohne Architektur-Prüfung, nur „höchstens“). Ein Release trägt genau zwei
        # (Echo und CAN, s. o.); fehlt eines, ist das ein Befund, kein stilles Weniger.
        own=()
        for p in "$DMG_MP/QTmux.app/Contents/PlugIns/"libqtmux_*.dylib; do
            if [[ -f "$p" ]]; then own+=(--own "Contents/PlugIns/$(basename "$p")"); fi
        done
        own_want=1
        [[ "${FAMILY_CODESIGN:-}" == "require" ]] && own_want=2
        if (( ${#own[@]} / 2 < own_want )); then
            DMG_FAIL="Im DMG liegen $(( ${#own[@]} / 2 )) eigene Plugins (libqtmux_*.dylib), erwartet $own_want."
        elif ! python3 -I "$REPO/tools/ci/dmg_minos.py" "$DMG_MP/QTmux.app" --expect "$MIN_TARGET" \
                --archs arm64 "${own[@]}"; then
            DMG_FAIL="macOS-Untergrenze bzw. Architektur im DMG falsch (dmg_minos.py, s. o.)."
        fi
    fi
fi
"$REPO/installer/macos/dmg-image.sh" eject "$DMG_MP"
rmdir "$DMG_MP"
if [[ -n "$DMG_FAIL" ]]; then
    rm -f "$DMG"
    echo "FEHLER: $DMG_FAIL DMG gelöscht." >&2
    exit 1
fi
echo "    Tauschhelfer im Bundle, kein Szenen-Baustein in der App, Untergrenze macOS $MIN_TARGET (arm64)."
DMG_CHECKED=1

echo "Fertig: $DMG"
du -h "$DMG" | cut -f1 | xargs -I{} echo "  Größe: {}"
