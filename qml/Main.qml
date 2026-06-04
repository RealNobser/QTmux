import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QTmux

ApplicationWindow {
    id: window
    width: 1100
    height: 720
    visible: true
    title: "QTmux"
    color: Theme.bgMain

    property int currentRow: -1

    SessionModel { id: sessions }

    function newSession() {
        currentRow = sessions.createShellSession()
    }
    function closeCurrent() {
        if (currentRow < 0) return
        sessions.closeSession(currentRow)
        currentRow = Math.min(currentRow, sessions.count - 1)
    }

    Component.onCompleted: newSession()

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
            MenuItem { action: actCloseSession }
            MenuSeparator {}
            MenuItem { action: actQuit }
        }
        Menu {
            title: qsTr("Ansicht")
            MenuItem { action: actToggleTheme }
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

                            // Status-Ring: 0=Starting 1=Running 2=WaitingInput 3=Error 4=Closed
                            Rectangle {
                                width: 10; height: 10; radius: 5
                                color: runState === 1 ? "#46d369"
                                     : runState === 2 ? "#f5c451"
                                     : runState === 3 ? "#e5534b"
                                     : runState === 4 ? "#5a5d6a"
                                     : Theme.textDim
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
                                Text {
                                    visible: agentId.length > 0
                                    text: qsTr("Agent: %1").arg(agentId)
                                    color: Theme.accent
                                    font.pixelSize: 10
                                    elide: Text.ElideRight
                                    Layout.fillWidth: true
                                }
                            }
                        }
                    }
                }

                Button {
                    id: newBtn
                    text: qsTr("+  Neue Session")
                    Layout.fillWidth: true
                    Layout.preferredHeight: 40
                    onClicked: window.newSession()
                    background: Rectangle {
                        radius: 8
                        color: newBtn.down ? Theme.sidebarSelected
                             : newBtn.hovered ? Theme.sidebarHover : Theme.bgElevated
                        border.color: Theme.border
                        border.width: 1
                    }
                    contentItem: Text {
                        text: newBtn.text
                        color: Theme.textBright
                        font.pixelSize: 13
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
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
                session: window.currentRow >= 0 ? sessions.sessionAt(window.currentRow) : null
            }
        }
    }

    // --- Über-Dialog --------------------------------------------------------
    Dialog {
        id: aboutDialog
        anchors.centerIn: parent
        modal: true
        title: qsTr("Über QTmux")
        standardButtons: Dialog.Ok
        Text {
            color: Theme.textBright
            text: qsTr("QTmux — plattformübergreifender Multi-KI-Agenten-Terminal.\nQt %1").arg(Qt.application.version || "0.1")
        }
    }
}
