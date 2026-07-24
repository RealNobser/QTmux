import QtQuick
import QtQuick.Controls.Basic
import QTmux

// Toolbar-Knopf: zeigt ein SVG-Icon; Tönung folgt Hover/aktiv/Theme.
// Aus Main.qml herausgezogen (QTMUX-47, Schritt 1), damit Haupt- und
// Einstellungsfenster dieselbe Komponente nutzen.
ToolButton {
    id: tb
    property string tip: ""
    property bool active: false        // dauerhaft hervorgehoben (z. B. Server an)
    display: AbstractButton.IconOnly
    icon.width: 18
    icon.height: 18
    icon.color: !tb.enabled ? Theme.border
              : (tb.down || tb.active) ? Theme.accent
              : tb.hovered ? Theme.textBright : Theme.textDim
    implicitWidth: 36
    implicitHeight: 30
    background: Rectangle {
        radius: 6
        color: tb.down ? Theme.sidebarSelected
             : tb.hovered ? Theme.sidebarHover : "transparent"
    }
    ToolTip.visible: hovered && tip.length > 0
    ToolTip.delay: 600
    ToolTip.text: tip
}
