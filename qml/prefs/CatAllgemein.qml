import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QTmux

// Kategorie „Allgemein" (QTMUX-47, Tabelle A4; Gliederung aus Design 1a C5):
// „Sprache & Design" · „Start & Beenden" (Restore-Modus, Beenden-Rückfrage, Quake) ·
// „Energie". Hier landen genau die Schalter, die mit Stufe 4 aus Datei/Ansicht/Sprache
// herausgefallen sind.
CatPage {
    id: page
    heading: qsTr("Allgemein")
    subtitle: qsTr("Sprache, Erscheinungs-Modus und Verhalten beim Start und Beenden.")

    PrefAnchor {
        settingKey: "allgemein.general"
        page: page
        PrefGroup {
            title: qsTr("Sprache & Design")
            PrefRow {
                title: qsTr("Design")
                description: qsTr("„Wie System“ folgt der Einstellung des Betriebssystems.")
                SegmentedControl {
                    model: [qsTr("Wie System"), qsTr("Hell"), qsTr("Dunkel")]
                    currentIndex: Theme.mode
                    onActivated: (i) => Theme.mode = i
                }
            }
            PrefRow {
                title: qsTr("Sprache")
                description: qsTr("Wirkt sofort; das native macOS-App-Menü folgt erst nach einem Neustart.")
                controlWidth: 180
                AppComboBox {
                    Layout.fillWidth: true
                    textRole: "name"
                    model: App.languageCodes().map(c => ({ code: c, name: App.languageName(c) }))
                    currentIndex: Math.max(0, App.languageCodes().indexOf(App.language))
                    onActivated: (i) => App.language = App.languageCodes()[i]
                }
            }
        }
    }

    // --- Start & Beenden ---
    PrefAnchor {
        settingKey: "allgemein.window"
        page: page
        PrefGroup {
            title: qsTr("Start & Beenden")
            // Wiederherstellung beim Start (QTMUX-99). Bewusst eine Wahl: Der teure Teil ist
            // der Verlauf, der nützliche sind Fenster, Panes und Arbeitsverzeichnisse. Ein
            // Schalter zwänge dazu, beides gemeinsam aufzugeben.
            PrefRow {
                title: qsTr("Sessions beim Start wiederherstellen")
                description: {
                    switch (page.host.app.restoreSessionMode) {
                    case 0: return qsTr("QTmux startet mit einer einzelnen, leeren Session. Der zuletzt "
                                      + "gespeicherte Stand bleibt erhalten — er wird beim Beenden nicht "
                                      + "überschrieben und ist wieder da, sobald hier erneut "
                                      + "wiederhergestellt wird.")
                    case 1: return qsTr("Fenster, Panes und deren Arbeitsverzeichnisse kommen zurück, die "
                                      + "Terminals starten aber leer. Der gespeicherte Verlauf bleibt "
                                      + "liegen und wird bei „Alles“ wieder angezeigt.")
                    default: return qsTr("Fenster, Panes und Arbeitsverzeichnisse kommen zurück, dazu der "
                                       + "farbige Verlauf jedes Panes.")
                    }
                }
                SegmentedControl {
                    model: [qsTr("Gar nicht"), qsTr("Ohne Verlauf"), qsTr("Alles")]
                    currentIndex: page.host.app.restoreSessionMode
                    onActivated: (i) => page.host.app.restoreSessionMode = i
                }
            }
            PrefRow {
                title: qsTr("Agenten in den Panes")
                description: qsTr("Ob die Agenten dabei erneut starten und ihre Unterhaltung fortsetzen, "
                                + "steht unter „Agenten & MCP“.")
                Text {
                    text: "→"
                    color: Theme.textDim
                    font.pixelSize: 15
                    TapHandler { onTapped: page.host.open("agenten", "agenten.restore") }
                    HoverHandler { cursorShape: Qt.PointingHandCursor }
                }
            }
            PrefRow {
                title: qsTr("Vor dem Beenden nachfragen")
                description: qsTr("Beenden schließt alle Sitzungen samt laufender Prozesse.")
                AppSwitch {
                    checked: page.host.app.confirmQuit
                    onToggled: page.host.app.confirmQuit = checked
                }
            }
            PrefRow {
                title: qsTr("Quake-Modus")
                description: Qt.platform.os === "osx"
                             ? qsTr("Globaler Hotkey Strg+^ blendet QTmux überall ein und aus.")
                             : qsTr("Derzeit nur unter macOS verfügbar.")
                rowEnabled: Qt.platform.os === "osx"
                AppSwitch {
                    objectName: "cbQuake"
                    checked: page.host.app.quakeMode
                    onToggled: page.host.app.quakeMode = checked
                }
            }
        }
    }

    // --- Energie (QTMUX-89) ---
    PrefAnchor {
        settingKey: "allgemein.energie"
        page: page
        PrefGroup {
            title: qsTr("Energie")
            PrefRow {
                title: qsTr("Ruhezustand verhindern, solange Agenten arbeiten")
                description: page.host.sessions.sleepInhibitSupported()
                             ? qsTr("Der Rechner bleibt wach, solange mindestens eine Session "
                                  + "„beschäftigt“ meldet — und nur dann. Wartet ein Agent auf eine "
                                  + "Antwort von dir, darf der Rechner schlafen. Der Bildschirm wird "
                                  + "nicht wachgehalten.")
                             : qsTr("Auf dieser Plattform noch nicht verfügbar.")
                rowEnabled: page.host.sessions.sleepInhibitSupported()
                AppSwitch {
                    checked: page.host.app.preventSleep
                    onToggled: page.host.app.preventSleep = checked
                }
            }
            // Sichtbar machen, WANN die Sperre greift — sonst wirkt ein wacher Rechner wie ein
            // Fehler und niemand kann es nachvollziehen.
            PrefRow {
                title: qsTr("Zustand")
                description: page.host.sessions.sleepInhibited
                             ? qsTr("Aktiv — der Ruhezustand ist gerade gesperrt.")
                             : qsTr("Zurzeit nicht gesperrt: keine Session arbeitet.")
                visible: page.host.app.preventSleep
                Rectangle {
                    implicitWidth: 10; implicitHeight: 10; radius: 5
                    color: page.host.sessions.sleepInhibited ? Theme.accent : Theme.textDim
                }
            }
        }
    }
}
