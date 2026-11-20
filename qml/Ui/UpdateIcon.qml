import QtQuick
import QtQuick.Controls.impl
import QTmux

// Ein Icon des Update-Wegs (Phosphor „regular“), eingefärbt in seiner Rollenfarbe.
// Den Namen nennt der Hub-Ablauf (view.iconName, hint.iconName …); `Updates.iconSource`
// lässt nur Namen aus `UpdateFlow::iconNames()` zu — ein unbekannter bleibt leer und
// steht als Warnung im Log, statt still ein falsches Bild zu zeigen.
// Gefärbt wie die Icons der Knöpfe (`icon.color`), über IconImage: Das rendert auch
// offscreen. Ein MultiEffect fehlte dort ersatzlos und ohne Warnung (docs/e2e-fallen.md),
// jedes Bild des Update-Dialogs wäre ohne Icons gewesen. IconImage steckt in einem Item,
// weil es selbst eine FINAL-Eigenschaft `name` hat.
Item {
    id: root
    property string name: ""
    property color color: Theme.accent
    property int size: 20
    implicitWidth: size
    implicitHeight: size
    visible: name.length > 0

    IconImage {
        anchors.fill: parent
        color: root.color
        source: Updates.iconSource(root.name)
        sourceSize.width: root.size
        sourceSize.height: root.size
        fillMode: Image.PreserveAspectFit
    }
}
