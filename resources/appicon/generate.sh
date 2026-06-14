#!/usr/bin/env bash
# Erzeugt aus qtmux.svg alle App-Icon-Formate: qtmux.icns (macOS),
# qtmux.ico (Windows) und qtmux.png (Linux, 512px).
#
# Es gibt kein rsvg/inkscape/magick auf den Build-Maschinen, daher wird ein
# winziges Qt-Tool (svgrender.cpp, nutzt QtSvg — dieselbe Engine wie die App)
# zum Rastern verwendet. Voraussetzung: Qt 6 mit QtSvg (brew install qt).
set -euo pipefail
cd "$(dirname "$0")"
QTP="${QT_PREFIX:-$(brew --prefix qt)}"
export QT_QPA_PLATFORM=offscreen

# 1) Renderer bauen
clang++ -std=c++17 svgrender.cpp -o /tmp/svgrender \
  -F"$QTP/lib" -framework QtCore -framework QtGui -framework QtSvg -Wl,-rpath,"$QTP/lib"
R=/tmp/svgrender

# 2) macOS .icns
rm -rf /tmp/qtmux.iconset && mkdir /tmp/qtmux.iconset
for m in 16:icon_16x16 32:icon_16x16@2x 32:icon_32x32 64:icon_32x32@2x \
         128:icon_128x128 256:icon_128x128@2x 256:icon_256x256 512:icon_256x256@2x \
         512:icon_512x512 1024:icon_512x512@2x; do
  $R qtmux.svg "/tmp/qtmux.iconset/${m##*:}.png" "${m%%:*}"
done
iconutil -c icns /tmp/qtmux.iconset -o qtmux.icns

# 3) Windows .ico (Vista+: PNG-komprimiert) + 4) Linux .png
for s in 16 24 32 48 64 128 256 512; do $R qtmux.svg "/tmp/ic_$s.png" "$s"; done
cp /tmp/ic_512.png qtmux.png
python3 - <<'PY'
import struct
sizes=[16,24,32,48,64,128,256]
imgs=[(s,open(f"/tmp/ic_{s}.png","rb").read()) for s in sizes]
out=bytearray(struct.pack("<HHH",0,1,len(imgs))); off=6+16*len(imgs)
for s,d in imgs:
    out+=struct.pack("<BBBBHHII",0 if s>=256 else s,0 if s>=256 else s,0,0,1,32,len(d),off); off+=len(d)
for s,d in imgs: out+=d
open("qtmux.ico","wb").write(out)
PY
echo "Fertig: qtmux.icns, qtmux.ico, qtmux.png"
