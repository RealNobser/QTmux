import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QTmux

// Online-Update, Vertrag V2 (K2): die QML-Oberfläche des gemeinsamen Ablaufs.
//
// 🔑 Dieser Dialog ENTSCHEIDET NICHTS. Überschrift, Sätze, Kästen, Fortschritt,
// Fußnote, welche Knöpfe in welcher Reihenfolge, welcher der Hauptknopf ist und was
// Enter und Esc tun — alles kommt fertig aus `Updates.flow.view` (Hub,
// appupdate::UpdateFlow). Ein Knopf meldet nur seine Aktion als Zahl zurück
// (`Updates.perform`). Der Wächter test_update_consumer_guard schlägt an, wenn hier
// am Zustand entschieden wird.
//
// Form nach dem Entwurf (§4, Rahmen QTmux): Fläche über dem Hauptfenster mit
// Abdunkelung, Radius 10, Titelstreifen 40 px mit dem Fenstertitel und ×, Inhalt 24 px
// Innenabstand, Breite 540. Das × verhält sich wie Esc. Im Zustand „Download läuft“
// heißt Schließen nur Ausblenden: Der Download läuft weiter, der Hinweis zeigt den
// Stand (der Ablauf hängt nicht am Dialog).
Popup {
    id: dlg
    // Gelesen wird mit der Textrevision, damit ein Sprachwechsel neu auswertet.
    readonly property var v: (Updates.textRevision, Updates.flow.view)
    // Knöpfe und Hinweise nur tauschen, wenn sich ihr INHALT ändert: Der Ablauf meldet
    // `view` bei jedem Download-Schritt neu (~10× je Sekunde). Ein neues Modell
    // erzeugte jedes Mal alle Knöpfe neu, und ein Klick, der in den Neuaufbau fällt,
    // ginge verloren; Fokus und Überfahren flackerten.
    property var buttons: []
    property var noteBlocks: []
    property string buttonsKey: ""
    property string notesKey: ""
    onVChanged: dlg.syncModels()
    Component.onCompleted: dlg.syncModels()
    function syncModels() {
        const b = dlg.v.buttons || []
        const bk = JSON.stringify(b)
        if (bk !== dlg.buttonsKey) {
            dlg.buttonsKey = bk
            dlg.buttons = b
            // Neue Knöpfe: Der Hauptknopf bekommt den Fokus (Entwurf §4), auch nach
            // einem Zustandswechsel bei offenem Dialog.
            if (dlg.opened) Qt.callLater(dlg.focusPrimary)
        }
        const n = Updates.flow.notes || []
        const nk = JSON.stringify(n)
        if (nk !== dlg.notesKey) { dlg.notesKey = nk; dlg.noteBlocks = n }
    }
    property bool detailsOpen: false
    property bool copied: false

    parent: Overlay.overlay
    anchors.centerIn: parent
    // Höchstens 540 breit und nie größer als das Fenster; der Inhalt rollt (Entwurf §4).
    width: Math.min(540, parent.width - 32)
    height: Math.min(implicitHeight, parent.height - 32)
    padding: 0
    modal: true
    focus: true
    // Esc und × gehen über die escapeAction des Zustands, nicht über Qts Schließen.
    closePolicy: Popup.NoAutoClose

    onOpened: { Updates.setDialogVisible(true); dlg.detailsOpen = false; dlg.focusPrimary() }
    onClosed: Updates.setDialogVisible(false)

    Connections {
        target: Updates.flow
        // Eine offene Proxy-Anmeldung hat Vorrang; der Dialog käme sonst darüber zu
        // liegen und nähme in diesem Zustand keine Aktion an.
        function onDialogRequested() {
            if (!dlg.opened && Updates.flow.proxyAuth.pending !== true) dlg.open()
        }
        function onCloseDialogRequested() { dlg.close() }
        // „Beenden und aktualisieren“: sofort loslassen, ohne eine Aktion auszulösen.
        function onQuitStarting() { dlg.close() }
    }

    function act(action) { if (action > 0) Updates.perform(action) }
    // Beim Öffnen hat der Hauptknopf den Fokus, sonst der einzige Knopf (Entwurf §4).
    function focusPrimary() {
        let last = null
        for (let i = 0; i < btnRow.children.length; ++i) {
            const c = btnRow.children[i]
            if (c.role === undefined) continue
            if (c.role === "Primary") { c.forceActiveFocus(); return }
            last = c
        }
        if (last) last.forceActiveFocus()
    }
    function roleColor(r) {
        switch (r) {
        case "accent":  return Theme.accent
        case "success": return Theme.ok
        case "warn":    return Theme.warn
        case "danger":  return Theme.danger
        default:        return Theme.textDim
        }
    }

    Shortcut {
        sequences: ["Escape"]
        enabled: dlg.opened
        onActivated: dlg.act(dlg.v.escapeAction)
    }
    Shortcut {
        sequences: ["Return", "Enter"]
        enabled: dlg.opened && dlg.v.enterAction > 0
        onActivated: dlg.act(dlg.v.enterAction)
    }

    // Abdunkelung in beiden Designs (im hellen wäre terminalBg ein heller Schleier).
    Overlay.modal: Rectangle { color: Qt.alpha(Theme.dark ? Theme.terminalBg : Theme.textBright, 0.45) }
    background: Rectangle {
        color: Theme.bgElevated
        radius: 10
        border.width: 1
        border.color: Theme.border
    }

    contentItem: ColumnLayout {
        spacing: 0

        // --- Titelstreifen 40 px --------------------------------------------
        Item {
            Layout.fillWidth: true
            implicitHeight: 40
            Text {
                anchors.left: parent.left
                anchors.leftMargin: 24
                anchors.verticalCenter: parent.verticalCenter
                text: dlg.v.windowTitle || ""
                color: Theme.textDim
                font.pixelSize: 12
                font.weight: Font.Medium
            }
            ToolButton {
                id: closeX
                anchors.right: parent.right
                anchors.rightMargin: 8
                anchors.verticalCenter: parent.verticalCenter
                implicitWidth: 28
                implicitHeight: 28
                icon.source: "qrc:/icons/x.svg"
                icon.width: 14
                icon.height: 14
                icon.color: Theme.textDim
                background: Rectangle {
                    radius: 6
                    color: closeX.hovered ? Theme.sidebarHover : "transparent"
                }
                onClicked: dlg.act(dlg.v.escapeAction)
            }
            Rectangle {
                anchors.bottom: parent.bottom
                width: parent.width; height: 1
                color: Theme.border
            }
        }

        // --- Inhalt -----------------------------------------------------------
        ScrollView {
            id: bodyScroll
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.preferredHeight: body.implicitHeight + 48
            Layout.minimumHeight: 0
            contentWidth: availableWidth
            contentHeight: body.implicitHeight + 48
            clip: true
        ColumnLayout {
            id: body
            x: 24
            y: 24
            width: bodyScroll.availableWidth - 48
            spacing: 0

            RowLayout {
                spacing: 10
                UpdateIcon {
                    name: dlg.v.iconName || ""
                    color: dlg.roleColor(dlg.v.iconRole)
                    size: 20
                }
                Text {
                    Layout.fillWidth: true
                    text: dlg.v.title || ""
                    color: Theme.textBright
                    font.pixelSize: 17
                    font.weight: Font.DemiBold
                    wrapMode: Text.WordWrap
                }
            }
            Text {
                Layout.fillWidth: true
                Layout.topMargin: 8
                visible: text.length > 0
                text: dlg.v.lead || ""
                color: Theme.textBright
                font.pixelSize: 14
                lineHeight: 20
                lineHeightMode: Text.FixedHeight
                wrapMode: Text.WordWrap
            }
            Repeater {
                model: dlg.v.extra || []
                Text {
                    required property string modelData
                    Layout.fillWidth: true
                    Layout.topMargin: 8
                    text: modelData
                    color: Theme.textDim
                    font.pixelSize: 13
                    lineHeight: 20
                    lineHeightMode: Text.FixedHeight
                    wrapMode: Text.WordWrap
                }
            }
            // Knöpfe „im Text“ (Zustand 13: „Von Hand installieren“ unter dem Satz).
            Flow {
                Layout.fillWidth: true
                Layout.topMargin: 4
                Layout.leftMargin: -8
                visible: inTextRep.count > 0
                Repeater {
                    id: inTextRep
                    model: dlg.buttons.filter(b => b.role === "InText")
                    UpdateButton {
                        required property var modelData
                        role: modelData.role
                        flowAction: modelData.action
                        text: modelData.label
                        enabled: modelData.enabled
                        onClicked: dlg.act(flowAction)
                    }
                }
            }

            // Warn-, Info- und Begründungskasten.
            Repeater {
                model: [
                    { text: dlg.v.warning || "",   glyph: "warning",              role: "warn" },
                    { text: dlg.v.notice || "",    glyph: "info",                 role: "accent" },
                    { text: dlg.v.reasonBox || "", glyph: dlg.v.reasonIcon || "", role: dlg.v.reasonRole || "muted" }
                ]
                Rectangle {
                    id: box
                    required property var modelData
                    visible: modelData.text.length > 0
                    Layout.fillWidth: true
                    Layout.topMargin: 16
                    radius: 8
                    color: modelData.role === "warn" ? Qt.alpha(Theme.warn, 0.14) : Theme.bgSidebar
                    implicitHeight: boxRow.implicitHeight + 24
                    RowLayout {
                        id: boxRow
                        anchors.fill: parent
                        anchors.margins: 12
                        anchors.leftMargin: 14
                        anchors.rightMargin: 14
                        spacing: 10
                        UpdateIcon {
                            Layout.alignment: Qt.AlignTop
                            name: box.modelData.glyph
                            color: dlg.roleColor(box.modelData.role)
                            size: 18
                        }
                        Text {
                            Layout.fillWidth: true
                            text: box.modelData.text
                            color: Theme.textBright
                            font.pixelSize: 13
                            lineHeight: 20
                            lineHeightMode: Text.FixedHeight
                            wrapMode: Text.WordWrap
                        }
                    }
                }
            }

            // Schritte (14).
            Repeater {
                model: dlg.v.steps || []
                RowLayout {
                    id: step
                    required property string modelData
                    required property int index
                    Layout.fillWidth: true
                    Layout.topMargin: index === 0 ? 16 : 8
                    spacing: 10
                    Rectangle {
                        Layout.alignment: Qt.AlignTop
                        width: 20; height: 20; radius: 10
                        color: Qt.alpha(Theme.accent, 0.16)
                        Text {
                            anchors.centerIn: parent
                            text: step.index + 1
                            color: Theme.accent
                            font.pixelSize: 11
                            font.weight: Font.DemiBold
                        }
                    }
                    Text {
                        Layout.fillWidth: true
                        text: step.modelData
                        color: Theme.textBright
                        font.pixelSize: 13
                        lineHeight: 20
                        lineHeightMode: Text.FixedHeight
                        wrapMode: Text.WordWrap
                    }
                }
            }

            UpdateNotes {
                visible: dlg.v.showNotes === true
                Layout.fillWidth: true
                Layout.topMargin: 16
                head: dlg.v.notesHead || ""
                meta: dlg.v.notesMeta || ""
                emptyText: dlg.v.notesEmpty || ""
                highlight: Updates.notesHighlight
                blocks: dlg.noteBlocks
            }

            // Fortschritt (1, 4).
            ColumnLayout {
                visible: dlg.v.showProgress === true
                Layout.fillWidth: true
                Layout.topMargin: 16
                spacing: 8
                RowLayout {
                    Layout.fillWidth: true
                    Text {
                        Layout.fillWidth: true
                        text: dlg.v.progressLeft || ""
                        color: Theme.textBright
                        font.pixelSize: 13
                        font.features: { "tnum": 1 }
                    }
                    Text {
                        text: dlg.v.progressRight || ""
                        color: Theme.textBright
                        font.pixelSize: 13
                        font.weight: Font.DemiBold
                        font.features: { "tnum": 1 }
                    }
                }
                Rectangle {
                    id: track
                    Layout.fillWidth: true
                    implicitHeight: 6
                    radius: 3
                    color: Theme.border
                    clip: true
                    Rectangle {
                        height: parent.height
                        radius: 3
                        color: Theme.accent
                        width: dlg.v.progressIndeterminate ? track.width * 0.3
                                                          : track.width * (dlg.v.progressPermille || 0) / 1000
                        // Unbestimmt (1, „Wird geprüft …“): ein Teilstück von 30 % wandert hin
                        // und her. Ohne Bewegung steht es still; nichts hängt davon ab. Die
                        // Animation schreibt `slide`, nicht `x`: Sonst bliebe der bestimmte
                        // Balken danach am letzten Animationswert versetzt stehen.
                        property real slide: 0
                        x: dlg.v.progressIndeterminate === true ? slide : 0
                        SequentialAnimation on slide {
                            running: dlg.opened && dlg.v.progressIndeterminate === true && !App.reduceMotion
                            loops: Animation.Infinite
                            NumberAnimation { from: 0; to: track.width * 0.7; duration: 900; easing.type: Easing.InOutQuad }
                            NumberAnimation { from: track.width * 0.7; to: 0; duration: 900; easing.type: Easing.InOutQuad }
                        }
                    }
                }
                Text {
                    visible: text.length > 0
                    Layout.fillWidth: true
                    text: dlg.v.progressNote || ""
                    color: Theme.textDim
                    font.pixelSize: 12
                    wrapMode: Text.WordWrap
                }
            }

            // Details (10–14): eingeklappt, Mono, markierbar, mit „Kopieren“.
            ColumnLayout {
                visible: (dlg.v.details || "").length > 0
                Layout.fillWidth: true
                Layout.topMargin: 12
                spacing: 6
                UpdateButton {
                    Layout.leftMargin: -8
                    role: "Quiet"
                    text: (Updates.textRevision, Updates.text("details"))
                    glyph: dlg.detailsOpen ? "caret-down" : "caret-right"
                    implicitHeight: 28
                    onClicked: dlg.detailsOpen = !dlg.detailsOpen
                }
                RowLayout {
                    visible: dlg.detailsOpen
                    Layout.fillWidth: true
                    spacing: 8
                    ScrollView {
                        Layout.fillWidth: true
                        Layout.preferredHeight: Math.min(108, detailsText.implicitHeight + 20)
                        TextArea {
                            id: detailsText
                            readOnly: true
                            selectByMouse: true
                            wrapMode: TextEdit.WrapAnywhere
                            text: dlg.v.details || ""
                            color: Theme.textBright
                            font.family: App.monospaceFonts().length > 0 ? App.monospaceFonts()[0] : ""
                            font.pixelSize: 12
                            background: Rectangle { radius: 6; color: Theme.bgSidebar }
                        }
                    }
                    UpdateButton {
                        Layout.alignment: Qt.AlignTop
                        implicitHeight: 28
                        text: (Updates.textRevision, dlg.copied ? Updates.text("details.copied")
                                                                : Updates.text("details.copy"))
                        onClicked: {
                            App.copyToClipboard(dlg.v.details || "")
                            dlg.copied = true
                            copiedTimer.restart()
                        }
                        Timer { id: copiedTimer; interval: 1500; onTriggered: dlg.copied = false }
                    }
                }
            }
        }
        }

        // --- Fußzeile -------------------------------------------------------
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: 1
            color: Theme.border
        }
        ColumnLayout {
            Layout.fillWidth: true
            Layout.leftMargin: 24
            Layout.rightMargin: 24
            Layout.topMargin: 16
            Layout.bottomMargin: 16
            spacing: 12

            // Fußnote: im Kasten „Was endet“ (5, 13) oder schlicht (4).
            Rectangle {
                visible: (dlg.v.footnote || []).length > 0
                Layout.fillWidth: true
                radius: 6
                color: dlg.v.footnoteBoxed ? Theme.bgSidebar : "transparent"
                implicitHeight: Math.max(dlg.v.footnoteBoxed ? 38 : 0,
                                         footRow.implicitHeight + (dlg.v.footnoteBoxed ? 20 : 0))
                RowLayout {
                    id: footRow
                    anchors.fill: parent
                    anchors.margins: dlg.v.footnoteBoxed ? 10 : 0
                    anchors.leftMargin: dlg.v.footnoteBoxed ? 12 : 0
                    anchors.rightMargin: dlg.v.footnoteBoxed ? 12 : 0
                    spacing: 10
                    UpdateIcon {
                        Layout.alignment: Qt.AlignTop
                        name: dlg.v.footnoteIcon || ""
                        color: Theme.textDim
                        size: 16
                    }
                    Text {
                        Layout.fillWidth: true
                        text: (dlg.v.footnote || []).join("\n")
                        color: dlg.v.footnoteBoxed ? Theme.textBright : Theme.textDim
                        font.pixelSize: dlg.v.footnoteBoxed ? 13 : 12
                        lineHeight: dlg.v.footnoteBoxed ? 19 : 17
                        lineHeightMode: Text.FixedHeight
                        wrapMode: Text.WordWrap
                    }
                }
            }

            // Knopfzeile: feste Reihenfolge auf allen Systemen, leise Knöpfe links,
            // Hauptknopf ganz rechts (Entwurf §2). Kein QDialogButtonBox-Verhalten.
            RowLayout {
                id: btnRow
                Layout.fillWidth: true
                spacing: 8
                Repeater {
                    model: dlg.buttons.filter(b => b.role === "Quiet")
                    UpdateButton {
                        required property var modelData
                        Layout.leftMargin: -8
                        role: modelData.role
                        flowAction: modelData.action
                        text: modelData.label
                        enabled: modelData.enabled
                        onClicked: dlg.act(flowAction)
                    }
                }
                Item { Layout.fillWidth: true }
                Repeater {
                    model: dlg.buttons.filter(b => b.role !== "Quiet" && b.role !== "InText")
                    UpdateButton {
                        required property var modelData
                        role: modelData.role
                        flowAction: modelData.action
                        text: modelData.label
                        enabled: modelData.enabled
                        onClicked: dlg.act(flowAction)
                    }
                }
            }
        }
    }
}
