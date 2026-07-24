import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QtQuick.Effects
import QTmux

// Kategorie „Verbindungen" (QTMUX-47, Tabelle A4): gespeicherte Verbindungsprofile mit
// Verbinden / SFTP / Bearbeiten / Löschen und „Neu …". Unverändert aus dem früheren
// connectionsDialog; der Profil-Editor (host.profileEditDialog) bleibt ein modaler Dialog.
CatPage {
    id: page
    heading: qsTr("Verbindungen")
    subtitle: qsTr("Wiederverwendbare Profile für SSH, seriell und Plugin-Backends.")

    PrefAnchor {
      settingKey: "verbindungen.list"
      page: page
      ColumnLayout {
        Layout.fillWidth: true
        spacing: 10

        RowLayout {
            Layout.fillWidth: true
            Label {
                text: qsTr("Gespeicherte Verbindungsprofile")
                color: Theme.textDim
                font.pixelSize: 11
                Layout.fillWidth: true
            }
            Button {
                text: qsTr("Neu …")
                onClicked: page.host.profileEditDialog.openNew()
            }
        }

        ListView {
            id: profList
            Layout.fillWidth: true
            Layout.preferredHeight: 300
            clip: true
            spacing: 4
            model: Profiles.profiles
            ScrollIndicator.vertical: ScrollIndicator {}

            delegate: Rectangle {
                required property var modelData
                width: profList.width
                height: 48
                radius: 6
                color: Theme.bgElevated

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 10
                    anchors.rightMargin: 6
                    spacing: 10

                    // Typ-Icon (explizite MultiEffect-Tönung — layer.effect greift im Delegate nicht).
                    Item {
                        Layout.preferredWidth: 18
                        Layout.preferredHeight: 18
                        Image {
                            id: typeImg
                            anchors.fill: parent
                            source: page.host.app.icon(page.host.app.profileIcon(modelData.type))
                            sourceSize.width: 18
                            sourceSize.height: 18
                            visible: false
                        }
                        MultiEffect {
                            anchors.fill: parent
                            source: typeImg
                            // Schwarzes SVG erst auf Weiß heben, dann colorize (sonst dunkel).
                            brightness: 1.0
                            colorization: 1.0
                            colorizationColor: Theme.textBright
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 0
                        Text {
                            text: modelData.name
                            color: Theme.textBright
                            font.pixelSize: 13
                            font.bold: true
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                        }
                        Text {
                            text: page.host.app.typeLabel(modelData.type) + " · " + page.host.app.profileSummary(modelData)
                            color: Theme.textDim
                            font.pixelSize: 11
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                        }
                    }

                    Button {
                        text: qsTr("Verbinden")
                        onClicked: { page.host.app.connectProfile(modelData); page.host.close() }
                    }
                    Button {
                        text: qsTr("SFTP")
                        visible: modelData.type === 1   // nur SSH-Profile
                        onClicked: { page.host.app.openSftp(modelData); page.host.close() }
                    }
                    IconToolButton {
                        icon.source: page.host.app.icon("gear")
                        tip: qsTr("Bearbeiten")
                        onClicked: page.host.profileEditDialog.openEdit(modelData)
                    }
                    IconToolButton {
                        icon.source: page.host.app.icon("trash")
                        tip: qsTr("Löschen")
                        onClicked: Profiles.removeProfile(modelData.name)
                    }
                }
            }
        }

        Label {
            visible: Profiles.profiles.length === 0
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            color: Theme.textDim
            font.pixelSize: 12
            text: qsTr("Noch keine Profile. Lege mit „Neu …“ eine wiederverwendbare Verbindung an.")
        }
      }
    }
}
