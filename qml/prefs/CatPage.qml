import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QTmux

// Gemeinsames Gerüst einer Einstellungs-Kategorie (QTMUX-47, Schritt 3):
// eigener Scrollbereich, Kopfzeile (Titel + einzeilige Erklärung), darunter der
// Seiteninhalt. Der View im PrefsWindow scrollt eigenständig — die Rail scrollt
// nie mit. Der konkrete Inhalt kommt als Default-Kinder in `content`.
Flickable {
    id: page

    // Kopfzeile.
    property string heading: ""
    property string subtitle: ""
    // Brücke zu Main.qml (app/sessions/mcp/Editier-Dialoge) — vom PrefsWindow gesetzt.
    property var host

    // Ziel-Einstellung eines Suchsprungs (Schritt 7): wird von PrefAnchor gelesen, das
    // passende Element blendet einmalig auf und scrollt in den Blick, dann wird der Wert
    // zurückgesetzt (kein Dauerblinken). Gespiegelt aus host.pendingSetting.
    readonly property string pendingSetting: host ? host.pendingSetting : ""
    function clearPending() { if (host) host.pendingSetting = "" }
    // Scrollt `item` (irgendwo in dieser Seite) in den sichtbaren Bereich.
    function scrollTo(item) {
        const p = item.mapToItem(page.contentItem, 0, 0)
        const target = Math.max(0, p.y - 24)
        const maxY = Math.max(0, contentHeight - height)
        page.contentY = Math.min(target, maxY)
    }

    // Seiteninhalt landet in dieser Spalte.
    default property alias content: contentCol.data

    contentWidth: width
    contentHeight: outer.y + outer.implicitHeight + 28
    clip: true
    boundsBehavior: Flickable.StopAtBounds
    ScrollIndicator.vertical: ScrollIndicator {}

    ColumnLayout {
        id: outer
        x: 28
        y: 24
        width: page.width - 56
        spacing: 18

        // Kopfzeile: Titel + einzeilige Erklärung.
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 3
            Text {
                text: page.heading
                color: Theme.textBright
                font.pixelSize: 22
                font.bold: true
            }
            Text {
                visible: page.subtitle.length > 0
                text: page.subtitle
                color: Theme.textDim
                font.pixelSize: 13
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
            }
        }

        ColumnLayout {
            id: contentCol
            Layout.fillWidth: true
            spacing: 18
        }
    }
}
