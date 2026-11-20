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

VERSION="${1:-1.9.15}"
REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PRESET="macos-release"
# Ausweich-Build-Verzeichnis wie beim AppImage-Skript (QTMUX_BUILD_DIR). Nötig, wenn
# aus build/macos-release gerade eine QTmux-Instanz LÄUFT: Der Release-Build würde
# deren Binary im Betrieb überschreiben und den Prozess mitsamt allen Terminal-
# Sessions abreißen. Dann hierhin ausweichen, statt die laufende Arbeit zu verlieren:
#   QTMUX_BUILD_DIR=build/macos-dmg installer/build-dmg.sh 1.9.15
BUILD_DIR="${QTMUX_BUILD_DIR:-$REPO/build/$PRESET}"
[[ "$BUILD_DIR" = /* ]] || BUILD_DIR="$REPO/$BUILD_DIR"
APP_SRC="$BUILD_DIR/qtmux.app"
STAGE="$REPO/dist/stage-mac"
APP="$STAGE/QTmux.app"           # Anzeigename im Finder/Applications
DMG="$REPO/dist/QTmux-$VERSION-macos.dmg"

# macdeployqt aus dem Qt-Prefix auflösen (PATH oder Homebrew).
MACDEPLOYQT="$(command -v macdeployqt || true)"
if [[ -z "$MACDEPLOYQT" ]]; then
    QT_PREFIX="$(brew --prefix qt 2>/dev/null || true)"
    [[ -n "$QT_PREFIX" && -x "$QT_PREFIX/bin/macdeployqt" ]] && MACDEPLOYQT="$QT_PREFIX/bin/macdeployqt"
fi
[[ -n "$MACDEPLOYQT" ]] || { echo "FEHLER: macdeployqt nicht gefunden (Qt-bin im PATH?)." >&2; exit 1; }

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
cmake --preset "$PRESET" -B "$BUILD_DIR" >/dev/null
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
# eigenen Exit-Code. Bei gleichem Inhalt ist das Image kleiner als mit hdiutil,
# weil diskutil stärker packt und das Volume knapper anlegt.
# Das Skript ist aus MacPCAN vendiert (Kontrakt 5 im Sync-Wächter).
"$REPO/installer/macos/dmg-image.sh" create "$DMG_ROOT" "QTmux $VERSION" "$DMG"
rm -rf "$DMG_ROOT"

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
    exit 1
fi
"$REPO/installer/macos/dmg-image.sh" eject "$DMG_MP"
rmdir "$DMG_MP"
echo "    QTmux.app und Link auf /Applications liegen in der Wurzel."

echo "Fertig: $DMG"
du -h "$DMG" | cut -f1 | xargs -I{} echo "  Größe: {}"
