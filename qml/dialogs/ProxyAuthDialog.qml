import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QTmux

// Proxy-Anmeldung (Zustand 15 des Entwurfs, Vertrag V2 §4.1). Zweistufig wie bisher in
// QTmux: Der Ablauf antwortet Qt nur aus seinem Sitzungsspeicher; fehlt etwas, meldet
// er `proxyAuth.pending`, dieser Dialog fragt, und `provideProxyCredentials()`
// wiederholt genau den unterbrochenen Vorgang. Jede Eingabe geht genau EINMAL hinaus
// (eine abgelehnte kommt nie wieder, sonst sperrt das Firmenkonto). Texte, Host und
// „vorheriger Versuch abgelehnt“ kommen fertig aus `Updates.flow.proxyAuth`.
// Form: 420 px breit, sonst wie der Update-Dialog. Nach einer Ablehnung bleibt der
// Benutzername, das Passwortfeld ist leer und hat den Fokus.
Popup {
    id: dlg
    readonly property var a: (Updates.textRevision, Updates.flow.proxyAuth)

    parent: Overlay.overlay
    anchors.centerIn: parent
    width: 420
    padding: 0
    modal: true
    focus: true
    closePolicy: Popup.NoAutoClose

    readonly property bool canSignIn: userField.text.length > 0 && pwField.text.length > 0

    Connections {
        target: Updates.flow
        // Direkt am Ablauf lesen: Die Bindung `a` hängt am selben Signal und ist
        // hier womöglich noch nicht neu ausgewertet.
        function onProxyAuthChanged() {
            const pending = Updates.flow.proxyAuth.pending === true
            if (pending && !dlg.opened) dlg.open()
            else if (!pending && dlg.opened) dlg.close()
        }
        function onQuitStarting() { dlg.close() }
    }
    onOpened: {
        userField.text = Updates.flow.proxyAuth.user || ""
        pwField.text = ""
        remember.checked = false
        if (userField.text.length === 0) userField.forceActiveFocus()
        else pwField.forceActiveFocus()
    }

    function signIn() {
        if (!dlg.canSignIn) return
        Updates.provideProxyCredentials(userField.text, pwField.text, remember.checked)
        pwField.text = ""
    }
    function cancel() { pwField.text = ""; Updates.cancelProxyAuth() }
    // Kein Passwort bleibt im verborgenen Feld liegen, gleich wie der Dialog schließt.
    onClosed: pwField.text = ""

    Shortcut { sequences: ["Escape"]; enabled: dlg.opened; onActivated: dlg.cancel() }

    Overlay.modal: Rectangle { color: Qt.alpha(Theme.dark ? Theme.terminalBg : Theme.textBright, 0.45) }
    background: Rectangle {
        color: Theme.bgElevated
        radius: 10
        border.width: 1
        border.color: Theme.border
    }

    contentItem: ColumnLayout {
        spacing: 0
        Item {
            Layout.fillWidth: true
            implicitHeight: 40
            Text {
                anchors.left: parent.left
                anchors.leftMargin: 24
                anchors.verticalCenter: parent.verticalCenter
                text: dlg.a.title || ""
                color: Theme.textDim
                font.pixelSize: 12
                font.weight: Font.Medium
            }
            Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: Theme.border }
        }
        ColumnLayout {
            Layout.fillWidth: true
            Layout.margins: 24
            spacing: 0
            RowLayout {
                spacing: 10
                UpdateIcon { name: "lock-key"; color: Theme.accent; size: 20 }
                Text {
                    Layout.fillWidth: true
                    text: dlg.a.title || ""
                    color: Theme.textBright
                    font.pixelSize: 17
                    font.weight: Font.DemiBold
                    wrapMode: Text.WordWrap
                }
            }
            Text {
                Layout.fillWidth: true
                Layout.topMargin: 8
                text: dlg.a.lead || ""
                color: Theme.textBright
                font.pixelSize: 14
                lineHeight: 20
                lineHeightMode: Text.FixedHeight
                wrapMode: Text.WordWrap
            }
            Rectangle {
                visible: dlg.a.previousRejected === true
                Layout.fillWidth: true
                Layout.topMargin: 16
                radius: 8
                color: Qt.alpha(Theme.danger, 0.14)
                implicitHeight: rejText.implicitHeight + 24
                Text {
                    id: rejText
                    anchors.fill: parent
                    anchors.margins: 12
                    anchors.leftMargin: 14
                    anchors.rightMargin: 14
                    text: dlg.a.rejected || ""
                    color: Theme.textBright
                    font.pixelSize: 13
                    lineHeight: 20
                    lineHeightMode: Text.FixedHeight
                    wrapMode: Text.WordWrap
                }
            }
            Text {
                Layout.topMargin: 16
                text: dlg.a.userLabel || ""
                color: Theme.textBright
                font.pixelSize: 13
                font.weight: Font.Medium
            }
            TextField {
                id: userField
                Layout.fillWidth: true
                Layout.topMargin: 6
                implicitHeight: 32
                color: Theme.textBright
                onAccepted: pwField.forceActiveFocus()
            }
            Text {
                Layout.topMargin: 12
                text: dlg.a.passwordLabel || ""
                color: Theme.textBright
                font.pixelSize: 13
                font.weight: Font.Medium
            }
            TextField {
                id: pwField
                Layout.fillWidth: true
                Layout.topMargin: 6
                implicitHeight: 32
                color: Theme.textBright
                echoMode: TextInput.Password
                onAccepted: dlg.signIn()
            }
            CheckBox {
                id: remember
                Layout.topMargin: 12
                Layout.leftMargin: -6
                text: dlg.a.rememberLabel || ""
                palette.windowText: Theme.textBright
            }
            Text {
                Layout.fillWidth: true
                Layout.topMargin: 4
                text: dlg.a.never || ""
                color: Theme.textDim
                font.pixelSize: 12
                wrapMode: Text.WordWrap
            }
        }
        Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: Theme.border }
        RowLayout {
            Layout.fillWidth: true
            Layout.margins: 16
            Layout.leftMargin: 24
            Layout.rightMargin: 24
            spacing: 8
            Item { Layout.fillWidth: true }
            UpdateButton {
                role: "Secondary"
                text: dlg.a.cancel || ""
                onClicked: dlg.cancel()
            }
            UpdateButton {
                role: "Primary"
                text: dlg.a.signIn || ""
                enabled: dlg.canSignIn
                onClicked: dlg.signIn()
            }
        }
    }
}
