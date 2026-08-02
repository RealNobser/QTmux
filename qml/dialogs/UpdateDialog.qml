import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QTmux

// QTMUX-125: Online-Update. Ein Dialog für den ganzen Ablauf — Prüfen,
// Anmerkungen, Herunterladen, Installieren — weil der Anwender ihn als EINEN
// Vorgang erlebt; welche Knöpfe erscheinen, entscheidet `Updates.state`.
//
// 🔑 Kein `standardButtons`: die Beschriftungen hängen am Zustand („Herunterladen"
// ist etwas anderes als „Installieren …"), und „Überspringen" hat in Qts
// Standardsatz kein Gegenstück.
// 🔑 Der Hinweis auf SmartScreen/Gatekeeper steht FEST im Dialog, nicht nur bei
// Fehlern: Die Pakete sind nicht signiert/notarisiert (Early-Adopter), und die
// Warnung des Betriebssystems erscheint erst NACH dem Download — wer sie dort
// zum ersten Mal sieht, hält den Download für kompromittiert.
AppDialog {
    id: dlg
    width: 540
    modal: true

    // Zustände aus UpdateViewModel::State — QML kennt das C++-Enum über die
    // Context-Property nicht namentlich, darum hier gespiegelt und benannt.
    readonly property int stIdle: 0
    readonly property int stChecking: 1
    readonly property int stUpToDate: 2
    readonly property int stAvailable: 3
    readonly property int stDownloading: 4
    readonly property int stReady: 5
    readonly property int stFailed: 6

    // Die Anmerkungen folgen der Oberflächensprache (Manifest trägt de und en).
    Binding { target: Updates; property: "language"; value: App.language }

    title: {
        switch (Updates.state) {
        case dlg.stChecking:    return qsTr("Nach Updates suchen …")
        case dlg.stUpToDate:    return qsTr("Kein Update verfügbar")
        case dlg.stDownloading: return qsTr("Update wird geladen")
        case dlg.stReady:       return qsTr("Update bereit zur Installation")
        case dlg.stFailed:      return qsTr("Update fehlgeschlagen")
        default:                return qsTr("Update verfügbar")
        }
    }

    // Menschliche Größenangabe — „98,4 MB" statt 103181234. Unter 1 kB wird die
    // Byte-Zahl gezeigt statt eines gerundeten „0 kB" (fällt nur bei Test-Fixtures
    // an, sah dort aber wie eine fehlende Angabe aus).
    function prettySize(bytes) {
        if (bytes <= 0) return ""
        if (bytes >= 1024 * 1024) return qsTr("%1 MB").arg((bytes / (1024 * 1024)).toFixed(1))
        if (bytes >= 1024) return qsTr("%1 kB").arg((bytes / 1024).toFixed(0))
        return qsTr("%1 Bytes").arg(bytes)
    }

    ColumnLayout {
        width: 500
        spacing: 12

        // --- Versionszeile ---------------------------------------------------
        Label {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            color: Theme.textBright
            font.pixelSize: 14
            text: {
                if (Updates.state === dlg.stChecking)
                    return qsTr("QTmux fragt den Update-Server …")
                if (Updates.state === dlg.stUpToDate)
                    return qsTr("QTmux %1 ist aktuell.").arg(Updates.currentVersion)
                if (Updates.remoteVersion === "")
                    return qsTr("QTmux %1").arg(Updates.currentVersion)
                return qsTr("QTmux %1 ist verfügbar — installiert ist %2.")
                       .arg(Updates.remoteVersion).arg(Updates.currentVersion)
            }
        }

        Label {
            Layout.fillWidth: true
            visible: Updates.published !== "" && Updates.remoteVersion !== ""
            color: Theme.textDim
            font.pixelSize: 11
            // Die Paketgröße gehört nur dorthin, wo sie eine Entscheidung stützt:
            // vor dem Herunterladen. Im Zustand „aktuell" beschreibt sie ein Paket,
            // das niemand holen wird — am Live-Bild als Rauschen aufgefallen.
            text: (Updates.downloadSize > 0 && Updates.state !== dlg.stUpToDate)
                  ? qsTr("Veröffentlicht am %1 · %2")
                    .arg(Updates.published).arg(dlg.prettySize(Updates.downloadSize))
                  : qsTr("Veröffentlicht am %1").arg(Updates.published)
        }

        // --- Downgrade-Warnung ------------------------------------------------
        // Owner-Entscheidung: Rückstufung ist erlaubt, aber nie beiläufig.
        Rectangle {
            Layout.fillWidth: true
            visible: Updates.isDowngrade
            color: Qt.rgba(0.9, 0.33, 0.29, 0.14)
            border.color: "#e5534b"
            border.width: 1
            radius: 6
            implicitHeight: downgradeText.implicitHeight + 16
            Label {
                id: downgradeText
                anchors.fill: parent
                anchors.margins: 8
                wrapMode: Text.WordWrap
                color: "#e5534b"
                font.pixelSize: 12
                text: qsTr("Achtung: Das ist eine ÄLTERE Version als die installierte. "
                         + "Eine Rückstufung kann Einstellungen und gespeicherte Sitzungen "
                         + "betreffen, die eine neuere Fassung geschrieben hat.")
            }
        }

        // --- Kein Paket für dieses System ------------------------------------
        Label {
            Layout.fillWidth: true
            visible: Updates.remoteVersion !== "" && !Updates.hasPackageForThisSystem
            wrapMode: Text.WordWrap
            color: Theme.textDim
            font.pixelSize: 12
            text: qsTr("Für dieses Betriebssystem liegt in dieser Veröffentlichung "
                     + "kein Paket bereit.")
        }

        // --- Anmerkungen zur Version ------------------------------------------
        SectionLabel {
            text: qsTr("Was ist neu")
            visible: Updates.notes !== ""
        }
        ScrollView {
            Layout.fillWidth: true
            Layout.preferredHeight: Math.min(200, notesText.implicitHeight + 8)
            visible: Updates.notes !== ""
            clip: true
            TextArea {
                id: notesText
                readOnly: true
                wrapMode: Text.WordWrap
                text: Updates.notes
                color: Theme.textBright
                font.pixelSize: 12
                background: null
            }
        }

        // --- Fortschritt -------------------------------------------------------
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 4
            visible: Updates.state === dlg.stDownloading || Updates.state === dlg.stReady
            ProgressBar {
                Layout.fillWidth: true
                from: 0; to: 1
                value: Updates.downloadProgress
            }
            Label {
                color: Theme.textDim
                font.pixelSize: 11
                text: Updates.state === dlg.stReady
                      ? qsTr("Heruntergeladen und geprüft: %1").arg(Updates.downloadedPath)
                      : qsTr("%1 %").arg(Math.round(Updates.downloadProgress * 100))
                elide: Text.ElideMiddle
                Layout.maximumWidth: 500
            }
        }

        // Kein Start-Plan: der Download ist gültig, QTmux kann ihn nur nicht selbst
        // starten. Das muss dastehen, sonst wirkt der fehlende Knopf wie ein Fehler.
        Label {
            Layout.fillWidth: true
            visible: Updates.state === dlg.stReady && !Updates.canLaunchInstaller()
            wrapMode: Text.WordWrap
            color: Theme.textDim
            font.pixelSize: 12
            text: qsTr("QTmux läuft nicht aus einem AppImage und kann sich deshalb nicht "
                     + "selbst ersetzen. Die Datei oben ist geprüft und kann von Hand "
                     + "installiert werden.")
        }

        // --- Fehler --------------------------------------------------------------
        Label {
            Layout.fillWidth: true
            visible: Updates.lastError !== ""
            wrapMode: Text.WordWrap
            color: "#e5534b"
            font.pixelSize: 12
            text: Updates.lastError
        }

        // --- Hinweis auf die Betriebssystem-Warnung -------------------------------
        Label {
            Layout.fillWidth: true
            visible: Updates.state === dlg.stAvailable || Updates.state === dlg.stReady
            wrapMode: Text.WordWrap
            color: Theme.textDim
            font.pixelSize: 11
            text: Qt.platform.os === "windows"
                  ? qsTr("Hinweis: Das Paket ist nicht signiert. Windows SmartScreen meldet "
                       + "beim Start „Der Computer wurde geschützt“ — über „Weitere "
                       + "Informationen“ → „Trotzdem ausführen“ fortfahren. QTmux prüft den "
                       + "Download selbst über eine Ed25519-Signatur und eine SHA-256-Summe.")
                  : Qt.platform.os === "osx"
                  ? qsTr("Hinweis: Das Paket ist nicht notariell beglaubigt. macOS Gatekeeper "
                       + "verweigert den ersten Start — die App im Finder mit Rechtsklick → "
                       + "„Öffnen“ starten. QTmux prüft den Download selbst über eine "
                       + "Ed25519-Signatur und eine SHA-256-Summe.")
                  : qsTr("Hinweis: Das Paket ist nicht signiert. QTmux prüft den Download "
                       + "selbst über eine Ed25519-Signatur und eine SHA-256-Summe.")
        }

        // --- Knöpfe ---------------------------------------------------------------
        RowLayout {
            Layout.fillWidth: true
            Layout.topMargin: 4
            spacing: 8

            Button {
                text: qsTr("Herunterladen")
                visible: Updates.state === dlg.stAvailable && Updates.hasPackageForThisSystem
                palette.buttonText: Theme.textBright
                onClicked: Updates.download()
            }
            Button {
                text: qsTr("Installieren …")
                // Ohne Start-Plan gibt es nichts zu klicken (Linux ohne $APPIMAGE) —
                // dann steht stattdessen der Hinweis mit dem Dateipfad da.
                visible: Updates.state === dlg.stReady && Updates.canLaunchInstaller()
                palette.buttonText: Theme.textBright
                onClicked: {
                    // Geführte Installation: QTmux übergibt an den Installer und
                    // bleibt selbst stehen. Der Anwender beendet die App, wenn der
                    // Installer danach fragt — QTmux beendet sich NICHT von selbst,
                    // das würde laufende Sessions ungefragt abreißen.
                    if (Updates.launchInstaller()) dlg.close()
                }
            }
            Button {
                text: qsTr("Erneut versuchen")
                visible: Updates.state === dlg.stFailed
                palette.buttonText: Theme.textBright
                onClicked: Updates.checkNow()
            }
            Item { Layout.fillWidth: true }
            Button {
                text: qsTr("Abbrechen")
                visible: Updates.busy
                palette.buttonText: Theme.textBright
                onClicked: Updates.abort()
            }
            Button {
                text: qsTr("Version überspringen")
                visible: Updates.state === dlg.stAvailable && !Updates.isDowngrade
                palette.buttonText: Theme.textDim
                onClicked: { Updates.skipVersion(); dlg.close() }
            }
            Button {
                text: Updates.state === dlg.stAvailable || Updates.state === dlg.stReady
                      ? qsTr("Später") : qsTr("Schließen")
                visible: !Updates.busy
                palette.buttonText: Theme.textBright
                onClicked: dlg.close()
            }
        }
    }
}
