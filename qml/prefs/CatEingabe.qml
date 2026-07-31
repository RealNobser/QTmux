import QtQuick
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
            PrefRow {
                title: qsTr("Programme dürfen die Zwischenablage füllen")
                description: qsTr("Der einzige Weg für Text aus einer SSH-Sitzung, einem Container oder einer Oberfläche mit eigener Auswahl. Auslesen bleibt immer verwehrt.")
                AppSwitch {
                    checked: page.host.app.appClipboardWrite
                    onToggled: page.host.app.appClipboardWrite = checked
                }
            }
        }
    }

    PrefAnchor {
        settingKey: "eingabe.maus"
        page: page
        PrefGroup {
            title: qsTr("Maus")
            PrefRow {
                title: qsTr("Mausrad in Vollbild-Anwendungen")
                description: qsTr("Vollbild-Anwendungen zeichnen ihren Verlauf selbst; der Verlauf von QTmux "
                                  + "bleibt dort leer, das Rad hat also nichts zu scrollen. Es kann nur wirken, "
                                  + "wenn QTmux daraus eine Taste macht, mit der die Anwendung selbst scrollt. "
                                  + "„Nur auf Anforderung“ tut das ausschließlich, wenn die Anwendung es "
                                  + "verlangt; „Immer“ deckt zusätzlich Anzeigeprogramme wie less und man ab, "
                                  + "bewegt in vim aber den Cursor statt zu scrollen. Für erkannte Agenten mit "
                                  + "bekannter Scroll-Taste — etwa Codex — wirkt das Rad unabhängig davon.")
                SegmentedControl {
                    model: [qsTr("Nur auf Anforderung"), qsTr("Immer")]
                    currentIndex: page.host.app.altScrollMode
                    onActivated: (i) => page.host.app.altScrollMode = i
                }
            }
        }
    }
}
