import QtQuick
import QtQuick.Controls.Basic
import QTmux

// Themen-ToolTip (QTMUX-101). Wie bei ThemedMenu/AppPopupBg gilt: Popups erben die
// Window-`palette` NICHT — ohne eigene Farben stünde im Dunkel-Modus dunkle Schrift auf
// dunklem Grund. Darum Hintergrund und Textfarbe explizit aus `Theme`.
ToolTip {
    id: control
    delay: 600          // beim blossen Überfahren soll nichts aufblitzen
    padding: 8
    // Nicht über den Bildschirmrand hinaus laufen lassen; lange Pfade brechen um.
    contentItem: Text {
        text: control.text
        color: Theme.textBright
        font.pixelSize: 12
        wrapMode: Text.WordWrap
        maximumLineCount: 4
        elide: Text.ElideMiddle          // Pfade in der Mitte kürzen, Anfang UND Ende bleiben lesbar
        width: Math.min(implicitWidth, 360)
    }
    background: AppPopupBg {}
}
