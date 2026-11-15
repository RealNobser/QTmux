import QtQuick
import QtQuick.Controls.Basic
import QTmux

// Rückfrage vor dem Öffnen einer ausführbaren Datei (QTMUX-151). Der Pfad stammt aus der Ausgabe
// im Terminal; ohne Rückfrage startete ein Strg-/Cmd-Klick das Programm sofort.
//
// Die Vorgabe ist ABBRECHEN: `Dialog.Open` zählt in AppDialog nicht als Bestätigen-Knopf, Enter
// bestätigt also nicht, und der Shortcut hier lehnt ausdrücklich ab. Es gibt bewusst kein „nicht
// mehr fragen“. Gezeigt wird der vollständige Pfad, umbrochen statt gekürzt.
AppDialog {
    id: dlg
    width: 460
    property string path: ""
    title: qsTr("Ausführbare Datei öffnen?")
    standardButtons: Dialog.Open | Dialog.Cancel

    Shortcut {
        objectName: "enterRejects"
        sequences: ["Return", "Enter"]
        enabled: dlg.visible
        onActivated: dlg.reject()
    }

    Column {
        width: 420
        spacing: 10
        Label {
            width: parent.width
            wrapMode: Text.WordWrap
            color: Theme.textBright
            text: qsTr("Diese Datei kann beim Öffnen ein Programm starten. Der Pfad stammt aus der Ausgabe im Terminal:")
        }
        Label {
            objectName: "pathLabel"
            width: parent.width
            wrapMode: Text.WrapAnywhere
            color: Theme.textBright
            font.bold: true
            text: dlg.path
        }
        Label {
            width: parent.width
            wrapMode: Text.WordWrap
            color: Theme.textBright
            text: qsTr("Öffnen Sie sie nur, wenn Sie der Datei vertrauen.")
        }
    }
}
