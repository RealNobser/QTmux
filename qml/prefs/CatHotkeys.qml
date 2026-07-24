import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QTmux

// Kategorie „Tastenkürzel" (QTMUX-47, Tabelle A4): konfigurierbare Aktionen (QTMUX-15).
// Unverändert aus dem settingsDialog-Abschnitt „Tastenkürzel". Die Aufnahme in der Zeile
// (statt des modalen hotkeyCaptureDialog) und die Gruppierung folgen in Schritt 5.
CatPage {
    id: page
    heading: qsTr("Tastenkürzel")
    subtitle: qsTr("Klick auf ein Kürzel öffnet die Aufnahme. „Standard“ erscheint nur bei Abweichung.")

    ColumnLayout {
        Layout.fillWidth: true
        spacing: 2
        Repeater {
            model: Hotkeys.actionIds()
            delegate: RowLayout {
                required property string modelData
                Layout.fillWidth: true
                spacing: 8
                Text {
                    text: page.host.app.hotkeyLabel(modelData)
                    color: Theme.textBright
                    font.pixelSize: 13
                    Layout.fillWidth: true
                    elide: Text.ElideRight
                }
                Button {
                    text: Hotkeys.bindings[modelData] || qsTr("(keins)")
                    font.pixelSize: 12
                    onClicked: page.host.hotkeyCaptureDialog.start(modelData, page.host.app.hotkeyLabel(modelData))
                }
                Button {
                    text: qsTr("Standard")
                    font.pixelSize: 12
                    visible: Hotkeys.bindings[modelData] !== Hotkeys.defaultSequence(modelData)
                    onClicked: Hotkeys.reset(modelData)
                }
            }
        }
    }
    Button {
        text: qsTr("Alle Kürzel zurücksetzen")
        onClicked: Hotkeys.resetAll()
    }
}
