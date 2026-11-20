import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QTmux

// „Was ist neu“ (Entwurf §7): Kasten mit Kopf (Titel links, Datum · Größe rechts),
// darunter „Das Wichtigste“ und die Gruppen mit Marke (Neu · Verbessert · Behoben),
// Unterüberschriften und Punkten. Den Inhalt zerlegt der Hub (`flow.notes`, HTML-Teilset
// aus <b>, <a href>, <code>); hier wird nur gezeichnet. Inhalt höchstens 300 px, dann
// rollt er, der Kopf bleibt stehen. Links öffnen nur, wenn sie mit https: beginnen.
Rectangle {
    id: root
    property string head: ""
    property string meta: ""
    property string emptyText: ""
    property string highlight: ""
    property var blocks: []
    readonly property bool empty: highlight.length === 0 && blocks.length === 0

    radius: 8
    color: Theme.bgSidebar
    border.width: 1
    border.color: Theme.border
    implicitHeight: headRow.height + 1 + Math.min(content.implicitHeight + 30, 300 + 30)

    function openLink(url) {
        if (url.indexOf("https:") === 0) Qt.openUrlExternally(url)
    }
    // StyledText kennt kein <code>; <tt> setzt denselben Inhalt in Mono.
    function styled(html) {
        return (html || "").replace(/<code>/g, "<tt>").replace(/<\/code>/g, "</tt>")
    }

    RowLayout {
        id: headRow
        anchors { left: parent.left; right: parent.right; top: parent.top }
        anchors.leftMargin: 16; anchors.rightMargin: 16
        height: 36
        Text {
            Layout.fillWidth: true
            text: root.head
            color: Theme.textBright
            font.pixelSize: 13
            font.weight: Font.DemiBold
            elide: Text.ElideRight
        }
        Text {
            text: root.meta
            color: Theme.textDim
            font.pixelSize: 12
            font.features: { "tnum": 1 }
        }
    }
    Rectangle {
        id: sep
        anchors { left: parent.left; right: parent.right; top: headRow.bottom }
        height: 1
        color: Theme.border
    }
    ScrollView {
        anchors { left: parent.left; right: parent.right; top: sep.bottom; bottom: parent.bottom }
        contentWidth: availableWidth
        clip: true
        ColumnLayout {
            id: content
            x: 16
            y: 14
            width: parent.width - 32
            spacing: 0

            Text {
                visible: root.empty
                Layout.fillWidth: true
                text: root.emptyText
                color: Theme.textDim
                font.pixelSize: 13
                wrapMode: Text.WordWrap
            }
            Text {
                visible: root.highlight.length > 0
                Layout.fillWidth: true
                Layout.bottomMargin: 10
                text: root.styled(root.highlight)
                textFormat: Text.StyledText
                color: Theme.textBright
                linkColor: Theme.accent
                font.pixelSize: 14
                font.weight: Font.Medium
                lineHeight: 21
                lineHeightMode: Text.FixedHeight
                wrapMode: Text.WordWrap
                onLinkActivated: (url) => root.openLink(url)
                HoverHandler { cursorShape: parent.hoveredLink.length > 0 ? Qt.PointingHandCursor : Qt.ArrowCursor }
                ToolTip.visible: hoveredLink.length > 0
                ToolTip.text: hoveredLink
            }
            Repeater {
                model: root.blocks
                delegate: Loader {
                    id: blk
                    required property var modelData
                    required property int index
                    Layout.fillWidth: true
                    Layout.topMargin: modelData.kind === "group" && index > 0 ? 14
                                    : modelData.kind === "subheading" ? 6 : 0
                    sourceComponent: modelData.kind === "group" ? groupC
                                   : modelData.kind === "subheading" ? subC : itemC
                    Component {
                        id: groupC
                        RowLayout {
                            readonly property string g: blk.modelData.group
                            readonly property bool badge: g === "new" || g === "improved" || g === "fixed"
                            spacing: 0
                            Rectangle {
                                visible: parent.badge
                                radius: 4
                                implicitHeight: 18
                                implicitWidth: badgeText.implicitWidth + 14
                                color: parent.g === "new" ? Qt.alpha(Theme.accent, 0.16)
                                     : parent.g === "fixed" ? Qt.alpha(Theme.ok, 0.16)
                                     : Qt.alpha(Theme.textDim, 0.18)
                                Text {
                                    id: badgeText
                                    anchors.centerIn: parent
                                    text: blk.modelData.title
                                    textFormat: Text.StyledText
                                    font.pixelSize: 11
                                    font.weight: Font.DemiBold
                                    color: parent.parent.g === "new" ? Theme.accent
                                         : parent.parent.g === "fixed" ? Theme.ok
                                         : Theme.textBright
                                }
                            }
                            Text {
                                visible: !parent.badge
                                text: blk.modelData.title
                                textFormat: Text.StyledText
                                color: Theme.textBright
                                font.pixelSize: 13
                                font.weight: Font.DemiBold
                            }
                        }
                    }
                    Component {
                        id: subC
                        Text {
                            text: blk.modelData.title
                            textFormat: Text.StyledText
                            color: Theme.textDim
                            font.pixelSize: 12
                            font.weight: Font.DemiBold
                        }
                    }
                    Component {
                        id: itemC
                        RowLayout {
                            spacing: 9
                            Rectangle {
                                Layout.alignment: Qt.AlignTop
                                Layout.topMargin: 8
                                width: 5; height: 5; radius: 2.5
                                color: Theme.textDim
                            }
                            Text {
                                Layout.fillWidth: true
                                Layout.topMargin: 4
                                text: root.styled(blk.modelData.html)
                                textFormat: Text.StyledText
                                color: Theme.textBright
                                linkColor: Theme.accent
                                font.pixelSize: 13
                                lineHeight: 20
                                lineHeightMode: Text.FixedHeight
                                wrapMode: Text.WordWrap
                                onLinkActivated: (url) => root.openLink(url)
                                HoverHandler { cursorShape: parent.hoveredLink.length > 0 ? Qt.PointingHandCursor : Qt.ArrowCursor }
                                ToolTip.visible: hoveredLink.length > 0
                                ToolTip.text: hoveredLink
                            }
                        }
                    }
                }
            }
            Item { implicitHeight: 16 }
        }
    }
}
