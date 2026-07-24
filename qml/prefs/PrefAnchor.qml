import QtQuick
import QtQuick.Layouts
import QTmux

// Verankerungs-Wrapper für die Einstellungs-Suche (QTMUX-47, Schritt 7). Umschließt eine
// Sektion einer Kategorie und blendet sie EINMALIG ~1,2 s mit Theme.sidebarSelected auf +
// scrollt sie in den Blick, sobald die Suche über host.pendingSetting ihren Schlüssel
// anspricht (kein Dauerblinken). Bewusst auf Sektions-Ebene, nicht pro Grid-Zelle — das
// bräche die zweispaltigen GridLayouts.
Item {
    id: anchor
    property string settingKey: ""
    property var page                       // umschließende CatPage
    default property alias content: holder.data

    Layout.fillWidth: true
    implicitWidth: holder.implicitWidth
    implicitHeight: holder.implicitHeight

    // Highlight-Hintergrund (liegt hinter dem Inhalt).
    Rectangle {
        id: hl
        anchors.fill: holder
        anchors.margins: -6
        radius: 8
        color: Theme.sidebarSelected
        opacity: 0
        z: -1
    }
    ColumnLayout {
        id: holder
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
    }

    SequentialAnimation {
        id: flashAnim
        NumberAnimation { target: hl; property: "opacity"; from: 0; to: 1; duration: 160 }
        PauseAnimation { duration: 900 }
        NumberAnimation { target: hl; property: "opacity"; to: 0; duration: 240 }
    }

    // Prüft, ob die Suche gerade diese Sektion meint; wenn ja: aufblenden, hinscrollen,
    // pendingSetting zurücksetzen (damit es nur einmal auslöst).
    function maybeFlash() {
        if (settingKey.length > 0 && page && page.pendingSetting === settingKey) {
            flashAnim.restart()
            page.scrollTo(anchor)
            page.clearPending()
        }
    }
    // Deckt den Fall ab, dass die Seite durch den Kategorie-Wechsel erst NACH dem Setzen
    // von pendingSetting entsteht (Suchsprung in eine andere Kategorie).
    Component.onCompleted: maybeFlash()
    Connections {
        target: anchor.page
        function onPendingSettingChanged() { anchor.maybeFlash() }
    }
}
