import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QTmux

// Kategorie „Agenten & MCP" (QTMUX-47, Tabelle A4): MCP-Server (an/aus + Port) und die
// Inter-Agenten-Benachrichtigungs-Abos. Unverändert aus den settingsDialog-Abschnitten
// „Agenten-Steuerung (MCP)" und „Agenten-Benachrichtigungen". Die Abo-Matrix (Quelle →
// Empfänger) folgt in Schritt 6.
CatPage {
    id: page
    heading: qsTr("Agenten & MCP")
    subtitle: qsTr("Steuerung durch KI-Agenten und Benachrichtigungen zwischen Sitzungen.")

    // Agenten-Benachrichtigungs-Abos (reaktiv aus dem Hub gespiegelt).
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
    // Arten-Checkbox-Status: kein Abo → aus; leere Arten-Liste → „alle" (alle an).
    function agentKindChecked(id, kind) {
        const s = agentSubFor(id)
        if (!s) return false
        return s.kinds.length === 0 || s.kinds.indexOf(kind) >= 0
    }

    // Portwechsel (QTMUX-46): erst auf Klick/Enter anwenden, nicht bei jedem
    // Tastenanschlag — sonst würde der Server bei jeder Zwischenzahl neu binden.
    // Lief er, wird er auf dem neuen Port wieder gestartet; scheitert das (Port
    // belegt), sagt mcpPortError das offen, statt still auszubleiben.
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

    // --- MCP-Server ---
    ColumnLayout {
        spacing: 6
        Layout.fillWidth: true
        SectionLabel { text: qsTr("Agenten-Steuerung (MCP)") }
        CheckBox {
            text: qsTr("MCP-Server aktiv (nur 127.0.0.1)")
            checked: page.host.mcp.listening
            onToggled: checked ? page.host.mcp.start() : page.host.mcp.stop()
        }
        RowLayout {
            Layout.fillWidth: true
            spacing: 8
            Layout.leftMargin: 6
            Text { text: qsTr("Port"); color: Theme.textBright }
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
        Text {
            text: page.mcpPortError.length > 0
                  ? page.mcpPortError
                  : qsTr("Wird gespeichert und beim nächsten Start verwendet. "
                       + "Die Umgebungsvariable QTMUX_MCP_PORT hat Vorrang.")
            color: page.mcpPortError.length > 0 ? "#e5534b" : Theme.textDim
            font.pixelSize: 11
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
            Layout.leftMargin: 6
        }
    }

    // --- Agenten-Benachrichtigungen ---
    ColumnLayout {
        Layout.fillWidth: true
        spacing: 6
        SectionLabel { text: qsTr("Agenten-Benachrichtigungen") }
        // Inter-Agenten-Benachrichtigung: eine Session wird benachrichtigt, wenn ein
        // Agent in einer ANDEREN Session fertig ist oder eine Frage hat. Hier wird je
        // Session ein Abo (auf alle anderen Quellen) ein-/ausgeschaltet und auf
        // Ereignisarten gefiltert. Agenten abonnieren sich i. d. R. selbst per MCP
        // (subscribe_events) und holen die Ereignisse per wait_for_events ab.
        Text {
            text: qsTr("Wähle je Session, ob sie über Ereignisse der anderen Sessions "
                     + "benachrichtigt wird. Feinere Quell-Filter sind über die "
                     + "MCP-Schnittstelle verfügbar.")
            color: Theme.textDim
            font.pixelSize: 11
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }
        Repeater {
            model: page.host.sessions
            delegate: ColumnLayout {
                required property var session
                required property string title
                required property string workingDir
                Layout.fillWidth: true
                spacing: 0
                // Titel + eindeutige Kennung (Session-ID, dazu CWD bei Shells),
                // damit gleichnamige Sessions (z. B. mehrere „Eingabeaufforderung")
                // unterscheidbar sind.
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 6
                    CheckBox {
                        text: title
                        checked: page.agentSubFor(session.sessionId) !== null
                        onToggled: {
                            if (checked) AgentEvents.subscribe(session.sessionId, [], [])
                            else AgentEvents.unsubscribe(session.sessionId)
                        }
                    }
                    Text {
                        Layout.fillWidth: true
                        text: workingDir.length > 0
                              ? qsTr("#%1 · %2").arg(session.sessionId).arg(workingDir)
                              : qsTr("#%1").arg(session.sessionId)
                        color: Theme.textDim
                        font.pixelSize: 11
                        elide: Text.ElideLeft
                    }
                }
                RowLayout {
                    id: kindRow
                    Layout.leftMargin: 26
                    spacing: 10
                    visible: page.agentSubFor(session.sessionId) !== null
                    property int sid: session.sessionId
                    // Aktuelle Arten-Auswahl aus den drei Checkboxen bauen und das
                    // Abo aktualisieren (leere Liste = alle Arten).
                    function apply() {
                        const ks = []
                        if (cbDone.checked) ks.push("done")
                        if (cbQuestion.checked) ks.push("question")
                        if (cbError.checked) ks.push("error")
                        AgentEvents.subscribe(sid, [], ks)
                    }
                    Text { text: qsTr("Arten:"); color: Theme.textDim; font.pixelSize: 11 }
                    CheckBox {
                        id: cbDone; text: qsTr("fertig"); font.pixelSize: 11
                        checked: page.agentKindChecked(kindRow.sid, "done")
                        onToggled: kindRow.apply()
                    }
                    CheckBox {
                        id: cbQuestion; text: qsTr("Frage"); font.pixelSize: 11
                        checked: page.agentKindChecked(kindRow.sid, "question")
                        onToggled: kindRow.apply()
                    }
                    CheckBox {
                        id: cbError; text: qsTr("Fehler"); font.pixelSize: 11
                        checked: page.agentKindChecked(kindRow.sid, "error")
                        onToggled: kindRow.apply()
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
