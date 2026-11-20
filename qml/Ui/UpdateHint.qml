import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QTmux

// Hinweis im Hauptfenster (Entwurf §8): „Update 0.8.0 verfügbar“, „Update wird geladen
// · 39 %“, „Update 0.8.0 bereit“, „Aktualisiert auf 0.8.0“, „Installation
// fehlgeschlagen“. Text, Icon, Farbrollen, × und Tooltip kommen fertig aus
// `Updates.flow.hint`; ein Klick öffnet den Dialog im Zustand, den der Ablauf wählt.
// Form: 22 px hoch, Radius 11, Icon 14, Text 12/16 500; nicht blinken, nicht pulsieren,
// höchstens 150 ms Einblenden.
Item {
    id: root
    readonly property var h: Updates.flow.hint
    readonly property bool shown: h.visible === true
    implicitWidth: shown ? chip.implicitWidth : 0
    implicitHeight: 28
    visible: shown
    opacity: shown ? 1 : 0
    // Per Tab erreichbar; Enter oder Leertaste wirken wie ein Klick (Entwurf §8).
    activeFocusOnTab: shown
    Keys.onReturnPressed: Updates.openFromHint()
    Keys.onEnterPressed: Updates.openFromHint()
    Keys.onSpacePressed: Updates.openFromHint()
    Behavior on opacity { NumberAnimation { duration: App.reduceMotion ? 0 : 150 } }

    function roleColor(r) {
        switch (r) {
        case "accent":  return Theme.accent
        case "success": return Theme.ok
        case "warn":    return Theme.warn
        case "danger":  return Theme.danger
        default:        return Theme.textDim
        }
    }
    function fillColor(r) {
        switch (r) {
        case "accentTint":  return Qt.alpha(Theme.accent, 0.16)
        case "successTint": return Qt.alpha(Theme.ok, 0.16)
        case "dangerTint":  return Qt.alpha(Theme.danger, 0.16)
        default:            return Theme.bgSidebar     // „raised“
        }
    }

    Rectangle {
        id: chip
        anchors.verticalCenter: parent.verticalCenter
        height: 22
        radius: 11
        color: root.fillColor(root.h.surfaceRole)
        implicitWidth: row.implicitWidth + 8 + (root.h.dismissable ? 6 : 10)
        Accessible.role: Accessible.Button
        Accessible.name: root.h.text || ""
        border.width: root.activeFocus ? 2 : 0
        border.color: Theme.accent

        RowLayout {
            id: row
            anchors.verticalCenter: parent.verticalCenter
            x: 8
            spacing: 6
            UpdateIcon {
                name: root.h.iconName || ""
                color: root.roleColor(root.h.iconRole)
                size: 14
            }
            Text {
                text: root.h.text || ""
                color: Theme.textBright
                font.pixelSize: 12
                font.weight: Font.Medium
                font.features: { "tnum": 1 }
            }
            Rectangle {
                visible: root.h.dismissable === true
                width: 1; height: 12
                color: Qt.alpha(Theme.textBright, 0.35)
            }
            ToolButton {
                id: dismiss
                visible: root.h.dismissable === true
                implicitWidth: 18
                implicitHeight: 18
                padding: 0
                icon.source: "qrc:/icons/x.svg"
                icon.width: 12
                icon.height: 12
                icon.color: Theme.textDim
                background: Rectangle {
                    radius: 9
                    color: dismiss.hovered ? Theme.sidebarHover : "transparent"
                }
                Accessible.name: root.h.dismissLabel || ""
                ToolTip.visible: hovered && (root.h.dismissLabel || "").length > 0
                ToolTip.delay: 600
                ToolTip.text: root.h.dismissLabel || ""
                onClicked: Updates.dismissHint()
            }
        }
        MouseArea {
            // Klick auf den Hinweis (nicht auf das ×) öffnet den Dialog.
            // Klickfläche 28 px hoch, auch wenn der Hinweis nur 22 px zeichnet.
            anchors.fill: parent
            anchors.topMargin: -3
            anchors.bottomMargin: -3
            anchors.rightMargin: root.h.dismissable === true ? 24 : 0
            cursorShape: Qt.PointingHandCursor
            hoverEnabled: true
            ToolTip.visible: containsMouse && (root.h.tooltip || "").length > 0
            ToolTip.delay: 600
            ToolTip.text: root.h.tooltip || ""
            onClicked: Updates.openFromHint()
        }
    }
}
