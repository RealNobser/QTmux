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
    color: "#1e1f29"

    readonly property color bgSidebar: "#16171f"
    readonly property color bgMain:    "#1e1f29"
    readonly property color accent:    "#5b8cff"
    readonly property color textDim:   "#8a8d9a"
    readonly property color textBright:"#e6e7ee"

    property int currentRow: -1

    SessionModel { id: sessions }

    function newSession() {
        currentRow = sessions.createShellSession()
    }

    // Beim Start eine erste Shell öffnen.
    Component.onCompleted: newSession()

    RowLayout {
        anchors.fill: parent
        spacing: 0

        // --- Vertikale Sidebar ----------------------------------------------
        Rectangle {
            Layout.preferredWidth: 240
            Layout.fillHeight: true
            color: window.bgSidebar

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 12
                spacing: 8

                Text {
                    text: "QTmux"
                    color: window.textBright
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
                        width: ListView.view.width
                        height: 48
                        radius: 8
                        color: index === window.currentRow ? "#2b2e42"
                             : hover.hovered ? "#262838" : "transparent"

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
                                     : window.textDim
                            }
                            Text {
                                text: title
                                color: window.textBright
                                font.pixelSize: 13
                                elide: Text.ElideRight
                                Layout.fillWidth: true
                            }
                        }
                    }
                }

                Button {
                    text: "+  Neue Session"
                    Layout.fillWidth: true
                    flat: true
                    onClicked: window.newSession()
                }
            }
        }

        // --- Hauptbereich: Terminal der aktuellen Session -------------------
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: window.bgMain

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
}
