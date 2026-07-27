pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QTmux

// Kategorie „Tastenkürzel" (QTMUX-47, Tabelle A4 + Schritt 5): konfigurierbare Aktionen
// (QTMUX-15), gruppiert nach Sitzungen · Panes · Verbindungen · Ansicht & App. Die Aufnahme
// passiert IN der Zeile (kein modaler hotkeyCaptureDialog mehr): Klick auf die Belegung
// schaltet die Zeile in den Aufnahmezustand (Akzentfeld, „Tasten drücken …"), Esc/Abbrechen
// bricht ab, ⏎/Zuweisen bestätigt, „Leeren" entfernt. Ein Konflikt erscheint inline unter
// der Zeile mit „Trotzdem zuweisen". Solange aufgenommen wird, deaktiviert host.capturing
// alle globalen App-Shortcuts (Main.qml) und die Fenster-Shortcuts (PrefsWindow).
CatPage {
    id: page
    heading: qsTr("Tastenkürzel")
    subtitle: qsTr("Klick auf ein Kürzel nimmt eine neue Belegung auf. „Standard“ erscheint nur bei Abweichung.")

    // --- Aufnahmezustand ---
    property string capturingId: ""          // welche Aktion gerade aufgenommen wird ("" = keine)
    property var capChords: []               // bislang erfasste Akkorde der aktiven Zeile
    readonly property string capSeq: capChords.join(", ")
    readonly property string capConflict: capSeq.length > 0 ? Hotkeys.conflict(capSeq, capturingId) : ""

    onCapturingIdChanged: page.host.capturing = (capturingId.length > 0)
    // Beim Verlassen der Seite (Kategorie-Wechsel / Fenster zu) die globale Sperre lösen.
    Component.onDestruction: page.host.capturing = false

    function startCapture(id) {
        capturingId = id
        capChords = []
        captureArea.forceActiveFocus()
    }
    function cancelCapture() {
        capturingId = ""
        capChords = []
    }
    function commitCapture() {
        if (capChords.length > 0) Hotkeys.setBinding(capturingId, capSeq)
        cancelCapture()
    }

    // Gruppen-Zuordnung (die Registry kennt keine Gruppen — hier definiert). Nur tatsächlich
    // vorhandene Aktionen werden gezeigt; unbekannte landen am Ende in „Weitere", damit nie
    // eine Aktion verschwindet.
    readonly property var groupDefs: [
        { title: qsTr("Sitzungen"),     ids: ["actNewSession", "actNewInstance", "actCloseSession", "actNextSession", "actPrevSession"] },
        { title: qsTr("Panes"),         ids: ["actClosePane", "actNextPane", "actPrevPane", "actSplitH", "actSplitV", "actBroadcast"] },
        { title: qsTr("Verbindungen"),  ids: ["actNewSsh", "actNewSerial", "actConnections", "actVault"] },
        { title: qsTr("Ansicht & App"), ids: ["actCommandPalette", "actFind", "actMcpToggle", "actZoomReset", "actToggleTheme", "actSettings", "actAbout", "actQuit"] }
    ]
    readonly property var groups: {
        const all = Hotkeys.actionIds()
        const used = {}
        const out = []
        for (let g = 0; g < groupDefs.length; ++g) {
            const ids = groupDefs[g].ids.filter(id => all.indexOf(id) >= 0)
            ids.forEach(id => used[id] = true)
            if (ids.length > 0) out.push({ title: groupDefs[g].title, ids: ids })
        }
        const rest = all.filter(id => !used[id])
        if (rest.length > 0) out.push({ title: qsTr("Weitere"), ids: rest })
        return out
    }

    // Einziger Fokus-Empfänger für die Tastenaufnahme (Logik wie der frühere
    // hotkeyCaptureDialog). Bekommt beim Start den Fokus; unsichtbar/größenlos.
    Item {
        id: captureArea
        Keys.onPressed: (event) => {
            if (page.capturingId.length === 0) return
            // Esc bricht ab, Enter bestätigt (jeweils ohne Modifier) — mit Modifier sind
            // sie als Kürzel aufnehmbar.
            if (event.key === Qt.Key_Escape && event.modifiers === Qt.NoModifier) {
                event.accepted = true; page.cancelCapture(); return
            }
            if ((event.key === Qt.Key_Return || event.key === Qt.Key_Enter)
                    && event.modifiers === Qt.NoModifier) {
                event.accepted = true; page.commitCapture(); return
            }
            var c = App.keyChord(event.key, event.modifiers)
            if (c.length === 0) return            // reine Modifier-Taste
            event.accepted = true
            var arr = page.capChords.slice()
            arr.push(c)
            if (arr.length > 4) arr = [c]         // QKeySequence: max. 4 Akkorde
            page.capChords = arr
        }
    }

    // --- Gruppen mit ihren Aktionen ---
    PrefAnchor { settingKey: "hotkeys.list"; page: page
    Repeater {
        model: page.groups
        delegate: ColumnLayout {
            id: grp
            required property var modelData      // { title, ids }
            Layout.fillWidth: true
            spacing: 2

            SectionLabel { text: grp.modelData.title }

            Repeater {
                model: grp.modelData.ids
                delegate: ColumnLayout {
                    id: row
                    required property string modelData   // actionId
                    Layout.fillWidth: true
                    spacing: 2
                    readonly property bool active: page.capturingId === row.modelData

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8
                        Text {
                            text: page.host.app.hotkeyLabel(row.modelData)
                            color: Theme.textBright
                            font.pixelSize: 13
                            Layout.fillWidth: true
                            elide: Text.ElideRight
                        }
                        // Normalzustand: aktuelle Belegung (Klick startet die Aufnahme).
                        Button {
                            visible: !row.active
                            text: Hotkeys.bindings[row.modelData] || qsTr("(keins)")
                            font.pixelSize: 12
                            onClicked: page.startCapture(row.modelData)
                        }
                        Button {
                            visible: !row.active && Hotkeys.bindings[row.modelData] !== Hotkeys.defaultSequence(row.modelData)
                            text: qsTr("Standard")
                            font.pixelSize: 12
                            onClicked: Hotkeys.reset(row.modelData)
                        }
                        // Aufnahmezustand: Akzent-Erfassungsfeld + Leeren + Abbrechen.
                        Rectangle {
                            visible: row.active
                            Layout.preferredWidth: 200
                            implicitHeight: 30
                            radius: 6
                            color: Theme.bgElevated
                            border.width: 1
                            border.color: Theme.accent
                            Text {
                                anchors.centerIn: parent
                                text: page.capSeq.length > 0 ? page.capSeq : qsTr("Tasten drücken …")
                                color: page.capSeq.length > 0 ? Theme.textBright : Theme.textDim
                                font.pixelSize: 13
                                font.bold: page.capSeq.length > 0
                            }
                        }
                        Button {
                            visible: row.active
                            text: qsTr("Leeren")
                            font.pixelSize: 12
                            enabled: page.capChords.length > 0
                            onClicked: page.capChords = []
                        }
                        Button {
                            visible: row.active
                            text: qsTr("Abbrechen")
                            font.pixelSize: 12
                            onClicked: page.cancelCapture()
                        }
                    }

                    // Konflikt-/Bestätigungszeile unter der Zeile (nur im Aufnahmezustand,
                    // sobald mindestens ein Akkord erfasst ist).
                    RowLayout {
                        visible: row.active && page.capSeq.length > 0
                        Layout.leftMargin: 8
                        spacing: 8
                        Text {
                            visible: page.capConflict.length > 0
                            text: qsTr("Bereits belegt von: %1").arg(page.host.app.hotkeyLabel(page.capConflict))
                            color: "#e0a040"   // Warn-Amber (kein Theme-Token vorhanden)
                            font.pixelSize: 11
                            Layout.fillWidth: true
                            wrapMode: Text.WordWrap
                        }
                        Item { visible: page.capConflict.length === 0; Layout.fillWidth: true }
                        Button {
                            text: page.capConflict.length > 0 ? qsTr("Trotzdem zuweisen") : qsTr("Zuweisen")
                            font.pixelSize: 11
                            onClicked: page.commitCapture()
                        }
                    }
                }
            }
        }
    }

    Button {
        text: qsTr("Alle Kürzel zurücksetzen")
        onClicked: Hotkeys.resetAll()
    }

    Text {
        text: qsTr("Mehrere nacheinander gedrückte Akkorde ergeben eine Tastenfolge (max. 4). "
                 + "Esc bricht ab, Eingabe bestätigt.")
        color: Theme.textDim
        font.pixelSize: 11
        Layout.fillWidth: true
        wrapMode: Text.WordWrap
    }
    }
}
