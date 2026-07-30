import QtQuick
import QtQuick.Controls.Basic
import QTmux

// Menüeintrag mit Kürzel-Anzeige rechts.
// Bewusst wird das contentItem NICHT ersetzt: das Default-IconLabel des Basic-Styles
// rendert Icon (getönt via icon.color) + Text korrekt UND treibt die Menübreite
// zuverlässig (eigene Versuche mit RowLayout/anchor-Item als contentItem führten zu
// abgeschnittenem/überlappendem Text, weil deren implicitWidth nicht sauber in den
// QQuickMenu-Breitenalgorithmus propagiert). Stattdessen:
//   - rightPadding um die Kürzelbreite erhöhen → implicitWidth (= contentItem + Padding)
//     wächst, das Menü wird breit genug;
//   - das Kürzel als angedocktes Label in diese reservierte rechte Zone legen
//     (liegt außerhalb des contentItem → kann den Text nie überlappen).
// Icon-Farbe folgt damit wieder Theme.menuIcon (app-theme-korrekt, s. Theme::menuIcon).
MenuItem {
    id: smi
    implicitHeight: 32
    // Ein deaktivierter Eintrag sah aus wie ein aktiver: das contentItem des
    // Basic-Styles färbt den Text UNBEDINGT mit `palette.windowText` (im Qt-Quelltext
    // geprüft, MenuItem.qml Z. 48/59) — es gibt also keinen Disabled-Zustand zu
    // theming. Auch `palette.disabled.*` bleibt darum wirkungslos (ausprobiert und am
    // Bild widerlegt). Deshalb hier die ganze Zeile dimmen; das nimmt das Kürzel-Label
    // mit. Aufgefallen an „Diese Seite zurücksetzen" (Stufe 6), gilt für JEDEN
    // deaktivierten Menüeintrag der App.
    opacity: enabled ? 1.0 : 0.45
    // Explizit gesetztes Kürzel (für Aktionen, deren Shortcut NICHT an der Action hängt,
    // z. B. das im TerminalItem fest verdrahtete Strg+C/Strg+V). Sonst aus der Action.
    property string shortcutOverride: ""
    readonly property string shortcutText: shortcutOverride.length > 0
                                            ? App.shortcutText(shortcutOverride)
                                            : (smi.action ? App.shortcutText(smi.action.shortcut) : "")
    TextMetrics { id: smiTmShort; font.pixelSize: 12; text: smi.shortcutText }
    rightPadding: 16 + (shortcutText.length > 0 ? Math.ceil(smiTmShort.width) + 22 : 0)
    // Eigener, themengebundener Highlight-Hintergrund. WICHTIG: der Basic-Style
    // färbt das Highlight sonst über palette.light/midlight — die in einem Menü-Popup
    // NICHT vom App-Theme stammen (Popups erben die Palette nicht), was im Hell-Modus
    // einen schwarzen Selektionsbalken mit unsichtbarem Text ergab. Daher hier explizit.
    background: Rectangle {
        radius: 6
        color: smi.highlighted ? Theme.sidebarHover : "transparent"
    }
    Label {
        visible: smi.shortcutText.length > 0
        anchors.right: parent.right
        anchors.rightMargin: 14
        anchors.verticalCenter: parent.verticalCenter
        text: smi.shortcutText
        font.pixelSize: 12
        color: Theme.textDim
    }
}
