import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QtQuick.Effects
import QtCore
import QTmux

ApplicationWindow {
    id: window
    width: 1100
    height: 720
    visible: true
    title: "QTmux"
    color: Theme.bgMain

    // Themengebundene Palette: alle (In-Window-)Basic-Controls — Dialoge, ComboBoxen,
    // Textfelder, Buttons, das typeMenu-Popup — erben diese Farben automatisch.
    palette.window: Theme.bgMain
    palette.windowText: Theme.textBright
    palette.base: Theme.bgElevated
    palette.alternateBase: Theme.bgSidebar
    palette.text: Theme.textBright
    palette.button: Theme.bgElevated
    palette.buttonText: Theme.textBright
    palette.highlight: Theme.accent
    palette.highlightedText: "#ffffff"
    palette.mid: Theme.border
    palette.dark: Theme.border
    palette.placeholderText: Theme.textDim
    palette.toolTipBase: Theme.bgElevated
    palette.toolTipText: Theme.textBright

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

    // Pfad zu einem Phosphor-SVG-Icon (eingebettet unter qrc:/icons/).
    function icon(name) { return "qrc:/icons/" + name + ".svg" }

    // --- Wiederverwendbares Icon-Steuerelement (Phosphor-SVG) ---------------
    // Toolbar-Knopf: zeigt ein SVG-Icon; Tönung folgt Hover/aktiv/Theme.
    component IconToolButton: ToolButton {
        id: tb
        property string tip: ""
        property bool active: false        // dauerhaft hervorgehoben (z. B. Server an)
        display: AbstractButton.IconOnly
        icon.width: 18
        icon.height: 18
        icon.color: !tb.enabled ? Theme.border
                  : (tb.down || tb.active) ? Theme.accent
                  : tb.hovered ? Theme.textBright : Theme.textDim
        implicitWidth: 36
        implicitHeight: 30
        background: Rectangle {
            radius: 6
            color: tb.down ? Theme.sidebarSelected
                 : tb.hovered ? Theme.sidebarHover : "transparent"
        }
        ToolTip.visible: hovered && tip.length > 0
        ToolTip.delay: 600
        ToolTip.text: tip
    }

    // Themen-Menüeintrag (In-Window): abgerundetes Highlight, app-getöntes Icon.
    component AppMenuItem: MenuItem {
        id: ami
        implicitHeight: 34
        icon.color: Theme.textBright
        icon.width: 16
        icon.height: 16
        background: Rectangle {
            radius: 6
            color: ami.highlighted ? Theme.sidebarHover : "transparent"
        }
    }

    // Themen-Popup-Hintergrund (Menüs/ComboBox): abgerundet, erhoben, gerahmt.
    component AppPopupBg: Rectangle {
        color: Theme.bgElevated
        border.color: Theme.border
        border.width: 1
        radius: 8
    }

    // Themen-ComboBox: gerahmtes Feld, Caret-Icon, abgerundetes Popup.
    component AppComboBox: ComboBox {
        id: cb
        implicitHeight: 32
        font.pixelSize: 13
        background: Rectangle {
            radius: 6
            color: Theme.bgElevated
            border.color: cb.activeFocus ? Theme.accent : Theme.border
            border.width: 1
        }
        indicator: Image {
            x: cb.width - width - 10
            y: (cb.height - height) / 2
            source: window.icon("caret-down")
            sourceSize.width: 14
            sourceSize.height: 14
            layer.enabled: true
            layer.effect: MultiEffect {
                colorization: 1.0
                colorizationColor: Theme.textDim
            }
        }
        popup: Popup {
            y: cb.height + 4
            width: cb.width
            padding: 4
            implicitHeight: Math.min(contentItem.implicitHeight + 8, 260)
            background: AppPopupBg {}
            contentItem: ListView {
                clip: true
                implicitHeight: contentHeight
                model: cb.popup.visible ? cb.delegateModel : null
                currentIndex: cb.highlightedIndex
                ScrollIndicator.vertical: ScrollIndicator {}
            }
        }
    }

    // Themen-Dialog: abgerundete erhobene Fläche, gestylter Titel, abgedunkelter Hintergrund.
    component AppDialog: Dialog {
        id: dlg
        anchors.centerIn: parent
        modal: true
        padding: 20
        background: Rectangle {
            color: Theme.bgElevated
            radius: 12
            border.color: Theme.border
            border.width: 1
        }
        header: Label {
            text: dlg.title
            visible: dlg.title.length > 0
            color: Theme.textBright
            font.pixelSize: 16
            font.bold: true
            elide: Label.ElideRight
            padding: 20
            bottomPadding: 6
        }
        Overlay.modal: Rectangle { color: "#88000000" }
    }

    // Session-Typ-Auswahl für den „+"-Split-Button (Sidebar + Toolbar).
    Menu {
        id: typeMenu
        padding: 4
        background: AppPopupBg { implicitWidth: 180 }
        AppMenuItem {
            text: qsTr("Shell")
            icon.source: window.icon("terminal-window")
            checkable: true
            checked: window.newSessionType === 0
            onTriggered: window.newSessionType = 0
        }
        AppMenuItem {
            text: qsTr("SSH …")
            icon.source: window.icon("plugs")
            checkable: true
            checked: window.newSessionType === 1
            onTriggered: window.newSessionType = 1
        }
        AppMenuItem {
            text: qsTr("Seriell …")
            icon.source: window.icon("usb")
            checkable: true
            checked: window.newSessionType === 2
            onTriggered: window.newSessionType = 2
        }
    }

    // Kontextmenü des Terminals (Rechtsklick): Kopieren/Einfügen.
    Menu {
        id: termContextMenu
        padding: 4
        background: AppPopupBg { implicitWidth: 160 }
        AppMenuItem { action: actCopy;  icon.source: window.icon("copy") }
        AppMenuItem { action: actPaste; icon.source: window.icon("clipboard") }
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

    // Beim Schließen erst den Zustand sichern (braucht laufende Prozesse für das
    // aktuelle Arbeitsverzeichnis), dann alle Prozesse/Verbindungen beenden.
    onClosing: {
        sessions.saveState()
        sessions.shutdownAll()
    }

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
    // Kopieren/Einfügen. Shortcut nur auf macOS (Cmd+C/V) — kapert dort NICHT das
    // Terminal-Ctrl+C (SIGINT). Auf Windows/Linux handhabt das TerminalItem selbst
    // Ctrl+Shift+C/V, damit Ctrl+C im Terminal weiter SIGINT bleibt.
    Action {
        id: actCopy
        text: qsTr("Kopieren")
        enabled: terminal.hasSelection
        shortcut: Qt.platform.os === "osx" ? StandardKey.Copy : ""
        onTriggered: terminal.copy()
    }
    Action {
        id: actPaste
        text: qsTr("Einfügen")
        shortcut: Qt.platform.os === "osx" ? StandardKey.Paste : ""
        onTriggered: terminal.paste()
    }

    // --- Toolbar oben: Schnellzugriff mit Phosphor-Icons --------------------
    header: ToolBar {
        // Feste Höhe: das innen mit anchors.fill verankerte RowLayout liefert
        // sonst keine implizite Höhe, die ToolBar würde auf 0 kollabieren.
        height: 44
        background: Rectangle {
            color: Theme.bgElevated
            Rectangle {
                anchors.bottom: parent.bottom
                width: parent.width; height: 1
                color: Theme.border
            }
        }
        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 8
            anchors.rightMargin: 8
            spacing: 3

            IconToolButton {
                icon.source: window.icon("plus")
                tip: qsTr("Neue Session: %1").arg(window.typeLabel(window.newSessionType))
                onClicked: window.openNewSession(window.newSessionType)
            }
            IconToolButton {
                icon.source: window.icon("caret-down")
                icon.width: 14; icon.height: 14
                implicitWidth: 22
                tip: qsTr("Session-Typ wählen")
                onClicked: typeMenu.popup(this, 0, height)
            }

            ToolSeparator {}

            IconToolButton {
                icon.source: window.icon("plugs")
                tip: qsTr("Neue SSH-Verbindung …")
                onClicked: sshDialog.open()
            }
            IconToolButton {
                icon.source: window.icon("usb")
                tip: qsTr("Neue serielle Verbindung …")
                onClicked: serialDialog.openDialog()
            }

            ToolSeparator {}

            IconToolButton {
                icon.source: window.icon("x")
                tip: qsTr("Session schließen")
                enabled: window.currentRow >= 0
                onClicked: window.closeCurrent()
            }

            Item { Layout.fillWidth: true }   // Abstandhalter

            IconToolButton {
                icon.source: Theme.dark ? window.icon("sun") : window.icon("moon")
                tip: Theme.dark ? qsTr("Helles Design") : qsTr("Dunkles Design")
                onClicked: Theme.toggle()
            }
            IconToolButton {
                icon.source: window.icon("broadcast")
                active: mcp.listening
                tip: mcp.listening ? qsTr("MCP-Server: an (127.0.0.1:%1)").arg(mcp.port)
                                   : qsTr("MCP-Server: aus")
                onClicked: mcp.listening ? mcp.stop() : mcp.start()
            }
            IconToolButton {
                icon.source: window.icon("info")
                tip: qsTr("Über QTmux")
                onClicked: aboutDialog.open()
            }
        }
    }

    // --- Menüleiste: bietet alle Oberflächen-Befehle ------------------------
    menuBar: MenuBar {
        Menu {
            title: qsTr("Datei")
            MenuItem { action: actNewSession; icon.source: window.icon("plus"); icon.color: Theme.menuIcon; icon.width: 16; icon.height: 16 }
            MenuItem {
                text: qsTr("Neue SSH-Verbindung …")
                icon.source: window.icon("plugs"); icon.color: Theme.menuIcon; icon.width: 16; icon.height: 16
                onTriggered: sshDialog.open()
            }
            MenuItem {
                text: qsTr("Neue serielle Verbindung …")
                icon.source: window.icon("usb"); icon.color: Theme.menuIcon; icon.width: 16; icon.height: 16
                onTriggered: serialDialog.openDialog()
            }
            MenuItem { action: actCloseSession; icon.source: window.icon("x"); icon.color: Theme.menuIcon; icon.width: 16; icon.height: 16 }
            MenuSeparator {}
            MenuItem { action: actQuit }
        }
        Menu {
            title: qsTr("Bearbeiten")
            MenuItem { action: actCopy;  icon.source: window.icon("copy");      icon.color: Theme.menuIcon; icon.width: 16; icon.height: 16 }
            MenuItem { action: actPaste; icon.source: window.icon("clipboard"); icon.color: Theme.menuIcon; icon.width: 16; icon.height: 16 }
        }
        Menu {
            title: qsTr("Ansicht")
            MenuItem {
                action: actToggleTheme
                icon.source: Theme.dark ? window.icon("sun") : window.icon("moon")
                icon.color: Theme.menuIcon
                icon.width: 16; icon.height: 16
            }
            MenuSeparator {}
            MenuItem {
                text: qsTr("Design: Wie System")
                icon.source: window.icon("gear"); icon.color: Theme.menuIcon; icon.width: 16; icon.height: 16
                checkable: true
                checked: Theme.mode === Theme.System
                onTriggered: Theme.mode = Theme.System
            }
            MenuItem {
                text: qsTr("Design: Hell")
                icon.source: window.icon("sun"); icon.color: Theme.menuIcon; icon.width: 16; icon.height: 16
                checkable: true
                checked: Theme.mode === Theme.Light
                onTriggered: Theme.mode = Theme.Light
            }
            MenuItem {
                text: qsTr("Design: Dunkel")
                icon.source: window.icon("moon"); icon.color: Theme.menuIcon; icon.width: 16; icon.height: 16
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
                    icon.source: window.icon("translate"); icon.color: Theme.menuIcon; icon.width: 16; icon.height: 16
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
                icon.source: window.icon("robot"); icon.color: Theme.menuIcon; icon.width: 16; icon.height: 16
                onTriggered: window.newSession()
            }
        }
        Menu {
            title: qsTr("Agent-Steuerung")
            MenuItem {
                text: mcp.listening ? qsTr("MCP-Server: an (127.0.0.1:%1)").arg(mcp.port)
                                    : qsTr("MCP-Server: aus")
                icon.source: window.icon("broadcast"); icon.color: Theme.menuIcon; icon.width: 16; icon.height: 16
                checkable: true
                checked: mcp.listening
                onTriggered: mcp.listening ? mcp.stop() : mcp.start()
            }
        }
        Menu {
            title: qsTr("Hilfe")
            MenuItem {
                text: qsTr("Über QTmux")
                icon.source: window.icon("info"); icon.color: Theme.menuIcon; icon.width: 16; icon.height: 16
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
                        required property bool mcpController
                        width: ListView.view.width
                        height: 48
                        radius: 8
                        color: index === window.currentRow ? Theme.sidebarSelected
                             : hover.hovered ? Theme.sidebarHover : "transparent"

                        HoverHandler { id: hover }
                        TapHandler { onTapped: window.currentRow = index }

                        // Roter Tab: diese Session steuert per MCP die anderen (Controller-Agent).
                        Rectangle {
                            visible: mcpController
                            width: 3
                            radius: 1.5
                            color: "#e5534b"
                            anchors.left: parent.left
                            anchors.verticalCenter: parent.verticalCenter
                            height: parent.height - 14
                        }

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
                                Image {
                                    anchors.centerIn: parent
                                    source: window.icon("x")
                                    sourceSize.width: 12
                                    sourceSize.height: 12
                                    // SVG ist monochrom -> über MultiEffect auf Theme-Farbe tönen.
                                    layer.enabled: true
                                    layer.effect: MultiEffect {
                                        colorization: 1.0
                                        colorizationColor: Theme.textDim
                                    }
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
                    Layout.fillHeight: false
                    Layout.preferredHeight: 40
                    Layout.maximumHeight: 40
                    spacing: 1

                    Button {
                        id: newBtn
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        text: window.typeLabel(window.newSessionType)
                        icon.source: window.icon("plus")
                        icon.color: Theme.textBright
                        icon.width: 15; icon.height: 15
                        display: AbstractButton.TextBesideIcon
                        spacing: 6
                        palette.buttonText: Theme.textBright
                        font.pixelSize: 13
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
                    }

                    Button {
                        id: caretBtn
                        Layout.preferredWidth: 32
                        Layout.fillHeight: true
                        display: AbstractButton.IconOnly
                        icon.source: window.icon("caret-down")
                        icon.color: Theme.textBright
                        icon.width: 14; icon.height: 14
                        onClicked: typeMenu.popup(caretBtn, 0, caretBtn.height)
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
                // Rechtsklick -> Kontextmenü (Kopieren/Einfügen) an der Mausposition.
                onContextMenuRequested: termContextMenu.popup()
            }
        }
    }

    // --- SSH-Verbindung öffnen ---------------------------------------------
    AppDialog {
        id: sshDialog
        width: 420
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
    AppDialog {
        id: serialDialog
        width: 420
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
            AppComboBox {
                id: portCombo
                Layout.fillWidth: true
                model: []
            }
            Text { text: qsTr("Baudrate"); color: Theme.textBright }
            AppComboBox {
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
    AppDialog {
        id: aboutDialog
        width: 420
        title: qsTr("Über QTmux")
        standardButtons: Dialog.Ok
        Label {
            width: 380
            wrapMode: Text.WordWrap
            color: Theme.textBright
            text: qsTr("QTmux — plattformübergreifender Multi-KI-Agenten-Terminal.\nQt %1").arg(Qt.application.version || "0.1")
        }
    }
}
