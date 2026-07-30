import QtQuick
import QtQuick.Layouts
import QTmux

// Einheitliche Einstellungs-Zeile (Design 1a, Teil C3): links Titel + Beschreibung,
// rechts das Control, vertikal zentriert; Zeilenpadding 13 × 16 px. Ersetzt das bisherige
// Muster „CheckBox + freistehender Erklärtext darunter" — die Erklärung gehört in die
// Zeile, nicht in den Fließtext dahinter.
//
// Benutzung (das Control ist Default-Kind und landet rechts):
//   PrefRow {
//       title: qsTr("Ligaturen"); description: qsTr("Verbindet …")
//       Switch { checked: …; onToggled: … }
//   }
//
// 🔑 Der Halter rechts ist ein RowLayout, kein Item mit `childrenRect`: Letzteres gerät mit
// Controls, die sich selbst füllen, in Bindungsschleifen. Im RowLayout wirkt am Kind
// zusätzlich `Layout.fillWidth: true` wie erwartet (zusammen mit `controlWidth`).
Item {
    id: row

    property string title: ""
    property string description: ""
    // Feste Breite der rechten Spalte (0 = das Control bestimmt sie). Für ComboBoxen und
    // Segment-Umschalter sinnvoll, damit die Zeilen einer Gruppe gleich breit ausfallen.
    property int controlWidth: 0
    // Trennlinie oben — setzt PrefGroup für alle Zeilen außer der ersten.
    property bool showSeparator: false
    // Ausgegraut wie ein Control: färbt den Titel UND deaktiviert das Kind.
    property bool rowEnabled: true

    default property alias control: holder.data

    Layout.fillWidth: true
    // Zeilenhöhe: der höhere von Text und Control plus 2 × 13 px.
    implicitHeight: Math.max(texts.implicitHeight, holder.implicitHeight) + 26

    Rectangle {
        visible: row.showSeparator
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: 1
        color: Theme.border
    }

    Column {
        id: texts
        anchors.left: parent.left
        anchors.leftMargin: 16
        anchors.right: holder.left
        anchors.rightMargin: 16
        anchors.verticalCenter: parent.verticalCenter
        spacing: 2
        Text {
            visible: row.title.length > 0
            width: parent.width
            text: row.title
            color: row.rowEnabled ? Theme.textBright : Theme.textDim
            font.pixelSize: 13
            wrapMode: Text.WordWrap
        }
        Text {
            visible: row.description.length > 0
            width: parent.width
            text: row.description
            color: Theme.textDim
            // Die Anweisung nennt 11,5 px — `font.pixelSize` ist aber ein **int**, eine
            // Gleitkommazahl scheitert erst zur Laufzeit („Invalid property assignment: int
            // expected") und reißt dann den GANZEN App-Start mit (PrefsWindow referenziert
            // alle neun Kategorien). Darum 11.
            font.pixelSize: 11
            opacity: row.rowEnabled ? 1.0 : 0.6
            wrapMode: Text.WordWrap
        }
    }

    RowLayout {
        id: holder
        anchors.right: parent.right
        anchors.rightMargin: 16
        anchors.verticalCenter: parent.verticalCenter
        width: row.controlWidth > 0 ? row.controlWidth : implicitWidth
        enabled: row.rowEnabled
        spacing: 8
    }
}
