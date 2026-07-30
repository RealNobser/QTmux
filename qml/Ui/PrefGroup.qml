import QtQuick
import QtQuick.Layouts
import QTmux

// Gerahmte Gruppe von `PrefRow`-Zeilen (Design 1a, Teil C3): 1 px `Theme.border`,
// Radius 10, Trennlinien zwischen den Zeilen, optionale Überschrift darüber.
// In der Arbeitsanweisung nicht als eigene Datei genannt — der Rahmen braucht aber einen
// Ort, und dieser hält zugleich die Trennlinien-Logik an EINER Stelle.
//
//   PrefGroup {
//       title: qsTr("Zwischenablage")
//       PrefRow { … }
//       PrefRow { … }
//   }
ColumnLayout {
    id: group
    property string title: ""
    default property alias rows: inner.data

    Layout.fillWidth: true
    spacing: 6

    SectionLabel {
        visible: group.title.length > 0
        text: group.title
    }

    Rectangle {
        Layout.fillWidth: true
        implicitHeight: inner.implicitHeight
        radius: 10
        color: "transparent"
        border.color: Theme.border
        border.width: 1

        ColumnLayout {
            id: inner
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            spacing: 0

            // Trennlinie an jeder Zeile außer der ersten — zentral hier statt an jeder
            // Aufrufstelle. `showSeparator` haben nur PrefRow-Zeilen; alles andere
            // (Repeater, Hilfs-Items) wird übersprungen.
            Component.onCompleted: {
                let seen = 0
                for (let i = 0; i < inner.children.length; ++i) {
                    const c = inner.children[i]
                    if (c.showSeparator === undefined) continue
                    c.showSeparator = (seen > 0)
                    seen++
                }
            }
        }
    }
}
