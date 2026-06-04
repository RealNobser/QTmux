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

    // Farbpalette (spaeter Theme-Engine) -------------------------------------
    readonly property color bgSidebar: "#16171f"
    readonly property color bgMain:    "#1e1f29"
    readonly property color accent:    "#5b8cff"
    readonly property color textDim:   "#8a8d9a"
    readonly property color textBright:"#e6e7ee"

    RowLayout {
        anchors.fill: parent
        spacing: 0

        // --- Vertikale Sidebar (cmux-Stil) ----------------------------------
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

                // Platzhalter-Sessions bis das SessionModel (Phase 2) steht.
                Repeater {
                    model: [
                        { name: "claude-code",  meta: "main · ~/proj",  state: "running" },
                        { name: "ssh: build-01", meta: "10.0.0.5",       state: "idle" },
                        { name: "serial: PCAN",  meta: "115200 8N1",     state: "waiting" }
                    ]
                    delegate: Rectangle {
                        required property var modelData
                        Layout.fillWidth: true
                        height: 48
                        radius: 8
                        color: ma.containsMouse ? "#262838" : "transparent"

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 10
                            anchors.rightMargin: 10
                            spacing: 10

                            // Status-Ring
                            Rectangle {
                                width: 10; height: 10; radius: 5
                                color: modelData.state === "running" ? "#46d369"
                                     : modelData.state === "waiting" ? "#f5c451"
                                     : modelData.state === "error"   ? "#e5534b"
                                     : window.textDim
                            }
                            ColumnLayout {
                                spacing: 2
                                Layout.fillWidth: true
                                Text {
                                    text: modelData.name
                                    color: window.textBright
                                    font.pixelSize: 13
                                    elide: Text.ElideRight
                                    Layout.fillWidth: true
                                }
                                Text {
                                    text: modelData.meta
                                    color: window.textDim
                                    font.pixelSize: 11
                                    elide: Text.ElideRight
                                    Layout.fillWidth: true
                                }
                            }
                        }
                        MouseArea { id: ma; anchors.fill: parent; hoverEnabled: true }
                    }
                }

                Item { Layout.fillHeight: true }

                Button {
                    text: "+  Neue Session"
                    Layout.fillWidth: true
                    flat: true
                }
            }
        }

        // --- Hauptbereich: echtes Terminal ----------------------------------
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
                Component.onCompleted: startShell()
                onTitleChanged: (t) => window.title = "QTmux — " + t
            }
        }
    }
}
