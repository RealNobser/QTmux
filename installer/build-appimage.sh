#!/usr/bin/env bash
# Baut einen verteilbaren Linux-Installer (AppImage) für QTmux.
#
# AppImage ist KEIN nativer CPack-Generator; Standard für Qt-Apps ist die
# linuxdeploy-Toolchain (linuxdeploy + linuxdeploy-plugin-qt + appimagetool).
# Analog zu build-dmg.sh (macOS) / build-msi.ps1 (Windows) ist dies ein bewusst
# hand-gerolltes Skript pro Plattform (volle Kontrolle über die Feinheiten).
#
# Ablauf:
#   1. Release bauen (Preset linux-release) — außer QTMUX_BUILD_DIR zeigt bereits
#      auf ein fertiges Build (die CI reicht ihr vorhandenes build/ herein).
#   2. AppDir aufbauen: qtmux-Binary + Echo-Plugin (PluginHost sucht <App>/plugins).
#   3. linuxdeploy mit dem Qt-Plugin laufen lassen (Qt-Frameworks + QML-Module +
#      Plattform-/Image-Plugins) und ein AppImage erzeugen.
#
# Ergebnis: dist/QTmux-<Version>-x86_64.AppImage  (dist/ ist git-ignoriert).
#
# Signierung: bewusst NICHT (Early-Adopter, wie DMG/MSI). AppImages laufen ohne
# Installation; unter Wayland/X11 genügt `chmod +x QTmux-*.AppImage && ./QTmux-*.AppImage`.
set -euo pipefail

VERSION="${1:-1.9.0}"
REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${QTMUX_BUILD_DIR:-$REPO/build/linux-release}"
OUT="$REPO/dist/QTmux-$VERSION-x86_64.AppImage"

# Gepinnte AppImage-Build-Tools (familienweit, kanonische Quelle: MacPCAN
# platform/linux/build-appimage.sh + docs/SHARED.md „Gepinnte AppImage-Build-Tools",
# Mechanik b74957d). Upstream-`continuous` ist eine rollende URL — der Pin geht über
# einen datierten Mirror-Ordner + SHA256. Anheben (Reihenfolge einhalten):
#   1. MacPCAN tools/updates/mirror-buildtools.sh --fetch-upstream <YYYY-MM-DD>
#   2. ein AppImage mit dem neuen Satz bauen und wirklich starten
#   3. LD_PIN + beide Checksummen ZUERST in MacPCAN anheben, dann 1:1 hierher
#      und nach RAFTNG spiegeln (gleiche Mechanik, gleiche Werte)
# Alte Pin-Ordner bleiben auf dem Mirror liegen — ältere Releases bauen weiter.
LD_PIN="2026-08-10"
LD_SHA256="421ca71d5c69ea97c6309276232990d43df1dcece0edfaa26bbf926ff96ed12e"
LD_QT_SHA256="be1b7e166bf9975cfb694ebe6759ba40502ffc6196440d3e64aa90c4dbd67e9f"
LD_MIRROR="${LD_MIRROR:-https://nobser.de/updates/tools/linuxdeploy}"

# Cache PRO Pin: ein angehobener LD_PIN darf nie einen alten Cache-Eintrag treffen.
TOOLS="${XDG_CACHE_HOME:-$HOME/.cache}/qtmux-appimage/$LD_PIN"

echo "==> 1/4  Release bauen (falls nötig)"
if [[ ! -x "$BUILD_DIR/qtmux" ]]; then
    cmake --preset linux-release
    cmake --build --preset linux-release
    BUILD_DIR="$REPO/build/linux-release"
fi
[[ -x "$BUILD_DIR/qtmux" ]] || { echo "FEHLER: $BUILD_DIR/qtmux nicht gefunden." >&2; exit 1; }

# qmake für linuxdeploy-plugin-qt auffinden (es lokalisiert damit Qt-Libs/-Plugins).
QMAKE_BIN="${QMAKE:-}"
[[ -z "$QMAKE_BIN" ]] && QMAKE_BIN="$(command -v qmake6 || command -v qmake || true)"
[[ -z "$QMAKE_BIN" && -n "${QT_ROOT_DIR:-}" && -x "$QT_ROOT_DIR/bin/qmake6" ]] && QMAKE_BIN="$QT_ROOT_DIR/bin/qmake6"
[[ -x "$QMAKE_BIN" ]] || { echo "FEHLER: qmake nicht gefunden (setze QMAKE oder QT_ROOT_DIR)." >&2; exit 1; }
export QMAKE="$QMAKE_BIN"
echo "    qmake: $QMAKE"

echo "==> 2/4  AppDir aufbauen"
APPDIR="$REPO/dist/AppDir"
rm -rf "$APPDIR"; mkdir -p "$APPDIR/usr/bin"
cp "$BUILD_DIR/qtmux" "$APPDIR/usr/bin/qtmux"
# Echo-Plugin mitliefern (MacPCAN ist macOS-only → auf Linux nicht vorhanden).
# PluginHost-Suchpfad 2 ist <App>/plugins relativ zum Binary (usr/bin/plugins).
if [[ -f "$BUILD_DIR/plugins/libqtmux_echo_plugin.so" ]]; then
    mkdir -p "$APPDIR/usr/bin/plugins"
    cp "$BUILD_DIR/plugins/libqtmux_echo_plugin.so" "$APPDIR/usr/bin/plugins/"
    echo "    Echo-Plugin übernommen"
fi

echo "==> 3/4  linuxdeploy + Qt-Plugin holen (gepinnt, SHA256-verifiziert)"
mkdir -p "$TOOLS"

sha256_of() {
    if command -v sha256sum >/dev/null 2>&1; then
        sha256sum "$1" | cut -d' ' -f1
    else
        shasum -a 256 "$1" | cut -d' ' -f1   # macOS hat kein sha256sum
    fi
}

# Prüft auch den CACHE, nicht nur frische Downloads — sonst hebelt eine einzige
# untergeschobene oder veraltete Cache-Datei den ganzen Pin still aus.
fetch_verified() {  # $1=URL $2=Zieldatei $3=Soll-SHA256
    local url="$1" out="$2" want="$3" got
    if [[ -f "$out" ]]; then
        got="$(sha256_of "$out")"
        if [[ "$got" == "$want" ]]; then
            chmod +x "$out"
            return 0
        fi
        echo "Gecachtes $(basename "$out") hat eine unerwartete Checksumme — lade neu." >&2
        rm -f "$out"
    fi
    echo "    lade $(basename "$out") vom gepinnten Mirror …"
    curl -fL --retry 3 -o "$out" "$url"
    got="$(sha256_of "$out")"
    if [[ "$got" != "$want" ]]; then
        rm -f "$out"
        echo "FEHLER: Checksummen-Abweichung bei $(basename "$out")." >&2
        echo "  erwartet $want" >&2
        echo "  erhalten $got" >&2
        echo "Baue nicht mit einem unverifizierten Build-Tool." >&2
        exit 1
    fi
    chmod +x "$out"
}

# Bewusst KEIN Upstream-Fallback: Bei einem Mirror-Ausfall auf `continuous`
# auszuweichen stellte still genau die Angriffsfläche wieder her, die der Pin schließt.
fetch_verified "$LD_MIRROR/$LD_PIN/linuxdeploy-x86_64.AppImage" \
               "$TOOLS/linuxdeploy-x86_64.AppImage" "$LD_SHA256"
fetch_verified "$LD_MIRROR/$LD_PIN/linuxdeploy-plugin-qt-x86_64.AppImage" \
               "$TOOLS/linuxdeploy-plugin-qt-x86_64.AppImage" "$LD_QT_SHA256"

# GitHub-Runner haben kein FUSE → die Tool-AppImages entpacken-und-ausführen lassen.
export APPIMAGE_EXTRACT_AND_RUN=1
# appimagetool braucht die Zielarchitektur explizit.
export ARCH="${ARCH:-x86_64}"
# Das Qt-Plugin scannt diese Verzeichnisse nach QML-Importen und bündelt die Module.
export QML_SOURCES_PATHS="$REPO/qml"
# linuxdeploy findet das Qt-Plugin über den Namen im PATH.
export PATH="$TOOLS:$PATH"
# Endgültiger AppImage-Name (sonst leitet linuxdeploy ihn aus dem .desktop ab).
export OUTPUT="QTmux-$VERSION-x86_64.AppImage"

echo "==> 4/4  AppImage bauen"
mkdir -p "$REPO/dist"; rm -f "$OUT"
( cd "$REPO/dist" && "$TOOLS/linuxdeploy-x86_64.AppImage" \
    --appdir "$APPDIR" \
    --executable "$APPDIR/usr/bin/qtmux" \
    --desktop-file "$REPO/installer/qtmux.desktop" \
    --icon-file "$REPO/resources/appicon/qtmux.png" \
    --plugin qt \
    --output appimage )

# linuxdeploy legt die AppImage im CWD (dist/) unter $OUTPUT ab.
[[ -f "$REPO/dist/$OUTPUT" ]] && mv -f "$REPO/dist/$OUTPUT" "$OUT" 2>/dev/null || true
[[ -f "$OUT" ]] || { echo "FEHLER: AppImage nicht erzeugt." >&2; exit 1; }
chmod +x "$OUT"
echo "Fertig: $OUT"
du -h "$OUT" | cut -f1 | xargs -I{} echo "  Größe: {}"
