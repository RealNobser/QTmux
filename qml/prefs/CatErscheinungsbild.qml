import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QTmux

// Kategorie „Erscheinungsbild" (QTMUX-47, Tabelle A4): die beiden Terminal-Farbschemata
// (Dunkel/Hell) mit 16-Farben-Vorschau und Import. Das je nach Modus aktive Schema färbt
// die GANZE App. Unverändert aus dem settingsDialog-Abschnitt „Erscheinungsbild".
CatPage {
    id: page
    heading: qsTr("Erscheinungsbild")
    subtitle: qsTr("Farbschemata für Dunkel und Hell. Das aktive Schema färbt die gesamte App.")

    PrefAnchor {
      settingKey: "erscheinung.schemes"
      page: page
      GridLayout {
        columns: 2
        columnSpacing: 12
        rowSpacing: 8
        Layout.fillWidth: true

        // Terminal-Farbschema (QTMUX-18): je ein Schema für Dunkel und Hell. Import
        // (iTerm/Xresources/Ghostty) landet im passenden Slot.
        Text { text: qsTr("Farbschema (Dunkel)"); color: Theme.textBright }
        RowLayout {
            Layout.fillWidth: true
            spacing: 6
            AppComboBox {
                Layout.fillWidth: true
                model: ColorSchemes.names
                currentIndex: Math.max(0, ColorSchemes.names.indexOf(ColorSchemes.darkScheme))
                onActivated: (i) => ColorSchemes.darkScheme = ColorSchemes.names[i]
            }
            Button {
                text: qsTr("Importieren …")
                font.pixelSize: 12
                onClicked: page.host.schemeFileDialog.open()
            }
        }
        Item { width: 1; height: 1 }   // Spalte 1 leer
        Row {
            Layout.fillWidth: true
            spacing: 3
            property var sc: ColorSchemes.colors(ColorSchemes.darkScheme)
            Repeater {
                model: 16
                Rectangle {
                    required property int index
                    width: 15; height: 15; radius: 3
                    color: parent.sc.ansi[index]
                    border.color: Theme.border; border.width: 1
                }
            }
        }

        Text { text: qsTr("Farbschema (Hell)"); color: Theme.textBright }
        AppComboBox {
            Layout.fillWidth: true
            model: ColorSchemes.names
            currentIndex: Math.max(0, ColorSchemes.names.indexOf(ColorSchemes.lightScheme))
            onActivated: (i) => ColorSchemes.lightScheme = ColorSchemes.names[i]
        }
        Item { width: 1; height: 1 }
        Row {
            Layout.fillWidth: true
            spacing: 3
            property var sc: ColorSchemes.colors(ColorSchemes.lightScheme)
            Repeater {
                model: 16
                Rectangle {
                    required property int index
                    width: 15; height: 15; radius: 3
                    color: parent.sc.ansi[index]
                    border.color: Theme.border; border.width: 1
                }
            }
        }
      }
    }
}
