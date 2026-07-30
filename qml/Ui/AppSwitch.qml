import QtQuick
import QtQuick.Controls.Basic
import QTmux

// Themengebundener Schalter (Design 1a, Teil C3: „Booleane Werte als Schalter, Theme.accent").
// Der Basic-Style zeichnet seinen Indikator über die **Palette** (palette.dark/mid/light) —
// die im Einstellungsfenster zwar gesetzt ist, für den Ein-Zustand aber nur ein graues Band
// ergibt. Deshalb ein eigener Indikator: Band in `Theme.accent` (ein) bzw. `Theme.border`
// (aus), Knopf in der Vordergrundfarbe. Dieselbe Linie wie AppComboBox/ShortcutMenuItem —
// Basic-Controls werden explizit gefärbt, nicht der Palette überlassen.
Switch {
    id: sw
    implicitWidth: 46
    implicitHeight: 26
    padding: 0

    indicator: Rectangle {
        implicitWidth: 42
        implicitHeight: 22
        x: sw.width - width - sw.rightPadding
        y: (sw.height - height) / 2
        radius: height / 2
        color: sw.checked ? Theme.accent : Theme.bgElevated
        border.color: sw.checked ? Theme.accent : Theme.border
        border.width: 1
        opacity: sw.enabled ? 1.0 : 0.5

        Behavior on color { ColorAnimation { duration: App.reduceMotion ? 0 : 120 } }

        Rectangle {
            x: sw.checked ? parent.width - width - 3 : 3
            y: 3
            width: 16
            height: 16
            radius: 8
            color: sw.checked ? Theme.accentText : Theme.textDim
            Behavior on x { NumberAnimation { duration: App.reduceMotion ? 0 : 120; easing.type: Easing.OutCubic } }
        }
    }
    // Kein Text am Schalter selbst — der Titel steht in der PrefRow.
    contentItem: Item {}
}
