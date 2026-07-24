import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QTmux

// Kategorie „Secrets-Vault" (QTMUX-47, Tabelle A4): Entsperren/Anlegen, Geheimnis-Liste,
// Master-Passwort ändern. Unverändert aus dem früheren vaultDialog. Das Anlegen/Ändern
// einzelner Secrets (host.secretEditDialog) und das Master-Passwort ändern
// (host.masterPwDialog) bleiben kleine modale Dialoge. Bewusst NIE über MCP erreichbar.
CatPage {
    id: page
    heading: qsTr("Secrets-Vault")
    subtitle: qsTr("Verschlüsselter Speicher für Passwörter, Passphrasen und Tokens.")

    property string vaultError: ""
    // Beim Wechsel auf diese Seite das Passwortfeld fokussieren, wenn gesperrt.
    Component.onCompleted: { if (!Vault.unlocked) vpw.forceActiveFocus() }

    // --- Gesperrt / Anlegen ---
    ColumnLayout {
        visible: !Vault.unlocked
        Layout.fillWidth: true
        spacing: 10
        Text {
            text: Vault.exists ? qsTr("Der Vault ist gesperrt. Master-Passwort eingeben:")
                               : qsTr("Noch kein Vault vorhanden. Lege ein Master-Passwort fest:")
            color: Theme.textBright; Layout.fillWidth: true; wrapMode: Text.WordWrap
        }
        TextField {
            id: vpw
            Layout.fillWidth: true
            echoMode: TextInput.Password
            placeholderText: qsTr("Master-Passwort")
            onAccepted: vaultPrimaryBtn.clicked()
        }
        TextField {
            id: vpwConfirm
            Layout.fillWidth: true
            echoMode: TextInput.Password
            visible: !Vault.exists
            placeholderText: qsTr("Master-Passwort bestätigen")
            onAccepted: vaultPrimaryBtn.clicked()
        }
        Text {
            visible: page.vaultError.length > 0
            text: page.vaultError
            color: "#e0a040"; font.pixelSize: 11; Layout.fillWidth: true; wrapMode: Text.WordWrap
        }
        Button {
            id: vaultPrimaryBtn
            text: Vault.exists ? qsTr("Entsperren") : qsTr("Vault anlegen")
            onClicked: {
                page.vaultError = ""
                if (Vault.exists) {
                    if (!Vault.unlock(vpw.text))
                        page.vaultError = qsTr("Falsches Master-Passwort.")
                } else if (vpw.text.length === 0) {
                    page.vaultError = qsTr("Bitte ein Passwort eingeben.")
                } else if (vpw.text !== vpwConfirm.text) {
                    page.vaultError = qsTr("Die Passwörter stimmen nicht überein.")
                } else if (!Vault.create(vpw.text)) {
                    page.vaultError = qsTr("Der Vault konnte nicht angelegt werden.")
                }
                if (Vault.unlocked) { vpw.text = ""; vpwConfirm.text = "" }
            }
        }
        Text {
            text: qsTr("Der Vault speichert Geheimnisse (Passwörter, Passphrasen, Tokens) verschlüsselt hinter dem Master-Passwort. Das Master-Passwort wird nicht gespeichert und kann nicht wiederhergestellt werden.")
            color: Theme.textDim; font.pixelSize: 11; Layout.fillWidth: true; wrapMode: Text.WordWrap
        }
    }

    // --- Entsperrt: Geheimnis-Liste ---
    ColumnLayout {
        visible: Vault.unlocked
        Layout.fillWidth: true
        spacing: 10
        RowLayout {
            Layout.fillWidth: true
            Label { text: qsTr("Gespeicherte Geheimnisse"); color: Theme.textDim; font.pixelSize: 11; Layout.fillWidth: true }
            Button { text: qsTr("Hinzufügen"); onClicked: page.host.secretEditDialog.openNew() }
        }
        ListView {
            id: secretList
            Layout.fillWidth: true
            Layout.preferredHeight: 260
            clip: true
            spacing: 4
            model: Vault.names
            ScrollIndicator.vertical: ScrollIndicator {}
            delegate: Rectangle {
                id: secretRow
                required property string modelData
                property bool revealed: false
                width: secretList.width
                height: 46
                radius: 6
                color: Theme.bgElevated
                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 10
                    anchors.rightMargin: 6
                    spacing: 6
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 0
                        Text { text: secretRow.modelData; color: Theme.textBright; font.pixelSize: 13; font.bold: true; elide: Text.ElideRight; Layout.fillWidth: true }
                        Text {
                            text: secretRow.revealed ? Vault.secret(secretRow.modelData) : "••••••••••"
                            color: Theme.textDim; font.pixelSize: 11; font.family: page.host.app.terminalFontFamily
                            elide: Text.ElideRight; Layout.fillWidth: true
                        }
                    }
                    IconToolButton {
                        icon.source: page.host.app.icon("eye")
                        tip: secretRow.revealed ? qsTr("Verbergen") : qsTr("Anzeigen")
                        active: secretRow.revealed
                        onClicked: secretRow.revealed = !secretRow.revealed
                    }
                    IconToolButton { icon.source: page.host.app.icon("copy"); tip: qsTr("In Zwischenablage kopieren"); onClicked: App.copyToClipboard(Vault.secret(secretRow.modelData)) }
                    IconToolButton { icon.source: page.host.app.icon("gear"); tip: qsTr("Bearbeiten"); onClicked: page.host.secretEditDialog.openEdit(secretRow.modelData) }
                    IconToolButton { icon.source: page.host.app.icon("trash"); tip: qsTr("Löschen"); onClicked: Vault.removeSecret(secretRow.modelData) }
                }
            }
        }
        Label {
            visible: Vault.names.length === 0
            text: qsTr("Noch keine Geheimnisse. Mit „Hinzufügen“ eines anlegen.")
            color: Theme.textDim; font.pixelSize: 12; Layout.fillWidth: true; wrapMode: Text.WordWrap
        }
        RowLayout {
            Layout.fillWidth: true
            Button { text: qsTr("Master-Passwort ändern …"); onClicked: page.host.masterPwDialog.open() }
            Item { Layout.fillWidth: true }
            Button { text: qsTr("Sperren"); onClicked: Vault.lock() }
        }
    }
}
