import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QTmux

// Kategorie „Eingabe & Zwischenablage" (QTMUX-47, Tabelle A4): die drei Schalter, die mit
// Stufe 4 das Bearbeiten-Menü verlassen haben. Zeilenformat aus Design 1a C3 — die
// Erklärung steht jetzt IN der Zeile, nicht als Fließtext darunter.
CatPage {
    id: page
    heading: qsTr("Eingabe & Zwischenablage")
    subtitle: qsTr("Auswahl und Zwischenablage.")

    PrefAnchor {
        settingKey: "eingabe.clipboard"
        page: page
        PrefGroup {
            title: qsTr("Zwischenablage")
            PrefRow {
                title: qsTr("Auswahl automatisch kopieren")
                description: qsTr("PuTTY-Stil: markierter Text landet sofort in der Zwischenablage.")
                AppSwitch {
                    checked: page.host.app.copyOnSelect
                    onToggled: page.host.app.copyOnSelect = checked
                }
            }
            PrefRow {
                title: qsTr("Rechtsklick fügt ein")
                description: qsTr("Statt des Kontextmenüs — dieses erreichst du dann über die Menüleiste.")
                AppSwitch {
                    checked: page.host.app.rightClickPaste
                    onToggled: page.host.app.rightClickPaste = checked
                }
            }
            PrefRow {
                title: qsTr("Vor mehrzeiligem Einfügen warnen")
                description: qsTr("Mehrere Zeilen wirken in einer Shell wie mehrere abgeschickte Befehle.")
                AppSwitch {
                    checked: page.host.app.pasteWarnMultiline
                    onToggled: page.host.app.pasteWarnMultiline = checked
                }
            }
        }
    }
}
