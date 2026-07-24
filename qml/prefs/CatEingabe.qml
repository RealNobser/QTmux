import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QTmux

// Kategorie „Eingabe" (QTMUX-47, Tabelle A4): Auswahl/Zwischenablage-Verhalten.
// Unverändert aus dem settingsDialog-Abschnitt „Eingabe & Zwischenablage".
CatPage {
    id: page
    heading: qsTr("Eingabe")
    subtitle: qsTr("Auswahl und Zwischenablage.")

    PrefAnchor {
        settingKey: "eingabe.clipboard"
        page: page
        ColumnLayout {
            spacing: 6
            Layout.fillWidth: true
            CheckBox {
                text: qsTr("Auswahl automatisch kopieren")
                checked: page.host.app.copyOnSelect
                onToggled: page.host.app.copyOnSelect = checked
            }
            CheckBox {
                text: qsTr("Rechtsklick fügt ein")
                checked: page.host.app.rightClickPaste
                onToggled: page.host.app.rightClickPaste = checked
            }
            CheckBox {
                text: qsTr("Vor mehrzeiligem Einfügen warnen")
                checked: page.host.app.pasteWarnMultiline
                onToggled: page.host.app.pasteWarnMultiline = checked
            }
        }
    }
}
