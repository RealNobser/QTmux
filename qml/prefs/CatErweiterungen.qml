import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QTmux

// Kategorie „Erweiterungen" (QTMUX-47, Tabelle A4): geladene Plugins mit ihren
// Backend-Typen und Pfad. Neu aus Plugins.plugins/Plugins.backendTypes — reine Anzeige
// (Plugins werden beim Start eingesammelt; eine Versionsangabe liefert das SDK nicht).
CatPage {
    id: page
    heading: qsTr("Erweiterungen")
    subtitle: qsTr("Geladene Plugins und die Backend-Typen, die sie bereitstellen.")

    // Backend-Typen eines Plugins als lesbare Liste („Name, Name").
    function typesOf(pluginId) {
        const all = Plugins.backendTypes
        const names = []
        for (let i = 0; i < all.length; ++i)
            if (all[i].pluginId === pluginId) names.push(all[i].name)
        return names.join(", ")
    }

    PrefAnchor {
      settingKey: "erweiterungen.list"
      page: page
      ColumnLayout {
        Layout.fillWidth: true
        spacing: 4

        Repeater {
            model: Plugins.plugins
            delegate: Rectangle {
                required property var modelData
                Layout.fillWidth: true
                implicitHeight: pluginCol.implicitHeight + 20
                radius: 6
                color: Theme.bgElevated

                ColumnLayout {
                    id: pluginCol
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.leftMargin: 12
                    anchors.rightMargin: 12
                    spacing: 2
                    Text {
                        text: modelData.name
                        color: Theme.textBright
                        font.pixelSize: 13
                        font.bold: true
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                    }
                    Text {
                        text: qsTr("Backend-Typen: %1").arg(page.typesOf(modelData.id) || qsTr("(keine)"))
                        color: Theme.textDim
                        font.pixelSize: 11
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                    }
                    Text {
                        text: modelData.file
                        color: Theme.textDim
                        font.pixelSize: 11
                        font.family: page.host.app.terminalFontFamily
                        elide: Text.ElideMiddle
                        Layout.fillWidth: true
                    }
                }
            }
        }

        Label {
            visible: Plugins.plugins.length === 0
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            color: Theme.textDim
            font.pixelSize: 12
            text: qsTr("Keine Plugins geladen. Plugins liegen in „<App>/plugins“ bzw. „<AppData>/QTmux/plugins“ und werden beim Start eingesammelt.")
        }
      }
    }
}
