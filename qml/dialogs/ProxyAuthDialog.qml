import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QTmux

// QTMUX-129: Anmeldung am Netzwerk-Proxy.
//
// 🔑 Warum dieser Dialog KEINE Antwort zurückgibt, sondern nur etwas ablegt:
// Qts `proxyAuthenticationRequired` wird **synchron** zugestellt — der
// `QAuthenticator*` gilt nur während des Slot-Aufrufs, ein QML-Dialog antwortet
// aber asynchron. Deshalb ist der Ablauf zweistufig: Die Lib fragt einen
// Lieferanten, der nur aus dem Sitzungsspeicher antwortet; ist er leer, endet
// die Anfrage mit „Anmeldung erforderlich", DIESER Dialog geht auf, und
// `Updates.provideProxyCredentials()` legt ab und **wiederholt** den Vorgang.
//
// ⚠️ Genau EIN Versuch je Passwort — die Regel sitzt in `ProxyCredentials`
// (C++), nicht hier: Wiederholtes Anmelden sperrt in einer AD-Umgebung das
// Domänen-Konto. Der Dialog erfährt über `retry`, dass es schiefging, und sagt
// es — sonst fragt er wortgleich noch einmal und wirkt kaputt.
AppDialog {
    id: dlg
    width: 460
    modal: true
    title: qsTr("Anmeldung am Proxy")

    property string proxyHost: ""
    property int proxyPort: 0
    property bool retry: false

    function ask(host, port, wasRejected) {
        dlg.proxyHost = host
        dlg.proxyPort = port
        dlg.retry = wasRejected
        // Der Benutzername bleibt stehen (er ist kein Geheimnis und oft
        // `DOMÄNE\name`), das Passwort nie.
        pwField.text = ""
        dlg.open()
        // Beim zweiten Mal gehört der Fokus aufs Passwort — der Name stimmt ja
        // meist schon.
        if (wasRejected) pwField.forceActiveFocus()
        else userField.forceActiveFocus()
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 12

        Text {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            color: Theme.textBright
            text: dlg.proxyPort > 0
                  ? qsTr("Der Proxy %1:%2 verlangt eine Anmeldung.").arg(dlg.proxyHost).arg(dlg.proxyPort)
                  : qsTr("Der Proxy %1 verlangt eine Anmeldung.").arg(dlg.proxyHost)
        }

        // Die Fehlzeile ist der Grund, warum `retry` überhaupt durchgereicht
        // wird: ohne sie sieht der zweite Anlauf aus wie der erste.
        Text {
            Layout.fillWidth: true
            visible: dlg.retry
            wrapMode: Text.WordWrap
            color: "#e5534b"   // wie im UpdateDialog — Theme hat keine Fehlerfarbe
            text: qsTr("Die letzte Anmeldung wurde abgelehnt. Es wird bewusst nur EIN Versuch "
                       + "je Eingabe unternommen — mehrere Fehlversuche sperren in einer "
                       + "Firmenumgebung das Benutzerkonto.")
        }

        GridLayout {
            Layout.fillWidth: true
            columns: 2
            columnSpacing: 10
            rowSpacing: 8

            Text { text: qsTr("Benutzer"); color: Theme.textDim }
            TextField {
                id: userField
                Layout.fillWidth: true
                color: Theme.textBright
                placeholderText: qsTr("Benutzername, ggf. mit Domäne")
                onAccepted: pwField.forceActiveFocus()
            }

            Text { text: qsTr("Passwort"); color: Theme.textDim }
            TextField {
                id: pwField
                Layout.fillWidth: true
                color: Theme.textBright
                echoMode: TextInput.Password
                onAccepted: dlg.submit()
            }
        }

        Text {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            font.pixelSize: 12
            color: Theme.textDim
            // Ehrlich sagen, was passiert — das ist im Firmenumfeld die Frage,
            // die als Erstes kommt.
            text: qsTr("Die Anmeldedaten gelten nur für diese Sitzung. Sie werden nicht "
                       + "gespeichert und erscheinen in keiner Exportdatei.")
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8
            Item { Layout.fillWidth: true }
            Button {
                text: qsTr("Abbrechen")
                onClicked: { Updates.cancelProxyAuthentication(); dlg.close() }
            }
            Button {
                text: qsTr("Anmelden")
                enabled: pwField.text.length > 0
                onClicked: dlg.submit()
            }
        }
    }

    function submit() {
        if (pwField.text.length === 0) return
        // Legt ab UND wiederholt den unterbrochenen Vorgang — der Mensch soll
        // nicht selbst noch einmal „Nach Updates suchen" anstoßen müssen.
        Updates.provideProxyCredentials(userField.text, pwField.text)
        pwField.text = ""
        dlg.close()
    }
}
