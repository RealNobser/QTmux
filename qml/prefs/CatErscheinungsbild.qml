pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QTmux

// Kategorie „Erscheinungsbild" (QTMUX-47, Tabelle A4; Zeilenformat aus Design 1a C3): die
// beiden Terminal-Farbschemata (Dunkel/Hell) mit 16-Farben-Vorschau und Import. Das je nach
// Modus aktive Schema färbt die GANZE App.
CatPage {
    id: page
    heading: qsTr("Erscheinungsbild")
    subtitle: qsTr("Farbschemata für Dunkel und Hell. Das aktive Schema färbt die gesamte App.")

    // 16-Farben-Streifen eines Schemas — zweimal gebraucht, deshalb als Inline-Komponente.
    component AnsiStrip: Row {
        id: strip
        required property var sc
        spacing: 3
        Repeater {
            model: 16
            Rectangle {
                required property int index
                width: 15; height: 15; radius: 3
                // Qualifizierte ID statt `parent.sc`: mit `pragma ComponentBehavior: Bound`
                // ist der Delegate-Scope gebunden, und `parent` ist hier die Row selbst —
                // eine Verwechslung mit `parent.parent` kostete beim ersten Versuch Zeit.
                color: strip.sc.ansi[index]
                border.color: Theme.border; border.width: 1
            }
        }
    }

    PrefAnchor {
      settingKey: "erscheinung.schemes"
      page: page
      PrefGroup {
        title: qsTr("Farbschemata")
        // Terminal-Farbschema (QTMUX-18): je ein Schema für Dunkel und Hell. Import
        // (iTerm/Xresources/Ghostty) landet im passenden Slot.
        PrefRow {
            title: qsTr("Farbschema (Dunkel)")
            description: qsTr("Gilt im Dunkel-Modus — für Terminal UND App-Chrome.")
            controlWidth: 300
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
        PrefRow {
            title: qsTr("Farben (Dunkel)")
            description: qsTr("Die 16 ANSI-Farben des gewählten Schemas.")
            AnsiStrip { sc: ColorSchemes.colors(ColorSchemes.darkScheme) }
        }
        PrefRow {
            title: qsTr("Farbschema (Hell)")
            description: qsTr("Gilt im Hell-Modus. Import landet immer im passenden Slot.")
            controlWidth: 300
            AppComboBox {
                Layout.fillWidth: true
                model: ColorSchemes.names
                currentIndex: Math.max(0, ColorSchemes.names.indexOf(ColorSchemes.lightScheme))
                onActivated: (i) => ColorSchemes.lightScheme = ColorSchemes.names[i]
            }
        }
        PrefRow {
            title: qsTr("Farben (Hell)")
            description: qsTr("Die 16 ANSI-Farben des gewählten Schemas.")
            AnsiStrip { sc: ColorSchemes.colors(ColorSchemes.lightScheme) }
        }
      }
    }
}
