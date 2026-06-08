import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QtQuick.Effects
import QtCore
import QTmux

ApplicationWindow {
    id: window
    width: 1100
    height: 720
    visible: true
    title: "QTmux"
    color: Theme.bgMain

    // Themengebundene Palette: alle (In-Window-)Basic-Controls — Dialoge, ComboBoxen,
    // Textfelder, Buttons, das typeMenu-Popup — erben diese Farben automatisch.
    palette.window: Theme.bgMain
    palette.windowText: Theme.textBright
    palette.base: Theme.bgElevated
    palette.alternateBase: Theme.bgSidebar
    palette.text: Theme.textBright
    palette.button: Theme.bgElevated
    palette.buttonText: Theme.textBright
    palette.highlight: Theme.accent
    palette.highlightedText: "#ffffff"
    palette.mid: Theme.border
    palette.dark: Theme.border
    palette.placeholderText: Theme.textDim
    palette.toolTipBase: Theme.bgElevated
    palette.toolTipText: Theme.textBright

    property int currentRow: -1
    // Aktive (fokussierte) Session ans Model melden -> löscht deren Aufmerksamkeits-Hinweis.
    onCurrentRowChanged: sessions.setActiveRow(currentRow)

    // --- Split-Panes ---------------------------------------------------------
    // paneModel: ein Eintrag je sichtbarem Terminal-Pane; `sessionRow` indexiert
    // die im Pane gezeigte Session (Sidebar-Reihenfolge). `activePane` ist das
    // fokussierte Pane; `currentRow` folgt dessen Session.
    ListModel { id: paneModel }
    property int activePane: 0
    property var activeTerminal: null

    SessionModel { id: sessions }

    // MCP-Server: externe Agenten-Steuerung über 127.0.0.1 (nur lokal).
    McpServer {
        id: mcp
        sessions: sessions
        port: 7345
        onFocusRequested: (row) => window.currentRow = row
        onSetThemeRequested: (mode) => Theme.mode = mode
        Component.onCompleted: start()
    }

    // Hält currentRow gültig, wenn Sessions entfernt werden (manuell oder bei Shell-Ende).
    Connections {
        target: sessions
        function onRowsRemoved(parent, first, last) {
            const removed = last - first + 1
            const adjust = function(r) {
                if (r > last) return r - removed
                if (r >= first) return Math.min(first, sessions.count - 1)
                return r
            }
            window.currentRow = adjust(window.currentRow)
            // Alle Panes auf gültige Session-Reihen nachführen (verschobene Indizes).
            for (let i = 0; i < paneModel.count; ++i)
                paneModel.setProperty(i, "sessionRow", adjust(paneModel.get(i).sessionRow))
        }
        // Fenster-Alert (Dock-Hüpfen/Taskbar-Blinken), wenn QTmux nicht im Vordergrund ist.
        function onAttentionRaised(row) {
            if (!window.active) window.alert(0)
        }
    }

    // Pfad zu einem Phosphor-SVG-Icon (eingebettet unter qrc:/icons/).
    function icon(name) { return "qrc:/icons/" + name + ".svg" }

    // --- Wiederverwendbares Icon-Steuerelement (Phosphor-SVG) ---------------
    // Toolbar-Knopf: zeigt ein SVG-Icon; Tönung folgt Hover/aktiv/Theme.
    component IconToolButton: ToolButton {
        id: tb
        property string tip: ""
        property bool active: false        // dauerhaft hervorgehoben (z. B. Server an)
        display: AbstractButton.IconOnly
        icon.width: 18
        icon.height: 18
        icon.color: !tb.enabled ? Theme.border
                  : (tb.down || tb.active) ? Theme.accent
                  : tb.hovered ? Theme.textBright : Theme.textDim
        implicitWidth: 36
        implicitHeight: 30
        background: Rectangle {
            radius: 6
            color: tb.down ? Theme.sidebarSelected
                 : tb.hovered ? Theme.sidebarHover : "transparent"
        }
        ToolTip.visible: hovered && tip.length > 0
        ToolTip.delay: 600
        ToolTip.text: tip
    }

    // Themen-Menüeintrag (In-Window): abgerundetes Highlight, app-getöntes Icon.
    component AppMenuItem: MenuItem {
        id: ami
        implicitHeight: 34
        icon.color: Theme.textBright
        icon.width: 16
        icon.height: 16
        background: Rectangle {
            radius: 6
            color: ami.highlighted ? Theme.sidebarHover : "transparent"
        }
    }

    // Themen-Popup-Hintergrund (Menüs/ComboBox): abgerundet, erhoben, gerahmt.
    component AppPopupBg: Rectangle {
        color: Theme.bgElevated
        border.color: Theme.border
        border.width: 1
        radius: 8
    }

    // Themen-ComboBox: gerahmtes Feld, Caret-Icon, abgerundetes Popup.
    component AppComboBox: ComboBox {
        id: cb
        implicitHeight: 32
        font.pixelSize: 13
        background: Rectangle {
            radius: 6
            color: Theme.bgElevated
            border.color: cb.activeFocus ? Theme.accent : Theme.border
            border.width: 1
        }
        indicator: Image {
            x: cb.width - width - 10
            y: (cb.height - height) / 2
            source: window.icon("caret-down")
            sourceSize.width: 14
            sourceSize.height: 14
            layer.enabled: true
            layer.effect: MultiEffect {
                colorization: 1.0
                colorizationColor: Theme.textDim
            }
        }
        popup: Popup {
            y: cb.height + 4
            width: cb.width
            padding: 4
            implicitHeight: Math.min(contentItem.implicitHeight + 8, 260)
            background: AppPopupBg {}
            contentItem: ListView {
                clip: true
                implicitHeight: contentHeight
                model: cb.popup.visible ? cb.delegateModel : null
                currentIndex: cb.highlightedIndex
                ScrollIndicator.vertical: ScrollIndicator {}
            }
        }
    }

    // Themen-Dialog: abgerundete erhobene Fläche, gestylter Titel, abgedunkelter Hintergrund.
    component AppDialog: Dialog {
        id: dlg
        anchors.centerIn: parent
        modal: true
        padding: 20
        background: Rectangle {
            color: Theme.bgElevated
            radius: 12
            border.color: Theme.border
            border.width: 1
        }
        header: Label {
            text: dlg.title
            visible: dlg.title.length > 0
            color: Theme.textBright
            font.pixelSize: 16
            font.bold: true
            elide: Label.ElideRight
            padding: 20
            bottomPadding: 6
        }
        Overlay.modal: Rectangle { color: "#88000000" }
    }

    // Session-Typ-Auswahl für den „+"-Split-Button (Sidebar + Toolbar).
    // Shell-Einträge kommen aus sessions.availableShells(): auf Windows mehrere
    // (PowerShell/cmd/…), auf Unix genau einer ("Shell"). Die Wahl setzt zugleich
    // die gemerkte Standard-Shell (window.defaultShellProgram).
    Menu {
        id: typeMenu
        padding: 4
        background: AppPopupBg { implicitWidth: 200 }
        Repeater {
            model: sessions.availableShells()
            delegate: AppMenuItem {
                required property var modelData
                text: modelData.name
                icon.source: window.icon("terminal-window")
                checkable: true
                checked: window.newSessionType === 0
                         && window.currentShellProgram() === modelData.program
                onTriggered: {
                    window.newSessionType = 0
                    window.defaultShellProgram = modelData.program
                }
            }
        }
        AppMenuItem {
            text: qsTr("SSH …")
            icon.source: window.icon("plugs")
            checkable: true
            checked: window.newSessionType === 1
            onTriggered: window.newSessionType = 1
        }
        AppMenuItem {
            text: qsTr("Seriell …")
            icon.source: window.icon("usb")
            checkable: true
            checked: window.newSessionType === 2
            onTriggered: window.newSessionType = 2
        }
    }

    // Kontextmenü des Terminals (Rechtsklick): Kopieren/Einfügen + Pane teilen/schließen.
    Menu {
        id: termContextMenu
        padding: 4
        background: AppPopupBg { implicitWidth: 200 }
        AppMenuItem { action: actCopy;  icon.source: window.icon("copy") }
        AppMenuItem { action: actPaste; icon.source: window.icon("clipboard") }
        MenuSeparator {}
        AppMenuItem { action: actSplitH;    icon.source: window.icon("split-h") }
        AppMenuItem { action: actSplitV;    icon.source: window.icon("split-v") }
        AppMenuItem { action: actClosePane; icon.source: window.icon("x") }
    }

    // Vom Split-Button gewählter Standardtyp (0=Shell, 1=SSH, 2=Seriell), persistiert.
    property int newSessionType: 0
    // Gewählte Standard-Shell (Programmname, z. B. "cmd.exe"); leer = Plattform-Vorgabe.
    // Persistiert; gilt für neue Shell-Sessions und Splits.
    property string defaultShellProgram: ""
    // Gibt es überhaupt eine Auswahl (Windows: ja; Unix: nur Login-Shell)?
    readonly property bool hasShellChoice: sessions.availableShells().length > 1

    // Terminal-Schriftgröße (global für alle Panes, persistiert). Zoom via
    // Cmd/Strg +/−/0 und Cmd/Strg+Mausrad. Auf 6..40 pt begrenzt.
    property int terminalFontSize: 13
    function zoomTerminal(delta) {
        terminalFontSize = Math.max(6, Math.min(40, terminalFontSize + delta))
    }
    function resetTerminalZoom() { terminalFontSize = 13 }

    // Broadcast-/Sync-Input: getippte Eingabe geht an ALLE Sessions (Multi-Agent).
    // Bewusst NICHT persistiert (Footgun) — startet je Sitzung aus.
    property bool broadcastInput: false

    // Aktuell wirksame Shell (für Häkchen in den Menüs): die gewählte, sonst die
    // erste verfügbare (= Plattform-Vorgabe).
    function currentShellProgram() {
        if (defaultShellProgram !== "") return defaultShellProgram
        const list = sessions.availableShells()
        return list.length > 0 ? list[0].program : ""
    }

    function newSession() {
        const row = sessions.createShellSession("", window.defaultShellProgram)
        // Neue Session ins aktive Pane laden und den Tastaturfokus daraufsetzen,
        // damit man sofort tippen kann (kein Klick ins Terminal nötig).
        if (window.activePane >= 0 && window.activePane < paneModel.count)
            window.assignToActivePane(row)
        else
            window.currentRow = row   // Startfall: Pane wird gleich erst erzeugt
    }
    function closeCurrent() {
        if (currentRow < 0) return
        sessions.closeSession(currentRow)
        currentRow = Math.min(currentRow, sessions.count - 1)
    }
    function typeLabel(t) {
        return t === 1 ? qsTr("SSH") : t === 2 ? qsTr("Seriell") : qsTr("Shell")
    }
    function openNewSession(t) {
        if (t === 1) sshDialog.open()
        else if (t === 2) serialDialog.openDialog()
        else newSession()
    }

    // --- Pane-Steuerung ------------------------------------------------------
    // Setzt das fokussierte Pane (+ dessen Terminal) und zieht currentRow nach.
    function setActivePane(index, term) {
        window.activePane = index
        window.activeTerminal = term
        if (index >= 0 && index < paneModel.count) {
            const row = paneModel.get(index).sessionRow
            if (row >= 0 && row < sessions.count) window.currentRow = row
        }
    }
    // Fokus nachträglich (nach Item-Erzeugung) auf das aktive Pane legen.
    function focusActivePane() {
        Qt.callLater(function() {
            const p = paneRepeater.itemAt(window.activePane)
            if (p) { window.activeTerminal = p.term; p.term.forceActiveFocus() }
        })
    }
    // Sidebar-Reorder: Session verschieben und alle Index-Referenzen (currentRow,
    // Pane-sessionRows) auf die neue Reihenfolge nachführen.
    function moveSession(from, to) {
        if (from === to) return
        sessions.moveSession(from, to)
        const remap = function(x) {
            if (x === from) return to
            if (from < to) { if (x > from && x <= to) return x - 1 }
            else { if (x >= to && x < from) return x + 1 }
            return x
        }
        window.currentRow = remap(window.currentRow)
        for (let i = 0; i < paneModel.count; ++i)
            paneModel.setProperty(i, "sessionRow", remap(paneModel.get(i).sessionRow))
    }

    // Sidebar-Klick: gewählte Session ins aktive Pane laden.
    function assignToActivePane(row) {
        window.currentRow = row
        if (window.activePane >= 0 && window.activePane < paneModel.count)
            paneModel.setProperty(window.activePane, "sessionRow", row)
        focusActivePane()
    }
    // Teilen: neue Shell-Session in einem neuen Pane (orientation gilt für alle Panes).
    function splitPane(orientation) {
        mainSplit.orientation = orientation
        const row = sessions.createShellSession("", window.defaultShellProgram)
        paneModel.append({ sessionRow: row })
        window.activePane = paneModel.count - 1
        window.currentRow = row
        focusActivePane()
    }
    // Aktives Pane schließen (letztes Pane -> normale Session-Schließung).
    function closePane() {
        if (paneModel.count <= 1) { window.closeCurrent(); return }
        const idx = window.activePane
        const row = paneModel.get(idx).sessionRow
        paneModel.remove(idx)
        window.activePane = Math.min(idx, paneModel.count - 1)
        sessions.closeSession(row)   // -> onRowsRemoved passt übrige Panes an
        focusActivePane()
        if (window.activePane < paneModel.count)
            window.currentRow = paneModel.get(window.activePane).sessionRow
    }

    // Beim Start die persistierten Sessions wiederherstellen; sonst eine neue öffnen.
    Component.onCompleted: {
        const active = sessions.restoreState()
        if (sessions.count === 0)
            newSession()
        else
            currentRow = (active >= 0 && active < sessions.count) ? active : 0
        // Start mit genau einem Pane, das die aktive Session zeigt.
        paneModel.append({ sessionRow: window.currentRow })
        window.activePane = 0
    }

    // Wird das Fenster (wieder) aktiv, den Tastaturfokus auf das aktive Pane legen,
    // damit man ohne Klick ins Terminal sofort tippen kann.
    onActiveChanged: if (active) focusActivePane()

    // Beim Schließen erst den Zustand sichern (braucht laufende Prozesse für das
    // aktuelle Arbeitsverzeichnis), dann alle Prozesse/Verbindungen beenden.
    onClosing: {
        sessions.saveState()
        sessions.shutdownAll()
    }

    // Fenstergeometrie + gewählter Session-Typ über Neustarts erhalten.
    Settings {
        category: "window"
        property alias x: window.x
        property alias y: window.y
        property alias width: window.width
        property alias height: window.height
        property alias newSessionType: window.newSessionType
        property alias defaultShellProgram: window.defaultShellProgram
        property alias terminalFontSize: window.terminalFontSize
    }

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
    // Terminal-Zoom: Schriftgröße global vergrößern/verkleinern/zurücksetzen.
    Action {
        id: actZoomIn
        text: qsTr("Schrift vergrößern")
        shortcut: StandardKey.ZoomIn        // Cmd++/Strg++ (inkl. „=" ohne Shift)
        onTriggered: window.zoomTerminal(1)
    }
    Action {
        id: actZoomOut
        text: qsTr("Schrift verkleinern")
        shortcut: StandardKey.ZoomOut        // Cmd+-/Strg+-
        onTriggered: window.zoomTerminal(-1)
    }
    Action {
        id: actZoomReset
        text: qsTr("Schriftgröße zurücksetzen")
        shortcut: "Ctrl+0"
        onTriggered: window.resetTerminalZoom()
    }
    // Broadcast-Input umschalten: Eingabe an alle Sessions.
    Action {
        id: actBroadcast
        text: qsTr("Eingabe an alle Sessions")
        shortcut: "Ctrl+Shift+B"
        checkable: true
        checked: window.broadcastInput
        onTriggered: window.broadcastInput = !window.broadcastInput
    }
    // Kopieren/Einfügen. Shortcut nur auf macOS (Cmd+C/V) — kapert dort NICHT das
    // Terminal-Ctrl+C (SIGINT). Auf Windows/Linux handhabt das TerminalItem selbst
    // Ctrl+Shift+C/V, damit Ctrl+C im Terminal weiter SIGINT bleibt.
    Action {
        id: actCopy
        text: qsTr("Kopieren")
        enabled: window.activeTerminal && window.activeTerminal.hasSelection
        shortcut: Qt.platform.os === "osx" ? StandardKey.Copy : ""
        onTriggered: if (window.activeTerminal) window.activeTerminal.copy()
    }
    Action {
        id: actPaste
        text: qsTr("Einfügen")
        shortcut: Qt.platform.os === "osx" ? StandardKey.Paste : ""
        onTriggered: if (window.activeTerminal) window.activeTerminal.paste()
    }
    // Split-Panes: nebeneinander / untereinander teilen, aktives Pane schließen.
    Action {
        id: actSplitH
        text: qsTr("Nebeneinander teilen")
        shortcut: "Ctrl+Shift+E"
        onTriggered: window.splitPane(Qt.Horizontal)
    }
    Action {
        id: actSplitV
        text: qsTr("Untereinander teilen")
        shortcut: "Ctrl+Shift+O"
        onTriggered: window.splitPane(Qt.Vertical)
    }
    Action {
        id: actClosePane
        text: qsTr("Pane schließen")
        shortcut: "Ctrl+Shift+W"
        enabled: paneModel.count > 1
        onTriggered: window.closePane()
    }

    // --- Toolbar oben: Schnellzugriff mit Phosphor-Icons --------------------
    header: ToolBar {
        // Feste Höhe: das innen mit anchors.fill verankerte RowLayout liefert
        // sonst keine implizite Höhe, die ToolBar würde auf 0 kollabieren.
        height: 44
        background: Rectangle {
            color: Theme.bgElevated
            Rectangle {
                anchors.bottom: parent.bottom
                width: parent.width; height: 1
                color: Theme.border
            }
        }
        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 8
            anchors.rightMargin: 8
            spacing: 3

            IconToolButton {
                icon.source: window.icon("plus")
                tip: qsTr("Neue Session: %1").arg(window.typeLabel(window.newSessionType))
                onClicked: window.openNewSession(window.newSessionType)
            }
            IconToolButton {
                icon.source: window.icon("caret-down")
                icon.width: 14; icon.height: 14
                implicitWidth: 22
                tip: qsTr("Session-Typ wählen")
                onClicked: typeMenu.popup(this, 0, height)
            }

            ToolSeparator {}

            IconToolButton {
                icon.source: window.icon("plugs")
                tip: qsTr("Neue SSH-Verbindung …")
                onClicked: sshDialog.open()
            }
            IconToolButton {
                icon.source: window.icon("usb")
                tip: qsTr("Neue serielle Verbindung …")
                onClicked: serialDialog.openDialog()
            }

            ToolSeparator {}

            IconToolButton {
                icon.source: window.icon("x")
                tip: qsTr("Session schließen")
                enabled: window.currentRow >= 0
                onClicked: window.closeCurrent()
            }

            ToolSeparator {}

            IconToolButton {
                icon.source: window.icon("split-h")
                tip: qsTr("Nebeneinander teilen")
                onClicked: window.splitPane(Qt.Horizontal)
            }
            IconToolButton {
                icon.source: window.icon("split-v")
                tip: qsTr("Untereinander teilen")
                onClicked: window.splitPane(Qt.Vertical)
            }

            ToolSeparator {}

            IconToolButton {
                icon.source: window.icon("broadcast-input")
                active: window.broadcastInput
                tip: window.broadcastInput ? qsTr("Broadcast-Eingabe: an (an alle Sessions)")
                                           : qsTr("Eingabe an alle Sessions (Broadcast)")
                onClicked: window.broadcastInput = !window.broadcastInput
            }

            Item { Layout.fillWidth: true }   // Abstandhalter

            IconToolButton {
                icon.source: Theme.dark ? window.icon("sun") : window.icon("moon")
                tip: Theme.dark ? qsTr("Helles Design") : qsTr("Dunkles Design")
                onClicked: Theme.toggle()
            }
            IconToolButton {
                icon.source: window.icon("broadcast")
                active: mcp.listening
                tip: mcp.listening ? qsTr("MCP-Server: an (127.0.0.1:%1)").arg(mcp.port)
                                   : qsTr("MCP-Server: aus")
                onClicked: mcp.listening ? mcp.stop() : mcp.start()
            }
            IconToolButton {
                icon.source: window.icon("info")
                tip: qsTr("Über QTmux")
                onClicked: aboutDialog.open()
            }
        }
    }

    // --- Menüleiste: bietet alle Oberflächen-Befehle ------------------------
    menuBar: MenuBar {
        Menu {
            title: qsTr("Datei")
            MenuItem { action: actNewSession; icon.source: window.icon("plus"); icon.color: Theme.menuIcon; icon.width: 16; icon.height: 16 }
            MenuItem {
                text: qsTr("Neue SSH-Verbindung …")
                icon.source: window.icon("plugs"); icon.color: Theme.menuIcon; icon.width: 16; icon.height: 16
                onTriggered: sshDialog.open()
            }
            MenuItem {
                text: qsTr("Neue serielle Verbindung …")
                icon.source: window.icon("usb"); icon.color: Theme.menuIcon; icon.width: 16; icon.height: 16
                onTriggered: serialDialog.openDialog()
            }
            MenuItem { action: actCloseSession; icon.source: window.icon("x"); icon.color: Theme.menuIcon; icon.width: 16; icon.height: 16 }
            MenuSeparator { visible: window.hasShellChoice }
            // Globale Standard-Shell (nur Windows, wo es mehrere gibt). Setzt dieselbe
            // Property wie die Schnellwahl im „+"-Menü → beide bleiben synchron.
            Menu {
                title: qsTr("Standard-Shell")
                visible: window.hasShellChoice
                Repeater {
                    model: sessions.availableShells()
                    delegate: MenuItem {
                        required property var modelData
                        text: modelData.name
                        icon.source: window.icon("terminal-window")
                        icon.color: Theme.menuIcon; icon.width: 16; icon.height: 16
                        checkable: true
                        checked: window.currentShellProgram() === modelData.program
                        onTriggered: window.defaultShellProgram = modelData.program
                    }
                }
            }
            MenuSeparator {}
            MenuItem { action: actQuit }
        }
        Menu {
            title: qsTr("Bearbeiten")
            MenuItem { action: actCopy;  icon.source: window.icon("copy");      icon.color: Theme.menuIcon; icon.width: 16; icon.height: 16 }
            MenuItem { action: actPaste; icon.source: window.icon("clipboard"); icon.color: Theme.menuIcon; icon.width: 16; icon.height: 16 }
        }
        Menu {
            title: qsTr("Ansicht")
            MenuItem {
                action: actSplitH
                icon.source: window.icon("split-h"); icon.color: Theme.menuIcon; icon.width: 16; icon.height: 16
            }
            MenuItem {
                action: actSplitV
                icon.source: window.icon("split-v"); icon.color: Theme.menuIcon; icon.width: 16; icon.height: 16
            }
            MenuItem { action: actClosePane; icon.source: window.icon("x"); icon.color: Theme.menuIcon; icon.width: 16; icon.height: 16 }
            MenuSeparator {}
            MenuItem { action: actZoomIn }
            MenuItem { action: actZoomOut }
            MenuItem { action: actZoomReset }
            MenuSeparator {}
            MenuItem {
                action: actBroadcast
                icon.source: window.icon("broadcast-input"); icon.color: Theme.menuIcon; icon.width: 16; icon.height: 16
            }
            MenuSeparator {}
            MenuItem {
                action: actToggleTheme
                icon.source: Theme.dark ? window.icon("sun") : window.icon("moon")
                icon.color: Theme.menuIcon
                icon.width: 16; icon.height: 16
            }
            MenuSeparator {}
            MenuItem {
                text: qsTr("Design: Wie System")
                icon.source: window.icon("gear"); icon.color: Theme.menuIcon; icon.width: 16; icon.height: 16
                checkable: true
                checked: Theme.mode === Theme.System
                onTriggered: Theme.mode = Theme.System
            }
            MenuItem {
                text: qsTr("Design: Hell")
                icon.source: window.icon("sun"); icon.color: Theme.menuIcon; icon.width: 16; icon.height: 16
                checkable: true
                checked: Theme.mode === Theme.Light
                onTriggered: Theme.mode = Theme.Light
            }
            MenuItem {
                text: qsTr("Design: Dunkel")
                icon.source: window.icon("moon"); icon.color: Theme.menuIcon; icon.width: 16; icon.height: 16
                checkable: true
                checked: Theme.mode === Theme.Dark
                onTriggered: Theme.mode = Theme.Dark
            }
        }
        Menu {
            title: qsTr("Sprache")
            Repeater {
                model: App.languageCodes()
                MenuItem {
                    required property string modelData
                    text: App.languageName(modelData)
                    icon.source: window.icon("translate"); icon.color: Theme.menuIcon; icon.width: 16; icon.height: 16
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
                icon.source: window.icon("robot"); icon.color: Theme.menuIcon; icon.width: 16; icon.height: 16
                onTriggered: window.newSession()
            }
        }
        Menu {
            title: qsTr("Agent-Steuerung")
            MenuItem {
                text: mcp.listening ? qsTr("MCP-Server: an (127.0.0.1:%1)").arg(mcp.port)
                                    : qsTr("MCP-Server: aus")
                icon.source: window.icon("broadcast"); icon.color: Theme.menuIcon; icon.width: 16; icon.height: 16
                checkable: true
                checked: mcp.listening
                onTriggered: mcp.listening ? mcp.stop() : mcp.start()
            }
        }
        Menu {
            title: qsTr("Hilfe")
            MenuItem {
                text: qsTr("Über QTmux")
                icon.source: window.icon("info"); icon.color: Theme.menuIcon; icon.width: 16; icon.height: 16
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
                        id: tile
                        required property int index
                        required property string title
                        required property int runState
                        required property string agentId
                        required property bool needsAttention
                        required property string lastNotification
                        required property bool mcpController
                        width: ListView.view.width
                        height: 48
                        radius: 8
                        color: index === window.currentRow ? Theme.sidebarSelected
                             : hover.hovered ? Theme.sidebarHover : "transparent"

                        // Während des Ziehens angehoben darstellen.
                        z: dragH.active ? 2 : 0
                        opacity: dragH.active ? 0.85 : 1.0
                        scale: dragH.active ? 1.02 : 1.0

                        HoverHandler { id: hover }
                        TapHandler { onTapped: window.assignToActivePane(index) }

                        // Drag-to-Reorder: vertikal ziehen, beim Loslassen Zielzeile
                        // aus der Position berechnen und die Session verschieben.
                        DragHandler {
                            id: dragH
                            target: tile
                            xAxis.enabled: false
                            yAxis.enabled: true
                            onActiveChanged: {
                                if (active) return
                                const slot = tile.height + sessionList.spacing
                                const from = tile.index
                                let ni = Math.round(tile.y / slot)
                                ni = Math.max(0, Math.min(sessions.count - 1, ni))
                                if (ni !== from) window.moveSession(from, ni)
                                tile.y = ni * slot   // ans Ziel-Slot einrasten
                            }
                        }

                        // Roter Tab: diese Session steuert per MCP die anderen (Controller-Agent).
                        Rectangle {
                            visible: mcpController
                            width: 3
                            radius: 1.5
                            color: "#e5534b"
                            anchors.left: parent.left
                            anchors.verticalCenter: parent.verticalCenter
                            height: parent.height - 14
                        }

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 10
                            anchors.rightMargin: 10
                            spacing: 10

                            // Status-Ring. Aufmerksamkeit (blau, pulsierend) hat Vorrang,
                            // sonst: 0=Starting 1=Running 2=WaitingInput 3=Error 4=Closed
                            Rectangle {
                                id: statusRing
                                width: 10; height: 10; radius: 5
                                color: needsAttention ? Theme.accent
                                     : runState === 1 ? "#46d369"
                                     : runState === 2 ? "#f5c451"
                                     : runState === 3 ? "#e5534b"
                                     : runState === 4 ? "#5a5d6a"
                                     : Theme.textDim
                                SequentialAnimation on opacity {
                                    running: needsAttention
                                    loops: Animation.Infinite
                                    alwaysRunToEnd: true
                                    NumberAnimation { to: 0.3; duration: 600 }
                                    NumberAnimation { to: 1.0; duration: 600 }
                                    onStopped: statusRing.opacity = 1.0
                                }
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
                                // Untertitel: Notification (Vorrang) oder erkannter Agent.
                                Text {
                                    visible: lastNotification.length > 0 || agentId.length > 0
                                    text: lastNotification.length > 0
                                          ? lastNotification
                                          : qsTr("Agent: %1").arg(agentId)
                                    color: needsAttention ? Theme.accent : Theme.textDim
                                    font.pixelSize: 10
                                    elide: Text.ElideRight
                                    Layout.fillWidth: true
                                }
                            }

                            // Schließen-Button (×), erscheint bei Hover oder Auswahl.
                            Rectangle {
                                Layout.preferredWidth: 20
                                Layout.preferredHeight: 20
                                radius: 4
                                visible: hover.hovered || index === window.currentRow
                                color: closeHover.hovered ? Theme.border : "transparent"
                                Image {
                                    anchors.centerIn: parent
                                    source: window.icon("x")
                                    sourceSize.width: 12
                                    sourceSize.height: 12
                                    // SVG ist monochrom -> über MultiEffect auf Theme-Farbe tönen.
                                    layer.enabled: true
                                    layer.effect: MultiEffect {
                                        colorization: 1.0
                                        colorizationColor: Theme.textDim
                                    }
                                }
                                HoverHandler { id: closeHover }
                                TapHandler { onTapped: sessions.closeSession(index) }
                            }
                        }
                    }
                }

                // Split-Button: "+ <Typ>" öffnet den gewählten Typ; Caret "▾" wählt den Typ.
                RowLayout {
                    Layout.fillWidth: true
                    Layout.fillHeight: false
                    Layout.preferredHeight: 40
                    Layout.maximumHeight: 40
                    spacing: 1

                    Button {
                        id: newBtn
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        text: window.typeLabel(window.newSessionType)
                        icon.source: window.icon("plus")
                        icon.color: Theme.textBright
                        icon.width: 15; icon.height: 15
                        display: AbstractButton.TextBesideIcon
                        spacing: 6
                        palette.buttonText: Theme.textBright
                        font.pixelSize: 13
                        onClicked: window.openNewSession(window.newSessionType)
                        background: Rectangle {
                            // links abgerundet, rechts eckig (verschmilzt mit dem Caret).
                            radius: 8
                            color: newBtn.down ? Theme.sidebarSelected
                                 : newBtn.hovered ? Theme.sidebarHover : Theme.bgElevated
                            border.color: Theme.border
                            border.width: 1
                            Rectangle {
                                anchors.right: parent.right
                                width: parent.radius; height: parent.height
                                color: parent.color
                                border.color: parent.border.color
                                border.width: parent.border.width
                            }
                        }
                    }

                    Button {
                        id: caretBtn
                        Layout.preferredWidth: 32
                        Layout.fillHeight: true
                        display: AbstractButton.IconOnly
                        icon.source: window.icon("caret-down")
                        icon.color: Theme.textBright
                        icon.width: 14; icon.height: 14
                        onClicked: typeMenu.popup(caretBtn, 0, caretBtn.height)
                        background: Rectangle {
                            // rechts abgerundet, links eckig.
                            radius: 8
                            color: caretBtn.down ? Theme.sidebarSelected
                                 : caretBtn.hovered ? Theme.sidebarHover : Theme.bgElevated
                            border.color: Theme.border
                            border.width: 1
                            Rectangle {
                                anchors.left: parent.left
                                width: parent.radius; height: parent.height
                                color: parent.color
                                border.color: parent.border.color
                                border.width: parent.border.width
                            }
                        }
                    }
                }
            }
        }

        // --- Hauptbereich: Broadcast-Banner + Terminal-Panes (Split-View) ---
        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            // Warn-Banner, solange die Eingabe an alle Sessions geht.
            Rectangle {
                visible: window.broadcastInput
                Layout.fillWidth: true
                implicitHeight: 26
                color: Theme.accent
                Text {
                    anchors.centerIn: parent
                    text: qsTr("⟫ Eingabe geht an ALLE Sessions — Strg/Cmd+Umschalt+B zum Beenden")
                    color: "#ffffff"
                    font.pixelSize: 12
                    font.bold: true
                    elide: Text.ElideRight
                }
            }

        SplitView {
            id: mainSplit
            Layout.fillWidth: true
            Layout.fillHeight: true
            orientation: Qt.Horizontal

            handle: Rectangle {
                implicitWidth: 6
                implicitHeight: 6
                color: SplitHandle.pressed ? Theme.accent
                     : SplitHandle.hovered ? Theme.border : Theme.bgMain
            }

            Repeater {
                id: paneRepeater
                model: paneModel

                delegate: Rectangle {
                    id: pane
                    required property int index
                    required property int sessionRow
                    // Terminal-Zugriff für copy/paste über paneRepeater.itemAt(...).term
                    property alias term: paneTerm

                    SplitView.fillWidth: true
                    SplitView.fillHeight: true
                    SplitView.minimumWidth: 140
                    SplitView.minimumHeight: 90

                    color: Theme.bgMain
                    radius: paneModel.count > 1 ? 6 : 0
                    // Aktives Pane bei Mehrfach-Layout durch Akzentrahmen markieren.
                    border.width: paneModel.count > 1 && index === window.activePane ? 2 : 0
                    border.color: Theme.accent

                    TerminalItem {
                        id: paneTerm
                        anchors.fill: parent
                        anchors.margins: 6
                        pointSize: window.terminalFontSize   // globaler Zoom
                        backgroundColor: Theme.terminalBg
                        foregroundColor: Theme.terminalFg
                        session: pane.sessionRow >= 0 && pane.sessionRow < sessions.count
                                 ? sessions.sessionAt(pane.sessionRow) : null
                        // Broadcast-Modus: Eingabe an ALLE Sessions verteilen.
                        broadcast: window.broadcastInput
                        onInputForBroadcast: (data) => sessions.writeToAll(data)
                        // Fokus (Klick/Tab) macht dieses Pane aktiv.
                        onActiveFocusChanged: if (activeFocus) window.setActivePane(pane.index, paneTerm)
                        // Cmd/Strg+Mausrad -> global zoomen.
                        onZoomRequested: (delta) => window.zoomTerminal(delta)
                        // Rechtsklick -> erst Pane aktivieren, dann Kontextmenü.
                        onContextMenuRequested: {
                            window.setActivePane(pane.index, paneTerm)
                            termContextMenu.popup()
                        }
                    }

                    Component.onCompleted: if (index === window.activePane) {
                        window.activeTerminal = paneTerm
                        paneTerm.forceActiveFocus()
                    }
                }
            }
        }
        }   // ColumnLayout (Banner + SplitView)
    }

    // --- SSH-Verbindung öffnen ---------------------------------------------
    AppDialog {
        id: sshDialog
        width: 420
        title: qsTr("SSH-Verbindung")
        standardButtons: Dialog.Ok | Dialog.Cancel

        onAccepted: {
            if (sshHost.text.length > 0) {
                window.currentRow = sessions.createSshSession(
                    sshHost.text, parseInt(sshPort.text) || 22, sshUser.text, sshIdentity.text)
            }
        }

        ColumnLayout {
            anchors.fill: parent
            spacing: 10
            GridLayout {
                columns: 2
                columnSpacing: 10
                rowSpacing: 8
                Layout.fillWidth: true
                Text { text: qsTr("Host"); color: Theme.textBright }
                TextField { id: sshHost; Layout.fillWidth: true; placeholderText: "example.com" }
                Text { text: qsTr("Benutzer"); color: Theme.textBright }
                TextField { id: sshUser; Layout.fillWidth: true; placeholderText: Qt.platform.os; text: "" }
                Text { text: qsTr("Port"); color: Theme.textBright }
                TextField { id: sshPort; Layout.fillWidth: true; text: "22" }
                Text { text: qsTr("Identity-Datei"); color: Theme.textBright }
                TextField { id: sshIdentity; Layout.fillWidth: true; placeholderText: "~/.ssh/id_ed25519 (optional)" }
            }
            Text {
                text: qsTr("Passwort/Schlüssel werden im Terminal abgefragt (System-ssh).")
                color: Theme.textDim
                font.pixelSize: 11
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
            }
        }
    }

    // --- Serielle Verbindung öffnen ----------------------------------------
    AppDialog {
        id: serialDialog
        width: 420
        title: qsTr("Serielle Verbindung")
        standardButtons: Dialog.Ok | Dialog.Cancel

        function openDialog() {
            portCombo.model = sessions.availableSerialPorts()
            open()
        }
        onAccepted: {
            if (portCombo.currentText.length > 0) {
                window.currentRow = sessions.createSerialSession(
                    portCombo.currentText, parseInt(baudCombo.currentText))
            }
        }

        ColumnLayout {
            anchors.fill: parent
            spacing: 10
            Text { text: qsTr("Port"); color: Theme.textBright }
            AppComboBox {
                id: portCombo
                Layout.fillWidth: true
                model: []
            }
            Text { text: qsTr("Baudrate"); color: Theme.textBright }
            AppComboBox {
                id: baudCombo
                Layout.fillWidth: true
                editable: true
                model: ["9600", "19200", "38400", "57600", "115200", "230400", "460800", "921600"]
                currentIndex: 4
            }
            Text {
                visible: portCombo.model.length === 0
                text: qsTr("Keine seriellen Ports gefunden.")
                color: Theme.textDim
                font.pixelSize: 11
            }
        }
    }

    // --- Über-Dialog --------------------------------------------------------
    AppDialog {
        id: aboutDialog
        width: 420
        title: qsTr("Über QTmux")
        standardButtons: Dialog.Ok
        Label {
            width: 380
            wrapMode: Text.WordWrap
            color: Theme.textBright
            text: qsTr("QTmux — plattformübergreifender Multi-KI-Agenten-Terminal.\nQt %1").arg(Qt.application.version || "0.1")
        }
    }
}
