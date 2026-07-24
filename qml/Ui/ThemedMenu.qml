import QtQuick
import QtQuick.Controls.Basic
import QTmux

// Themengebundenes Menü für die MenuBar. Qt-Quick-Menü-Popups erben die
// überschriebene palette des ApplicationWindow NICHT (sie nutzen sonst eine feste,
// oft dunkle Palette → im Hell-Modus dunkles Menü). Daher hier die Palette explizit
// ans Theme binden + AppPopupBg-Hintergrund + automatische Breite.
Menu {
    id: menu
    padding: 4
    // Menübreite an den breitesten Eintrag anpassen. Früher window.sizeMenu(this);
    // beim Herausziehen (QTMUX-47) selbstständig gemacht, damit die Komponente ohne
    // den window-Kontext auskommt und auch das Einstellungsfenster sie nutzen kann.
    onAboutToShow: {
        let w = 0
        for (let i = 0; i < count; ++i) {
            const it = itemAt(i)
            if (it && it.implicitWidth > w) w = it.implicitWidth
        }
        if (w > 0) contentWidth = w
    }
    palette.window: Theme.bgElevated
    palette.windowText: Theme.textBright
    palette.text: Theme.textBright
    palette.highlight: Theme.sidebarHover
    palette.highlightedText: Theme.textBright
    background: AppPopupBg {}
}
