import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QTmux

// Kategorie „Darstellung & Shell" (QTMUX-47, Tabelle A4; Zeilenformat aus Design 1a C3):
// Schriftart/-größe, Ligaturen, GPU-Atlas, Standard-Shell — Letztere hat mit Stufe 4 das
// Datei-Menü verlassen. Darunter die Live-Vorschau.
CatPage {
    id: page
    heading: qsTr("Darstellung & Shell")
    subtitle: qsTr("Schrift, Ligaturen, Rendering und die Shell neuer Sessions.")

    // Aktives Schema (das die ganze App färbt) — reaktiv über ColorSchemes.current.
    readonly property var scheme: ColorSchemes.colors(ColorSchemes.current)

    PrefAnchor { settingKey: "terminal.options"; page: page
    PrefGroup {
        title: qsTr("Schrift")
        PrefRow {
            title: qsTr("Schriftart")
            description: qsTr("Nur Monospace-Schriften — Proportionalschrift zerlegt das Zellraster.")
            controlWidth: 240
            AppComboBox {
                Layout.fillWidth: true
                model: App.monospaceFonts()
                currentIndex: Math.max(0, App.monospaceFonts().indexOf(page.host.app.terminalFontFamily))
                onActivated: (i) => page.host.app.terminalFontFamily = App.monospaceFonts()[i]
            }
        }
        PrefRow {
            title: qsTr("Schriftgröße")
            description: qsTr("Wirkt auf alle Sessions; einzelne Fenster zoomst du mit Strg/Cmd +/−.")
            SpinBox {
                from: 6; to: 40
                value: page.host.app.terminalFontSize
                onValueModified: page.host.app.terminalFontSize = value
            }
        }
        PrefRow {
            title: qsTr("Programmier-Ligaturen")
            description: qsTr("Verbindet Zeichenfolgen wie != oder => zu einem Glyph (z. B. FiraCode).")
            AppSwitch {
                checked: page.host.app.terminalLigatures
                onToggled: page.host.app.terminalLigatures = checked
            }
        }
        PrefRow {
            title: qsTr("GPU-Glyph-Atlas")
            description: qsTr("Schneller; aus = QPainter-Fallback. Bei Darstellungsfehlern hilft „aus“.")
            AppSwitch {
                checked: page.host.app.terminalGpuRendering
                onToggled: page.host.app.terminalGpuRendering = checked
            }
        }
    }
    }

    PrefAnchor { settingKey: "terminal.shell"; page: page
    PrefGroup {
        title: qsTr("Shell")
        visible: page.host.app.hasShellChoice
        PrefRow {
            title: qsTr("Standard-Shell")
            description: qsTr("Gilt für neue Sessions. Dieselbe Wahl steckt im „+“-Menü der Leiste.")
            controlWidth: 240
            AppComboBox {
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
