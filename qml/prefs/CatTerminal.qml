import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QTmux

// Kategorie „Terminal" (QTMUX-47, Tabelle A4): Schriftart/-größe, Ligaturen, GPU-Atlas,
// Standard-Shell. Unverändert aus dem settingsDialog-Abschnitt „Terminal". Die Mini-
// Terminal-Vorschau kommt in Schritt 4 unter diese Optionen.
CatPage {
    id: page
    heading: qsTr("Terminal")
    subtitle: qsTr("Schrift, Ligaturen und Rendering des Terminals.")

    GridLayout {
        columns: 2
        columnSpacing: 12
        rowSpacing: 8
        Layout.fillWidth: true

        Text { text: qsTr("Schriftart"); color: Theme.textBright }
        AppComboBox {
            Layout.fillWidth: true
            model: App.monospaceFonts()
            currentIndex: Math.max(0, App.monospaceFonts().indexOf(page.host.app.terminalFontFamily))
            onActivated: (i) => page.host.app.terminalFontFamily = App.monospaceFonts()[i]
        }
        Text { text: qsTr("Schriftgröße"); color: Theme.textBright }
        SpinBox {
            from: 6; to: 40
            value: page.host.app.terminalFontSize
            onValueModified: page.host.app.terminalFontSize = value
        }
        Text { text: qsTr("Ligaturen"); color: Theme.textBright }
        CheckBox {
            text: qsTr("Programmier-Ligaturen (z. B. FiraCode)")
            checked: page.host.app.terminalLigatures
            onToggled: page.host.app.terminalLigatures = checked
        }
        Text { text: qsTr("Rendering"); color: Theme.textBright }
        CheckBox {
            text: qsTr("GPU-Glyph-Atlas (schneller; aus = QPainter-Fallback)")
            checked: page.host.app.terminalGpuRendering
            onToggled: page.host.app.terminalGpuRendering = checked
        }
        Text {
            text: qsTr("Standard-Shell"); color: Theme.textBright
            visible: page.host.app.hasShellChoice
        }
        AppComboBox {
            visible: page.host.app.hasShellChoice
            Layout.fillWidth: true
            textRole: "name"
            model: page.host.sessions.availableShells()
            currentIndex: {
                const l = page.host.sessions.availableShells()
                for (let i = 0; i < l.length; ++i)
                    if (l[i].program === page.host.app.currentShellProgram()) return i
                return 0
            }
            onActivated: (i) => page.host.app.defaultShellProgram = page.host.sessions.availableShells()[i].program
        }
    }
}
