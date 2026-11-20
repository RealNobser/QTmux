import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QTmux

// Ein Knopf des Update-Dialogs, Aussehen nach Rolle (Entwurf §6):
//   Primary   Akzent gefüllt, Text auf Akzent — genau einer, ganz rechts
//   Secondary Fläche, Text, Rand
//   Warning   Fläche, Text und Rand in Warnfarbe („Trotzdem herunterladen“)
//   Quiet/InText ohne Fläche und Rand, Text in Textfarbe; beim Überfahren Linie
// Was der Knopf TUT, steht nicht hier: `flowAction` kommt aus `flow.view.buttons`.
Button {
    id: b
    property string role: "Secondary"
    property int flowAction: 0
    property string glyph: ""
    readonly property bool quiet: role === "Quiet" || role === "InText"
    readonly property bool primary: role === "Primary"
    readonly property bool warning: role === "Warning"

    implicitHeight: 32
    leftPadding: quiet ? 8 : 14
    rightPadding: quiet ? 8 : 14
    font.pixelSize: 13
    font.weight: Font.Medium
    opacity: enabled ? 1.0 : 0.45
    focusPolicy: Qt.StrongFocus

    contentItem: RowLayout {
        spacing: 6
        readonly property color fg: b.primary ? Theme.accentText
                                    : b.warning ? Theme.warn
                                    : Theme.textBright
        // Optionales Zeichen vor dem Text (Caret am Aufklapper „Details“).
        UpdateIcon {
            visible: b.glyph.length > 0
            name: b.glyph
            color: parent.fg
            size: 14
        }
        Text {
            Layout.fillWidth: true
            text: b.text
            font: b.font
            color: parent.fg
            horizontalAlignment: b.glyph.length > 0 ? Text.AlignLeft : Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideNone      // Knopftexte brechen nie um (Entwurf §4)
        }
    }
    background: Rectangle {
        radius: 6
        color: b.primary ? (b.down ? Qt.darker(Theme.accent, 1.15) : Theme.accent)
             : b.quiet ? (b.hovered ? Theme.sidebarHover : "transparent")
             : (b.down ? Theme.sidebarSelected : b.hovered ? Theme.sidebarHover : Theme.bgElevated)
        border.width: b.quiet ? 0 : 1
        border.color: b.primary ? Theme.accent : b.warning ? Theme.warn : Theme.border
        // Fokus: 2-px-Ring in Akzent, 2 px Abstand (Entwurf §4).
        Rectangle {
            anchors.fill: parent
            anchors.margins: -4
            radius: 8
            color: "transparent"
            border.width: 2
            border.color: Theme.accent
            visible: b.visualFocus
        }
    }
}
