import QtQuick
import QtQuick.Controls.Basic
import QTmux

// Themen-Dialog: abgerundete erhobene Fläche, gestylter Titel, abgedunkelter Hintergrund.
Dialog {
    id: dlg
    anchors.centerIn: parent
    modal: true
    padding: 20
    // Tastatur-Unterstützung für modale Dialoge:
    //  • ESC = Abbrechen/Schließen — via closePolicy (CloseOnEscape → reject()/close()).
    //  • Enter = OK — über einen Shortcut INNERHALB des Dialogs (im Fokus-Scope des
    //    modalen Popups; ein fensterweiter Shortcut feuert dort NICHT). Greift nur, wenn
    //    der Dialog einen Bestätigen-Button hat. Hat ein einzeiliges TextField den Fokus,
    //    beansprucht es Return selbst (ShortcutOverride) → dann bestätigt dessen
    //    onAccepted; ein mehrzeiliges TextArea behält Enter für Zeilenumbrüche.
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    property bool hasAccept: (standardButtons & Dialog.Ok)
                             || (standardButtons & Dialog.Save)
                             || (standardButtons & Dialog.Yes)
    Shortcut {
        sequences: ["Return", "Enter"]
        enabled: dlg.visible && dlg.hasAccept
        onActivated: dlg.accept()
    }
    background: Rectangle {
        color: Theme.bgElevated
        radius: 12
        border.color: Theme.border
        border.width: 1
    }
    header: Label {
        text: dlg.title
        visible: dlg.title.length > 0
        color: Theme.textBright
        font.pixelSize: 16
        font.bold: true
        elide: Label.ElideRight
        padding: 20
        bottomPadding: 6
    }
    Overlay.modal: Rectangle { color: "#88000000" }
}
