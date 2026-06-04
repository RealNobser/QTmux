import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QtCore
import QTmux

ApplicationWindow {
    id: window
    width: 1100
    height: 720
    visible: true
    title: "QTmux"
    color: Theme.bgMain

    property int currentRow: -1
    // Aktive (fokussierte) Session ans Model melden -> löscht deren Aufmerksamkeits-Hinweis.
    onCurrentRowChanged: sessions.setActiveRow(currentRow)

    SessionModel { id: sessions }

    // MCP-Server: externe Agenten-Steuerung über 127.0.0.1 (nur lokal).
    McpServer {
        id: mcp
        sessions: sessions
        port: 7345
        onFocusRequested: (row) => window.currentRow = row
        onSetThemeRequested: (mode) => Theme.mode = mode
        Component.onCompleted: start()
    }

    // Hält currentRow gültig, wenn Sessions entfernt werden (manuell oder bei Shell-Ende).
    Connections {
        target: sessions
        function onRowsRemoved(parent, first, last) {
            const removed = last - first + 1
            if (window.currentRow > last)
                window.currentRow -= removed
            else if (window.currentRow >= first)
                window.currentRow = Math.min(first, sessions.count - 1)
        }
        // Fenster-Alert (Dock-Hüpfen/Taskbar-Blinken), wenn QTmux nicht im Vordergrund ist.
        function onAttentionRaised(row) {
            if (!window.active) window.alert(0)
        }
    }

    // Vom Split-Button gewählter Standardtyp (0=Shell, 1=SSH, 2=Seriell), persistiert.
    property int newSessionType: 0

    function newSession() {
        currentRow = sessions.createShellSession()
    }
    function closeCurrent() {
        if (currentRow < 0) return
        sessions.closeSession(currentRow)
        currentRow = Math.min(currentRow, sessions.count - 1)
    }
    function typeLabel(t) {
        return t === 1 ? qsTr("SSH") : t === 2 ? qsTr("Seriell") : qsTr("Shell")
    }
    function openNewSession(t) {
        if (t === 1) sshDialog.open()
        else if (t === 2) serialDialog.openDialog()
        else newSession()
    }

    // Beim Start die persistierten Sessions wiederherstellen; sonst eine neue öffnen.
    Component.onCompleted: {
        const active = sessions.restoreState()
        if (sessions.count === 0)
            newSession()
        else
            currentRow = (active >= 0 && active < sessions.count) ? active : 0
    }

    // Beim Schließen den finalen Zustand sichern (u. a. aktuelles Arbeitsverzeichnis).
    onClosing: sessions.saveState()

    // Fenstergeometrie + gewählter Session-Typ über Neustarts erhalten.
    Settings {
        category: "window"
        property alias x: window.x
        property alias y: window.y
        property alias width: window.width
        property alias height: window.height
        property alias newSessionType: window.newSessionType
    }

    // --- Zentrale Aktionen: im Menü UND per Shortcut/Button nutzbar ----------
    Action {
        id: actNewSession
        text: qsTr("Neue Session")
        shortcut: "Ctrl+T"
        onTriggered: window.newSession()
    }
    Action {
        id: actCloseSession
        text: qsTr("Session schließen")
        shortcut: "Ctrl+W"
        enabled: window.currentRow >= 0
        onTriggered: window.closeCurrent()
    }
    Action {
        id: actToggleTheme
        text: Theme.dark ? qsTr("Helles Design") : qsTr("Dunkles Design")
        shortcut: "Ctrl+D"
        onTriggered: Theme.toggle()
    }
    Action {
        id: actQuit
        text: qsTr("Beenden")
        shortcut: "Ctrl+Q"
        onTriggered: Qt.quit()
    }

    // --- Menüleiste: bietet alle Oberflächen-Befehle ------------------------
    menuBar: MenuBar {
        Menu {
            title: qsTr("Datei")
            MenuItem { action: actNewSession }
            MenuItem {
                text: qsTr("Neue SSH-Verbindung …")
                onTriggered: sshDialog.open()
            }
            MenuItem {
                text: qsTr("Neue serielle Verbindung …")
                onTriggered: serialDialog.openDialog()
            }
            MenuItem { action: actCloseSession }
            MenuSeparator {}
            MenuItem { action: actQuit }
        }
        Menu {
            title: qsTr("Ansicht")
            MenuItem { action: actToggleTheme }
            MenuSeparator {}
            MenuItem {
                text: qsTr("Design: Wie System")
                checkable: true
                checked: Theme.mode === Theme.System
                onTriggered: Theme.mode = Theme.System
            }
            MenuItem {
                text: qsTr("Design: Hell")
                checkable: true
                checked: Theme.mode === Theme.Light
                onTriggered: Theme.mode = Theme.Light
            }
            MenuItem {
                text: qsTr("Design: Dunkel")
                checkable: true
                checked: Theme.mode === Theme.Dark
                onTriggered: Theme.mode = Theme.Dark
            }
        }
        Menu {
            title: qsTr("Sprache")
            Repeater {
                model: App.languageCodes()
                MenuItem {
                    required property string modelData
                    text: App.languageName(modelData)
                    checkable: true
                    checked: App.language === modelData
                    onTriggered: App.language = modelData
                }
            }
        }
        Menu {
            title: qsTr("Agent")
            MenuItem {
                text: qsTr("Neue Agent-Session …")
                onTriggered: window.newSession()
            }
        }
        Menu {
            title: qsTr("Agent-Steuerung")
            MenuItem {
                text: mcp.listening ? qsTr("MCP-Server: an (127.0.0.1:%1)").arg(mcp.port)
                                    : qsTr("MCP-Server: aus")
                checkable: true
                checked: mcp.listening
                onTriggered: mcp.listening ? mcp.stop() : mcp.start()
            }
        }
        Menu {
            title: qsTr("Hilfe")
            MenuItem {
                text: qsTr("Über QTmux")
                onTriggered: aboutDialog.open()
            }
        }
    }

    RowLayout {
        anchors.fill: parent
        spacing: 0

        // --- Vertikale Sidebar ----------------------------------------------
        Rectangle {
            Layout.preferredWidth: 240
            Layout.fillHeight: true
            color: Theme.bgSidebar

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 12
                spacing: 8

                Text {
                    text: "QTmux"
                    color: Theme.textBright
                    font.pixelSize: 18
                    font.bold: true
                    Layout.bottomMargin: 8
                }

                ListView {
                    id: sessionList
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    spacing: 4
                    model: sessions

                    delegate: Rectangle {
                        required property int index
                        required property string title
                        required property int runState
                        required property string agentId
                        required property bool needsAttention
                        required property string lastNotification
                        width: ListView.view.width
                        height: 48
                        radius: 8
                        color: index === window.currentRow ? Theme.sidebarSelected
                             : hover.hovered ? Theme.sidebarHover : "transparent"

                        HoverHandler { id: hover }
                        TapHandler { onTapped: window.currentRow = index }

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 10
                            anchors.rightMargin: 10
                            spacing: 10

                            // Status-Ring. Aufmerksamkeit (blau, pulsierend) hat Vorrang,
                            // sonst: 0=Starting 1=Running 2=WaitingInput 3=Error 4=Closed
                            Rectangle {
                                id: statusRing
                                width: 10; height: 10; radius: 5
                                color: needsAttention ? Theme.accent
                                     : runState === 1 ? "#46d369"
                                     : runState === 2 ? "#f5c451"
                                     : runState === 3 ? "#e5534b"
                                     : runState === 4 ? "#5a5d6a"
                                     : Theme.textDim
                                SequentialAnimation on opacity {
                                    running: needsAttention
                                    loops: Animation.Infinite
                                    alwaysRunToEnd: true
                                    NumberAnimation { to: 0.3; duration: 600 }
                                    NumberAnimation { to: 1.0; duration: 600 }
                                    onStopped: statusRing.opacity = 1.0
                                }
                            }
                            ColumnLayout {
                                spacing: 1
                                Layout.fillWidth: true
                                Text {
                                    text: title
                                    color: Theme.textBright
                                    font.pixelSize: 13
                                    elide: Text.ElideRight
                                    Layout.fillWidth: true
                                }
                                // Untertitel: Notification (Vorrang) oder erkannter Agent.
                                Text {
                                    visible: lastNotification.length > 0 || agentId.length > 0
                                    text: lastNotification.length > 0
                                          ? lastNotification
                                          : qsTr("Agent: %1").arg(agentId)
                                    color: needsAttention ? Theme.accent : Theme.textDim
                                    font.pixelSize: 10
                                    elide: Text.ElideRight
                                    Layout.fillWidth: true
                                }
                            }

                            // Schließen-Button (×), erscheint bei Hover oder Auswahl.
                            Rectangle {
                                Layout.preferredWidth: 20
                                Layout.preferredHeight: 20
                                radius: 4
                                visible: hover.hovered || index === window.currentRow
                                color: closeHover.hovered ? Theme.border : "transparent"
                                Text {
                                    anchors.centerIn: parent
                                    text: "×"
                                    color: Theme.textDim
                                    font.pixelSize: 16
                                }
                                HoverHandler { id: closeHover }
                                TapHandler { onTapped: sessions.closeSession(index) }
                            }
                        }
                    }
                }

                // Split-Button: "+ <Typ>" öffnet den gewählten Typ; Caret "▾" wählt den Typ.
                RowLayout {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 40
                    spacing: 1

                    Button {
                        id: newBtn
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        text: "+  " + window.typeLabel(window.newSessionType)
                        onClicked: window.openNewSession(window.newSessionType)
                        background: Rectangle {
                            // links abgerundet, rechts eckig (verschmilzt mit dem Caret).
                            radius: 8
                            color: newBtn.down ? Theme.sidebarSelected
                                 : newBtn.hovered ? Theme.sidebarHover : Theme.bgElevated
                            border.color: Theme.border
                            border.width: 1
                            Rectangle {
                                anchors.right: parent.right
                                width: parent.radius; height: parent.height
                                color: parent.color
                                border.color: parent.border.color
                                border.width: parent.border.width
                            }
                        }
                        contentItem: Text {
                            text: newBtn.text
                            color: Theme.textBright
                            font.pixelSize: 13
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                    }

                    Button {
                        id: caretBtn
                        Layout.preferredWidth: 32
                        Layout.fillHeight: true
                        text: "▾"
                        onClicked: typeMenu.popup()
                        background: Rectangle {
                            // rechts abgerundet, links eckig.
                            radius: 8
                            color: caretBtn.down ? Theme.sidebarSelected
                                 : caretBtn.hovered ? Theme.sidebarHover : Theme.bgElevated
                            border.color: Theme.border
                            border.width: 1
                            Rectangle {
                                anchors.left: parent.left
                                width: parent.radius; height: parent.height
                                color: parent.color
                                border.color: parent.border.color
                                border.width: parent.border.width
                            }
                        }
                        contentItem: Text {
                            text: caretBtn.text
                            color: Theme.textBright
                            font.pixelSize: 13
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }

                        Menu {
                            id: typeMenu
                            MenuItem {
                                text: qsTr("Shell")
                                checkable: true
                                checked: window.newSessionType === 0
                                onTriggered: window.newSessionType = 0
                            }
                            MenuItem {
                                text: qsTr("SSH …")
                                checkable: true
                                checked: window.newSessionType === 1
                                onTriggered: window.newSessionType = 1
                            }
                            MenuItem {
                                text: qsTr("Seriell …")
                                checkable: true
                                checked: window.newSessionType === 2
                                onTriggered: window.newSessionType = 2
                            }
                        }
                    }
                }
            }
        }

        // --- Hauptbereich: Terminal der aktuellen Session -------------------
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: Theme.bgMain

            TerminalItem {
                id: terminal
                anchors.fill: parent
                anchors.margins: 6
                focus: true
                pointSize: 13
                backgroundColor: Theme.terminalBg
                foregroundColor: Theme.terminalFg
                session: window.currentRow >= 0 ? sessions.sessionAt(window.currentRow) : null
            }
        }
    }

    // --- SSH-Verbindung öffnen ---------------------------------------------
    Dialog {
        id: sshDialog
        anchors.centerIn: parent
        width: 420
        modal: true
        title: qsTr("SSH-Verbindung")
        standardButtons: Dialog.Ok | Dialog.Cancel

        onAccepted: {
            if (sshHost.text.length > 0) {
                window.currentRow = sessions.createSshSession(
                    sshHost.text, parseInt(sshPort.text) || 22, sshUser.text, sshIdentity.text)
            }
        }

        ColumnLayout {
            anchors.fill: parent
            spacing: 10
            GridLayout {
                columns: 2
                columnSpacing: 10
                rowSpacing: 8
                Layout.fillWidth: true
                Text { text: qsTr("Host"); color: Theme.textBright }
                TextField { id: sshHost; Layout.fillWidth: true; placeholderText: "example.com" }
                Text { text: qsTr("Benutzer"); color: Theme.textBright }
                TextField { id: sshUser; Layout.fillWidth: true; placeholderText: Qt.platform.os; text: "" }
                Text { text: qsTr("Port"); color: Theme.textBright }
                TextField { id: sshPort; Layout.fillWidth: true; text: "22" }
                Text { text: qsTr("Identity-Datei"); color: Theme.textBright }
                TextField { id: sshIdentity; Layout.fillWidth: true; placeholderText: "~/.ssh/id_ed25519 (optional)" }
            }
            Text {
                text: qsTr("Passwort/Schlüssel werden im Terminal abgefragt (System-ssh).")
                color: Theme.textDim
                font.pixelSize: 11
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
            }
        }
    }

    // --- Serielle Verbindung öffnen ----------------------------------------
    Dialog {
        id: serialDialog
        anchors.centerIn: parent
        width: 420
        modal: true
        title: qsTr("Serielle Verbindung")
        standardButtons: Dialog.Ok | Dialog.Cancel

        function openDialog() {
            portCombo.model = sessions.availableSerialPorts()
            open()
        }
        onAccepted: {
            if (portCombo.currentText.length > 0) {
                window.currentRow = sessions.createSerialSession(
                    portCombo.currentText, parseInt(baudCombo.currentText))
            }
        }

        ColumnLayout {
            anchors.fill: parent
            spacing: 10
            Text { text: qsTr("Port"); color: Theme.textBright }
            ComboBox {
                id: portCombo
                Layout.fillWidth: true
                model: []
            }
            Text { text: qsTr("Baudrate"); color: Theme.textBright }
            ComboBox {
                id: baudCombo
                Layout.fillWidth: true
                editable: true
                model: ["9600", "19200", "38400", "57600", "115200", "230400", "460800", "921600"]
                currentIndex: 4
            }
            Text {
                visible: portCombo.model.length === 0
                text: qsTr("Keine seriellen Ports gefunden.")
                color: Theme.textDim
                font.pixelSize: 11
            }
        }
    }

    // --- Über-Dialog --------------------------------------------------------
    Dialog {
        id: aboutDialog
        anchors.centerIn: parent
        width: 420
        modal: true
        title: qsTr("Über QTmux")
        standardButtons: Dialog.Ok
        contentItem: Text {
            wrapMode: Text.WordWrap
            color: Theme.textBright
            text: qsTr("QTmux — plattformübergreifender Multi-KI-Agenten-Terminal.\nQt %1").arg(Qt.application.version || "0.1")
        }
    }
}
