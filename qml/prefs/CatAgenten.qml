pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QTmux

// Kategorie „Agenten & MCP" (QTMUX-47, Tabelle A4 + Schritt 6): Abo-Matrix (Empfänger ↓ /
// Quelle →) für Inter-Agenten-Benachrichtigungen, darunter der MCP-Server. Mapping auf die
// bestehende API (AgentEvents.subscribe): leere Quell-Liste = „alle anderen"; wird eine
// einzelne Zelle abgewählt, wird die explizite Quell-Liste geschrieben. Arten-Chips je
// Empfänger (fertig/Frage/Fehler, leere Liste = alle). Die Diagonale (Session hört sich
// selbst) ist gesperrt.
CatPage {
    id: page
    heading: qsTr("Agenten & MCP")
    subtitle: qsTr("Steuerung durch KI-Agenten und Benachrichtigungen zwischen Sitzungen.")

    // Abos reaktiv aus dem Hub gespiegelt.
    property var agentSubs: AgentEvents.subscriptions()
    Connections {
        target: AgentEvents
        function onSubscriptionsChanged() { page.agentSubs = AgentEvents.subscriptions() }
    }
    function agentSubFor(id) {
        for (let i = 0; i < agentSubs.length; ++i)
            if (agentSubs[i].subscriberSessionId === id) return agentSubs[i]
        return null
    }
    function agentKindChecked(id, kind) {
        const s = agentSubFor(id)
        if (!s) return false
        return s.kinds.length === 0 || s.kinds.indexOf(kind) >= 0
    }

    // Alle aktuellen Session-IDs (zum Materialisieren von „alle Quellen").
    function allSessionIds() {
        const ids = []
        const n = page.host.sessions.count
        for (let i = 0; i < n; ++i) {
            const s = page.host.sessions.sessionAt(i)
            if (s) ids.push(s.sessionId)
        }
        return ids
    }

    // Hört Empfänger r die Quelle s? (leere Quell-Liste = alle anderen)
    function cellOn(r, s) {
        const sub = agentSubFor(r)
        if (!sub) return false
        if (sub.sources.length === 0) return true
        return sub.sources.indexOf(s) >= 0
    }
    // Zelle (r hört s) umschalten. „alle" wird beim Abwählen materialisiert; sind wieder
    // alle Quellen an, auf die leere Liste (= alle) zusammengefasst; keine Quelle → Abo weg.
    function toggleCell(r, s) {
        const sub = agentSubFor(r)
        const all = allSessionIds().filter(id => id !== r)
        let cur = sub ? (sub.sources.length === 0 ? all.slice() : sub.sources.slice()) : []
        const idx = cur.indexOf(s)
        if (idx >= 0) cur.splice(idx, 1)
        else cur.push(s)
        const kinds = sub ? sub.kinds : []
        if (cur.length === 0) { AgentEvents.unsubscribe(r); return }
        if (cur.length === all.length) AgentEvents.subscribe(r, [], kinds)
        else AgentEvents.subscribe(r, cur, kinds)
    }
    // Ereignisart je Empfänger umschalten (nur bei bestehendem Abo). Leere Arten-Liste = alle.
    function toggleKind(r, kind) {
        const sub = agentSubFor(r)
        if (!sub) return
        let cur = sub.kinds.length === 0 ? ["done", "question", "error"] : sub.kinds.slice()
        const idx = cur.indexOf(kind)
        if (idx >= 0) cur.splice(idx, 1)
        else cur.push(kind)
        if (cur.length === 3) cur = []   // wieder alle → leere Liste
        AgentEvents.subscribe(r, sub.sources, cur)
    }

    readonly property var kindDefs: [
        { k: "done",     label: qsTr("fertig") },
        { k: "question", label: qsTr("Frage") },
        { k: "error",    label: qsTr("Fehler") }
    ]

    readonly property int labelW: 172
    readonly property int cellW: 42

    // --- Wiederherstellung beim Start (QTMUX-85) ---
    PrefAnchor {
        settingKey: "agenten.restore"
        page: page
        PrefGroup {
            title: qsTr("Wiederherstellung")
            PrefRow {
                title: qsTr("Agenten beim Start wiederherstellen")
                description: qsTr("Setzt in jedem Pane den zuletzt erkannten Agenten erneut ab, sobald die "
                                + "Shell bereit ist. Es wird ausschließlich ein bekannter Agent gestartet — "
                                + "beliebige Befehle laufen nicht automatisch los.")
                AppSwitch {
                    checked: page.host.app.restoreAgents
                    onToggled: page.host.app.restoreAgents = checked
                }
            }
            // Der richtige Weg hängt am Nutzungsverhalten — deshalb je Wahl der konkrete
            // Preis, nicht nur die Funktion. Vier Optionen: bleibt eine ComboBox (der
            // Segment-Umschalter ist für ≤ 3 gedacht, s. Design 1a C3).
            PrefRow {
                title: qsTr("Unterhaltung fortsetzen")
                description: {
                    switch (page.host.app.resumeAgentMode) {
                    case 1: return qsTr("Der Agent nimmt die JÜNGSTE Unterhaltung seines Arbeitsverzeichnisses. "
                                      + "Richtig, solange dort nur ein Agent arbeitet — laufen mehrere im selben "
                                      + "Ordner, bekommen sie alle dieselbe.")
                    case 2: return qsTr("Der Agent öffnet beim Start seine eigene Auswahlliste; du entscheidest je "
                                      + "Pane. Es wird nichts geraten, kostet aber einen Klick. Derzeit bietet nur "
                                      + "Claude Code eine solche Liste an.")
                    case 3: return qsTr("Genau die Unterhaltung, die der Agent zuletzt selbst gemeldet hat "
                                      + "(MCP-Werkzeug set_agent_session) — auch bei mehreren Agenten im selben "
                                      + "Ordner eindeutig. Meldet er nichts, startet er frisch. QTmux kann die "
                                      + "Kennung nicht selbst ermitteln: sie entsteht im Agenten und ändert sich "
                                      + "bei /resume oder /clear.")
                    default: return qsTr("Der Agent startet mit einer frischen Unterhaltung.")
                    }
                }
                rowEnabled: page.host.app.restoreAgents
                controlWidth: 210
                AppComboBox {
                    Layout.fillWidth: true
                    model: [qsTr("Gar nicht"), qsTr("Jüngste im Verzeichnis"),
                            qsTr("Auswahl beim Start"), qsTr("Gemeldete Sitzung")]
                    currentIndex: page.host.app.resumeAgentMode
                    onActivated: (i) => page.host.app.resumeAgentMode = i
                }
            }
        }
    }

    // --- Agenten-Benachrichtigungen (Matrix) ---
    PrefAnchor { settingKey: "agenten.notifications"; page: page
    ColumnLayout {
        Layout.fillWidth: true
        spacing: 6
        SectionLabel { text: qsTr("Benachrichtigungen") }
        Text {
            text: qsTr("Zeile = Empfänger, Spalte = Quelle. Ein Häkchen bedeutet: der "
                     + "Empfänger wird über Ereignisse der Quelle benachrichtigt. Agenten "
                     + "abonnieren sich meist selbst per MCP (subscribe_events).")
            color: Theme.textDim
            font.pixelSize: 11
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }

        // Waagerecht scrollbar, falls viele Sessions offen sind.
        Flickable {
            Layout.fillWidth: true
            visible: page.host.sessions.count > 0
            implicitHeight: matrixCol.implicitHeight
            contentWidth: matrixCol.implicitWidth
            contentHeight: matrixCol.implicitHeight
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            ScrollIndicator.horizontal: ScrollIndicator {}

            ColumnLayout {
                id: matrixCol
                spacing: 4

                // Kopfzeile: Quell-Spalten (#id, Tooltip Titel + CWD) + „Arten".
                RowLayout {
                    spacing: 0
                    Item { Layout.preferredWidth: page.labelW; Layout.preferredHeight: 1 }
                    Repeater {
                        model: page.host.sessions
                        delegate: Item {
                            id: hcol
                            required property var session
                            required property string title
                            required property string workingDir
                            Layout.preferredWidth: page.cellW
                            Layout.preferredHeight: 22
                            Text {
                                anchors.centerIn: parent
                                text: "#" + hcol.session.sessionId
                                color: Theme.textDim
                                font.pixelSize: 11
                            }
                            HoverHandler { id: hcolHover }
                            ToolTip.visible: hcolHover.hovered
                            ToolTip.text: hcol.workingDir.length > 0
                                          ? hcol.title + " · " + hcol.workingDir : hcol.title
                        }
                    }
                    Text {
                        text: qsTr("Arten")
                        color: Theme.textDim
                        font.pixelSize: 11
                        Layout.leftMargin: 12
                    }
                }

                // Eine Zeile je Empfänger.
                Repeater {
                    model: page.host.sessions
                    delegate: RowLayout {
                        id: recv
                        required property var session
                        required property string title
                        required property string workingDir
                        spacing: 0

                        Text {
                            Layout.preferredWidth: page.labelW
                            text: recv.title + "  #" + recv.session.sessionId
                            color: Theme.textBright
                            font.pixelSize: 12
                            elide: Text.ElideRight
                        }

                        // Quell-Zellen.
                        Repeater {
                            model: page.host.sessions
                            delegate: Item {
                                id: cell
                                required property var session
                                Layout.preferredWidth: page.cellW
                                Layout.preferredHeight: 28
                                readonly property bool selfCell: cell.session.sessionId === recv.session.sessionId
                                // Diagonale gesperrt.
                                Text {
                                    visible: cell.selfCell
                                    anchors.centerIn: parent
                                    text: "—"
                                    color: Theme.textDim
                                    font.pixelSize: 13
                                }
                                // Toggle-Kachel (kein CheckBox → Bindung an das Modell bleibt
                                // beim Klick erhalten, wichtig für die Kreuzeffekte der Matrix).
                                Rectangle {
                                    visible: !cell.selfCell
                                    anchors.centerIn: parent
                                    width: 20; height: 20; radius: 4
                                    readonly property bool on: page.cellOn(recv.session.sessionId, cell.session.sessionId)
                                    color: on ? Theme.accent : Theme.bgElevated
                                    border.color: on ? Theme.accent : Theme.border
                                    border.width: 1
                                    Text {
                                        anchors.centerIn: parent
                                        visible: parent.on
                                        text: "✓"
                                        color: Theme.accentText
                                        font.pixelSize: 12
                                    }
                                    TapHandler {
                                        onTapped: page.toggleCell(recv.session.sessionId, cell.session.sessionId)
                                    }
                                }
                            }
                        }

                        // Arten-Chips (nur bei bestehendem Abo aktiv).
                        RowLayout {
                            id: kinds
                            Layout.leftMargin: 12
                            spacing: 6
                            readonly property int rid: recv.session.sessionId
                            readonly property bool hasSub: page.agentSubFor(rid) !== null
                            Repeater {
                                model: page.kindDefs
                                delegate: Rectangle {
                                    id: chip
                                    required property var modelData
                                    radius: 10
                                    implicitWidth: chipText.implicitWidth + 16
                                    implicitHeight: 22
                                    readonly property bool on: kinds.hasSub && page.agentKindChecked(kinds.rid, chip.modelData.k)
                                    color: on ? Theme.accent : Theme.bgElevated
                                    opacity: kinds.hasSub ? 1.0 : 0.4
                                    border.color: on ? Theme.accent : Theme.border
                                    border.width: 1
                                    Text {
                                        id: chipText
                                        anchors.centerIn: parent
                                        text: chip.modelData.label
                                        color: chip.on ? Theme.accentText : Theme.textDim
                                        font.pixelSize: 11
                                    }
                                    TapHandler {
                                        enabled: kinds.hasSub
                                        onTapped: page.toggleKind(kinds.rid, chip.modelData.k)
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        Text {
            visible: page.host.sessions.count === 0
            text: qsTr("Keine Sessions geöffnet.")
            color: Theme.textDim
            font.pixelSize: 11
        }
    }
    }

    // --- MCP-Server (darunter) ---
    PrefAnchor { settingKey: "agenten.mcp"; page: page
    PrefGroup {
        title: qsTr("Agenten-Steuerung (MCP)")
        PrefRow {
            title: qsTr("MCP-Server aktiv")
            description: page.host.mcp.lastError.length > 0
                         ? page.host.mcp.lastError
                         : qsTr("Erreichbar unter %1:%2. Der Secrets-Vault ist über MCP "
                              + "bewusst NICHT erreichbar, und die Einstellungen dieser "
                              + "Gruppe lassen sich über MCP nicht ändern.")
                           .arg(page.host.mcp.effectiveBindAddress).arg(page.host.mcp.port)
            AppSwitch {
                checked: page.host.mcp.listening
                onToggled: checked ? page.host.mcp.start() : page.host.mcp.stop()
            }
        }
        PrefRow {
            title: qsTr("Port")
            // Fehlermeldung steht in derselben Zeile — sie gehört zu diesem Feld und war
            // vorher ein freistehender Absatz weiter unten.
            description: page.mcpPortError.length > 0
                         ? page.mcpPortError
                         : qsTr("Wird gespeichert und beim nächsten Start verwendet; die "
                              + "Umgebungsvariable QTMUX_MCP_PORT hat Vorrang.")
            TextField {
                id: mcpPortField
                Layout.preferredWidth: 90
                text: page.host.mcp.port
                inputMethodHints: Qt.ImhDigitsOnly
                validator: IntValidator { bottom: 1024; top: 65535 }
                onAccepted: page.applyMcpPort(mcpPortField)
            }
            Button {
                text: qsTr("Übernehmen")
                font.pixelSize: 12
                enabled: mcpPortField.text != page.host.mcp.port
                onClicked: page.applyMcpPort(mcpPortField)
            }
        }
        // --- Netzzugang (QTMUX-127) ---
        // Der Schalter ist die eigentliche Entscheidung; das Adressfeld darunter ist für
        // den selteneren Fall „nur an dieser einen Schnittstelle". Beide schreiben
        // DIESELBE Einstellung (mcp.bindAddress), es gibt also keine zweite Wahrheit.
        PrefRow {
            title: qsTr("Im Netzwerk erreichbar")
            description: page.host.mcp.bindFromEnvironment
                         ? qsTr("Vorgegeben durch QTMUX_MCP_BIND — diese Einstellung wirkt "
                              + "gerade nicht.")
                         : (page.host.mcp.networkAccess
                            ? qsTr("Andere Rechner können QTmux fernsteuern. Über MCP lässt "
                                 + "sich beliebiger Text in laufende Terminals schreiben, "
                                 + "deshalb ist das Token Pflicht. Zusätzlich auf Netzebene "
                                 + "einschränken (macOS: pf, s. tools/pf/).")
                            : qsTr("Aus: nur Programme auf diesem Rechner (127.0.0.1) — das "
                                 + "ist die Vorgabe und braucht kein Token."))
            AppSwitch {
                enabled: !page.host.mcp.bindFromEnvironment
                checked: page.host.mcp.networkAccess
                onToggled: page.applyNetworkAccess(checked)
            }
        }
        PrefRow {
            title: qsTr("Bind-Adresse")
            description: page.mcpBindError.length > 0
                         ? page.mcpBindError
                         : qsTr("127.0.0.1 = nur dieser Rechner · 0.0.0.0 = alle "
                              + "Schnittstellen · oder eine bestimmte Adresse wie "
                              + "192.168.0.10. QTMUX_MCP_BIND hat Vorrang.")
            TextField {
                id: mcpBindField
                Layout.preferredWidth: 150
                enabled: !page.host.mcp.bindFromEnvironment
                text: page.host.mcp.effectiveBindAddress
                onAccepted: page.applyMcpBind(mcpBindField.text)
            }
            Button {
                text: qsTr("Übernehmen")
                font.pixelSize: 12
                enabled: !page.host.mcp.bindFromEnvironment
                         && mcpBindField.text != page.host.mcp.effectiveBindAddress
                onClicked: page.applyMcpBind(mcpBindField.text)
            }
        }
        PrefRow {
            title: qsTr("Zugriffs-Token")
            description: page.host.mcp.tokenFromEnvironment
                         ? qsTr("Kommt aus QTMUX_MCP_TOKEN — hier nicht änderbar.")
                         : (page.host.mcp.networkAccess
                            ? qsTr("Clients schicken es als Kopfzeile "
                                 + "„Authorization: Bearer <token>“; ohne gültiges Token "
                                 + "antwortet der Server mit 401.")
                            : qsTr("Wird erst geprüft, wenn der Server im Netzwerk "
                                 + "erreichbar ist. Lokale Clients brauchen keins."))
            TextField {
                id: mcpTokenField
                Layout.preferredWidth: 260
                readOnly: true
                // Sichtbar erst auf Klick: Das Fenster steht oft offen, während jemand
                // zusieht oder den Bildschirm teilt.
                echoMode: page.tokenVisible ? TextInput.Normal : TextInput.Password
                text: page.host.mcp.token
                placeholderText: qsTr("kein Token")
                font.family: "monospace"
            }
            Button {
                text: page.tokenVisible ? qsTr("Verbergen") : qsTr("Anzeigen")
                font.pixelSize: 12
                enabled: page.host.mcp.token.length > 0
                onClicked: page.tokenVisible = !page.tokenVisible
            }
            Button {
                text: qsTr("Kopieren")
                font.pixelSize: 12
                enabled: page.host.mcp.token.length > 0
                onClicked: App.copyToClipboard(page.host.mcp.token)
            }
            Button {
                text: qsTr("Neu erzeugen")
                font.pixelSize: 12
                enabled: !page.host.mcp.tokenFromEnvironment
                onClicked: {
                    page.host.mcp.generateToken()
                    page.tokenVisible = true   // frisch erzeugt = jetzt zum Übertragen da
                }
            }
        }
    }
    }

    // Portwechsel (QTMUX-46): erst auf Klick/Enter anwenden, nicht bei jedem Anschlag.
    property string mcpPortError: ""
    function applyMcpPort(field) {
        const p = parseInt(field.text)
        if (!(p >= 1024 && p <= 65535)) { mcpPortError = qsTr("Bitte einen Port zwischen 1024 und 65535 angeben."); return }
        mcpPortError = ""
        const wasListening = page.host.mcp.listening
        if (wasListening) page.host.mcp.stop()
        page.host.mcp.port = p
        if (wasListening && !page.host.mcp.start())
            mcpPortError = qsTr("Port %1 ließ sich nicht öffnen (belegt?). Server ist aus.").arg(p)
    }

    // Netzzugang (QTMUX-127). Wie beim Port gilt: neu binden heißt stoppen und starten —
    // eine laufende Bindung lässt sich nicht umhängen. Scheitert der Start (kein Token,
    // Adresse ungültig, Port belegt), steht der Grund in mcp.lastError und der Server
    // bleibt aus, statt still auf der alten Adresse weiterzuhören.
    property string mcpBindError: ""
    property bool tokenVisible: false
    function applyMcpBind(text) {
        mcpBindError = ""
        const wasListening = page.host.mcp.listening
        if (wasListening) page.host.mcp.stop()
        page.host.mcp.bindAddress = text
        if (wasListening && !page.host.mcp.start())
            mcpBindError = page.host.mcp.lastError
    }
    function applyNetworkAccess(on) {
        mcpBindError = ""
        const wasListening = page.host.mcp.listening
        if (wasListening) page.host.mcp.stop()
        page.host.mcp.setNetworkAccess(on)
        if (on) page.tokenVisible = true    // das erzeugte Token muss man ablesen können
        if (wasListening && !page.host.mcp.start())
            mcpBindError = page.host.mcp.lastError
    }
}
