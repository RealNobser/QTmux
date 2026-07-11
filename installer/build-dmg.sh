#!/usr/bin/env bash
# Baut einen verteilbaren macOS-Installer (DMG) für QTmux.
#
# Ablauf:
#   1. Release bauen (Preset macos-release — DASSELBE wie für die normale
#      Release-Konvention; kein separates Build-Verzeichnis).
#   2. Das .app-Bundle nach dist/stage-mac stagen und mit `macdeployqt`
#      self-contained machen: Qt-Frameworks, QML-Module und Plugins (inkl.
#      unseres Echo-Plugins in Contents/PlugIns) werden hineinkopiert und die
#      rpaths umgeschrieben. -qmldir=qml lässt macdeployqt die genutzten
#      QML-Importe (QtQuick, Controls, …) auflösen.
#   3. Ein „Drag-to-Applications"-DMG via hdiutil bauen (kein Zusatztool nötig):
#      Inhalt = QTmux.app + Symlink auf /Applications.
#
# Ergebnis: dist/QTmux-<Version>-macos.dmg  (dist/ ist git-ignoriert).
#
# Signierung/Notarisierung: bewusst NICHT (Early-Adopter-Build, wie das
# unsignierte Windows-MSI). Folge: Gatekeeper-Quarantäne — Nutzer öffnen das
# Programm beim ersten Mal per Rechtsklick → „Öffnen" bzw. entfernen das
# Quarantäne-Attribut (`xattr -dr com.apple.quarantine /Applications/QTmux.app`).
set -euo pipefail

VERSION="${1:-1.3.0}"
REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PRESET="macos-release"
BUILD_DIR="$REPO/build/$PRESET"
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

echo "==> 1/3  Release bauen ($PRESET)"
cmake --preset "$PRESET" >/dev/null
cmake --build --preset "$PRESET"

echo "==> 2/3  Bundle stagen + macdeployqt"
rm -rf "$STAGE"; mkdir -p "$STAGE"
cp -R "$APP_SRC" "$APP"
# -qmldir: QML-Importe auflösen; macdeployqt bündelt Qt-Frameworks + Plugins
# (auch unser Contents/PlugIns/libqtmux_echo_plugin.dylib wird mit-prozessiert).
# Die ERROR-Zeilen über QtVirtualKeyboard/QtMultimedia/QtPdf sind harmlos: das sind
# optionale QML-Module, die QtQuick.Controls referenziert, die wir aber nicht nutzen
# und die hier nicht installiert sind — der Lauf bündelt die benötigten Module trotzdem.
"$MACDEPLOYQT" "$APP" -qmldir="$REPO/qml" || true

# macdeployqt schreibt rpaths NACH seiner internen Ad-hoc-Signatur um → die Signatur
# einzelner mitkopierter dylibs (z. B. libbrotlicommon) wird ungültig, und auf Apple
# Silicon startet ein Bundle ohne gültige Signatur nicht. Daher das gesamte Bundle
# selbst ad-hoc neu signieren (Signatur "-"; KEINE Notarisierung — Early-Adopter).
echo "    Ad-hoc-Signatur erneuern …"
codesign --force --deep --sign - "$APP"
codesign -v --deep "$APP" && echo "    Signatur gültig"

echo "==> 3/3  DMG bauen (hdiutil, Drag-to-Applications)"
mkdir -p "$REPO/dist"
rm -f "$DMG"
DMG_ROOT="$(mktemp -d)"
cp -R "$APP" "$DMG_ROOT/QTmux.app"
ln -s /Applications "$DMG_ROOT/Applications"
hdiutil create -volname "QTmux $VERSION" -srcfolder "$DMG_ROOT" \
    -ov -format UDZO "$DMG" >/dev/null
rm -rf "$DMG_ROOT"

echo "Fertig: $DMG"
du -h "$DMG" | cut -f1 | xargs -I{} echo "  Größe: {}"
