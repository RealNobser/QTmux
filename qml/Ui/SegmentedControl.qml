import QtQuick
import QtQuick.Layouts
import QTmux

// Segment-Umschalter für Auswahlen mit wenigen Optionen (Design 1a, Teil C3): eine Reihe
// Schaltflächen in einem gemeinsamen Rahmen, aktives Segment in `Theme.accent`. Gedacht für
// ≤ 3 Optionen (Design-Modus, Restore-Modus) — darüber bleibt die `AppComboBox` besser.
//
// 🔑 `currentIndex` wird hier NIE selbst geschrieben, nur `activated(index)` gemeldet —
// genau wie bei `AppComboBox.onActivated`. Ein internes Setzen würde die Bindung der
// Aufrufstelle (`currentIndex: Theme.mode`) beim ersten Klick zerreißen; danach zeigte der
// Umschalter seinen eigenen Zustand statt den der Einstellung (dieselbe Falle wie bei der
// Abo-Matrix in QTMUX-47).
Item {
    id: seg

    property var model: []            // Liste von Zeichenketten
    property int currentIndex: 0
    signal activated(int index)

    implicitWidth: segRow.implicitWidth + 4
    implicitHeight: 30

    Rectangle {
        anchors.fill: parent
        radius: 6
        color: Theme.bgElevated
        border.color: Theme.border
        border.width: 1
    }

    RowLayout {
        id: segRow
        anchors.fill: parent
        anchors.margins: 2
        spacing: 2

        Repeater {
            model: seg.model
            delegate: Rectangle {
                id: segItem
                required property int index
                required property string modelData
                readonly property bool active: seg.currentIndex === segItem.index

                Layout.fillWidth: true
                Layout.fillHeight: true
                implicitWidth: segLabel.implicitWidth + 22
                radius: 4
                color: segItem.active ? Theme.accent
                     : segHover.hovered ? Theme.sidebarHover : "transparent"

                Text {
                    id: segLabel
                    anchors.centerIn: parent
                    text: segItem.modelData
                    color: segItem.active ? Theme.accentText : Theme.textBright
                    font.pixelSize: 12
                    font.weight: segItem.active ? Font.Medium : Font.Normal
                }
                HoverHandler { id: segHover; enabled: seg.enabled }
                TapHandler {
                    enabled: seg.enabled
                    onTapped: seg.activated(segItem.index)
                }
            }
        }
    }
}
