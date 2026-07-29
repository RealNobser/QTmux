import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QTmux

// Kategorie „Allgemein" (QTMUX-47, Tabelle A4): Sprache, Design und Verhalten beim
// Beenden — zusammengeführt aus den früheren Abschnitten „Erscheinungsbild" (Design/
// Sprache) und „Fenster" (Beenden-Rückfrage, Quake-Modus) des settingsDialog.
CatPage {
    id: page
    heading: qsTr("Allgemein")
    subtitle: qsTr("Sprache, Erscheinungs-Modus und Verhalten beim Beenden.")

    PrefAnchor {
        settingKey: "allgemein.general"
        page: page
        GridLayout {
            columns: 2
            columnSpacing: 12
            rowSpacing: 8
            Layout.fillWidth: true

            Text { text: qsTr("Design"); color: Theme.textBright }
            AppComboBox {
                Layout.fillWidth: true
                model: [qsTr("Wie System"), qsTr("Hell"), qsTr("Dunkel")]
                currentIndex: Theme.mode
                onActivated: (i) => Theme.mode = i
            }
            Text { text: qsTr("Sprache"); color: Theme.textBright }
            AppComboBox {
                Layout.fillWidth: true
                textRole: "name"
                model: App.languageCodes().map(c => ({ code: c, name: App.languageName(c) }))
                currentIndex: Math.max(0, App.languageCodes().indexOf(App.language))
                onActivated: (i) => App.language = App.languageCodes()[i]
            }
        }
    }

    // --- Fenster / Beenden ---
    PrefAnchor {
        settingKey: "allgemein.window"
        page: page
        ColumnLayout {
            spacing: 4
            Layout.fillWidth: true
            SectionLabel { text: qsTr("Fenster") }
            CheckBox {
                text: qsTr("Vor dem Beenden nachfragen")
                checked: page.host.app.confirmQuit
                onToggled: page.host.app.confirmQuit = checked
            }
            Text {
                text: qsTr("Beenden schließt alle Sitzungen samt laufender Prozesse.")
                color: Theme.textDim
                font.pixelSize: 12
                Layout.leftMargin: 6
                Layout.bottomMargin: 4
            }
            // --- Wiederherstellung beim Start (QTMUX-99) ---
            // Bewusst eine Wahl: Der teure Teil ist der Verlauf, der nützliche sind
            // Fenster, Panes und Arbeitsverzeichnisse. Ein Schalter zwänge dazu, beides
            // gemeinsam aufzugeben.
            RowLayout {
                Layout.fillWidth: true
                Layout.topMargin: 4
                spacing: 8
                Text { text: qsTr("Sessions beim Start wiederherstellen"); color: Theme.textBright }
                AppComboBox {
                    Layout.fillWidth: true
                    model: [qsTr("Gar nicht"), qsTr("Ohne Verlauf"), qsTr("Alles")]
                    currentIndex: page.host.app.restoreSessionMode
                    onActivated: (i) => page.host.app.restoreSessionMode = i
                }
            }
            Text {
                text: {
                    switch (page.host.app.restoreSessionMode) {
                    case 0: return qsTr("QTmux startet mit einer einzelnen, leeren Session. "
                                      + "Der zuletzt gespeicherte Stand bleibt dabei erhalten — er "
                                      + "wird beim Beenden nicht überschrieben und ist wieder da, "
                                      + "sobald hier erneut wiederhergestellt wird.")
                    case 1: return qsTr("Fenster, Panes und deren Arbeitsverzeichnisse kommen zurück, "
                                      + "die Terminals starten aber leer. Der gespeicherte Verlauf "
                                      + "bleibt liegen und wird bei „Alles“ wieder angezeigt.")
                    default: return qsTr("Fenster, Panes und Arbeitsverzeichnisse kommen zurück, dazu "
                                       + "der farbige Verlauf jedes Panes.")
                    }
                }
                color: Theme.textDim
                font.pixelSize: 12
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
                Layout.leftMargin: 6
                Layout.bottomMargin: 4
            }
            Text {
                text: qsTr("Ob die Agenten in den Panes dabei erneut starten, steht unter "
                         + "„Agenten & MCP“.")
                color: Theme.textDim
                font.pixelSize: 11
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
                Layout.leftMargin: 6
                Layout.bottomMargin: 4
            }
            CheckBox {
                text: qsTr("Quake-Modus: per globalem Hotkey ein-/ausblenden")
                checked: page.host.app.quakeMode
                enabled: Qt.platform.os === "osx"   // vorerst nur macOS
                onToggled: page.host.app.quakeMode = checked
            }
            Text {
                text: Qt.platform.os === "osx"
                      ? qsTr("Globaler Hotkey: Strg+^ (blendet QTmux überall ein/aus)")
                      : qsTr("Derzeit nur unter macOS verfügbar.")
                color: Theme.textDim
                font.pixelSize: 11
                Layout.leftMargin: 26
            }
        }
    }
}
