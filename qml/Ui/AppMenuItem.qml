import QtQuick
import QTmux

// Themen-Menüeintrag (In-Window): app-getöntes Icon, höher. Highlight-Hintergrund
// und Kürzel-Anzeige kommen von ShortcutMenuItem.
ShortcutMenuItem {
    id: ami
    implicitHeight: 34
    icon.color: Theme.textBright
    icon.width: 16
    icon.height: 16
}
