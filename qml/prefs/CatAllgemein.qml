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

    // --- Aktualisierung (QTMUX-125) ---
    PrefAnchor {
        settingKey: "allgemein.updates"
        page: page
        PrefGroup {
            title: qsTr("Aktualisierung")
            PrefRow {
                title: qsTr("Beim Start automatisch nach Updates suchen")
                description: qsTr("Höchstens einmal am Tag und still: Gibt es nichts Neues "
                                + "oder ist der Server nicht erreichbar, passiert gar nichts. "
                                + "Über das Hilfe-Menü lässt sich jederzeit von Hand suchen.")
                AppSwitch {
                    objectName: "swUpdateAutoCheck"
                    checked: Updates.autoCheck
                    onToggled: Updates.autoCheck = checked
                }
            }
            PrefRow {
                title: qsTr("Jetzt suchen")
                description: Updates.currentVersion !== ""
                             ? qsTr("Installiert ist Version %1.").arg(Updates.currentVersion)
                             : ""
                Button {
                    text: qsTr("Jetzt nach Updates suchen")
                    enabled: !Updates.busy
                    palette.buttonText: Theme.textBright
                    onClicked: page.host.app.checkForUpdates()
                }
            }
        }
    }

    // --- Netzwerk-Proxy (QTMUX-129) ---
    // 🔑 GERÜST: Die Felder schreiben bereits ihre Einstellungen, wirksam werden
    // sie erst, wenn der Proxy-fähige `appupdate`-Kern nachvendiert ist. Der
    // Abschnitt steht trotzdem schon hier, weil Settings-Schlüssel, Suchindex
    // und Übersetzungen sonst später in einem Rutsch nachgezogen werden müssten.
    PrefAnchor {
        settingKey: "allgemein.proxy"
        page: page
        PrefGroup {
            title: qsTr("Netzwerk-Proxy")
            PrefRow {
                title: qsTr("Proxy für die Update-Prüfung")
                // Warum „System“ die Vorgabe ist, gehört hierhin: Im Firmenumfeld
                // steht der Proxy meist schon per WPAD/Systemeinstellung.
                description: qsTr("„System“ übernimmt, was das Betriebssystem vorgibt — im "
                                + "Firmennetz meist das Richtige, ohne dass du etwas eintragen "
                                + "musst. „Direkt“ umgeht den Proxy bewusst; das hilft, wenn eine "
                                + "hinterlegte Konfigurationsdatei nicht erreichbar ist. „Manuell“ "
                                + "nutzt die Angaben darunter.")
                SegmentedControl {
                    model: [qsTr("System"), qsTr("Direkt"), qsTr("Manuell")]
                    currentIndex: Updates.proxyMode
                    onActivated: (i) => Updates.proxyMode = i
                }
            }
            PrefRow {
                title: qsTr("Proxy-Adresse")
                description: qsTr("Nur für „Manuell“. Host und Port des Proxys, dazu die Art der "
                                + "Verbindung.")
                rowEnabled: Updates.proxyMode === 2
                RowLayout {
                    spacing: 8
                    AppComboBox {
                        model: [qsTr("HTTP"), qsTr("SOCKS5")]
                        currentIndex: Updates.proxyType
                        onActivated: Updates.proxyType = currentIndex
                    }
                    TextField {
                        Layout.preferredWidth: 170
                        color: Theme.textBright
                        placeholderText: qsTr("proxy.firma.local")
                        text: Updates.proxyHost
                        onEditingFinished: Updates.proxyHost = text
                    }
                    TextField {
                        Layout.preferredWidth: 70
                        color: Theme.textBright
                        placeholderText: qsTr("Port")
                        text: Updates.proxyPort > 0 ? String(Updates.proxyPort) : ""
                        validator: IntValidator { bottom: 0; top: 65535 }
                        onEditingFinished: Updates.proxyPort = parseInt(text || "0")
                    }
                }
            }
            PrefRow {
                title: qsTr("Benutzername")
                // Der wichtigste Satz des Abschnitts — im Firmenumfeld ist das
                // die erste Frage, und die Antwort ist ein Alleinstellungsmerkmal.
                description: qsTr("Optional. Verlangt der Proxy eine Anmeldung, wird beim nächsten "
                                + "Versuch nach dem Passwort gefragt. Das Passwort wird "
                                + "ausschließlich für die laufende Sitzung im Arbeitsspeicher "
                                + "gehalten — es landet weder in den Einstellungen noch in einer "
                                + "Exportdatei. Der Benutzername wird gespeichert, aber nicht "
                                + "mitexportiert.")
                rowEnabled: Updates.proxyMode === 2
                TextField {
                    Layout.preferredWidth: 200
                    color: Theme.textBright
                    placeholderText: qsTr("Benutzername, ggf. mit Domäne")
                    text: Updates.proxyUser
                    onEditingFinished: Updates.proxyUser = text
                }
            }
            PrefRow {
                title: qsTr("Anmeldedaten dieser Sitzung verwerfen")
                description: Updates.hasProxyCredentials()
                             ? qsTr("Für diese Sitzung liegt ein Passwort im Arbeitsspeicher.")
                             : qsTr("Zurzeit ist kein Passwort gespeichert.")
                Button {
                    text: qsTr("Verwerfen")
                    palette.buttonText: Theme.textBright
                    onClicked: Updates.forgetProxyCredentials()
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
