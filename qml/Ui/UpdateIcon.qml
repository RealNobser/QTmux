import QtQuick
import QtQuick.Effects
import QTmux

// Ein Icon des Update-Wegs (Phosphor „regular“), eingefärbt in seiner Rollenfarbe.
// Den Namen nennt der Hub-Ablauf (view.iconName, hint.iconName …); `Updates.iconSource`
// lässt nur Namen aus `UpdateFlow::iconNames()` zu — ein unbekannter bleibt leer und
// steht als Warnung im Log, statt still ein falsches Bild zu zeigen.
// Färben wie im Einstellungsfenster: schwarzes SVG erst auf Weiß heben, dann
// einfärben (sonst gewichtet die Colorization mit der Luminanz ~0).
Item {
    id: root
    property string name: ""
    property color color: Theme.accent
    property int size: 20
    implicitWidth: size
    implicitHeight: size
    visible: name.length > 0

    Image {
        id: img
        anchors.fill: parent
        source: Updates.iconSource(root.name)
        sourceSize.width: root.size * 2
        sourceSize.height: root.size * 2
        visible: false
    }
    MultiEffect {
        anchors.fill: img
        source: img
        brightness: 1.0
        colorization: 1.0
        colorizationColor: root.color
    }
}
