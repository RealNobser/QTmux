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

    // Aktives Schema (das die ganze App färbt) — reaktiv über ColorSchemes.current.
    readonly property var scheme: ColorSchemes.colors(ColorSchemes.current)

    PrefAnchor { settingKey: "terminal.options"; page: page
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

    // --- Live-Vorschau (nicht interaktiv, QTMUX-47 Schritt 4) -------------------
    // Spiegelt sofort Schema, Schriftart/-größe und Ligaturen wider. Bewusst ein
    // gestylter Text-Block statt eines PTY-losen TerminalItem — sichtbares Ergebnis
    // ist verbindlich, nicht der Weg.
    PrefAnchor { settingKey: "terminal.preview"; page: page
    ColumnLayout {
        Layout.fillWidth: true
        spacing: 6
        SectionLabel { text: qsTr("Vorschau") }
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: previewCol.implicitHeight + 24
            radius: 8
            color: page.scheme.bg
            border.color: Theme.border
            border.width: 1
            clip: true

            ColumnLayout {
                id: previewCol
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                anchors.leftMargin: 14
                anchors.rightMargin: 14
                spacing: 3

                // Prompt-Zeile mit ANSI-Farben.
                RowLayout {
                    spacing: 0
                    Text { text: "➜";            color: page.scheme.ansi[10]; font.family: page.host.app.terminalFontFamily; font.pixelSize: page.host.app.terminalFontSize }
                    Text { text: "  ~/projects"; color: page.scheme.ansi[12]; font.family: page.host.app.terminalFontFamily; font.pixelSize: page.host.app.terminalFontSize }
                    Text { text: " git:(";       color: page.scheme.fg;       font.family: page.host.app.terminalFontFamily; font.pixelSize: page.host.app.terminalFontSize }
                    Text { text: "main";         color: page.scheme.ansi[9];  font.family: page.host.app.terminalFontFamily; font.pixelSize: page.host.app.terminalFontSize }
                    Text { text: ")";            color: page.scheme.fg;       font.family: page.host.app.terminalFontFamily; font.pixelSize: page.host.app.terminalFontSize }
                }
                // Beispielausgabe in Vordergrundfarbe.
                Text {
                    text: qsTr("$ qtmux --version   # Beispieltext in der gewählten Schrift")
                    color: page.scheme.fg
                    font.family: page.host.app.terminalFontFamily
                    font.pixelSize: page.host.app.terminalFontSize
                    Layout.fillWidth: true
                    elide: Text.ElideRight
                }
                // ANSI-Palette (16 Farben) als Blöcke.
                Row {
                    spacing: 2
                    Repeater {
                        model: 16
                        Rectangle {
                            required property int index
                            width: page.host.app.terminalFontSize
                            height: page.host.app.terminalFontSize
                            radius: 2
                            color: page.scheme.ansi[index]
                        }
                    }
                }
                // Ligaturzeile — folgt der Ligatureneinstellung über font.features.
                Text {
                    text: "!= <= => --> === |> :: <> =~ ++"
                    color: page.scheme.fg
                    font.family: page.host.app.terminalFontFamily
                    font.pixelSize: page.host.app.terminalFontSize
                    // Programmier-Ligaturen sitzen bei den meisten Fonts in `calt`
                    // (kontextuelle Alternativen); `liga` deckt die Standardligaturen ab.
                    font.features: ({
                        "liga": page.host.app.terminalLigatures ? 1 : 0,
                        "calt": page.host.app.terminalLigatures ? 1 : 0
                    })
                }
            }
        }
    }
    }
}
