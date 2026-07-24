import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Effects
import QTmux

// Themen-ComboBox: gerahmtes Feld, Caret-Icon, abgerundetes Popup.
ComboBox {
    id: cb
    implicitHeight: 32
    font.pixelSize: 13
    background: Rectangle {
        radius: 6
        color: Theme.bgElevated
        border.color: cb.activeFocus ? Theme.accent : Theme.border
        border.width: 1
    }
    indicator: Image {
        x: cb.width - width - 10
        y: (cb.height - height) / 2
        // Früher window.icon("caret-down"); beim Herausziehen (QTMUX-47) direkt auf
        // den qrc-Pfad gesetzt, damit die Komponente ohne den window-Kontext auskommt.
        source: "qrc:/icons/caret-down.svg"
        sourceSize.width: 14
        sourceSize.height: 14
        layer.enabled: true
        layer.effect: MultiEffect {
            colorization: 1.0
            colorizationColor: Theme.textDim
        }
    }
    popup: Popup {
        y: cb.height + 4
        width: cb.width
        padding: 4
        implicitHeight: Math.min(contentItem.implicitHeight + 8, 260)
        background: AppPopupBg {}
        contentItem: ListView {
            clip: true
            implicitHeight: contentHeight
            model: cb.popup.visible ? cb.delegateModel : null
            currentIndex: cb.highlightedIndex
            ScrollIndicator.vertical: ScrollIndicator {}
        }
    }
}
