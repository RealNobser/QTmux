import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QtQuick.Effects
import QtQuick.Dialogs
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

    // --- Split-Panes (rekursiver Baum, QTMUX-3) ------------------------------
    // `layout` ist die Wurzel eines Baums aus Blättern und Splits — beliebig
    // verschachtelbar (H in V usw.). Knotentypen:
    //   Blatt: { paneId: int, sessionRow: int }   (ein Terminal)
    //   Split: { orientation: int, children: [...] }
    // Strukturänderungen (teilen/schließen/reorder) bauen den Baum neu auf
    // (rebuildLayout); der Tastaturfokus wird über `paneItems` (paneId->Term)
    // wiederhergestellt. `currentRow` folgt der Session des aktiven Blatts.
    property var layout: null
    property int nextPaneId: 1
    property int activePaneId: -1
    property int paneCount: 1
    property var paneItems: ({})         // paneId -> TerminalItem (für Fokus + Hit-Test)
    property var activeTerminal: null
    // Pane-Reorder per Drag (QTMUX-4): aktives Quell-Pane + aktuell überfahrenes Ziel.
    property int dragPaneId: -1
    property int dragOverPaneId: -1
    property var dragScenePt: null

    SessionModel { id: sessions }

    // MCP-Server: externe Agenten-Steuerung über 127.0.0.1 (nur lokal).
    McpServer {
        id: mcp
        sessions: sessions
        // Port NICHT hart setzen: McpServer::defaultPort() liest QTMUX_MCP_PORT bzw.
        // die Einstellung mcp/port (Vorgabe 7345). Eine Testinstanz startet damit auf
        // einem eigenen Port, ohne der produktiven Instanz den Port wegzunehmen.
        // Nicht nur currentRow (Sidebar-Markierung) setzen, sondern die Session auch
        // ins aktive Pane laden — sonst zeigt das Terminal nach MCP-create/focus_session
        // weiter die alte Session, bis das Fenster aktiviert wird. assignToActivePane
        // fällt vor dem Layout-Aufbau sauber auf reines currentRow zurück.
        onFocusRequested: (row) => window.assignToActivePane(row)
        onSetThemeRequested: (mode) => Theme.mode = mode

        // --- Layout-/Profil-Steuerung über MCP (QTMUX-29). Die Handler laufen
        // synchron zum Tool-Aufruf und antworten über mcp.provideResult. ---
        // QTMUX-33: Der Baum allein beantwortet die Frage eines Controllers nicht —
        // er sieht nur die Panes, nicht aber, welche seiner Sessions gerade GAR NICHT
        // sichtbar sind (die liegen nur in der Seitenleiste). Deshalb liefern wir den
        // Baum unter "layout" plus eine Sitzungsübersicht mit Pane-Zuordnung.
        onLayoutRequested: {
            const inPane = ({})   // sessionId -> paneId
            function ser(node) {
                if (window.isLeaf(node)) {
                    const s = sessions.sessionAt(node.sessionRow)
                    if (s) inPane[s.sessionId] = node.paneId
                    return { paneId: node.paneId,
                             sessionId: s ? s.sessionId : -1,
                             active: node.paneId === window.activePaneId }
                }
                return { orientation: node.orientation === Qt.Vertical ? "v" : "h",
                         children: node.children.map(ser) }
            }
            if (!window.layout) { mcp.provideResult(false, qsTr("Kein Layout vorhanden.")); return }
            const tree = ser(window.layout)
            const list = []
            for (let i = 0; i < sessions.rowCount(); ++i) {
                const s = sessions.sessionAt(i)
                if (!s) continue
                const pid = inPane[s.sessionId]
                list.push({ sessionId: s.sessionId,
                            title: s.title,
                            // Ohne Pane heißt: läuft weiter, ist aber nicht zu sehen.
                            paneId: pid === undefined ? null : pid,
                            visible: pid !== undefined,
                            active: pid !== undefined && pid === window.activePaneId })
            }
            mcp.provideResult(true, JSON.stringify({ layout: tree,
                                                     activePaneId: window.activePaneId,
                                                     sessions: list }))
        }
        onSplitPaneRequested: (o) => {
            window.splitPane(o === "v" ? Qt.Vertical : Qt.Horizontal)
            const s = sessions.sessionAt(window.currentRow)
            mcp.provideResult(true, String(s ? s.sessionId : -1))
        }
        onClosePaneRequested: (paneId) => {
            if (paneId >= 0) {
                if (!window.findLeaf(paneId)) { mcp.provideResult(false, qsTr("Unbekannte paneId.")); return }
                window.activePaneId = paneId
            }
            window.closePane()
            mcp.provideResult(true, "ok")
        }
        onAssignPaneRequested: (row, paneId) => {
            if (paneId >= 0) {
                if (!window.findLeaf(paneId)) { mcp.provideResult(false, qsTr("Unbekannte paneId.")); return }
                window.activePaneId = paneId
            }
            window.assignToActivePane(row)
            window.focusActivePane()
            mcp.provideResult(true, "ok")
        }
        onConnectProfileRequested: (name) => {
            const p = Profiles.profile(name)
            if (!p || !p.name) { mcp.provideResult(false, qsTr("Unbekanntes Profil.")); return }
            window.connectProfile(p)   // löst ein Vault-Passwort intern auf (wie der GUI-Klick)
            const s = sessions.sessionAt(window.currentRow)
            mcp.provideResult(true, String(s ? s.sessionId : -1))
        }
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
            if (window.layout) {
                // Panes der ENTFERNTEN Sessions schließen (Splits kollabieren) statt
                // sie auf eine überlebende Session umzubiegen — sonst zeigen mehrere
                // Panes dieselbe Session und kämpfen mit verschiedenen Grids um deren
                // resize() (Symptom: alle Panes dieselbe Session, Anzeige verzerrt).
                const emptied = window.pruneLeaves(function(l) {
                    return l.sessionRow >= first && l.sessionRow <= last
                })
                if (emptied)   // ganzer Baum weg -> ein Blatt mit der Folge-Session
                    window.layout = { paneId: window.nextPaneId++,
                                      sessionRow: window.currentRow }
                // Verbliebene Blätter auf die verschobenen Indizes nachführen.
                window.remapLeaves(function(r) { return r > last ? r - removed : r })
                // Das aktive Pane kann mit entfernt worden sein -> neu wählen.
                if (!window.findLeaf(window.activePaneId)) {
                    const fl = window.firstLeaf(window.layout)
                    if (fl) { window.activePaneId = fl.paneId; window.currentRow = fl.sessionRow }
                }
            }
            window.rebuildLayout()
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

    // Menüeintrag mit Kürzel-Anzeige rechts.
    // Bewusst wird das contentItem NICHT ersetzt: das Default-IconLabel des Basic-Styles
    // rendert Icon (getönt via icon.color) + Text korrekt UND treibt die Menübreite
    // zuverlässig (eigene Versuche mit RowLayout/anchor-Item als contentItem führten zu
    // abgeschnittenem/überlappendem Text, weil deren implicitWidth nicht sauber in den
    // QQuickMenu-Breitenalgorithmus propagiert). Stattdessen:
    //   - rightPadding um die Kürzelbreite erhöhen → implicitWidth (= contentItem + Padding)
    //     wächst, das Menü wird breit genug;
    //   - das Kürzel als angedocktes Label in diese reservierte rechte Zone legen
    //     (liegt außerhalb des contentItem → kann den Text nie überlappen).
    // Icon-Farbe folgt damit wieder Theme.menuIcon (app-theme-korrekt, s. Theme::menuIcon).
    component ShortcutMenuItem: MenuItem {
        id: smi
        implicitHeight: 32
        // Explizit gesetztes Kürzel (für Aktionen, deren Shortcut NICHT an der Action hängt,
        // z. B. das im TerminalItem fest verdrahtete Strg+C/Strg+V). Sonst aus der Action.
        property string shortcutOverride: ""
        readonly property string shortcutText: shortcutOverride.length > 0
                                                ? App.shortcutText(shortcutOverride)
                                                : (smi.action ? App.shortcutText(smi.action.shortcut) : "")
        TextMetrics { id: smiTmShort; font.pixelSize: 12; text: smi.shortcutText }
        rightPadding: 16 + (shortcutText.length > 0 ? Math.ceil(smiTmShort.width) + 22 : 0)
        // Eigener, themengebundener Highlight-Hintergrund. WICHTIG: der Basic-Style
        // färbt das Highlight sonst über palette.light/midlight — die in einem Menü-Popup
        // NICHT vom App-Theme stammen (Popups erben die Palette nicht), was im Hell-Modus
        // einen schwarzen Selektionsbalken mit unsichtbarem Text ergab. Daher hier explizit.
        background: Rectangle {
            radius: 6
            color: smi.highlighted ? Theme.sidebarHover : "transparent"
        }
        Label {
            visible: smi.shortcutText.length > 0
            anchors.right: parent.right
            anchors.rightMargin: 14
            anchors.verticalCenter: parent.verticalCenter
            text: smi.shortcutText
            font.pixelSize: 12
            color: Theme.textDim
        }
    }

    // Themen-Menüeintrag (In-Window): app-getöntes Icon, höher. Highlight-Hintergrund
    // und Kürzel-Anzeige kommen von ShortcutMenuItem.
    component AppMenuItem: ShortcutMenuItem {
        id: ami
        implicitHeight: 34
        icon.color: Theme.textBright
        icon.width: 16
        icon.height: 16
    }

    // Themen-Popup-Hintergrund (Menüs/ComboBox): abgerundet, erhoben, gerahmt.
    component AppPopupBg: Rectangle {
        color: Theme.bgElevated
        border.color: Theme.border
        border.width: 1
        radius: 8
    }

    // Themengebundenes Menü für die MenuBar. Qt-Quick-Menü-Popups erben die
    // überschriebene palette des ApplicationWindow NICHT (sie nutzen sonst eine feste,
    // oft dunkle Palette → im Hell-Modus dunkles Menü). Daher hier die Palette explizit
    // ans Theme binden + AppPopupBg-Hintergrund + automatische Breite (sizeMenu).
    component ThemedMenu: Menu {
        padding: 4
        onAboutToShow: window.sizeMenu(this)
        palette.window: Theme.bgElevated
        palette.windowText: Theme.textBright
        palette.text: Theme.textBright
        palette.highlight: Theme.sidebarHover
        palette.highlightedText: Theme.textBright
        background: AppPopupBg {}
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
        // Tastatur-Unterstützung für modale Dialoge:
        //  • ESC = Abbrechen/Schließen — via closePolicy (CloseOnEscape → reject()/close()).
        //  • Enter = OK — über einen Shortcut INNERHALB des Dialogs (im Fokus-Scope des
        //    modalen Popups; ein fensterweiter Shortcut feuert dort NICHT). Greift nur, wenn
        //    der Dialog einen Bestätigen-Button hat. Hat ein einzeiliges TextField den Fokus,
        //    beansprucht es Return selbst (ShortcutOverride) → dann bestätigt dessen
        //    onAccepted; ein mehrzeiliges TextArea behält Enter für Zeilenumbrüche.
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        property bool hasAccept: (standardButtons & Dialog.Ok)
                                 || (standardButtons & Dialog.Save)
                                 || (standardButtons & Dialog.Yes)
        Shortcut {
            sequences: ["Return", "Enter"]
            enabled: dlg.visible && dlg.hasAccept
            onActivated: dlg.accept()
        }
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
        onAboutToShow: window.sizeMenu(this)
        palette.window: Theme.bgElevated
        palette.windowText: Theme.textBright
        palette.text: Theme.textBright
        palette.highlight: Theme.sidebarHover
        palette.highlightedText: Theme.textBright
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
        // Plugin-Backends (QTMUX-8): je geladenem Plugin-Typ ein Eintrag. Anders als
        // Shell/SSH/Seriell wird hier sofort eine Session erzeugt (kein Default-Typ).
        Repeater {
            model: Plugins.backendTypes
            delegate: AppMenuItem {
                required property var modelData
                text: qsTr("%1 (Plugin)").arg(modelData.name)
                icon.source: window.icon("robot")
                onTriggered: window.newPluginSession(modelData.pluginId, modelData.typeId)
            }
        }
    }

    // Kontextmenü des Terminals (Rechtsklick): Kopieren/Einfügen + Pane teilen/schließen.
    Menu {
        id: termContextMenu
        padding: 4
        onAboutToShow: window.sizeMenu(this)
        palette.window: Theme.bgElevated
        palette.windowText: Theme.textBright
        palette.text: Theme.textBright
        palette.highlight: Theme.sidebarHover
        palette.highlightedText: Theme.textBright
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
    // Terminal-Schriftfamilie (leer = Plattform-Standard, beim Start gesetzt) und
    // Programmier-Ligaturen (opt-in). Beide global + persistiert.
    property string terminalFontFamily: ""
    property bool terminalLigatures: false
    // GPU-Glyph-Atlas-Rendering (QTMUX-6). Aus = QPainter-Fallback. Bei aktiven
    // Ligaturen nutzt das TerminalItem ohnehin den Fallback (Run-Shaping nötig).
    property bool terminalGpuRendering: true
    function zoomTerminal(delta) {
        terminalFontSize = Math.max(6, Math.min(40, terminalFontSize + delta))
    }
    function resetTerminalZoom() { terminalFontSize = 13 }

    // Broadcast-/Sync-Input: getippte Eingabe geht an ALLE Sessions (Multi-Agent).
    // Bewusst NICHT persistiert (Footgun) — startet je Sitzung aus.
    property bool broadcastInput: false

    // Quake-Modus (QTMUX-20): globaler Hotkey (Ctrl+`) blendet das Fenster ein/aus,
    // auch wenn QTmux nicht im Vordergrund ist. Persistiert.
    property bool quakeMode: false
    onQuakeModeChanged: QuakeHotkey.setEnabled(quakeMode)
    function toggleQuake() {
        if (window.visible && window.active) {
            window.hide()
        } else {
            window.showNormal()
            window.raise()
            window.requestActivate()
            focusActivePane()
        }
    }
    Connections {
        target: QuakeHotkey
        function onActivated() { window.toggleQuake() }
    }

    // Terminal-Komfortoptionen (PuTTY-Stil), persistiert:
    property bool copyOnSelect: false       // Auswahl automatisch kopieren
    property bool rightClickPaste: false    // Rechtsklick fügt ein (statt Kontextmenü)
    property bool pasteWarnMultiline: true  // Vor mehrzeiligem Einfügen warnen
    property bool confirmQuit: true         // Vor dem Beenden nachfragen (QTMUX-41)

    // Beenden mit Rückfrage (QTMUX-41): Cmd+Q/Alt+F4 reißt sonst alle Sessions
    // samt laufender Prozesse ohne Vorwarnung mit. `quitConfirmed` schaltet die
    // Rückfrage für den bestätigten Durchlauf ab (sonst fragte onClosing erneut).
    property bool quitConfirmed: false
    function requestQuit() {
        if (confirmQuit && !quitConfirmed && sessions.count > 0) {
            quitConfirmDialog.open()
            return
        }
        window.close()
    }

    // --- Sitzungsgruppen (QTMUX-42) ----------------------------------------
    // Die Zuordnung selbst liegt im Model (persistiert, blockweise sortiert);
    // hier nur die Anzeige-Logik: Klappzustand, Farbe, Größe.
    property string collapsedGroupsJson: "[]"   // persistiert (Settings)
    property var collapsedGroups: []

    // groups()/groupSize() sind Model-Funktionen ohne eigenes Änderungssignal.
    // Bindungen darauf würden einfrieren; dieser Zähler ist ihr Anker — er wird
    // in den Bindungen mitgelesen und bei jeder Zuordnungsänderung hochgezählt.
    property int groupsRevision: 0
    Connections {
        target: sessions
        function onGroupsChanged() { window.groupsRevision++ }
    }

    function isGroupCollapsed(name) { return collapsedGroups.indexOf(name) >= 0 }
    function toggleGroupCollapsed(name) {
        const list = collapsedGroups.slice()
        const i = list.indexOf(name)
        if (i >= 0) list.splice(i, 1); else list.push(name)
        collapsedGroups = list
        collapsedGroupsJson = JSON.stringify(list)
    }
    function groupSize(name) { return sessions.groupSize(name) }
    // Farbe deterministisch aus dem Namen ableiten: gleiche Gruppe = gleiche Farbe,
    // über Neustarts hinweg und ohne dass irgendwo eine Zuordnung gepflegt werden muss.
    function groupColor(name) {
        let h = 0
        for (let i = 0; i < name.length; ++i) h = (h * 31 + name.charCodeAt(i)) % 360
        return Qt.hsla(h / 360, 0.55, Theme.dark ? 0.62 : 0.45, 1.0)
    }
    // Zeile, deren Kachel dem übergebenen y-Mittelpunkt am nächsten liegt
    // (`exclude` = die gerade gezogene). Ignoriert eingeklappte Kacheln (Höhe 0).
    function rowNearestTo(cy, exclude) {
        let best = -1, bestD = Number.MAX_VALUE
        for (let i = 0; i < sessions.count; ++i) {
            if (i === exclude) continue
            const it = sessionList.itemAtIndex(i)
            if (!it || it.height <= 0) continue
            const d = Math.abs(it.y + it.height / 2 - cy)
            if (d < bestD) { bestD = d; best = i }
        }
        return best
    }

    // Mehrzeilige Einfügung: das betroffene Terminal merken und nachfragen.
    property var pendingPasteTerm: null
    function askMultilinePaste(term, lines) {
        pendingPasteTerm = term
        pasteWarnDialog.lineCount = lines
        pasteWarnDialog.open()
    }

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
        if (window.layout)
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

    // Neue Plugin-Session (QTMUX-8): Backend kommt vom geladenen Plugin.
    function newPluginSession(pluginId, typeId) {
        const row = sessions.createPluginSession(pluginId, typeId)
        if (row < 0) return   // Plugin/Typ nicht (mehr) verfügbar
        if (window.layout)
            window.assignToActivePane(row)
        else
            window.currentRow = row
    }

    // Startet eine Session aus einem gespeicherten Verbindungsprofil (Connection-
    // Manager, QTMUX-7). `p` ist die Profil-Map aus Profiles.profiles / .profile().
    function connectProfile(p) {
        if (!p || !p.name) return
        var row
        var ls = p.loginScript || ""
        if (p.type === 1) {
            // Passwort aus dem Vault auflösen (nur wenn entsperrt + Geheimnis gesetzt).
            // Das Klartext-Passwort verlässt QML nur flüchtig an die Session.
            var pw = (p.passwordSecret && Vault.unlocked) ? Vault.secret(p.passwordSecret) : ""
            row = sessions.createSshSession(p.host, p.port || 22, p.user, p.identity, ls, pw)
        }
        else if (p.type === 2)
            row = sessions.createSerialSession(p.serialPort, p.baud || 115200, ls)
        else
            row = sessions.createShellSession(p.workingDir || "", p.program || "", ls)
        // Wie bei newSession: ins aktive Pane laden (sofort tippbereit).
        if (window.layout) window.assignToActivePane(row)
        else window.currentRow = row
    }
    // Öffnet den SFTP-Browser für ein SSH-Profil (löst das Vault-Passwort wie beim
    // Verbinden auf). QTMUX-7-Rest: Dateitransfer über System-sftp.
    function openSftp(p) {
        if (!p || p.type !== 1) return
        var pw = (p.passwordSecret && Vault.unlocked) ? Vault.secret(p.passwordSecret) : ""
        sftpDialog.targetLabel = (p.user ? p.user + "@" : "") + (p.host || "")
        sftpDialog.open()
        sftpClient.connectTo(p.host, p.port || 22, p.user, p.identity, pw)
    }
    // Menschliche Größe (B/KB/MB/GB) für die SFTP-Liste.
    function humanSize(n) {
        if (n < 1024) return n + " B"
        var u = ["KB","MB","GB","TB"], i = -1
        do { n /= 1024; i++ } while (n >= 1024 && i < u.length - 1)
        return n.toFixed(1) + " " + u[i]
    }
    // Kurzbeschreibung eines Profils für die Listenanzeige (Ziel/Programm).
    function profileSummary(p) {
        if (!p) return ""
        if (p.type === 1)
            return (p.user ? p.user + "@" : "") + (p.host || "") + (p.port && p.port !== 22 ? ":" + p.port : "")
        if (p.type === 2)
            return (p.serialPort || "") + (p.baud ? " · " + p.baud : "")
        return p.program || qsTr("Standard-Shell")
    }
    // Icon-Name je Profiltyp (Sidebar-/Listen-Icon).
    function profileIcon(t) {
        return t === 1 ? "plugs" : t === 2 ? "usb" : "terminal-window"
    }

    // Stabiler, übersetzbarer Anzeigename je konfigurierbarer Aktion (QTMUX-15) —
    // bewusst eigene Strings (nicht die ggf. dynamischen Action.text, z. B. Theme).
    function hotkeyLabel(id) {
        switch (id) {
        case "actNewSession":     return qsTr("Neue Session")
        case "actCloseSession":   return qsTr("Session schließen")
        case "actClosePane":      return qsTr("Pane schließen")
        case "actNextSession":    return qsTr("Nächste Session")
        case "actPrevSession":    return qsTr("Vorige Session")
        case "actSplitH":         return qsTr("Nebeneinander teilen")
        case "actSplitV":         return qsTr("Untereinander teilen")
        case "actCommandPalette": return qsTr("Befehlspalette")
        case "actBroadcast":      return qsTr("Eingabe an alle Sessions")
        case "actNewSsh":         return qsTr("Neue SSH-Verbindung")
        case "actNewSerial":      return qsTr("Neue serielle Verbindung")
        case "actConnections":    return qsTr("Verbindungen verwalten")
        case "actVault":          return qsTr("Secrets-Vault")
        case "actMcpToggle":      return qsTr("MCP-Server umschalten")
        case "actZoomReset":      return qsTr("Schriftgröße zurücksetzen")
        case "actToggleTheme":    return qsTr("Design umschalten")
        case "actSettings":       return qsTr("Einstellungen")
        case "actAbout":          return qsTr("Über QTmux")
        case "actQuit":           return qsTr("Beenden")
        }
        return id
    }

    // Nächste/vorige Session ins aktive Pane laden (mit Umlauf). dir = +1/-1.
    function cycleSession(dir) {
        const n = sessions.count
        if (n <= 0) return
        let r = (window.currentRow < 0 ? 0 : window.currentRow) + dir
        if (r < 0) r = n - 1
        else if (r >= n) r = 0
        window.assignToActivePane(r)
    }

    // Setzt die Breite eines Menüs explizit auf das Maximum der Item-implicitWidths.
    // Nötig, weil QQuickMenu (Qt 6.11) die contentWidth NICHT zuverlässig als Maximum
    // aller Einträge bestimmt → lange Texte/Kürzel wurden sonst abgeschnitten/überlappten.
    // Wird je Menü an onAboutToShow gehängt; die Einträge sind dann bereits instanziiert.
    function sizeMenu(menu) {
        let w = 0
        for (let i = 0; i < menu.count; ++i) {
            const it = menu.itemAt(i)
            if (it && it.implicitWidth > w) w = it.implicitWidth
        }
        if (w > 0) menu.contentWidth = w
    }

    // --- Pane-Steuerung (Baum-Operationen) -----------------------------------
    // SplitNode greift nur über diese window.*-Helfer auf Modell/Globals zu.

    // Baum-Helfer: rekursiv über alle Blätter; isLeaf = kein `children`.
    function isLeaf(n) { return n && n.children === undefined }
    function forEachLeaf(n, fn) {
        if (!n) return
        if (isLeaf(n)) { fn(n); return }
        for (let i = 0; i < n.children.length; ++i) forEachLeaf(n.children[i], fn)
    }
    function findLeaf(id) {
        // Liefert { leaf, parent, idx } für paneId oder null.
        let res = null
        const walk = function(n, parent, idx) {
            if (isLeaf(n)) { if (n.paneId === id) res = { leaf: n, parent: parent, idx: idx }; return }
            for (let i = 0; i < n.children.length; ++i) walk(n.children[i], n, i)
        }
        walk(window.layout, null, -1)
        return res
    }
    function firstLeaf(n) {
        let f = null
        forEachLeaf(n, function(l) { if (!f) f = l })
        return f
    }
    function findContainerOf(target) {
        // Knoten, dessen children-Array `target` enthält (oder null, wenn Wurzel).
        let res = null
        const walk = function(n) {
            if (!n || isLeaf(n)) return
            for (let i = 0; i < n.children.length; ++i) {
                if (n.children[i] === target) { res = n; return }
                walk(n.children[i])
            }
        }
        walk(window.layout)
        return res
    }
    function collapseSplit(split) {
        // Split mit nur noch einem Kind durch dieses Kind ersetzen.
        if (!split || split.children.length !== 1) return
        const only = split.children[0]
        if (window.layout === split) { window.layout = only; return }
        const cont = findContainerOf(split)
        if (cont) {
            const i = cont.children.indexOf(split)
            if (i >= 0) cont.children[i] = only
        }
    }
    function leafCount() { let c = 0; forEachLeaf(window.layout, function() { ++c }); return c }
    function remapLeaves(fn) {
        forEachLeaf(window.layout, function(l) { l.sessionRow = fn(l.sessionRow) })
    }
    // Entfernt alle Blätter, auf die `pred` zutrifft, aus dem Layout-Baum und
    // kollabiert dabei leere/einelementige Splits. true = Baum wurde komplett leer.
    function pruneLeaves(pred) {
        const prune = function(n) {
            if (isLeaf(n)) return pred(n) ? null : n
            const kept = []
            for (let i = 0; i < n.children.length; ++i) {
                const c = prune(n.children[i])
                if (c) kept.push(c)
            }
            if (kept.length === 0) return null
            if (kept.length === 1) return kept[0]
            n.children = kept
            return n
        }
        window.layout = prune(window.layout)
        return !window.layout
    }

    // Registry Term<->paneId (Fokus nach Rebuild zurückgeben).
    function registerPane(id, term) { window.paneItems[id] = term }
    function unregisterPane(id) { delete window.paneItems[id] }
    function sessionObject(row) {
        return (row >= 0 && row < sessions.count) ? sessions.sessionAt(row) : null
    }
    function broadcastWrite(data) { sessions.writeToAll(data) }
    function popupTermContextMenu(term) { termContextMenu.popup() }

    // Baum neu aufbauen (nach Strukturänderung) + Blattzahl/Fokus aktualisieren.
    function rebuildLayout() {
        window.paneCount = leafCount()
        paneTreeLoader.sourceComponent = null
        paneTreeLoader.sourceComponent = paneTreeComp
        focusActivePane()
    }

    // Aktives Pane setzen (per paneId) und currentRow nachziehen.
    function setActivePaneById(id, term) {
        window.activePaneId = id
        if (term) window.activeTerminal = term
        const f = findLeaf(id)
        if (f && f.leaf.sessionRow >= 0 && f.leaf.sessionRow < sessions.count)
            window.currentRow = f.leaf.sessionRow
    }
    // Fokus (nach Item-Erzeugung) auf das aktive Pane legen.
    function focusActivePane() {
        Qt.callLater(function() {
            const t = window.paneItems[window.activePaneId]
            if (t) { window.activeTerminal = t; t.forceActiveFocus() }
        })
    }

    // Sidebar-Reorder: Session verschieben und alle Index-Referenzen nachführen.
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
        remapLeaves(remap)
        rebuildLayout()
    }

    // Sidebar-Klick: gewählte Session ins aktive Blatt laden.
    function assignToActivePane(row) {
        window.currentRow = row
        const f = findLeaf(window.activePaneId)
        if (f) { f.leaf.sessionRow = row; rebuildLayout() }
        else focusActivePane()
    }

    // Teilen: aktives Blatt durch einen Split [Blatt, neues Blatt] ersetzen.
    // Hat der Eltern-Split bereits dieselbe Orientierung, wird nur ein Geschwister
    // eingefügt — so entstehen saubere verschachtelte H+V-Mischungen (QTMUX-3).
    function splitPane(orientation) {
        const row = sessions.createShellSession("", window.defaultShellProgram)
        const newLeaf = { paneId: window.nextPaneId++, sessionRow: row }
        const f = findLeaf(window.activePaneId)
        if (!f) {                                   // Fallback: ersetze die Wurzel
            window.layout = { orientation: orientation,
                              children: [window.layout, newLeaf] }
        } else if (f.parent && f.parent.orientation === orientation) {
            f.parent.children.splice(f.idx + 1, 0, newLeaf)
        } else {
            const replacement = { orientation: orientation,
                                  children: [f.leaf, newLeaf] }
            if (f.parent) f.parent.children[f.idx] = replacement
            else window.layout = replacement
        }
        window.activePaneId = newLeaf.paneId
        window.currentRow = row
        rebuildLayout()
    }

    // Aktives Pane schließen (letztes Pane -> normale Session-Schließung).
    function closePane() {
        if (leafCount() <= 1) { window.closeCurrent(); return }
        const f = findLeaf(window.activePaneId)
        if (!f || !f.parent) return
        const row = f.leaf.sessionRow
        const parent = f.parent
        parent.children.splice(f.idx, 1)
        collapseSplit(parent)        // Eltern-Split mit nur einem Kind kollabieren
        // Neues aktives Blatt wählen (irgendein verbleibendes).
        const fl = firstLeaf(window.layout)
        window.activePaneId = fl ? fl.paneId : -1
        if (fl) window.currentRow = fl.sessionRow
        sessions.closeSession(row)   // -> onRowsRemoved führt übrige Blätter nach
        rebuildLayout()
    }

    // Pane-Reorder (QTMUX-4): Inhalte (Session) zweier Blätter tauschen.
    function swapPanes(idA, idB) {
        if (idA === idB) return
        const a = findLeaf(idA), b = findLeaf(idB)
        if (!a || !b) return
        const tmp = a.leaf.sessionRow
        a.leaf.sessionRow = b.leaf.sessionRow
        b.leaf.sessionRow = tmp
        window.activePaneId = idB
        window.currentRow = b.leaf.sessionRow
        rebuildLayout()
    }

    // Welches Pane liegt unter einem Szenenpunkt? (für Drag-Reorder-Hit-Test)
    function paneIdAt(pt) {
        if (!pt) return -1
        for (const id in window.paneItems) {
            const t = window.paneItems[id]
            if (!t) continue
            const tl = t.mapToItem(null, 0, 0)   // Szenenkoordinaten der Term-Ecke
            if (pt.x >= tl.x && pt.x < tl.x + t.width &&
                pt.y >= tl.y && pt.y < tl.y + t.height)
                return parseInt(id)
        }
        return -1
    }
    // Drag-Reorder-Lebenszyklus (vom Greifpunkt im Pane-Header gesteuert).
    function beginPaneDrag(id) {
        window.dragPaneId = id
        window.dragOverPaneId = -1
    }
    function updatePaneDrag(scenePt) {
        window.dragScenePt = scenePt
        window.dragOverPaneId = paneIdAt(scenePt)
    }
    function endPaneDrag() {
        const target = paneIdAt(window.dragScenePt)
        const src = window.dragPaneId
        window.dragPaneId = -1
        window.dragOverPaneId = -1
        window.dragScenePt = null
        if (target >= 0 && src >= 0 && target !== src) swapPanes(src, target)
    }

    // Beim Start die persistierten Sessions wiederherstellen; sonst eine neue öffnen.
    Component.onCompleted: {
        if (terminalFontFamily === "") terminalFontFamily = App.defaultMonospaceFont()
        if (quakeMode) QuakeHotkey.setEnabled(true)
        // Eingeklappte Gruppen (QTMUX-42) aus den Einstellungen holen. Als JSON-Text
        // persistiert, weil Settings keine Listen speichert; defekter Inhalt darf den
        // Start nicht verhindern.
        try { window.collapsedGroups = JSON.parse(window.collapsedGroupsJson) || [] }
        catch (e) { window.collapsedGroups = [] }
        const active = sessions.restoreState()
        if (sessions.count === 0)
            newSession()
        else
            currentRow = (active >= 0 && active < sessions.count) ? active : 0
        // Start mit genau einem Blatt, das die aktive Session zeigt.
        window.layout = { paneId: window.nextPaneId++, sessionRow: window.currentRow }
        window.activePaneId = window.layout.paneId
        window.paneCount = 1
    }

    // Wird das Fenster (wieder) aktiv, den Tastaturfokus auf das aktive Pane legen,
    // damit man ohne Klick ins Terminal sofort tippen kann.
    onActiveChanged: if (active) focusActivePane()

    // Beim Schließen erst nachfragen (QTMUX-41), dann den Zustand sichern (braucht
    // laufende Prozesse für das aktuelle Arbeitsverzeichnis) und alle Prozesse/
    // Verbindungen beenden.
    // Dieser Handler ist der ZENTRALE Wächter: Seit Qt 6.5 läuft auch ein
    // Anwendungs-Quit (natives macOS-Menü/Cmd+Q, Qt.quit()) über das Schließen aller
    // Fenster — lehnt eines ab, bricht der Quit ab. Deshalb genügt es NICHT, nur die
    // Beenden-Aktion abzufangen; umgekehrt greift die Rückfrage hier auch für das
    // Fenster-Schließkreuz und Alt+F4.
    onClosing: (close) => {
        if (window.confirmQuit && !window.quitConfirmed && sessions.count > 0) {
            close.accepted = false
            quitConfirmDialog.open()
            return
        }
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
        property alias terminalFontFamily: window.terminalFontFamily
        property alias terminalLigatures: window.terminalLigatures
        property alias terminalGpuRendering: window.terminalGpuRendering
        property alias quakeMode: window.quakeMode
        property alias copyOnSelect: window.copyOnSelect
        property alias rightClickPaste: window.rightClickPaste
        property alias pasteWarnMultiline: window.pasteWarnMultiline
        property alias confirmQuit: window.confirmQuit
        property alias collapsedGroups: window.collapsedGroupsJson
    }

    // --- Zentrale Aktionen: im Menü UND per Shortcut/Button nutzbar ----------
    Action {
        id: actNewSession
        text: qsTr("Neue Session")
        shortcut: Hotkeys.bindings["actNewSession"]
        enabled: !hotkeyCaptureDialog.capturing
        onTriggered: window.newSession()
    }
    Action {
        id: actCloseSession
        text: qsTr("Session schließen")
        shortcut: Hotkeys.bindings["actCloseSession"]
        enabled: window.currentRow >= 0 && !hotkeyCaptureDialog.capturing
        onTriggered: window.closeCurrent()
    }
    Action {
        id: actToggleTheme
        text: Theme.dark ? qsTr("Helles Design") : qsTr("Dunkles Design")
        shortcut: Hotkeys.bindings["actToggleTheme"]
        enabled: !hotkeyCaptureDialog.capturing
        onTriggered: Theme.toggle()
    }
    Action {
        id: actQuit
        text: qsTr("Beenden")
        shortcut: Hotkeys.bindings["actQuit"]
        enabled: !hotkeyCaptureDialog.capturing
        onTriggered: window.requestQuit()
    }
    // Einstellungen-Dialog öffnen (macOS: Cmd+, ; sonst Strg+,).
    Action {
        id: actSettings
        text: qsTr("Einstellungen …")
        // Bewusst KEIN StandardKey.Preferences: macOS verschiebt solche Aktionen ins
        // App-Menü und der In-Window-Shortcut greift dann nicht (Komma lief ins Terminal).
        // „Ctrl+," wird auf macOS zu Cmd+, gemappt — native Optik, aber zuverlässig.
        shortcut: Hotkeys.bindings["actSettings"]
        enabled: !hotkeyCaptureDialog.capturing
        onTriggered: settingsDialog.open()
    }
    // Terminal-Zoom: Schriftgröße global vergrößern/verkleinern/zurücksetzen.
    Action {
        id: actZoomIn
        text: qsTr("Schrift vergrößern")
        shortcut: StandardKey.ZoomIn        // Cmd++/Strg++ (inkl. „=" ohne Shift)
        enabled: !hotkeyCaptureDialog.capturing
        onTriggered: window.zoomTerminal(1)
    }
    Action {
        id: actZoomOut
        text: qsTr("Schrift verkleinern")
        shortcut: StandardKey.ZoomOut        // Cmd+-/Strg+-
        enabled: !hotkeyCaptureDialog.capturing
        onTriggered: window.zoomTerminal(-1)
    }
    Action {
        id: actZoomReset
        text: qsTr("Schriftgröße zurücksetzen")
        shortcut: Hotkeys.bindings["actZoomReset"]
        enabled: !hotkeyCaptureDialog.capturing
        onTriggered: window.resetTerminalZoom()
    }
    // Broadcast-Input umschalten: Eingabe an alle Sessions.
    Action {
        id: actBroadcast
        text: qsTr("Eingabe an alle Sessions")
        shortcut: Hotkeys.bindings["actBroadcast"]
        enabled: !hotkeyCaptureDialog.capturing
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
        enabled: window.activeTerminal && window.activeTerminal.hasSelection && !hotkeyCaptureDialog.capturing
        shortcut: Qt.platform.os === "osx" ? StandardKey.Copy : ""
        onTriggered: if (window.activeTerminal) window.activeTerminal.copy()
    }
    Action {
        id: actPaste
        text: qsTr("Einfügen")
        enabled: !hotkeyCaptureDialog.capturing
        shortcut: Qt.platform.os === "osx" ? StandardKey.Paste : ""
        onTriggered: if (window.activeTerminal) window.activeTerminal.paste()
    }
    // Split-Panes: nebeneinander / untereinander teilen, aktives Pane schließen.
    Action {
        id: actSplitH
        text: qsTr("Nebeneinander teilen")
        shortcut: Hotkeys.bindings["actSplitH"]
        enabled: !hotkeyCaptureDialog.capturing
        onTriggered: window.splitPane(Qt.Horizontal)
    }
    Action {
        id: actSplitV
        text: qsTr("Untereinander teilen")
        shortcut: Hotkeys.bindings["actSplitV"]
        enabled: !hotkeyCaptureDialog.capturing
        onTriggered: window.splitPane(Qt.Vertical)
    }
    Action {
        id: actClosePane
        text: qsTr("Pane schließen")
        shortcut: Hotkeys.bindings["actClosePane"]
        enabled: window.paneCount > 1 && !hotkeyCaptureDialog.capturing
        onTriggered: window.closePane()
    }
    // Befehlspalette: fokussiert das dauerhafte Such-/Befehlsfeld in der Toolbar
    // (öffnet dadurch das Befehls-Popup) und markiert den Inhalt zum Überschreiben.
    Action {
        id: actCommandPalette
        text: qsTr("Befehlspalette …")
        shortcut: Hotkeys.bindings["actCommandPalette"]
        enabled: !hotkeyCaptureDialog.capturing
        // Explizit öffnen (nicht nur über onActiveFocusChanged) — sonst bleibt die
        // Palette tot, wenn das Feld nach einem Befehl noch den Fokus hat.
        onTriggered: { cmdInput.forceActiveFocus(); cmdInput.selectAll(); cmdPopup.openFor() }
    }
    // Session-Navigation (nächste/vorige, mit Umlauf).
    Action {
        id: actNextSession
        text: qsTr("Nächste Session")
        shortcut: Hotkeys.bindings["actNextSession"]
        enabled: sessions.count > 1 && !hotkeyCaptureDialog.capturing
        onTriggered: window.cycleSession(1)
    }
    Action {
        id: actPrevSession
        text: qsTr("Vorige Session")
        shortcut: Hotkeys.bindings["actPrevSession"]
        enabled: sessions.count > 1 && !hotkeyCaptureDialog.capturing
        onTriggered: window.cycleSession(-1)
    }
    // Verbindungs-/Dialog-Aktionen (vorher nur Toolbar/Menü ohne Kürzel).
    Action {
        id: actNewSsh
        text: qsTr("Neue SSH-Verbindung …")
        shortcut: Hotkeys.bindings["actNewSsh"]
        enabled: !hotkeyCaptureDialog.capturing
        onTriggered: sshDialog.open()
    }
    Action {
        id: actNewSerial
        text: qsTr("Neue serielle Verbindung …")
        shortcut: Hotkeys.bindings["actNewSerial"]
        enabled: !hotkeyCaptureDialog.capturing
        onTriggered: serialDialog.openDialog()
    }
    Action {
        id: actConnections
        text: qsTr("Verbindungen verwalten …")
        shortcut: Hotkeys.bindings["actConnections"]
        enabled: !hotkeyCaptureDialog.capturing
        onTriggered: connectionsDialog.open()
    }
    Action {
        id: actVault
        text: qsTr("Secrets-Vault …")
        shortcut: Hotkeys.bindings["actVault"]
        enabled: !hotkeyCaptureDialog.capturing
        onTriggered: vaultDialog.open()
    }
    Action {
        id: actMcpToggle
        text: qsTr("MCP-Server umschalten")
        shortcut: Hotkeys.bindings["actMcpToggle"]
        enabled: !hotkeyCaptureDialog.capturing
        onTriggered: mcp.listening ? mcp.stop() : mcp.start()
    }
    Action {
        id: actAbout
        text: qsTr("Über QTmux")
        shortcut: Hotkeys.bindings["actAbout"]
        enabled: !hotkeyCaptureDialog.capturing
        onTriggered: aboutDialog.open()
    }
    // Direktsprung zu Session 1..9 (feste, nicht konfigurierbare Kürzel — sonst
    // würden 9 Einträge die Kürzel-Liste überladen). Ctrl+<N> lädt Session N.
    Shortcut { sequence: "Ctrl+1"; enabled: !hotkeyCaptureDialog.capturing; onActivated: if (sessions.count > 0) window.assignToActivePane(0) }
    Shortcut { sequence: "Ctrl+2"; enabled: !hotkeyCaptureDialog.capturing; onActivated: if (sessions.count > 1) window.assignToActivePane(1) }
    Shortcut { sequence: "Ctrl+3"; enabled: !hotkeyCaptureDialog.capturing; onActivated: if (sessions.count > 2) window.assignToActivePane(2) }
    Shortcut { sequence: "Ctrl+4"; enabled: !hotkeyCaptureDialog.capturing; onActivated: if (sessions.count > 3) window.assignToActivePane(3) }
    Shortcut { sequence: "Ctrl+5"; enabled: !hotkeyCaptureDialog.capturing; onActivated: if (sessions.count > 4) window.assignToActivePane(4) }
    Shortcut { sequence: "Ctrl+6"; enabled: !hotkeyCaptureDialog.capturing; onActivated: if (sessions.count > 5) window.assignToActivePane(5) }
    Shortcut { sequence: "Ctrl+7"; enabled: !hotkeyCaptureDialog.capturing; onActivated: if (sessions.count > 6) window.assignToActivePane(6) }
    Shortcut { sequence: "Ctrl+8"; enabled: !hotkeyCaptureDialog.capturing; onActivated: if (sessions.count > 7) window.assignToActivePane(7) }
    Shortcut { sequence: "Ctrl+9"; enabled: !hotkeyCaptureDialog.capturing; onActivated: if (sessions.count > 8) window.assignToActivePane(8) }

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
            IconToolButton {
                icon.source: window.icon("bookmark")
                tip: qsTr("Verbindungen verwalten …")
                onClicked: connectionsDialog.open()
            }
            IconToolButton {
                icon.source: window.icon("key")
                active: Vault.unlocked
                tip: qsTr("Secrets-Vault …")
                onClicked: vaultDialog.open()
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

            Item { Layout.fillWidth: true }   // linker Abstandhalter (zentriert das Feld)

            // --- Dauerhaftes Such-/Befehlsfeld (VSCode-Stil) -----------------
            // Immer sichtbar; bei Fokus (Klick oder Strg/Cmd+K) klappt darunter
            // die Befehlsliste (cmdPopup) auf. Tippen filtert, ↑/↓ wählt, Enter führt aus.
            Rectangle {
                id: cmdBar
                Layout.preferredWidth: 340
                Layout.maximumWidth: 340
                Layout.preferredHeight: 28
                Layout.alignment: Qt.AlignVCenter
                radius: 6
                color: Theme.bgMain
                border.color: cmdInput.activeFocus ? Theme.accent : Theme.border
                border.width: 1

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 9
                    anchors.rightMargin: 8
                    spacing: 7

                    // Command-Icon (getönt, folgt Fokus).
                    Item {
                        implicitWidth: 15; implicitHeight: 15
                        Layout.alignment: Qt.AlignVCenter
                        Image {
                            id: cmdBarIco
                            anchors.fill: parent
                            source: window.icon("command")
                            sourceSize.width: 15; sourceSize.height: 15
                            visible: false
                        }
                        MultiEffect {
                            anchors.fill: parent
                            source: cmdBarIco
                            brightness: 1.0   // s. cmdPopup-Delegate: erst weiß, dann colorize
                            colorization: 1.0
                            colorizationColor: cmdInput.activeFocus ? Theme.accent : Theme.textDim
                        }
                    }

                    TextField {
                        id: cmdInput
                        Layout.fillWidth: true
                        Layout.alignment: Qt.AlignVCenter
                        placeholderText: qsTr("Befehl suchen …")
                        font.pixelSize: 12
                        color: Theme.textBright
                        placeholderTextColor: Theme.textDim
                        verticalAlignment: TextInput.AlignVCenter
                        background: null
                        padding: 0
                        // Fokus öffnet das Popup, Fokusverlust schließt es (Klick ins
                        // Terminal/anderswo); Item-Klicks im Popup nehmen keinen Fokus.
                        onActiveFocusChanged: activeFocus ? cmdPopup.openFor() : cmdPopup.close()
                        // Tippen filtert; falls das Popup (nach einem Befehl) zu war, wieder öffnen.
                        onTextChanged: {
                            cmdPopup.applyFilter(text)
                            if (activeFocus && !cmdPopup.visible) cmdPopup.openFor()
                        }
                        Keys.onDownPressed: cmdList.incrementCurrentIndex()
                        Keys.onUpPressed: cmdList.decrementCurrentIndex()
                        Keys.onReturnPressed: cmdPopup.runCurrent()
                        Keys.onEnterPressed: cmdPopup.runCurrent()
                        Keys.onEscapePressed: { cmdPopup.close(); window.focusActivePane() }
                    }

                    // Tastenkürzel-Hinweis.
                    Text {
                        text: Qt.platform.os === "osx" ? "⌘K" : "Ctrl+K"
                        color: Theme.textDim
                        font.pixelSize: 10
                        Layout.alignment: Qt.AlignVCenter
                    }
                }

                // Aufklappende Befehlsliste, unter dem Feld verankert.
                Popup {
                    id: cmdPopup
                    parent: cmdBar
                    y: cmdBar.height + 4
                    x: 0
                    width: 420
                    padding: 4
                    focus: false           // Textfeld behält den Fokus
                    closePolicy: Popup.CloseOnEscape
                    background: AppPopupBg {}

                    property var allCommands: []
                    property var filtered: []

                    // Befehle: feste Aktionen + je offener Session ein „Wechseln zu: …".
                    // Kürzel-Anzeige der Palette: an die LIVE-Belegung (Hotkeys.bindings)
                    // gebunden und nativ formatiert (macOS ⌘-Symbole) — wie in den Menüs.
                    function hk(id) { return App.shortcutText(Hotkeys.bindings[id] || "") }
                    function buildCommands() {
                        var c = [
                            { title: qsTr("Neue Session"),               sub: hk("actNewSession"), icon: "plus",            run: function(){ window.newSession() } },
                            { title: qsTr("Neue SSH-Verbindung …"),      sub: hk("actNewSsh"), icon: "plugs",           run: function(){ sshDialog.open() } },
                            { title: qsTr("Neue serielle Verbindung …"), sub: hk("actNewSerial"), icon: "usb",             run: function(){ serialDialog.openDialog() } },
                            { title: qsTr("Verbindungen verwalten …"),   sub: hk("actConnections"), icon: "bookmark",        run: function(){ connectionsDialog.open() } },
                            { title: qsTr("Secrets-Vault …"),            sub: hk("actVault"), icon: "key",             run: function(){ vaultDialog.open() } },
                            { title: qsTr("Session schließen"),          sub: hk("actCloseSession"), icon: "x",               run: function(){ window.closeCurrent() } },
                            { title: qsTr("Nebeneinander teilen"),       sub: hk("actSplitH"), icon: "split-h",         run: function(){ window.splitPane(Qt.Horizontal) } },
                            { title: qsTr("Untereinander teilen"),       sub: hk("actSplitV"), icon: "split-v",         run: function(){ window.splitPane(Qt.Vertical) } },
                            { title: qsTr("Pane schließen"),             sub: hk("actClosePane"), icon: "x",               run: function(){ window.closePane() } },
                            { title: qsTr("Schrift vergrößern"),         sub: "",             icon: "plus",            run: function(){ window.zoomTerminal(1) } },
                            { title: qsTr("Schrift verkleinern"),        sub: "",             icon: "x",               run: function(){ window.zoomTerminal(-1) } },
                            { title: qsTr("Schriftgröße zurücksetzen"),  sub: hk("actZoomReset"), icon: "gear",            run: function(){ window.resetTerminalZoom() } },
                            { title: qsTr("Eingabe an alle Sessions"),   sub: hk("actBroadcast"), icon: "broadcast-input", run: function(){ window.broadcastInput = !window.broadcastInput } },
                            { title: qsTr("Design umschalten"),          sub: hk("actToggleTheme"), icon: "moon",            run: function(){ Theme.toggle() } },
                            { title: qsTr("Einstellungen …"),            sub: hk("actSettings"), icon: "gear",            run: function(){ settingsDialog.open() } },
                            { title: qsTr("MCP-Server umschalten"),      sub: hk("actMcpToggle"), icon: "broadcast",       run: function(){ mcp.listening ? mcp.stop() : mcp.start() } },
                            { title: qsTr("Kopieren"),                   sub: App.shortcutText("Ctrl+C"), icon: "copy",            run: function(){ if (window.activeTerminal) window.activeTerminal.copy() } },
                            { title: qsTr("Einfügen"),                   sub: App.shortcutText("Ctrl+V"), icon: "clipboard",       run: function(){ if (window.activeTerminal) window.activeTerminal.paste() } },
                            { title: qsTr("Nächste Session"),            sub: hk("actNextSession"), icon: "terminal-window", run: function(){ window.cycleSession(1) } },
                            { title: qsTr("Vorige Session"),             sub: hk("actPrevSession"), icon: "terminal-window", run: function(){ window.cycleSession(-1) } },
                            { title: qsTr("Auswahl automatisch kopieren"), sub: "",           icon: "copy",            run: function(){ window.copyOnSelect = !window.copyOnSelect } },
                            { title: qsTr("Rechtsklick fügt ein"),       sub: "",             icon: "clipboard",       run: function(){ window.rightClickPaste = !window.rightClickPaste } },
                            { title: qsTr("Vor mehrzeiligem Einfügen warnen"), sub: "",       icon: "info",            run: function(){ window.pasteWarnMultiline = !window.pasteWarnMultiline } },
                            { title: qsTr("Design: Wie System"),         sub: "",             icon: "gear",            run: function(){ Theme.mode = Theme.System } },
                            { title: qsTr("Design: Hell"),               sub: "",             icon: "sun",             run: function(){ Theme.mode = Theme.Light } },
                            { title: qsTr("Design: Dunkel"),             sub: "",             icon: "moon",            run: function(){ Theme.mode = Theme.Dark } },
                            { title: qsTr("Sprache: Deutsch"),           sub: "",             icon: "translate",       run: function(){ App.language = "de" } },
                            { title: qsTr("Sprache: English"),           sub: "",             icon: "translate",       run: function(){ App.language = "en" } },
                            { title: qsTr("Über QTmux"),                 sub: hk("actAbout"), icon: "info",            run: function(){ aboutDialog.open() } },
                            { title: qsTr("Beenden"),                    sub: hk("actQuit"), icon: "x",               run: function(){ window.requestQuit() } },
                        ]
                        // Je geladenem Plugin-Backend ein Eintrag (wie im „+"-Menü).
                        var pts = Plugins.backendTypes
                        for (var k = 0; k < pts.length; ++k) {
                            c.push({ title: qsTr("%1 (Plugin)").arg(pts[k].name), sub: qsTr("Neue Plugin-Session"),
                                     icon: "robot",
                                     run: (function(pt){ return function(){ window.newPluginSession(pt.pluginId, pt.typeId) } })(pts[k]) })
                        }
                        // Je gespeichertem Profil ein Schnellverbinden.
                        var profs = Profiles.profiles
                        for (var j = 0; j < profs.length; ++j) {
                            c.push({ title: qsTr("Verbinden: %1").arg(profs[j].name),
                                     sub: window.profileSummary(profs[j]),
                                     icon: window.profileIcon(profs[j].type),
                                     run: (function(p){ return function(){ window.connectProfile(p) } })(profs[j]) })
                        }
                        for (var i = 0; i < sessions.count; ++i) {
                            var s = sessions.sessionAt(i)
                            var t = s ? s.title : qsTr("Session %1").arg(i + 1)
                            c.push({ title: qsTr("Wechseln zu: %1").arg(t), sub: qsTr("Session"),
                                     icon: "terminal-window",
                                     run: (function(row){ return function(){ window.assignToActivePane(row) } })(i) })
                        }
                        return c
                    }

                    function applyFilter(text) {
                        var q = text.trim().toLowerCase()
                        filtered = (q.length === 0)
                            ? allCommands
                            : allCommands.filter(function(cmd){ return cmd.title.toLowerCase().indexOf(q) >= 0 })
                        cmdList.currentIndex = filtered.length > 0 ? 0 : -1
                    }

                    // Bei Fokus öffnen: Befehle frisch zusammenstellen (aktuelle Sessions)
                    // und nach dem aktuellen Feldinhalt filtern.
                    function openFor() {
                        allCommands = buildCommands()
                        applyFilter(cmdInput.text)
                        if (!visible) open()
                    }

                    // Markierten Befehl ausführen: erst schließen + Feld leeren, dann
                    // via Qt.callLater ausführen (damit Folge-Dialoge nicht verdeckt werden).
                    function runCurrent() {
                        if (cmdList.currentIndex < 0 || cmdList.currentIndex >= filtered.length) return
                        var cmd = filtered[cmdList.currentIndex]
                        close()
                        cmdInput.text = ""
                        // Fokus zurück ins Terminal → Feld-Status ist sauber „unfokussiert",
                        // sodass das nächste Cmd+K / der nächste Klick zuverlässig öffnet.
                        window.focusActivePane()
                        Qt.callLater(cmd.run)
                    }

                    contentItem: ListView {
                        id: cmdList
                        implicitHeight: Math.min(contentHeight, 360)
                        clip: true
                        model: cmdPopup.filtered
                        currentIndex: 0
                        ScrollIndicator.vertical: ScrollIndicator {}

                        delegate: Rectangle {
                            id: cmdRow
                            required property var modelData
                            required property int index
                            width: ListView.view.width
                            height: 38
                            radius: 6
                            color: index === cmdList.currentIndex ? Theme.sidebarSelected
                                 : rowHover.hovered ? Theme.sidebarHover : "transparent"
                            HoverHandler { id: rowHover }
                            TapHandler {
                                onTapped: { cmdList.currentIndex = cmdRow.index; cmdPopup.runCurrent() }
                            }
                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 10
                                anchors.rightMargin: 10
                                spacing: 10
                                // Monochromes SVG themegerecht einfärben (explizite
                                // MultiEffect-Form: layer.effect greift im Delegate nicht).
                                Item {
                                    implicitWidth: 16; implicitHeight: 16
                                    Image {
                                        id: cmdIcon
                                        anchors.fill: parent
                                        source: window.icon(cmdRow.modelData.icon)
                                        sourceSize.width: 16; sourceSize.height: 16
                                        visible: false
                                    }
                                    MultiEffect {
                                        anchors.fill: parent
                                        source: cmdIcon
                                        // Schwarzes SVG erst auf Weiß heben (brightness),
                                        // dann colorize → volle Zielhelligkeit (sonst
                                        // gewichtet colorize mit der Quell-Luminanz ~0).
                                        brightness: 1.0
                                        colorization: 1.0
                                        colorizationColor: cmdRow.index === cmdList.currentIndex
                                                           ? Theme.accent : Theme.textBright
                                    }
                                }
                                Text {
                                    text: cmdRow.modelData.title
                                    color: Theme.textBright
                                    font.pixelSize: 13
                                    elide: Text.ElideRight
                                    Layout.fillWidth: true
                                }
                                Text {
                                    text: cmdRow.modelData.sub
                                    visible: cmdRow.modelData.sub.length > 0
                                    color: Theme.textDim
                                    font.pixelSize: 11
                                }
                            }
                        }

                        Text {
                            anchors.centerIn: parent
                            visible: cmdList.count === 0
                            text: qsTr("Keine Treffer")
                            color: Theme.textDim
                            font.pixelSize: 12
                        }
                    }
                }
            }

            Item { Layout.fillWidth: true }   // rechter Abstandhalter (zentriert das Feld)

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
                icon.source: window.icon("gear")
                tip: qsTr("Einstellungen …")
                onClicked: settingsDialog.open()
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
        ThemedMenu {
            title: qsTr("&Datei")
            ShortcutMenuItem { action: actNewSession; icon.source: window.icon("plus"); icon.color: Theme.menuIcon; icon.width: 16; icon.height: 16 }
            ShortcutMenuItem { action: actNewSsh;     icon.source: window.icon("plugs");    icon.color: Theme.menuIcon; icon.width: 16; icon.height: 16 }
            ShortcutMenuItem { action: actNewSerial;  icon.source: window.icon("usb");      icon.color: Theme.menuIcon; icon.width: 16; icon.height: 16 }
            ShortcutMenuItem { action: actConnections; icon.source: window.icon("bookmark"); icon.color: Theme.menuIcon; icon.width: 16; icon.height: 16 }
            ShortcutMenuItem { action: actVault;      icon.source: window.icon("key");      icon.color: Theme.menuIcon; icon.width: 16; icon.height: 16 }
            ShortcutMenuItem { action: actCloseSession; icon.source: window.icon("x"); icon.color: Theme.menuIcon; icon.width: 16; icon.height: 16 }
            MenuSeparator { visible: window.hasShellChoice }
            // Globale Standard-Shell (nur Windows, wo es mehrere gibt). Setzt dieselbe
            // Property wie die Schnellwahl im „+"-Menü → beide bleiben synchron.
            ThemedMenu {
                title: qsTr("Standard-Shell")
                visible: window.hasShellChoice
                Repeater {
                    model: sessions.availableShells()
                    delegate: ShortcutMenuItem {
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
            ShortcutMenuItem { action: actQuit }
        }
        ThemedMenu {
            title: qsTr("&Bearbeiten")
            // Strg+C/Strg+V sind im TerminalItem fest verdrahtet (Smart-Copy: kopiert bei
            // Auswahl, sonst SIGINT) — daher als Override anzeigen. macOS nutzt die
            // StandardKey-Shortcuts der Action (Cmd+C/V).
            ShortcutMenuItem { action: actCopy;  icon.source: window.icon("copy");      icon.color: Theme.menuIcon; icon.width: 16; icon.height: 16
                               shortcutOverride: Qt.platform.os === "osx" ? "" : "Ctrl+C" }
            ShortcutMenuItem { action: actPaste; icon.source: window.icon("clipboard"); icon.color: Theme.menuIcon; icon.width: 16; icon.height: 16
                               shortcutOverride: Qt.platform.os === "osx" ? "" : "Ctrl+V" }
            MenuSeparator {}
            ShortcutMenuItem {
                text: qsTr("Auswahl automatisch kopieren")
                icon.source: window.icon("copy"); icon.color: Theme.menuIcon; icon.width: 16; icon.height: 16
                checkable: true
                checked: window.copyOnSelect
                onTriggered: window.copyOnSelect = !window.copyOnSelect
            }
            ShortcutMenuItem {
                text: qsTr("Rechtsklick fügt ein")
                icon.source: window.icon("clipboard"); icon.color: Theme.menuIcon; icon.width: 16; icon.height: 16
                checkable: true
                checked: window.rightClickPaste
                onTriggered: window.rightClickPaste = !window.rightClickPaste
            }
            ShortcutMenuItem {
                text: qsTr("Vor mehrzeiligem Einfügen warnen")
                icon.source: window.icon("info"); icon.color: Theme.menuIcon; icon.width: 16; icon.height: 16
                checkable: true
                checked: window.pasteWarnMultiline
                onTriggered: window.pasteWarnMultiline = !window.pasteWarnMultiline
            }
        }
        ThemedMenu {
            title: qsTr("&Ansicht")
            ShortcutMenuItem { action: actCommandPalette; icon.source: window.icon("command"); icon.color: Theme.menuIcon; icon.width: 16; icon.height: 16 }
            MenuSeparator {}
            ShortcutMenuItem {
                action: actSplitH
                icon.source: window.icon("split-h"); icon.color: Theme.menuIcon; icon.width: 16; icon.height: 16
            }
            ShortcutMenuItem {
                action: actSplitV
                icon.source: window.icon("split-v"); icon.color: Theme.menuIcon; icon.width: 16; icon.height: 16
            }
            ShortcutMenuItem { action: actClosePane; icon.source: window.icon("x"); icon.color: Theme.menuIcon; icon.width: 16; icon.height: 16 }
            MenuSeparator {}
            ShortcutMenuItem { action: actZoomIn }
            ShortcutMenuItem { action: actZoomOut }
            ShortcutMenuItem { action: actZoomReset }
            MenuSeparator {}
            ShortcutMenuItem {
                action: actBroadcast
                icon.source: window.icon("broadcast-input"); icon.color: Theme.menuIcon; icon.width: 16; icon.height: 16
            }
            MenuSeparator {}
            ShortcutMenuItem { action: actSettings; icon.source: window.icon("gear"); icon.color: Theme.menuIcon; icon.width: 16; icon.height: 16 }
            MenuSeparator {}
            // (Der frühere Umschalt-Eintrag „Helles/Dunkles Design" war redundant zu den
            //  drei expliziten Modus-Einträgen darunter — entfernt. Toggle bleibt per
            //  Toolbar-Knopf und Strg+D erhalten.)
            ShortcutMenuItem {
                text: qsTr("Design: Wie System")
                icon.source: window.icon("gear"); icon.color: Theme.menuIcon; icon.width: 16; icon.height: 16
                checkable: true
                checked: Theme.mode === Theme.System
                onTriggered: Theme.mode = Theme.System
            }
            ShortcutMenuItem {
                text: qsTr("Design: Hell")
                icon.source: window.icon("sun"); icon.color: Theme.menuIcon; icon.width: 16; icon.height: 16
                checkable: true
                checked: Theme.mode === Theme.Light
                onTriggered: Theme.mode = Theme.Light
            }
            ShortcutMenuItem {
                text: qsTr("Design: Dunkel")
                icon.source: window.icon("moon"); icon.color: Theme.menuIcon; icon.width: 16; icon.height: 16
                checkable: true
                checked: Theme.mode === Theme.Dark
                onTriggered: Theme.mode = Theme.Dark
            }
        }
        ThemedMenu {
            title: qsTr("&Sprache")
            Repeater {
                model: App.languageCodes()
                ShortcutMenuItem {
                    required property string modelData
                    text: App.languageName(modelData)
                    icon.source: window.icon("translate"); icon.color: Theme.menuIcon; icon.width: 16; icon.height: 16
                    checkable: true
                    checked: App.language === modelData
                    onTriggered: App.language = modelData
                }
            }
        }
        ThemedMenu {
            title: qsTr("A&gent")
            ShortcutMenuItem {
                text: qsTr("Neue Agent-Session …")
                icon.source: window.icon("robot"); icon.color: Theme.menuIcon; icon.width: 16; icon.height: 16
                onTriggered: window.newSession()
            }
        }
        ThemedMenu {
            title: qsTr("Agent-S&teuerung")
            ShortcutMenuItem {
                text: mcp.listening ? qsTr("MCP-Server: an (127.0.0.1:%1)").arg(mcp.port)
                                    : qsTr("MCP-Server: aus")
                icon.source: window.icon("broadcast"); icon.color: Theme.menuIcon; icon.width: 16; icon.height: 16
                checkable: true
                checked: mcp.listening
                onTriggered: mcp.listening ? mcp.stop() : mcp.start()
            }
        }
        ThemedMenu {
            title: qsTr("&Hilfe")
            ShortcutMenuItem { action: actAbout; icon.source: window.icon("info"); icon.color: Theme.menuIcon; icon.width: 16; icon.height: 16 }
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

                    // Gruppen (QTMUX-42) über ListView-Sections: Das Model hält die
                    // Gruppen als zusammenhängende Blöcke, deshalb genügt hier die
                    // Standard-Section-Mechanik. Leere Gruppe = keine Kopfzeile.
                    section.property: "group"
                    section.criteria: ViewSection.FullString
                    section.delegate: Item {
                        id: groupHeader
                        required property string section
                        width: sessionList.width
                        height: section.length > 0 ? 26 : 0
                        visible: section.length > 0

                        readonly property bool collapsed: window.isGroupCollapsed(section)

                        Rectangle {
                            anchors.fill: parent
                            anchors.topMargin: 4
                            anchors.bottomMargin: 2
                            radius: 6
                            color: hdrHover.hovered ? Theme.sidebarHover : "transparent"

                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 6
                                anchors.rightMargin: 6
                                spacing: 6

                                // Klapp-Pfeil, zugleich Farbmarke der Gruppe.
                                Text {
                                    text: groupHeader.collapsed ? "▸" : "▾"
                                    color: window.groupColor(groupHeader.section)
                                    font.pixelSize: 11
                                }
                                Text {
                                    text: groupHeader.section
                                    color: Theme.textBright
                                    font.pixelSize: 11
                                    font.bold: true
                                    elide: Text.ElideRight
                                    Layout.fillWidth: true
                                }
                                Text {
                                    // groupsRevision wird bewusst mitgelesen: sonst
                                    // bliebe die Zahl beim ersten Wert stehen.
                                    text: (window.groupsRevision, window.groupSize(groupHeader.section))
                                    color: Theme.textDim
                                    font.pixelSize: 10
                                }
                            }
                            HoverHandler { id: hdrHover }
                            TapHandler { onTapped: window.toggleGroupCollapsed(groupHeader.section) }
                            TapHandler {
                                acceptedButtons: Qt.RightButton
                                onTapped: {
                                    groupMenu.groupName = groupHeader.section
                                    groupMenu.popup()
                                }
                            }
                        }
                    }

                    delegate: Rectangle {
                        id: tile
                        required property int index
                        required property string title
                        required property int runState
                        required property string agentId
                        required property bool needsAttention
                        required property string lastNotification
                        required property bool mcpController
                        required property bool progressActive
                        required property int progressState
                        required property int progressValue
                        required property string workingDir
                        required property string group
                        // Eingeklappte Gruppen: Kachel verschwindet, ohne dass das
                        // Model angefasst wird (die Session läuft ja weiter).
                        readonly property bool hidden: group.length > 0 && window.isGroupCollapsed(group)
                        width: ListView.view.width
                        visible: !hidden
                        height: hidden ? 0 : 52
                        radius: 8
                        color: index === window.currentRow ? Theme.sidebarSelected
                             : hover.hovered ? Theme.sidebarHover : "transparent"

                        // Während des Ziehens angehoben darstellen.
                        z: dragH.active ? 2 : 0
                        opacity: dragH.active ? 0.85 : 1.0
                        scale: dragH.active ? 1.02 : 1.0

                        HoverHandler { id: hover }
                        TapHandler { onTapped: window.assignToActivePane(index) }
                        // Rechtsklick: Gruppenzuordnung dieser Session (QTMUX-42).
                        TapHandler {
                            acceptedButtons: Qt.RightButton
                            onTapped: {
                                sessionMenu.row = tile.index
                                sessionMenu.currentGroup = tile.group
                                sessionMenu.groupList = sessions.groups()
                                sessionMenu.popup()
                            }
                        }

                        // Drag-to-Reorder: vertikal ziehen, beim Loslassen die Zielzeile
                        // aus der Position bestimmen und die Session verschieben.
                        DragHandler {
                            id: dragH
                            target: tile
                            xAxis.enabled: false
                            yAxis.enabled: true
                            onActiveChanged: {
                                if (active) return
                                const from = tile.index
                                // Zielzeile über die Layout-Positionen der übrigen Kacheln
                                // suchen statt über eine feste Slot-Höhe: Gruppenköpfe und
                                // eingeklappte (0 px hohe) Kacheln machen die Liste
                                // ungleichmäßig, eine reine Division läge daneben.
                                const ni = window.rowNearestTo(tile.y + tile.height / 2, from)
                                if (ni >= 0 && ni !== from) window.moveSession(from, ni)
                                sessionList.forceLayout()   // Kachel wieder einrasten lassen
                            }
                        }

                        // Farbmarke der Gruppe am linken Rand — macht auf einen Blick
                        // sichtbar, welche Sessions zusammengehören, auch beim Scrollen.
                        Rectangle {
                            visible: tile.group.length > 0 && !tile.mcpController
                            width: 3
                            radius: 1.5
                            color: window.groupColor(tile.group)
                            anchors.left: parent.left
                            anchors.verticalCenter: parent.verticalCenter
                            height: parent.height - 14
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
                                // Arbeitsverzeichnis (klein) — nützlich bei mehreren Agenten.
                                // ElideLeft hält das tiefste (relevanteste) Verzeichnis sichtbar.
                                Text {
                                    visible: workingDir.length > 0
                                    text: workingDir
                                    color: Theme.textDim
                                    font.pixelSize: 9
                                    elide: Text.ElideLeft
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

                        // Fortschrittsbalken (OSC 9;4) am unteren Kachelrand.
                        // State: 1=normal, 2=Fehler (rot), 3=unbestimmt (pulsierend), 4=pausiert (gelb).
                        Rectangle {
                            visible: tile.progressActive
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.bottom: parent.bottom
                            anchors.leftMargin: 10
                            anchors.rightMargin: 10
                            anchors.bottomMargin: 4
                            height: 3
                            radius: 1.5
                            color: Theme.border
                            Rectangle {
                                anchors.left: parent.left
                                height: parent.height
                                radius: parent.radius
                                width: tile.progressState === 3
                                       ? parent.width
                                       : parent.width * Math.max(0, Math.min(100, tile.progressValue)) / 100
                                color: tile.progressState === 2 ? "#e5534b"
                                     : tile.progressState === 4 ? "#f5c451"
                                     : Theme.accent
                                Behavior on width { NumberAnimation { duration: 120 } }
                                SequentialAnimation on opacity {
                                    running: tile.progressActive && tile.progressState === 3
                                    loops: Animation.Infinite
                                    alwaysRunToEnd: true
                                    NumberAnimation { to: 0.3; duration: 700 }
                                    NumberAnimation { to: 1.0; duration: 700 }
                                    onStopped: parent.opacity = 1.0
                                }
                            }
                        }

                        // Aufmerksamkeit: der ganze Tab pulsiert mit blauem Rahmen
                        // (Theme.accent). Rot ist bewusst dem MCP-Controller-Tab
                        // vorbehalten. Ergänzt den ebenfalls pulsierenden Status-Ring;
                        // der Rahmen liegt am Rand und überdeckt keinen Inhalt.
                        Rectangle {
                            anchors.fill: parent
                            radius: parent.radius
                            color: "transparent"
                            visible: needsAttention
                            border.color: Theme.accent
                            border.width: 2
                            SequentialAnimation on opacity {
                                running: needsAttention
                                loops: Animation.Infinite
                                alwaysRunToEnd: true
                                NumberAnimation { to: 0.25; duration: 600 }
                                NumberAnimation { to: 1.0; duration: 600 }
                                onStopped: parent.opacity = 1.0
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

        // Rekursiver Split-Baum (QTMUX-3). Strukturänderungen bauen den Baum über
        // window.rebuildLayout() neu auf (sourceComponent kurz null setzen).
        Loader {
            id: paneTreeLoader
            Layout.fillWidth: true
            Layout.fillHeight: true
            sourceComponent: paneTreeComp
        }
        Component {
            id: paneTreeComp
            SplitNode { node: window.layout; win: window }
        }
        }   // ColumnLayout (Banner + SplitView)
    }

    // --- SSH-Verbindung öffnen ---------------------------------------------
    AppDialog {
        id: sshDialog
        width: 420
        title: qsTr("SSH-Verbindung")
        standardButtons: Dialog.Ok | Dialog.Cancel
        // Erstes Feld fokussieren (statt des OK-Buttons); Enter aus den Feldern bestätigt
        // über deren onAccepted.
        onOpened: sshHost.forceActiveFocus()

        onAccepted: {
            if (sshHost.text.length > 0) {
                window.currentRow = sessions.createSshSession(
                    sshHost.text, parseInt(sshPort.text) || 22, sshUser.text, sshIdentity.text)
            }
        }

        ColumnLayout {
            anchors.fill: parent
            spacing: 10
            // Enter aus einem Eingabefeld bestätigt den Dialog (TextField.onAccepted ist
            // der einzige Weg, der greift, solange ein Feld den Fokus hat — der fokussierte
            // TextField beansprucht Return selbst, sodass weder der Fenster-Shortcut noch
            // ein Eltern-Keys-Handler feuert).
            GridLayout {
                columns: 2
                columnSpacing: 10
                rowSpacing: 8
                Layout.fillWidth: true
                Text { text: qsTr("Host"); color: Theme.textBright }
                TextField { id: sshHost; Layout.fillWidth: true; placeholderText: "example.com"; onAccepted: sshDialog.accept() }
                Text { text: qsTr("Benutzer"); color: Theme.textBright }
                TextField { id: sshUser; Layout.fillWidth: true; placeholderText: Qt.platform.os; text: ""; onAccepted: sshDialog.accept() }
                Text { text: qsTr("Port"); color: Theme.textBright }
                TextField { id: sshPort; Layout.fillWidth: true; text: "22"; onAccepted: sshDialog.accept() }
                Text { text: qsTr("Identity-Datei"); color: Theme.textBright }
                TextField { id: sshIdentity; Layout.fillWidth: true; placeholderText: "~/.ssh/id_ed25519 (optional)"; onAccepted: sshDialog.accept() }
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

    // --- Connection-Manager: gespeicherte Profile (QTMUX-7) -----------------
    AppDialog {
        id: connectionsDialog
        width: 540
        title: qsTr("Verbindungen")
        standardButtons: Dialog.Close

        ColumnLayout {
            anchors.fill: parent
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
                    onClicked: profileEditDialog.openNew()
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
                                source: window.icon(window.profileIcon(modelData.type))
                                sourceSize.width: 18
                                sourceSize.height: 18
                                visible: false
                            }
                            MultiEffect {
                                anchors.fill: parent
                                source: typeImg
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
                                text: window.typeLabel(modelData.type) + " · " + window.profileSummary(modelData)
                                color: Theme.textDim
                                font.pixelSize: 11
                                elide: Text.ElideRight
                                Layout.fillWidth: true
                            }
                        }

                        Button {
                            text: qsTr("Verbinden")
                            onClicked: { window.connectProfile(modelData); connectionsDialog.close() }
                        }
                        Button {
                            text: qsTr("SFTP")
                            visible: modelData.type === 1   // nur SSH-Profile
                            onClicked: { window.openSftp(modelData); connectionsDialog.close() }
                        }
                        IconToolButton {
                            icon.source: window.icon("gear")
                            tip: qsTr("Bearbeiten")
                            onClicked: profileEditDialog.openEdit(modelData)
                        }
                        IconToolButton {
                            icon.source: window.icon("trash")
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

    // --- Profil-Editor (anlegen/bearbeiten) ---------------------------------
    AppDialog {
        id: profileEditDialog
        width: 460
        title: qsTr("Verbindungsprofil")
        standardButtons: Dialog.Ok | Dialog.Cancel
        // Enter bestätigt über den In-Dialog-Shortcut, solange kein Eingabefeld den Fokus
        // hat; das mehrzeilige Login-Skript-Feld behält Enter für Zeilenumbrüche.

        // Ursprungsname: leer = neues Profil; gesetzt = Bearbeiten (Upsert-/Umbenenn-Schlüssel).
        property string originalName: ""
        // Gewähltes Vault-Geheimnis für die SSH-Passwortabfrage (Name, nicht der Wert).
        property string pwSecret: ""

        function openNew() {
            originalName = ""
            pName.text = ""
            pType.currentIndex = 0
            pHost.text = ""; pUser.text = ""; pPort.text = "22"; pIdentity.text = ""
            pProgram.text = ""; pWorkdir.text = ""
            pSerialPort.text = ""; pBaud.editText = "115200"
            pLogin.text = ""
            pwSecret = ""
            open()
        }
        function openEdit(p) {
            originalName = p.name
            pName.text = p.name
            pType.currentIndex = p.type
            pHost.text = p.host || ""; pUser.text = p.user || ""
            pPort.text = (p.port || 22).toString(); pIdentity.text = p.identity || ""
            pProgram.text = p.program || ""; pWorkdir.text = p.workingDir || ""
            pSerialPort.text = p.serialPort || ""
            pBaud.editText = (p.baud || 115200).toString()
            pLogin.text = p.loginScript || ""
            pwSecret = p.passwordSecret || ""
            open()
        }
        onAccepted: {
            var name = pName.text.trim()
            if (name.length === 0) return
            // Beim Umbenennen das alte Profil entfernen (Upsert läuft über den Namen).
            if (originalName.length > 0 && originalName !== name)
                Profiles.removeProfile(originalName)
            Profiles.saveProfile({
                name: name, type: pType.currentIndex,
                host: pHost.text, port: parseInt(pPort.text) || 22,
                user: pUser.text, identity: pIdentity.text,
                passwordSecret: pwSecret,
                program: pProgram.text, workingDir: pWorkdir.text,
                serialPort: pSerialPort.text, baud: parseInt(pBaud.editText) || 115200,
                loginScript: pLogin.text
            })
        }

        ColumnLayout {
            anchors.fill: parent
            spacing: 10

            GridLayout {
                columns: 2
                columnSpacing: 10
                rowSpacing: 8
                Layout.fillWidth: true
                Text { text: qsTr("Name"); color: Theme.textBright }
                TextField { id: pName; Layout.fillWidth: true; placeholderText: qsTr("z. B. Prod-Server") }
                Text { text: qsTr("Typ"); color: Theme.textBright }
                AppComboBox {
                    id: pType
                    Layout.fillWidth: true
                    model: [ qsTr("Shell"), qsTr("SSH"), qsTr("Seriell") ]
                }
            }

            // SSH-Felder.
            GridLayout {
                visible: pType.currentIndex === 1
                columns: 2
                columnSpacing: 10
                rowSpacing: 8
                Layout.fillWidth: true
                Text { text: qsTr("Host"); color: Theme.textBright }
                TextField { id: pHost; Layout.fillWidth: true; placeholderText: "example.com" }
                Text { text: qsTr("Benutzer"); color: Theme.textBright }
                TextField { id: pUser; Layout.fillWidth: true }
                Text { text: qsTr("Port"); color: Theme.textBright }
                TextField { id: pPort; Layout.fillWidth: true; text: "22" }
                Text { text: qsTr("Identity-Datei"); color: Theme.textBright }
                TextField { id: pIdentity; Layout.fillWidth: true; placeholderText: "~/.ssh/id_ed25519 (optional)" }
                Text { text: qsTr("Passwort (Vault)"); color: Theme.textBright }
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2
                    // Auswahl eines Vault-Geheimnisses, dessen Wert bei der SSH-Passwort-
                    // abfrage automatisch gesendet wird. Gespeichert wird nur der Name.
                    AppComboBox {
                        id: pPwSecret
                        Layout.fillWidth: true
                        property var opts: {
                            var o = [ qsTr("(keines)") ]
                            var names = Vault.names
                            for (var i = 0; i < names.length; i++) o.push(names[i])
                            // Bei gesperrtem Vault den gespeicherten Namen trotzdem zeigen.
                            if (profileEditDialog.pwSecret && o.indexOf(profileEditDialog.pwSecret) < 0)
                                o.push(profileEditDialog.pwSecret)
                            return o
                        }
                        model: opts
                        currentIndex: Math.max(0, opts.indexOf(profileEditDialog.pwSecret))
                        onActivated: (i) => profileEditDialog.pwSecret = (i > 0 ? opts[i] : "")
                    }
                    Text {
                        visible: !Vault.unlocked
                        Layout.fillWidth: true
                        text: qsTr("Vault gesperrt – beim Verbinden entsperren, sonst kein Auto-Fill.")
                        color: Theme.textDim; font.pixelSize: 11; wrapMode: Text.WordWrap
                    }
                }
            }

            // Shell-Felder.
            GridLayout {
                visible: pType.currentIndex === 0
                columns: 2
                columnSpacing: 10
                rowSpacing: 8
                Layout.fillWidth: true
                Text { text: qsTr("Programm"); color: Theme.textBright }
                TextField { id: pProgram; Layout.fillWidth: true; placeholderText: qsTr("leer = Standard-Shell") }
                Text { text: qsTr("Arbeitsverzeichnis"); color: Theme.textBright }
                TextField { id: pWorkdir; Layout.fillWidth: true; placeholderText: qsTr("leer = Home") }
            }

            // Seriell-Felder.
            GridLayout {
                visible: pType.currentIndex === 2
                columns: 2
                columnSpacing: 10
                rowSpacing: 8
                Layout.fillWidth: true
                Text { text: qsTr("Port"); color: Theme.textBright }
                TextField { id: pSerialPort; Layout.fillWidth: true; placeholderText: "/dev/tty… · COM3" }
                Text { text: qsTr("Baudrate"); color: Theme.textBright }
                AppComboBox {
                    id: pBaud
                    Layout.fillWidth: true
                    editable: true
                    model: ["9600", "19200", "38400", "57600", "115200", "230400", "460800", "921600"]
                }
            }

            Text {
                visible: pType.currentIndex === 1
                text: qsTr("Passwort/Schlüssel werden im Terminal abgefragt (System-ssh).")
                color: Theme.textDim
                font.pixelSize: 11
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
            }

            // Login-Script: Auto-Befehle nach Verbindungsaufbau (QTMUX-23), eine pro Zeile.
            Text { text: qsTr("Befehle nach Verbindung (eine pro Zeile)"); color: Theme.textBright }
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 84
                radius: 6
                color: Theme.bgElevated
                border.width: 1
                border.color: pLogin.activeFocus ? Theme.accent : Theme.border
                ScrollView {
                    anchors.fill: parent
                    anchors.margins: 6
                    clip: true
                    TextArea {
                        id: pLogin
                        wrapMode: TextArea.NoWrap
                        color: Theme.textBright
                        font.family: window.terminalFontFamily
                        font.pixelSize: 13
                        background: null
                        placeholderText: qsTr("z. B. cd ~/projekt\\nsource .venv/bin/activate")
                        placeholderTextColor: Theme.textDim
                    }
                }
            }
            Text {
                text: qsTr("Werden gesendet, sobald die Shell bereit ist (Shell-Integration: am ersten Prompt, sonst kurz nach Verbindungsaufbau). Geeignet für key-/agent-authentifizierte Verbindungen.")
                color: Theme.textDim
                font.pixelSize: 11
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
            }
        }
    }

    // --- SFTP-Browser (QTMUX-7-Rest) ----------------------------------------
    SftpClient {
        id: sftpClient
        onError: (m) => { sftpDialog.lastError = m }
        onTransferFinished: (ok, m) => { sftpDialog.lastError = ok ? "" : m }
    }
    FolderDialog {
        id: sftpDestDialog
        property string fileName: ""
        title: qsTr("Zielordner für den Download")
        onAccepted: sftpClient.download(fileName, selectedFolder.toString())
    }
    FileDialog {
        id: sftpUploadDialog
        title: qsTr("Datei zum Hochladen")
        onAccepted: sftpClient.upload(selectedFile.toString())
    }
    AppDialog {
        id: sftpDialog
        width: 660
        property string targetLabel: ""
        property string lastError: ""
        title: qsTr("SFTP – %1").arg(targetLabel)
        standardButtons: Dialog.Close
        onClosed: sftpClient.close()

        ColumnLayout {
            anchors.fill: parent
            spacing: 8

            // Navigationsleiste: hoch / Pfad / aktualisieren.
            RowLayout {
                Layout.fillWidth: true
                spacing: 6
                IconToolButton {
                    icon.source: window.icon("caret-down")   // gedreht als „hoch"
                    rotation: 180
                    tip: qsTr("Übergeordnetes Verzeichnis")
                    enabled: sftpClient.connected && !sftpClient.busy
                    onClicked: sftpClient.cdUp()
                }
                Label {
                    Layout.fillWidth: true
                    text: sftpClient.currentPath || "…"
                    color: Theme.textBright
                    font.family: window.terminalFontFamily
                    elide: Text.ElideMiddle
                }
                BusyIndicator {
                    running: sftpClient.busy
                    visible: sftpClient.busy
                    implicitWidth: 20; implicitHeight: 20
                }
                IconToolButton {
                    icon.source: window.icon("plus")
                    rotation: 45                              // „+" gedreht ≈ Refresh-Ersatz
                    tip: qsTr("Aktualisieren")
                    enabled: sftpClient.connected && !sftpClient.busy
                    onClicked: sftpClient.refresh()
                }
            }

            ListView {
                id: sftpList
                Layout.fillWidth: true
                Layout.preferredHeight: 320
                clip: true
                model: sftpClient.entries
                currentIndex: -1
                delegate: Rectangle {
                    width: ListView.view.width
                    height: 30
                    color: sftpList.currentIndex === index ? Theme.selection
                                                           : (hov.hovered ? Theme.hover : "transparent")
                    radius: 4
                    HoverHandler { id: hov }
                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 8
                        anchors.rightMargin: 8
                        spacing: 8
                        Image {
                            source: window.icon(modelData.isDir ? "terminal-window" : "copy")
                            sourceSize.width: 16; sourceSize.height: 16
                            opacity: 0.8
                        }
                        Label {
                            Layout.fillWidth: true
                            text: modelData.name + (modelData.isDir ? "/" : "")
                            color: Theme.textBright
                            elide: Text.ElideRight
                        }
                        Label {
                            visible: !modelData.isDir
                            text: window.humanSize(modelData.size)
                            color: Theme.textDim
                            font.pixelSize: 11
                        }
                    }
                    TapHandler {
                        onTapped: sftpList.currentIndex = index
                        onDoubleTapped: {
                            if (modelData.isDir) sftpClient.cd(modelData.name)
                        }
                    }
                }
            }

            // Aktionen + Status.
            RowLayout {
                Layout.fillWidth: true
                spacing: 6
                Button {
                    text: qsTr("Herunterladen")
                    enabled: sftpClient.connected && !sftpClient.busy
                             && sftpList.currentIndex >= 0
                             && !(sftpClient.entries[sftpList.currentIndex]
                                  && sftpClient.entries[sftpList.currentIndex].isDir)
                    onClicked: {
                        sftpDestDialog.fileName = sftpClient.entries[sftpList.currentIndex].name
                        sftpDestDialog.open()
                    }
                }
                Button {
                    text: qsTr("Hochladen …")
                    enabled: sftpClient.connected && !sftpClient.busy
                    onClicked: sftpUploadDialog.open()
                }
                Item { Layout.fillWidth: true }
                Label {
                    text: sftpDialog.lastError !== "" ? sftpDialog.lastError : sftpClient.status
                    color: sftpDialog.lastError !== "" ? "#e5534b" : Theme.textDim
                    font.pixelSize: 11
                    elide: Text.ElideRight
                    Layout.maximumWidth: 320
                }
            }
        }
    }

    // --- Secrets-Vault (QTMUX-22) -------------------------------------------
    AppDialog {
        id: vaultDialog
        width: 540
        title: qsTr("Secrets-Vault")
        standardButtons: Dialog.Close
        property string vaultError: ""
        // Beim Öffnen das Passwortfeld fokussieren, damit man sofort tippen kann
        // (kein Klick nötig) — nur sinnvoll im gesperrten Zustand.
        onOpened: {
            vaultError = ""; vpw.text = ""; vpwConfirm.text = ""
            if (!Vault.unlocked) vpw.forceActiveFocus()
        }

        ColumnLayout {
            anchors.fill: parent
            spacing: 10

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
                visible: vaultDialog.vaultError.length > 0
                text: vaultDialog.vaultError
                color: "#e0a040"; font.pixelSize: 11; Layout.fillWidth: true; wrapMode: Text.WordWrap
            }
            Button {
                id: vaultPrimaryBtn
                text: Vault.exists ? qsTr("Entsperren") : qsTr("Vault anlegen")
                onClicked: {
                    vaultDialog.vaultError = ""
                    if (Vault.exists) {
                        if (!Vault.unlock(vpw.text))
                            vaultDialog.vaultError = qsTr("Falsches Master-Passwort.")
                    } else if (vpw.text.length === 0) {
                        vaultDialog.vaultError = qsTr("Bitte ein Passwort eingeben.")
                    } else if (vpw.text !== vpwConfirm.text) {
                        vaultDialog.vaultError = qsTr("Die Passwörter stimmen nicht überein.")
                    } else if (!Vault.create(vpw.text)) {
                        vaultDialog.vaultError = qsTr("Der Vault konnte nicht angelegt werden.")
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
                Button { text: qsTr("Hinzufügen"); onClicked: secretEditDialog.openNew() }
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
                                color: Theme.textDim; font.pixelSize: 11; font.family: window.terminalFontFamily
                                elide: Text.ElideRight; Layout.fillWidth: true
                            }
                        }
                        IconToolButton {
                            icon.source: window.icon("eye")
                            tip: secretRow.revealed ? qsTr("Verbergen") : qsTr("Anzeigen")
                            active: secretRow.revealed
                            onClicked: secretRow.revealed = !secretRow.revealed
                        }
                        IconToolButton { icon.source: window.icon("copy"); tip: qsTr("In Zwischenablage kopieren"); onClicked: App.copyToClipboard(Vault.secret(secretRow.modelData)) }
                        IconToolButton { icon.source: window.icon("gear"); tip: qsTr("Bearbeiten"); onClicked: secretEditDialog.openEdit(secretRow.modelData) }
                        IconToolButton { icon.source: window.icon("trash"); tip: qsTr("Löschen"); onClicked: Vault.removeSecret(secretRow.modelData) }
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
                Button { text: qsTr("Master-Passwort ändern …"); onClicked: vaultChangePwDialog.open() }
                Item { Layout.fillWidth: true }
                Button { text: qsTr("Sperren"); onClicked: Vault.lock() }
            }
        }
        }
    }

    // --- Vault: Geheimnis anlegen/bearbeiten --------------------------------
    AppDialog {
        id: secretEditDialog
        width: 440
        title: qsTr("Geheimnis")
        standardButtons: Dialog.Ok | Dialog.Cancel
        property bool editing: false
        function openNew() { editing = false; sName.text = ""; sValue.text = ""; sReveal.checked = false; open() }
        function openEdit(name) { editing = true; sName.text = name; sValue.text = Vault.secret(name); sReveal.checked = false; open() }
        onOpened: (editing ? sValue : sName).forceActiveFocus()
        onAccepted: if (sName.text.trim().length > 0) Vault.setSecret(sName.text.trim(), sValue.text)
        ColumnLayout {
            anchors.fill: parent
            spacing: 10
            GridLayout {
                columns: 2; columnSpacing: 10; rowSpacing: 8; Layout.fillWidth: true
                Text { text: qsTr("Name"); color: Theme.textBright }
                TextField { id: sName; Layout.fillWidth: true; readOnly: secretEditDialog.editing; placeholderText: qsTr("z. B. ssh/prod"); onAccepted: secretEditDialog.accept() }
                Text { text: qsTr("Wert"); color: Theme.textBright }
                TextField { id: sValue; Layout.fillWidth: true; echoMode: sReveal.checked ? TextInput.Normal : TextInput.Password; placeholderText: qsTr("Passwort / Token / Passphrase"); onAccepted: secretEditDialog.accept() }
            }
            CheckBox { id: sReveal; text: qsTr("Wert anzeigen") }
        }
    }

    // --- Vault: Master-Passwort ändern --------------------------------------
    AppDialog {
        id: vaultChangePwDialog
        width: 420
        title: qsTr("Master-Passwort ändern")
        standardButtons: Dialog.Cancel
        property string err: ""
        onOpened: { cpOld.text = ""; cpNew.text = ""; cpConfirm.text = ""; err = ""; cpOld.forceActiveFocus() }
        ColumnLayout {
            anchors.fill: parent
            spacing: 8
            TextField { id: cpOld; Layout.fillWidth: true; echoMode: TextInput.Password; placeholderText: qsTr("Aktuelles Master-Passwort") }
            TextField { id: cpNew; Layout.fillWidth: true; echoMode: TextInput.Password; placeholderText: qsTr("Neues Master-Passwort") }
            TextField { id: cpConfirm; Layout.fillWidth: true; echoMode: TextInput.Password; placeholderText: qsTr("Neues Passwort bestätigen") }
            Text { visible: vaultChangePwDialog.err.length > 0; text: vaultChangePwDialog.err; color: "#e0a040"; font.pixelSize: 11; Layout.fillWidth: true; wrapMode: Text.WordWrap }
            Button {
                text: qsTr("Ändern")
                onClicked: {
                    vaultChangePwDialog.err = ""
                    if (cpNew.text.length === 0) vaultChangePwDialog.err = qsTr("Bitte ein neues Passwort eingeben.")
                    else if (cpNew.text !== cpConfirm.text) vaultChangePwDialog.err = qsTr("Die neuen Passwörter stimmen nicht überein.")
                    else if (!Vault.changeMasterPassword(cpOld.text, cpNew.text)) vaultChangePwDialog.err = qsTr("Das aktuelle Master-Passwort ist falsch.")
                    else vaultChangePwDialog.close()
                }
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
            // Qt.application.version ist die APP-Version (setApplicationVersion) —
            // nicht die Qt-Bibliotheksversion; Label entsprechend "Version".
            text: qsTr("QTmux — plattformübergreifender Multi-KI-Agenten-Terminal.\nVersion %1").arg(Qt.application.version || "1.0")
        }
    }

    // --- Farbschema importieren (iTerm .itermcolors / Xresources / Ghostty) -
    FileDialog {
        id: schemeFileDialog
        title: qsTr("Farbschema importieren")
        nameFilters: [ qsTr("Farbschemata (*.itermcolors *.Xresources *.conf *.txt)"),
                       qsTr("Alle Dateien (*)") ]
        onAccepted: {
            const name = ColorSchemes.importFile(selectedFile)
            if (name.length === 0) schemeImportError.open()
        }
    }
    AppDialog {
        id: schemeImportError
        width: 380
        title: qsTr("Import fehlgeschlagen")
        standardButtons: Dialog.Ok
        Label {
            width: 340
            wrapMode: Text.WordWrap
            color: Theme.textBright
            text: qsTr("Die Datei konnte nicht als Farbschema gelesen werden (unterstützt: iTerm .itermcolors, Xresources, Ghostty).")
        }
    }

    // --- Mehrzeilige-Einfügung-Warnung -------------------------------------
    AppDialog {
        id: pasteWarnDialog
        width: 420
        property int lineCount: 0
        title: qsTr("Mehrzeilig einfügen?")
        standardButtons: Dialog.Ok | Dialog.Cancel
        onAccepted: if (window.pendingPasteTerm) window.pendingPasteTerm.confirmPaste()
        onRejected: if (window.pendingPasteTerm) window.pendingPasteTerm.cancelPaste()
        Label {
            width: 380
            wrapMode: Text.WordWrap
            color: Theme.textBright
            text: qsTr("Der Inhalt der Zwischenablage hat %1 Zeilen und könnte mehrere Befehle ausführen. Trotzdem einfügen?").arg(pasteWarnDialog.lineCount)
        }
    }

    // --- Gruppen-Kontextmenüs (QTMUX-42) ------------------------------------
    // Rechtsklick auf eine Kachel: Gruppe zuordnen, wechseln oder verlassen.
    ThemedMenu {
        id: sessionMenu
        property int row: -1
        property string currentGroup: ""

        MenuItem {
            enabled: false
            height: 26
            contentItem: Text {
                text: qsTr("Gruppe")
                color: Theme.textDim
                font.pixelSize: 10
                font.bold: true
                verticalAlignment: Text.AlignVCenter
                leftPadding: 8
            }
        }
        // Bestehende Gruppen als Ziele — beim Öffnen frisch geholt (s. popup unten).
        property var groupList: []
        Repeater {
            model: sessionMenu.groupList
            delegate: AppMenuItem {
                required property string modelData
                text: modelData
                checkable: true
                checked: modelData === sessionMenu.currentGroup
                onTriggered: sessions.setSessionGroup(sessionMenu.row, modelData)
            }
        }
        AppMenuItem {
            text: qsTr("Neue Gruppe …")
            icon.source: window.icon("plus"); icon.color: Theme.menuIcon
            onTriggered: groupNameDialog.start(sessionMenu.row)
        }
        AppMenuItem {
            text: qsTr("Aus Gruppe entfernen")
            enabled: sessionMenu.currentGroup.length > 0
            icon.source: window.icon("x"); icon.color: Theme.menuIcon
            onTriggered: sessions.setSessionGroup(sessionMenu.row, "")
        }
    }

    // Rechtsklick auf eine Gruppen-Kopfzeile: umbenennen oder auflösen.
    ThemedMenu {
        id: groupMenu
        property string groupName: ""
        AppMenuItem {
            text: qsTr("Gruppe umbenennen …")
            onTriggered: groupNameDialog.startRename(groupMenu.groupName)
        }
        AppMenuItem {
            text: qsTr("Gruppe auflösen")
            icon.source: window.icon("x"); icon.color: Theme.menuIcon
            // Auflösen betrifft nur die Zuordnung — die Sessions laufen weiter.
            onTriggered: sessions.renameGroup(groupMenu.groupName, "")
        }
    }

    // Name für eine neue Gruppe bzw. für das Umbenennen einer bestehenden.
    AppDialog {
        id: groupNameDialog
        width: 380
        property int row: -1          // >= 0: Session dieser Zeile zuordnen
        property string renaming: ""  // nicht leer: bestehende Gruppe umbenennen
        title: renaming.length > 0 ? qsTr("Gruppe umbenennen") : qsTr("Neue Gruppe")
        standardButtons: Dialog.Ok | Dialog.Cancel

        function start(r) { row = r; renaming = ""; groupNameField.text = ""; open() }
        function startRename(name) { row = -1; renaming = name; groupNameField.text = name; open() }

        onOpened: { groupNameField.forceActiveFocus(); groupNameField.selectAll() }
        onAccepted: {
            const name = groupNameField.text.trim()
            if (name.length === 0) return
            if (renaming.length > 0) sessions.renameGroup(renaming, name)
            else if (row >= 0) sessions.setSessionGroup(row, name)
        }
        ColumnLayout {
            spacing: 8
            Label {
                Layout.preferredWidth: 340
                wrapMode: Text.WordWrap
                color: Theme.textDim
                font.pixelSize: 11
                text: qsTr("Sitzungen einer Gruppe stehen in der Seitenleiste zusammen und lassen sich gemeinsam ein- und ausklappen.")
            }
            TextField {
                id: groupNameField
                Layout.preferredWidth: 340
                placeholderText: qsTr("z. B. Release 1.5")
                onAccepted: groupNameDialog.accept()
            }
        }
    }

    // --- Beenden bestätigen (QTMUX-41) --------------------------------------
    // Bewusst mit Aufzählung der offenen Sitzungen: Wer mehrere Agenten laufen
    // hat, sieht so, was er gerade mitreißen würde. Abschaltbar in den
    // Einstellungen (Abschnitt „Fenster").
    AppDialog {
        id: quitConfirmDialog
        width: 420
        title: qsTr("QTmux beenden?")
        standardButtons: Dialog.Ok | Dialog.Cancel
        // `quitConfirmed` verhindert, dass der onClosing-Wächter gleich noch einmal fragt.
        onAccepted: { window.quitConfirmed = true; window.close() }
        ColumnLayout {
            spacing: 8
            Label {
                Layout.preferredWidth: 380
                wrapMode: Text.WordWrap
                color: Theme.textBright
                // Bewusst ohne Zahl im Satz: Singular/Plural wäre in jeder Sprache
                // anders zu bilden — die Anzahl zeigt die Aufzählung darunter.
                text: qsTr("Beim Beenden werden alle offenen Sitzungen samt ihrer laufenden Prozesse und Verbindungen geschlossen.")
            }
            Label {
                text: qsTr("Offene Sitzungen:")
                color: Theme.textBright
                font.pixelSize: 12
                font.bold: true
            }
            ColumnLayout {
                spacing: 2
                Layout.leftMargin: 4
                Repeater {
                    model: sessions
                    delegate: Label {
                        required property int index
                        required property string title
                        visible: index < 8
                        Layout.preferredWidth: 376
                        elide: Label.ElideRight
                        color: Theme.textDim
                        font.pixelSize: 12
                        text: "• " + title
                    }
                }
                Label {
                    visible: sessions.count > 8
                    color: Theme.textDim
                    font.pixelSize: 12
                    text: qsTr("… und %1 weitere").arg(Math.max(0, sessions.count - 8))
                }
            }
        }
    }

    // --- Tastenkürzel aufnehmen (QTMUX-15) ----------------------------------
    // Modaler Aufnahme-Dialog: erfasst gedrückte Tasten als Akkord(e). Solange er
    // offen ist (`capturing`), sind alle App-Shortcuts deaktiviert, damit die Tasten
    // nicht versehentlich eine Aktion auslösen statt aufgenommen zu werden.
    AppDialog {
        id: hotkeyCaptureDialog
        width: 420
        title: qsTr("Tastenkürzel ändern")
        standardButtons: Dialog.Ok | Dialog.Cancel
        // Enter wird vom Aufnahme-Feld selbst behandelt (onOpened fokussiert es, nicht den
        // OK-Button): bestätigt die Aufnahme bzw. ist als Akkord mit Modifier aufnehmbar.

        property string targetId: ""
        property string targetLabel: ""
        property var chords: []
        property bool capturing: false
        readonly property string seqStr: chords.join(", ")
        readonly property string conflictId: seqStr.length > 0 ? Hotkeys.conflict(seqStr, targetId) : ""

        function start(id, label) {
            targetId = id
            targetLabel = label
            chords = []
            capturing = true
            open()
        }
        onOpened: captureArea.forceActiveFocus()
        onClosed: capturing = false
        onAccepted: if (chords.length > 0) Hotkeys.setBinding(targetId, seqStr)

        ColumnLayout {
            anchors.fill: parent
            spacing: 10

            Text {
                text: qsTr("Neues Kürzel für „%1“").arg(hotkeyCaptureDialog.targetLabel)
                color: Theme.textBright
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
            }

            Rectangle {
                Layout.fillWidth: true
                implicitHeight: 46
                radius: 6
                color: Theme.bgElevated
                border.width: 1
                border.color: captureArea.activeFocus ? Theme.accent : Theme.border

                Text {
                    anchors.centerIn: parent
                    text: hotkeyCaptureDialog.seqStr.length > 0
                          ? hotkeyCaptureDialog.seqStr : qsTr("Tasten drücken …")
                    color: hotkeyCaptureDialog.seqStr.length > 0 ? Theme.textBright : Theme.textDim
                    font.pixelSize: 15
                    font.bold: hotkeyCaptureDialog.seqStr.length > 0
                }

                // Unsichtbarer Fokus-Empfänger für die Tastenaufnahme.
                Item {
                    id: captureArea
                    anchors.fill: parent
                    focus: true
                    Keys.onPressed: (event) => {
                        // Esc bricht ab, Enter bestätigt (jeweils ohne Modifier) —
                        // mit Modifier sind sie als Kürzel aufnehmbar.
                        if (event.key === Qt.Key_Escape && event.modifiers === Qt.NoModifier) {
                            event.accepted = true; hotkeyCaptureDialog.reject(); return
                        }
                        if ((event.key === Qt.Key_Return || event.key === Qt.Key_Enter)
                                && event.modifiers === Qt.NoModifier) {
                            event.accepted = true; hotkeyCaptureDialog.accept(); return
                        }
                        var c = App.keyChord(event.key, event.modifiers)
                        if (c.length === 0) return            // reine Modifier-Taste
                        event.accepted = true
                        var arr = hotkeyCaptureDialog.chords.slice()
                        arr.push(c)
                        if (arr.length > 4) arr = [c]         // QKeySequence: max. 4 Akkorde
                        hotkeyCaptureDialog.chords = arr
                    }
                }
                MouseArea { anchors.fill: parent; onClicked: captureArea.forceActiveFocus() }
            }

            Text {
                visible: hotkeyCaptureDialog.conflictId.length > 0
                text: qsTr("Bereits belegt von: %1").arg(window.hotkeyLabel(hotkeyCaptureDialog.conflictId))
                color: "#e0a040"   // Warn-Amber (kein Theme-Token vorhanden)
                font.pixelSize: 11
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
            }

            RowLayout {
                Layout.fillWidth: true
                Button {
                    text: qsTr("Leeren")
                    enabled: hotkeyCaptureDialog.chords.length > 0
                    onClicked: hotkeyCaptureDialog.chords = []
                }
                Item { Layout.fillWidth: true }
                Button {
                    text: qsTr("Auf Standard")
                    onClicked: { Hotkeys.reset(hotkeyCaptureDialog.targetId); hotkeyCaptureDialog.close() }
                }
            }

            Text {
                text: qsTr("Mehrere nacheinander gedrückte Akkorde ergeben eine Tastenfolge (max. 4). Esc bricht ab, Eingabe bestätigt.")
                color: Theme.textDim
                font.pixelSize: 11
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
            }
        }
    }

    // --- Einstellungen ------------------------------------------------------
    // Bündelt die persistierten Optionen; Änderungen wirken sofort (Zwei-Wege-Bindung
    // an die window-Properties / Theme / App-Singletons).
    AppDialog {
        id: settingsDialog
        width: 480
        // Höhe begrenzen + Inhalt scrollbar — sonst wächst der Dialog (viele Abschnitte,
        // u. a. die Tastenkürzel-Liste) über den Bildschirm hinaus.
        contentHeight: Math.min(settingsCol.implicitHeight, window.height - 180)
        title: qsTr("Einstellungen")
        standardButtons: Dialog.Close

        // Agenten-Benachrichtigungs-Abos (reaktiv aus dem Hub gespiegelt).
        property var agentSubs: AgentEvents.subscriptions()
        Connections {
            target: AgentEvents
            function onSubscriptionsChanged() { settingsDialog.agentSubs = AgentEvents.subscriptions() }
        }
        function agentSubFor(id) {
            for (let i = 0; i < agentSubs.length; ++i)
                if (agentSubs[i].subscriberSessionId === id) return agentSubs[i]
            return null
        }
        // Arten-Checkbox-Status: kein Abo → aus; leere Arten-Liste → „alle" (alle an).
        function agentKindChecked(id, kind) {
            const s = agentSubFor(id)
            if (!s) return false
            return s.kinds.length === 0 || s.kinds.indexOf(kind) >= 0
        }

        // Abschnittsüberschrift im Dialog.
        component SectionLabel: Text {
            color: Theme.textDim
            font.pixelSize: 11
            font.bold: true
            Layout.topMargin: 6
        }

        Flickable {
            anchors.fill: parent
            contentWidth: width
            contentHeight: settingsCol.implicitHeight
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            ScrollIndicator.vertical: ScrollIndicator {}

        ColumnLayout {
            id: settingsCol
            width: parent.width
            spacing: 12

            SectionLabel { text: qsTr("Erscheinungsbild") }
            GridLayout {
                columns: 2; columnSpacing: 12; rowSpacing: 8; Layout.fillWidth: true
                Text { text: qsTr("Design"); color: Theme.textBright }
                AppComboBox {
                    Layout.fillWidth: true
                    model: [qsTr("Wie System"), qsTr("Hell"), qsTr("Dunkel")]
                    currentIndex: Theme.mode
                    onActivated: (i) => Theme.mode = i
                }
                Text { text: qsTr("Sprache"); color: Theme.textBright }
                AppComboBox {
                    Layout.fillWidth: true
                    textRole: "name"
                    model: App.languageCodes().map(c => ({ code: c, name: App.languageName(c) }))
                    currentIndex: Math.max(0, App.languageCodes().indexOf(App.language))
                    onActivated: (i) => App.language = App.languageCodes()[i]
                }

                // Terminal-Farbschema (QTMUX-18): je ein Schema für Dunkel und Hell.
                // Das je nach Modus aktive Schema färbt die GANZE App. Import
                // (iTerm/Xresources/Ghostty) landet im passenden Slot.
                Text { text: qsTr("Farbschema (Dunkel)"); color: Theme.textBright }
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 6
                    AppComboBox {
                        Layout.fillWidth: true
                        model: ColorSchemes.names
                        currentIndex: Math.max(0, ColorSchemes.names.indexOf(ColorSchemes.darkScheme))
                        onActivated: (i) => ColorSchemes.darkScheme = ColorSchemes.names[i]
                    }
                    Button {
                        text: qsTr("Importieren …")
                        font.pixelSize: 12
                        onClicked: schemeFileDialog.open()
                    }
                }
                Item { width: 1; height: 1 }   // Spalte 1 leer
                Row {
                    Layout.fillWidth: true
                    spacing: 3
                    property var sc: ColorSchemes.colors(ColorSchemes.darkScheme)
                    Repeater {
                        model: 16
                        Rectangle {
                            required property int index
                            width: 15; height: 15; radius: 3
                            color: parent.sc.ansi[index]
                            border.color: Theme.border; border.width: 1
                        }
                    }
                }

                Text { text: qsTr("Farbschema (Hell)"); color: Theme.textBright }
                AppComboBox {
                    Layout.fillWidth: true
                    model: ColorSchemes.names
                    currentIndex: Math.max(0, ColorSchemes.names.indexOf(ColorSchemes.lightScheme))
                    onActivated: (i) => ColorSchemes.lightScheme = ColorSchemes.names[i]
                }
                Item { width: 1; height: 1 }
                Row {
                    Layout.fillWidth: true
                    spacing: 3
                    property var sc: ColorSchemes.colors(ColorSchemes.lightScheme)
                    Repeater {
                        model: 16
                        Rectangle {
                            required property int index
                            width: 15; height: 15; radius: 3
                            color: parent.sc.ansi[index]
                            border.color: Theme.border; border.width: 1
                        }
                    }
                }
            }

            SectionLabel { text: qsTr("Terminal") }
            GridLayout {
                columns: 2; columnSpacing: 12; rowSpacing: 8; Layout.fillWidth: true
                Text { text: qsTr("Schriftart"); color: Theme.textBright }
                AppComboBox {
                    Layout.fillWidth: true
                    model: App.monospaceFonts()
                    currentIndex: Math.max(0, App.monospaceFonts().indexOf(window.terminalFontFamily))
                    onActivated: (i) => window.terminalFontFamily = App.monospaceFonts()[i]
                }
                Text { text: qsTr("Schriftgröße"); color: Theme.textBright }
                SpinBox {
                    from: 6; to: 40
                    value: window.terminalFontSize
                    onValueModified: window.terminalFontSize = value
                }
                Text { text: qsTr("Ligaturen"); color: Theme.textBright }
                CheckBox {
                    text: qsTr("Programmier-Ligaturen (z. B. FiraCode)")
                    checked: window.terminalLigatures
                    onToggled: window.terminalLigatures = checked
                }
                Text { text: qsTr("Rendering"); color: Theme.textBright }
                CheckBox {
                    text: qsTr("GPU-Glyph-Atlas (schneller; aus = QPainter-Fallback)")
                    checked: window.terminalGpuRendering
                    onToggled: window.terminalGpuRendering = checked
                }
                Text {
                    text: qsTr("Standard-Shell"); color: Theme.textBright
                    visible: window.hasShellChoice
                }
                AppComboBox {
                    visible: window.hasShellChoice
                    Layout.fillWidth: true
                    textRole: "name"
                    model: sessions.availableShells()
                    currentIndex: {
                        const l = sessions.availableShells()
                        for (let i = 0; i < l.length; ++i)
                            if (l[i].program === window.currentShellProgram()) return i
                        return 0
                    }
                    onActivated: (i) => window.defaultShellProgram = sessions.availableShells()[i].program
                }
            }

            SectionLabel { text: qsTr("Eingabe & Zwischenablage") }
            ColumnLayout {
                spacing: 6; Layout.fillWidth: true
                CheckBox {
                    text: qsTr("Auswahl automatisch kopieren")
                    checked: window.copyOnSelect
                    onToggled: window.copyOnSelect = checked
                }
                CheckBox {
                    text: qsTr("Rechtsklick fügt ein")
                    checked: window.rightClickPaste
                    onToggled: window.rightClickPaste = checked
                }
                CheckBox {
                    text: qsTr("Vor mehrzeiligem Einfügen warnen")
                    checked: window.pasteWarnMultiline
                    onToggled: window.pasteWarnMultiline = checked
                }
            }

            SectionLabel { text: qsTr("Fenster") }
            ColumnLayout {
                spacing: 4; Layout.fillWidth: true
                CheckBox {
                    text: qsTr("Vor dem Beenden nachfragen")
                    checked: window.confirmQuit
                    onToggled: window.confirmQuit = checked
                }
                Text {
                    text: qsTr("Beenden schließt alle Sitzungen samt laufender Prozesse.")
                    color: Theme.textDim
                    font.pixelSize: 12
                    Layout.leftMargin: 6
                    Layout.bottomMargin: 4
                }
                CheckBox {
                    text: qsTr("Quake-Modus: per globalem Hotkey ein-/ausblenden")
                    checked: window.quakeMode
                    enabled: Qt.platform.os === "osx"   // vorerst nur macOS
                    onToggled: window.quakeMode = checked
                }
                Text {
                    text: Qt.platform.os === "osx"
                          ? qsTr("Globaler Hotkey: Strg+^ (blendet QTmux überall ein/aus)")
                          : qsTr("Derzeit nur unter macOS verfügbar.")
                    color: Theme.textDim
                    font.pixelSize: 11
                    Layout.leftMargin: 26
                }
            }

            SectionLabel { text: qsTr("Agenten-Benachrichtigungen") }
            // Inter-Agenten-Benachrichtigung: eine Session wird benachrichtigt, wenn ein
            // Agent in einer ANDEREN Session fertig ist oder eine Frage hat. Hier wird je
            // Session ein Abo (auf alle anderen Quellen) ein-/ausgeschaltet und auf
            // Ereignisarten gefiltert. Agenten abonnieren sich i. d. R. selbst per MCP
            // (subscribe_events) und holen die Ereignisse per wait_for_events ab.
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 6
                Text {
                    text: qsTr("Wähle je Session, ob sie über Ereignisse der anderen Sessions "
                             + "benachrichtigt wird. Feinere Quell-Filter sind über die "
                             + "MCP-Schnittstelle verfügbar.")
                    color: Theme.textDim
                    font.pixelSize: 11
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }
                Repeater {
                    model: sessions
                    delegate: ColumnLayout {
                        required property var session
                        required property string title
                        required property string workingDir
                        Layout.fillWidth: true
                        spacing: 0
                        // Titel + eindeutige Kennung (Session-ID, dazu CWD bei Shells),
                        // damit gleichnamige Sessions (z. B. mehrere „Eingabeaufforderung")
                        // unterscheidbar sind.
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 6
                            CheckBox {
                                text: title
                                checked: settingsDialog.agentSubFor(session.sessionId) !== null
                                onToggled: {
                                    if (checked) AgentEvents.subscribe(session.sessionId, [], [])
                                    else AgentEvents.unsubscribe(session.sessionId)
                                }
                            }
                            Text {
                                Layout.fillWidth: true
                                text: workingDir.length > 0
                                      ? qsTr("#%1 · %2").arg(session.sessionId).arg(workingDir)
                                      : qsTr("#%1").arg(session.sessionId)
                                color: Theme.textDim
                                font.pixelSize: 11
                                elide: Text.ElideLeft
                            }
                        }
                        RowLayout {
                            id: kindRow
                            Layout.leftMargin: 26
                            spacing: 10
                            visible: settingsDialog.agentSubFor(session.sessionId) !== null
                            property int sid: session.sessionId
                            // Aktuelle Arten-Auswahl aus den drei Checkboxen bauen und das
                            // Abo aktualisieren (leere Liste = alle Arten).
                            function apply() {
                                const ks = []
                                if (cbDone.checked) ks.push("done")
                                if (cbQuestion.checked) ks.push("question")
                                if (cbError.checked) ks.push("error")
                                AgentEvents.subscribe(sid, [], ks)
                            }
                            Text { text: qsTr("Arten:"); color: Theme.textDim; font.pixelSize: 11 }
                            CheckBox {
                                id: cbDone; text: qsTr("fertig"); font.pixelSize: 11
                                checked: settingsDialog.agentKindChecked(kindRow.sid, "done")
                                onToggled: kindRow.apply()
                            }
                            CheckBox {
                                id: cbQuestion; text: qsTr("Frage"); font.pixelSize: 11
                                checked: settingsDialog.agentKindChecked(kindRow.sid, "question")
                                onToggled: kindRow.apply()
                            }
                            CheckBox {
                                id: cbError; text: qsTr("Fehler"); font.pixelSize: 11
                                checked: settingsDialog.agentKindChecked(kindRow.sid, "error")
                                onToggled: kindRow.apply()
                            }
                        }
                    }
                }
                Text {
                    visible: sessions.count === 0
                    text: qsTr("Keine Sessions geöffnet.")
                    color: Theme.textDim
                    font.pixelSize: 11
                }
            }

            SectionLabel { text: qsTr("Tastenkürzel") }
            // Konfigurierbare Aktionen (QTMUX-15). Repeater statt eigener ListView —
            // das umgebende Flickable des Dialogs scrollt (kein verschachteltes Scrollen).
            // Klick auf das Kürzel öffnet den Aufnahme-Dialog; „Standard" erscheint nur
            // bei Abweichung vom Default.
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2
                Repeater {
                    model: Hotkeys.actionIds()
                    delegate: RowLayout {
                        required property string modelData
                        Layout.fillWidth: true
                        spacing: 8
                        Text {
                            text: window.hotkeyLabel(modelData)
                            color: Theme.textBright
                            font.pixelSize: 13
                            Layout.fillWidth: true
                            elide: Text.ElideRight
                        }
                        Button {
                            text: Hotkeys.bindings[modelData] || qsTr("(keins)")
                            font.pixelSize: 12
                            onClicked: hotkeyCaptureDialog.start(modelData, window.hotkeyLabel(modelData))
                        }
                        Button {
                            text: qsTr("Standard")
                            font.pixelSize: 12
                            visible: Hotkeys.bindings[modelData] !== Hotkeys.defaultSequence(modelData)
                            onClicked: Hotkeys.reset(modelData)
                        }
                    }
                }
            }
            Button {
                text: qsTr("Alle Kürzel zurücksetzen")
                onClicked: Hotkeys.resetAll()
            }
        }
        }
    }

}
