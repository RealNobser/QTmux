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
    // Eigener, themengebundener Popup-Eintrag. WICHTIG: das Default-ItemDelegate des
    // Basic-Styles färbt das Highlight über palette.light und lässt die Textfarbe auf
    // palette.text — beides stammt im Popup NICHT vom App-Theme (Popups erben die Palette
    // nicht), was im Dark Mode helle Schrift auf hellem Highlight ergab. Daher explizit
    // (gleiche Lösung wie ShortcutMenuItem).
    delegate: ItemDelegate {
        id: cbItem
        width: ListView.view ? ListView.view.width : implicitWidth
        highlighted: cb.highlightedIndex === index
        contentItem: Text {
            text: cb.textRole
                  ? (Array.isArray(cb.model) ? modelData[cb.textRole] : model[cb.textRole])
                  : modelData
            color: Theme.textBright
            font.pixelSize: 13
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }
        background: Rectangle {
            radius: 6
            color: cbItem.highlighted ? Theme.sidebarHover : "transparent"
        }
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
            // Schwarzes SVG (fill="currentColor") erst auf Weiß heben, dann colorize —
            // sonst gewichtet die Colorization mit der Quell-Luminanz ~0 (dunkles Icon).
            brightness: 1.0
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
