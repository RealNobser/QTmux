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

    // --- Per-Window-Layout (QTMUX-83, B1) ------------------------------------
    // Die Sidebar listet **Windows** (Tabs). Jedes Window hat SEINEN EIGENEN
    // Split-Layout-Baum (Authority: `Window::layoutJson` in C++). QML rendert immer
    // nur den Baum des AKTIVEN Windows als Live-JS-Kopie in `layout`; bei jeder
    // Strukturänderung wird sie über syncActiveTree() ins Window-Objekt zurück-
    // serialisiert. Beim Umschalten (loadWindow) sichert QML den alten Baum und lädt
    // den neuen. Knotentypen:
    //   Blatt: { paneId: int, sessionId: int }   (ein Terminal; sessionId ist STABIL)
    //   Split: { orientation: int, children: [...], sizes: [...] }
    // Blätter referenzieren Sessions per stabiler `sessionId` (nicht mehr per Zeilen-
    // index) — Umsortieren/Schließen in der Sidebar braucht damit keinen Index-Remap.
    property var layout: null
    property int nextPaneId: 1
    // Stabile ID des aktuell im Hauptbereich gerenderten Windows (-1 = keines).
    property int activeWindowId: -1
    property int activePaneId: -1
    property int paneCount: 1
    // Anker für Sidebar-Bindungen, die den aggregierten Window-Status/-Titel aus den
    // Sessions ableiten: `sessionsRevision` wird bei jeder Session-Änderung erhöht,
    // `windowsRevision` bei jeder Layout-/Fenster-Änderung. Ohne solche Anker frören die
    // in QML berechneten Window-Kacheln auf ihrem ersten Stand ein (wie groupsRevision).
    property int sessionsRevision: 0
    property int windowsRevision: 0
    property var paneItems: ({})         // paneId -> TerminalItem (für Fokus + Hit-Test)
    property var activeTerminal: null
    // Ob das AKTIVE Terminal gerade eine Auswahl hat — treibt actCopy.enabled (Menü/
    // Kontextmenü „Kopieren"). Bewusst als eigene Property + Connections statt der
    // direkten Bindung `activeTerminal.hasSelection`: die Änderungsbenachrichtigung einer
    // Sub-Property eines `var`-gehaltenen QObjects propagiert in QML NICHT zuverlässig,
    // wodurch der Menüeintrag dauerhaft ausgegraut blieb (Cmd+C ging weiter, weil das
    // fokussierte Terminal die Taste selbst behandelt). Hier wird der Wert bei jedem
    // Terminal-Wechsel UND bei jedem selectionChanged des aktiven Terminals nachgeführt.
    property bool activeHasSelection: false
    function refreshActiveSelection() {
        activeHasSelection = activeTerminal ? activeTerminal.hasSelection : false
    }
    onActiveTerminalChanged: refreshActiveSelection()
    Connections {
        target: window.activeTerminal
        ignoreUnknownSignals: true
        function onSelectionChanged() { window.refreshActiveSelection() }
    }
    // Pane-Reorder per Drag (QTMUX-4): aktives Quell-Pane + aktuell überfahrenes Ziel.
    property int dragPaneId: -1
    property int dragOverPaneId: -1
    property var dragScenePt: null

    SessionModel { id: sessions }
    // Per-Window-Layouts (QTMUX-83, Stufe 2): die künftige Sidebar-Quelle. Vorerst
    // instanziiert, aber noch NICHT verdrahtet — der Flip (Sidebar/Layout auf Windows)
    // folgt zusammenhängend. So bleibt die App auf dem alten Modell lauffähig.
    WindowModel { id: windows }

    // Nicht-modales Einstellungsfenster (QTMUX-47). Öffnen über actSettings/Toolbar/
    // Palette per prefs.open(kategorie) — ersetzt die früheren settingsDialog/
    // connectionsDialog/vaultDialog/hotkeyCaptureDialog. Ein eigenes Window sieht die IDs
    // aus Main.qml nicht — die noch modalen Editier-Dialoge werden als Handles hereingereicht.
    PrefsWindow {
        id: prefs
        app: window
        sessions: sessions
        mcp: mcp
        profileEditDialog: profileEditDialog
        secretEditDialog: secretEditDialog
        masterPwDialog: vaultChangePwDialog
        schemeFileDialog: schemeFileDialog
    }

    // MCP-Server: externe Agenten-Steuerung über 127.0.0.1 (nur lokal).
    McpServer {
        id: mcp
        sessions: sessions
        windows: windows          // Window-Gruppen (QTMUX-83, Stufe 5)
        // Port NICHT hart setzen: McpServer::defaultPort() liest QTMUX_MCP_PORT bzw.
        // die Einstellung mcp/port (Vorgabe 7345). Eine Testinstanz startet damit auf
        // einem eigenen Port, ohne der produktiven Instanz den Port wegzunehmen.
        // MCP focus_session (Zeilenindex): im Window-Modell heißt „fokussieren" = das
        // Window aktivieren, in dem die Session als Pane liegt (loadWindow). Liegt die
        // Session in keinem Window (sollte nicht vorkommen), passiert nichts.
        onFocusRequested: (row) => {
            const s = sessions.sessionAt(row)
            if (s && s.windowId >= 0) window.loadWindow(s.windowId)
        }
        onSetThemeRequested: (mode) => Theme.mode = mode

        // --- Layout-/Profil-Steuerung über MCP (QTMUX-29). Die Handler laufen
        // synchron zum Tool-Aufruf und antworten über mcp.provideResult. ---
        // QTMUX-33: Der Baum allein beantwortet die Frage eines Controllers nicht —
        // er sieht nur die Panes, nicht aber, welche seiner Sessions gerade GAR NICHT
        // sichtbar sind (die liegen nur in der Seitenleiste). Deshalb liefern wir den
        // Baum unter "layout" plus eine Sitzungsübersicht mit Pane-Zuordnung.
        onLayoutRequested: (windowId) => {
            const w = (windowId >= 0) ? windows.windowById(windowId) : window.activeWindowObj()
            if (!w) { mcp.provideResult(false, qsTr("Unbekannte windowId.")); return }
            const isActive = (w.windowId === window.activeWindowId)
            // Aktives Window: der Live-Baum (window.layout) ist am frischesten; andere
            // Windows aus ihrem gespeicherten layoutJson.
            let tree = isActive ? window.layout : null
            if (!isActive) { try { tree = JSON.parse(w.layoutJson) } catch (e) { tree = null } }
            if (!tree) { mcp.provideResult(false, qsTr("Kein Layout vorhanden.")); return }
            const activePaneId = isActive ? window.activePaneId : w.activePaneId
            const inPane = ({})   // sessionId -> paneId (nur die dieses Windows)
            function ser(node) {
                if (window.isLeaf(node)) {
                    inPane[node.sessionId] = node.paneId
                    return { paneId: node.paneId,
                             sessionId: node.sessionId,
                             active: node.paneId === activePaneId }
                }
                return { orientation: node.orientation === Qt.Vertical ? "v" : "h",
                         children: node.children.map(ser) }
            }
            const serTree = ser(tree)
            // Sitzungsübersicht: alle Sessions, mit Pane-Zuordnung IN DIESEM Window
            // (QTMUX-33). Sessions anderer Windows laufen weiter, sind hier „unsichtbar".
            const list = []
            for (let i = 0; i < sessions.rowCount(); ++i) {
                const s = sessions.sessionAt(i)
                if (!s) continue
                const pid = inPane[s.sessionId]
                list.push({ sessionId: s.sessionId,
                            title: s.title,
                            windowId: s.windowId,
                            paneId: pid === undefined ? null : pid,
                            visible: pid !== undefined,
                            active: pid !== undefined && pid === activePaneId })
            }
            mcp.provideResult(true, JSON.stringify({ layout: serTree,
                                                     windowId: w.windowId,
                                                     activePaneId: activePaneId,
                                                     sessions: list }))
        }
        // --- Window-Steuerung (QTMUX-83, Stufe 4) ---
        onListWindowsRequested: {
            const arr = []
            for (let i = 0; i < windows.count; ++i) {
                const w = windows.windowAt(i)
                arr.push({ windowId: w.windowId,
                           title: window.windowTitle(w),
                           group: w.group || "",
                           paneCount: w.paneIds().length,
                           active: w.windowId === window.activeWindowId,
                           sessionIds: w.sessionIds() })
            }
            mcp.provideResult(true, JSON.stringify(arr))
        }
        onFocusWindowRequested: (id) => {
            if (!windows.windowById(id)) { mcp.provideResult(false, qsTr("Unbekannte windowId.")); return }
            window.loadWindow(id)
            mcp.provideResult(true, "ok")
        }
        onNewWindowRequested: {
            window.newSession()                 // neue Shell in eigenem Window (wird aktiv)
            const w = window.activeWindowObj()
            const ids = w ? w.sessionIds() : []
            mcp.provideResult(true, String(ids.length ? ids[0] : -1))
        }
        onRenameWindowRequested: (id, name) => {
            const w = windows.windowById(id)
            if (!w) { mcp.provideResult(false, qsTr("Unbekannte windowId.")); return }
            w.name = name
            mcp.provideResult(true, "ok")
        }
        onCloseWindowRequested: (id) => {
            const row = windows.rowForId(id)
            if (row < 0) { mcp.provideResult(false, qsTr("Unbekannte windowId.")); return }
            // Das letzte Fenster zu schließen beendet in der GUI die App (QTMUX-87). Über MCP
            // wird das bewusst NICHT getan: ein aufräumender Agent würde sonst QTmux samt
            // seiner eigenen Sitzung beenden. Stattdessen ein klarer Hinweis.
            if (windows.count <= 1) {
                mcp.provideResult(false, qsTr("Letztes Fenster: Schließen würde QTmux beenden — über MCP nicht möglich. Nutze close_pane/close_session oder beende die App in der Oberfläche."))
                return
            }
            window.closeWindowRow(row)
            mcp.provideResult(true, "ok")
        }
        onSplitPaneRequested: (o) => {
            const sid = window.splitPane(o === "v" ? Qt.Vertical : Qt.Horizontal)
            mcp.provideResult(true, String(sid))
        }
        onClosePaneRequested: (paneId) => {
            if (paneId >= 0) {
                if (!window.findLeaf(paneId)) { mcp.provideResult(false, qsTr("Unbekannte paneId.")); return }
                window.activePaneId = paneId
            }
            window.closePane()
            mcp.provideResult(true, "ok")
        }
        onFocusPaneRequested: (paneId) => {
            const f = window.findLeaf(paneId)
            if (!f) { mcp.provideResult(false, qsTr("Unbekannte paneId.")); return }
            window.setActivePaneById(paneId, window.paneItems[paneId])
            window.focusActivePane()
            mcp.provideResult(true, "ok")
        }
        onZoomPaneRequested: (paneId) => {
            if (window.zoomPaneById(paneId)) mcp.provideResult(true, "ok")
            else mcp.provideResult(false, qsTr("Unbekannte paneId."))
        }
        // assign_session ist im Window-Modell (QTMUX-83) bedeutungslos: eine Session gehört
        // fest zu genau einem Window/Pane, es gibt kein „Session in ein Pane laden" mehr.
        // Bewusst als klarer Hinweis statt still zu scheitern (endgültige Deprecation: Stufe 4).
        onAssignPaneRequested: (row, paneId) => {
            mcp.provideResult(false, qsTr("assign_session entfällt im Window-Modell — nutze focus_session bzw. focus_window."))
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

    // Zwischenspeicher für die sessionIds, die gerade aus dem Model entfernt werden
    // (in onRowsAboutToBeRemoved erfasst, in onRowsRemoved verarbeitet).
    property var _removingIds: []
    // Neu eingefügte sessionIds, die auf ihr Auto-Wrapping in ein Window warten.
    property var _pendingWrap: []
    // Solange der Restore läuft, darf ein kurzzeitig leerer Fensterstand die App NICHT
    // beenden (s. pruneSessionsFromWindows).
    property bool _starting: true

    // Extern (per MCP/C++) erzeugte Sessions haben keinen Window — sie kommen mit
    // windowId==-1 an und würden sonst in keinem Pane sichtbar. QML-erzeugte Sessions
    // (newSession/splitPane/openWindowWithSession) setzen windowId dagegen SYNCHRON,
    // bevor dieser verzögerte Check läuft → nur die fensterlosen werden eingepackt.
    function _wrapPending() {
        const ids = window._pendingWrap; window._pendingWrap = []
        for (let i = 0; i < ids.length; ++i) {
            const s = window.sessionById(ids[i])
            if (s && s.windowId < 0) {
                const row = window.rowForSessionId(ids[i])
                if (row >= 0) window.openWindowWithSession(row, "")   // eigenes Window (Tab)
            }
        }
    }

    // Session-Lebenszyklus im Window-Modell: verschwindet eine Session (manuell, Shell-Ende
    // oder MCP close_session), müssen ihre Blätter aus ALLEN Window-Bäumen verschwinden und
    // leer gewordene Windows geschlossen werden. Die zu entfernenden sessionIds werden VOR
    // dem Entfernen erfasst — danach ist die Zeile→ID-Zuordnung nicht mehr abfragbar.
    Connections {
        target: sessions
        function onRowsAboutToBeRemoved(parent, first, last) {
            const ids = []
            for (let r = first; r <= last; ++r) { const s = sessions.sessionAt(r); if (s) ids.push(s.sessionId) }
            window._removingIds = ids
        }
        function onRowsRemoved(parent, first, last) {
            window.pruneSessionsFromWindows(window._removingIds)
            window._removingIds = []
        }
        function onRowsInserted(parent, first, last) {
            for (let r = first; r <= last; ++r) { const s = sessions.sessionAt(r); if (s) window._pendingWrap.push(s.sessionId) }
            Qt.callLater(window._wrapPending)
        }
        // Anker für die in QML berechneten Window-Kacheln (Titel/Status aus den Sessions).
        function onDataChanged(topLeft, bottomRight, roles) { window.sessionsRevision++ }
        function onCountChanged() { window.sessionsRevision++ }
        // Fenster-Alert (Dock-Hüpfen/Taskbar-Blinken), wenn QTmux nicht im Vordergrund ist.
        function onAttentionRaised(row) {
            if (!window.active) window.alert(0)
        }
    }

    // Pfad zu einem Phosphor-SVG-Icon (eingebettet unter qrc:/icons/).
    function icon(name) { return "qrc:/icons/" + name + ".svg" }

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

    // Umfang der Wiederherstellung beim Start (QTMUX-99, qtmux::RestoreMode):
    // 0 gar nicht · 1 ohne Verlauf · 2 alles (Vorgabe = bisheriges Verhalten).
    // Die Regeln stehen Gui-frei in RestoreMode.h; abgefragt wird ausschließlich über
    // windows.restoresLayout/restoresHistory/persistsOnQuit, damit ein defekter Wert
    // an EINER Stelle normalisiert wird statt an dreien.
    property int restoreSessionMode: 2

    // Agenten-Wiederherstellung (QTMUX-85) — Vorgabe AUS: Beim Start liefe sonst
    // unaufgefordert ein Programm los.
    property bool restoreAgents: false
    // Wie die Unterhaltung fortgesetzt wird (QTMUX-98, qtmux::ResumeMode):
    // 0 gar nicht (Vorgabe) · 1 jüngste im Verzeichnis · 2 Auswahl beim Start ·
    // 3 die vom Agenten gemeldete Sitzung. Bewusst eine WAHL statt eines Schalters:
    // Wer einen Agenten je Verzeichnis fährt, ist mit 1 richtig bedient; wer mehrere
    // im selben Ordner laufen lässt, bekäme damit überall dieselbe Unterhaltung und
    // braucht 2 oder 3. Kann ein Agent den Modus nicht, startet er frisch — es wird
    // NIE auf einen anderen Weg ausgewichen.
    property int resumeAgentMode: 0

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
        target: windows
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
    function groupSize(name) { return windows.groupSize(name) }
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
        for (let i = 0; i < windows.count; ++i) {
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
        window.openWindowWithSession(row, "")   // neue Session -> eigenes Window (Tab)
    }
    // „Session schließen" = aktives Pane schließen (beim letzten Pane das ganze Window).
    function closeCurrent() { window.closePane() }
    // Kurzer, selbstverschwindender Hinweis unten im Fenster (z. B. nach „Neues Fenster").
    function notifyToast(msg) { toast.text = msg; toast.restart() }
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
        window.openWindowWithSession(row, "")
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
        // Wie bei newSession: eigenes Window (Tab) anlegen und aktivieren.
        window.openWindowWithSession(row, "")
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
        case "actNewInstance":    return qsTr("Neues Fenster")
        case "actCloseSession":   return qsTr("Session schließen")
        case "actClosePane":      return qsTr("Pane schließen")
        case "actNextPane":       return qsTr("Nächstes Pane")
        case "actPrevPane":       return qsTr("Vorheriges Pane")
        case "actZoomPane":       return qsTr("Pane zoomen")
        case "actNextSession":    return qsTr("Nächste Session")
        case "actPrevSession":    return qsTr("Vorige Session")
        case "actSplitH":         return qsTr("Nebeneinander teilen")
        case "actSplitV":         return qsTr("Untereinander teilen")
        case "actCommandPalette": return qsTr("Befehlspalette")
        case "actFind":           return qsTr("Suchen (Scrollback)")
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

    // Nächstes/voriges Window aktivieren (mit Umlauf). dir = +1/-1.
    function cycleSession(dir) {
        const n = windows.count
        if (n <= 0) return
        let r = (windows.activeRow < 0 ? 0 : windows.activeRow) + dir
        if (r < 0) r = n - 1
        else if (r >= n) r = 0
        window.loadWindowRow(r)
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
    // Session-Objekt zu einer stabilen sessionId (SplitNode-Blätter binden hierüber).
    // sessions.sessionById ist nicht Q_INVOKABLE → hier per Iteration (kleine Listen).
    function sessionById(id) {
        for (let i = 0; i < sessions.count; ++i) {
            const s = sessions.sessionAt(i)
            if (s && s.sessionId === id) return s
        }
        return null
    }

    // --- Window-Anzeige (Sidebar-Kacheln berechnen Titel/Status in QML) ---------
    // Die aggregierten Rollen (Titel/Status/Attention/Controller) werden bewusst in QML
    // aus den Sessions abgeleitet statt in WindowModel — so bleibt das Model schlank.
    // Die Bindungen lesen window.sessionsRevision/windowsRevision mit, damit sie live sind.

    // sessionId des Blatts mit paneId im Baum eines (evtl. nicht aktiven) Windows.
    function sessionIdForPane(w, paneId) {
        if (!w) return -1
        let tree = null; try { tree = JSON.parse(w.layoutJson) } catch (e) { return -1 }
        let found = -1
        const walk = function(n) {
            if (!n || found >= 0) return
            if (n.children === undefined) { if (n.paneId === paneId) found = n.sessionId; return }
            for (let i = 0; i < n.children.length; ++i) walk(n.children[i])
        }
        walk(tree); return found
    }
    // Session-ID des AKTIVEN Panes eines Windows — das ist die Nummer, mit der man die
    // Sitzung per MCP anspricht (`send_text`, `read_screen` …), und genau das, was die
    // Kachel anzeigt (QTMUX-44). Beim aktiven Window ist der Live-Baum maßgeblich, sonst
    // das gespeicherte layoutJson. Fällt auf das erste Blatt zurück.
    function windowActiveSessionId(w) {
        if (!w) return -1
        if (w.windowId === window.activeWindowId) {
            const f = findLeaf(window.activePaneId)
            if (f && f.leaf) return f.leaf.sessionId
        }
        const sid = window.sessionIdForPane(w, w.activePaneId)
        if (sid >= 0) return sid
        const ids = w.sessionIds()
        return ids.length ? ids[0] : -1
    }
    function windowTitle(w) {
        if (!w) return qsTr("Fenster")
        if (w.name && w.name.length > 0) return w.name
        const ids = w.sessionIds()
        if (ids.length === 0) return qsTr("Fenster %1").arg(w.windowId)
        let sid = window.sessionIdForPane(w, w.activePaneId)
        let s = (sid >= 0) ? window.sessionById(sid) : null
        if (!s) s = window.sessionById(ids[0])
        return s ? s.title : qsTr("Fenster %1").arg(w.windowId)
    }
    // Aggregierter Laufzustand (wie Session.state: 0 Start,1 Run,2 Warte,3 Fehler,4 Zu),
    // höchste Dringlichkeit gewinnt: Fehler > WartetEingabe > Läuft > Start > Zu.
    function windowRunState(w) {
        if (!w) return 0
        const ids = w.sessionIds(); let best = -1
        const prioOf = function(st) { return st === 3 ? 4 : st === 2 ? 3 : st === 1 ? 2 : st === 0 ? 1 : 0 }
        for (let i = 0; i < ids.length; ++i) {
            const s = window.sessionById(ids[i]); if (!s) continue
            if (best < 0 || prioOf(s.state) > prioOf(best)) best = s.state
        }
        return best < 0 ? 0 : best
    }
    function windowAttention(w) {
        if (!w) return false
        const ids = w.sessionIds()
        for (let i = 0; i < ids.length; ++i) { const s = window.sessionById(ids[i]); if (s && s.needsAttention) return true }
        return false
    }
    function windowController(w) {
        if (!w) return false
        const ids = w.sessionIds()
        for (let i = 0; i < ids.length; ++i) { const s = window.sessionById(ids[i]); if (s && s.mcpController) return true }
        return false
    }
    function broadcastWrite(data) { sessions.writeToAll(data) }
    function popupTermContextMenu(term) { termContextMenu.popup() }

    // Baum eines Windows auf die reinen Knoten-Properties reduzieren
    // ({paneId,sessionId} / {orientation,children,sizes}). Split-Proportionen (sizes)
    // werden zuvor über captureSplitStates() aus den Live-SplitViews eingesammelt.
    function serializeLayoutNode(n) {
        if (!n) return null
        if (n.children === undefined) return { paneId: n.paneId, sessionId: n.sessionId }
        const kids = []
        for (let i = 0; i < n.children.length; ++i) kids.push(serializeLayoutNode(n.children[i]))
        const o = { orientation: n.orientation, children: kids }
        if (n.sizes) o.sizes = n.sizes
        return o
    }
    function maxPaneIdIn(n) {
        if (!n) return 0
        if (n.children === undefined) return n.paneId
        let m = 0
        for (let i = 0; i < n.children.length; ++i) m = Math.max(m, maxPaneIdIn(n.children[i]))
        return m
    }

    // --- Window-Umschaltung & -Sync (QTMUX-83) -------------------------------
    function activeWindowObj() { return windows.windowById(window.activeWindowId) }

    // Live-SplitView-Größen als Proportionen in den Baum (node.sizes) übernehmen, damit
    // sie über Window-Wechsel und Serialisierung erhalten bleiben (Mechanik aus QTMUX-82).
    function captureSplitStates() {
        const walk = function(n) {
            if (!n || n.children === undefined) return
            const sv = n.__sv
            if (sv && sv.contentChildren && sv.contentChildren.length === n.children.length) {
                const horiz = (n.orientation === Qt.Horizontal)
                const total = horiz ? sv.width : sv.height
                if (total > 0) {
                    const sizes = []
                    for (let i = 0; i < n.children.length; ++i) {
                        const it = sv.contentChildren[i]
                        sizes.push((horiz ? it.width : it.height) / total)
                    }
                    n.sizes = sizes
                }
            }
            for (let i = 0; i < n.children.length; ++i) walk(n.children[i])
        }
        walk(window.layout)
    }

    // Den Live-Baum (window.layout) + aktives Pane ins aktive Window-Objekt zurückschreiben.
    function syncActiveTree() {
        const w = activeWindowObj()
        if (!w) return
        captureSplitStates()
        w.layoutJson = JSON.stringify(serializeLayoutNode(window.layout))
        w.activePaneId = window.activePaneId
        // Strukturänderung im aktiven Window (Split/Schließen/Pane-Wechsel) → die in QML
        // berechneten Kachel-Werte (Pane-Zahl, Session-ID des aktiven Panes) neu auswerten.
        // Ohne diesen Anker fror z. B. das ▦-Badge auf dem Stand beim Anlegen ein.
        window.windowsRevision++
    }
    // persistLayout = syncActiveTree (Alias, in rebuildLayout genutzt).
    function persistLayout() { syncActiveTree() }

    // currentRow (= Session-Zeile des aktiven Panes) aus dem Live-Baum nachziehen.
    function syncCurrentRow() {
        const f = findLeaf(window.activePaneId)
        window.currentRow = (f && f.leaf) ? window.rowForSessionId(f.leaf.sessionId) : -1
    }

    // Ein Window aktivieren: aktuellen Baum sichern, den Baum des Zielfensters laden,
    // aktives Pane wiederherstellen, Layout neu bauen. Die anderen Windows laufen
    // unverändert weiter (ihre Sessions leben im Registry, nicht in der View).
    function loadWindow(id) {
        if (id < 0) return
        if (id === window.activeWindowId) { window.focusActivePane(); return }
        syncActiveTree()                       // aktuellen Stand sichern
        const w = windows.windowById(id)
        if (!w) return
        window.activeWindowId = id
        windows.setActiveRow(windows.rowForId(id))
        let tree = null
        try { tree = JSON.parse(w.layoutJson) } catch (e) { tree = null }
        window.layout = tree
        window.nextPaneId = Math.max(window.nextPaneId, window.maxPaneIdIn(window.layout) + 1)
        if (window.findLeaf(w.activePaneId)) window.activePaneId = w.activePaneId
        else { const fl = firstLeaf(window.layout); window.activePaneId = fl ? fl.paneId : -1 }
        syncCurrentRow()
        window.windowsRevision++
        window.rebuildLayout()
    }
    function loadWindowRow(row) {
        const w = windows.windowAt(row)
        if (w) window.loadWindow(w.windowId)
    }

    // Neues Window mit genau einem Pane für die Session `row` anlegen und aktivieren.
    function openWindowWithSession(row, name) {
        const s = sessions.sessionAt(row)
        if (!s) return -1
        syncActiveTree()                       // bisheriges Window sichern
        const wr = windows.createWindow(name || "")
        const w = windows.windowAt(wr)
        const pid = window.nextPaneId++
        s.windowId = w.windowId
        w.layoutJson = JSON.stringify({ paneId: pid, sessionId: s.sessionId })
        w.activePaneId = pid
        window.activeWindowId = -1             // Reload erzwingen
        window.loadWindow(w.windowId)
        return wr
    }

    // --- Per-Window-Persistenz (QTMUX-83, Stufe 3) --------------------------
    // Beim Start das persistierte `windows`-Schema wiederherstellen (Layout + Proportionen
    // + farbiger Scrollback je Pane). Migriert einmalig ein altes `sessions`-Profil.
    function restoreWindows() {
        // Restore-Modus: sonst erbt eine Session mit leerem `workingDir` das Verzeichnis
        // der zuletzt angelegten (SessionModel::createShellSession) — der Agent startete
        // dann im falschen Projekt. Der Guard hing bisher am toten restoreState()-Pfad.
        sessions.setRestoring(true)
        try { window._restoreWindowsInner() } finally { sessions.setRestoring(false) }
    }
    function _restoreWindowsInner() {
        // „Gar nicht wiederherstellen" (QTMUX-99): mit einer leeren Session starten und den
        // gespeicherten Stand NICHT anfassen — weder laden noch (beim Beenden) überschreiben,
        // s. persistWindows(). Die Migration läuft trotzdem: sie ist ein reiner Transform des
        // alten Schemas und muss auch dann erledigt sein, wenn gerade niemand restauriert.
        windows.runMigration()                    // altes sessions-Array -> windows (idempotent)
        if (!windows.restoresLayout(window.restoreSessionMode)) { newSession(); return }
        const data = windows.readWindows()
        if (!data.present || !data.windows || data.windows.length === 0) { newSession(); return }
        window.nextPaneId = Math.max(1, data.nextPaneId)
        const activeRow = data.activeRow
        let activeWid = -1
        for (let i = 0; i < data.windows.length; ++i) {
            const pw = data.windows[i]
            let tree = null
            try { tree = JSON.parse(pw.layoutJson) } catch (e) { tree = null }
            const built = window._restoreTreeLeaves(tree, pw.id)   // Sessions je Blatt erzeugen
            if (!built) continue                  // leeres/kaputtes Window überspringen
            const wr = windows.createWindowWithId(pw.id, pw.name || "", pw.group || "")
            const w = windows.windowAt(wr)
            w.layoutJson = JSON.stringify(built)
            w.activePaneId = pw.activePaneId
            if (i === activeRow) activeWid = pw.id
        }
        if (windows.count === 0) { newSession(); return }
        if (activeWid < 0) {
            const r = Math.max(0, Math.min(activeRow, windows.count - 1))
            activeWid = windows.windowAt(r).windowId
        }
        window.activeWindowId = -1
        window.loadWindow(activeWid)
        window.pruneAllHistory()                  // Dumps geschlossener Panes aufräumen
    }
    // Baut einen persistierten Baum (Blätter tragen `cfg`) in einen Live-Baum um: je Blatt
    // eine Session aus cfg erzeugen, sessionId zuweisen, Scrollback nach paneId laden.
    // Leere/kaputte Blätter (z. B. fehlendes Plugin) entfallen; Splits kollabieren dann.
    function _restoreTreeLeaves(node, windowId) {
        if (!node) return null
        if (node.children === undefined) {
            const row = window._createSessionFromCfg(node.cfg || {})
            if (row < 0) return null
            const s = sessions.sessionAt(row)
            if (s) s.windowId = windowId
            window.nextPaneId = Math.max(window.nextPaneId, (node.paneId || 0) + 1)
            // Farbiger Scrollback (muss VOR der ersten Ausgabe geladen sein). Im Modus
            // „ohne Verlauf" (QTMUX-99) bleibt er weg: Fenster, Panes und Arbeits-
            // verzeichnisse kommen zurück, die Terminals starten aber leer. Der Dump auf
            // der Platte bleibt liegen — ein späteres Umschalten auf „alles" findet ihn
            // wieder vor.
            if (s && windows.restoresHistory(window.restoreSessionMode))
                sessions.loadHistoryFor(row, node.paneId)
            return { paneId: node.paneId, sessionId: s ? s.sessionId : -1 }
        }
        const kids = []
        for (let i = 0; i < node.children.length; ++i) {
            const c = window._restoreTreeLeaves(node.children[i], windowId)
            if (c) kids.push(c)
        }
        if (kids.length === 0) return null
        if (kids.length === 1) return kids[0]
        const o = { orientation: node.orientation, children: kids }
        if (node.sizes && node.sizes.length === kids.length) o.sizes = node.sizes
        return o
    }
    function _createSessionFromCfg(cfg) {
        const t = cfg.type || 0                   // Session::Type: 0 Shell,1 Ssh,2 Serial,3 App
        if (t === 2) return sessions.createSerialSession(cfg.serialPort || "", cfg.baud || 115200)
        if (t === 3) return sessions.createPluginSession(cfg.pluginId || "", cfg.pluginType || "")
        // Agenten-Wiederherstellung (QTMUX-85, Vorgabe AUS): Der Agent lief nicht als
        // `program`, sondern wurde in die Shell getippt — er kommt darum als Kommando
        // über das Login-Script zurück, sobald der erste Prompt steht (nur Shell/SSH,
        // seriell und Plugin nehmen kein Login-Script).
        // 🔑 Die Startzeile MUSS als Argument von create*Session mitgehen: nur dort
        // steht sie VOR dem Start fest. Nachträglich gesetzt kann der Prompt schon
        // durch sein, und der Fallback-Timer wird erst beim nächsten Output scharf —
        // bei einer wartenden Shell kommt der nie.
        const wantsAgent = window.restoreAgents && !!cfg.agentCommand
        const ref = cfg.agentSessionRef || ""
        const launch = wantsAgent
            ? sessions.agentLaunchCommand(cfg.agentCommand, window.resumeAgentMode, ref) : ""
        const row = (t === 1)
            ? sessions.createSshSession(cfg.host || "", cfg.sshPort || 22, cfg.user || "",
                                        cfg.identity || "", launch)
            : sessions.createShellSession(cfg.workingDir || "", cfg.program || "", launch)
        if (row >= 0 && cfg.agentCommand) {
            if (wantsAgent) sessions.markRestoredAgent(row, cfg.agentId || "", cfg.agentCommand, ref)
            // Auch OHNE Wiederherstellung vormerken, sonst überschreibt das nächste
            // Beenden den gespeicherten Befehl mit Leer und ein späteres Einschalten
            // des Schalters fände nichts mehr vor.
            else sessions.seedAgentConfig(row, cfg.agentId || "", cfg.agentCommand, ref)
        }
        return row
    }

    // Beim Beenden alle Windows persistieren: je Blatt den SessionConfig (`cfg`) in den
    // Baum schreiben und den farbigen Scrollback nach paneId sichern.
    function persistWindows() {
        // ⚠️ QTMUX-99: Bei „gar nicht wiederherstellen" wird auch NICHT gespeichert. Sonst
        // schriebe das erste Beenden die eine frisch geöffnete Session über den gesamten
        // gespeicherten Stand — ein einmaliges Umstellen wäre unwiderruflich. Aus demselben
        // Grund unterbleibt hier das pruneHistoryExcept: die Dumps gehören zum eingefrorenen
        // Stand und dürfen nicht als „verwaist" weggeräumt werden.
        if (!windows.persistsOnQuit(window.restoreSessionMode)) return
        syncActiveTree()                          // aktives Window in sein layoutJson spiegeln
        const wins = []
        const paneKeys = []
        for (let i = 0; i < windows.count; ++i) {
            const w = windows.windowAt(i)
            const enriched = window._enrichTree(w.layoutJson, paneKeys)
            wins.push({ id: w.windowId, name: w.name || "", group: w.group || "",
                        activePaneId: w.activePaneId, layoutJson: enriched })
        }
        windows.writeWindows(wins, windows.activeRow, window.nextPaneId)
        sessions.pruneHistoryExcept(paneKeys)     // Dumps geschlossener Panes wegräumen
    }
    // Persistierbaren Baum bauen: Blatt {paneId, cfg}; Scrollback je Pane sichern.
    function _enrichTree(json, paneKeys) {
        let tree = null; try { tree = JSON.parse(json) } catch (e) { return "null" }
        const walk = function(n) {
            if (!n) return null
            if (n.children === undefined) {
                const row = window.rowForSessionId(n.sessionId)
                let cfg = {}
                if (row >= 0) { cfg = sessions.sessionConfig(row); sessions.saveHistoryFor(row, n.paneId); paneKeys.push(n.paneId) }
                return { paneId: n.paneId, cfg: cfg }
            }
            const kids = []
            for (let i = 0; i < n.children.length; ++i) { const c = walk(n.children[i]); if (c) kids.push(c) }
            if (kids.length === 0) return null
            if (kids.length === 1) return kids[0]
            const o = { orientation: n.orientation, children: kids }
            if (n.sizes) o.sizes = n.sizes
            return o
        }
        const built = walk(tree)
        return JSON.stringify(built)
    }
    // paneIds aller Windows sammeln und fremde .ans-Dumps entfernen.
    function pruneAllHistory() {
        const keys = []
        for (let i = 0; i < windows.count; ++i) {
            const w = windows.windowAt(i)
            const ids = w.paneIds()
            for (let k = 0; k < ids.length; ++k) keys.push(ids[k])
        }
        sessions.pruneHistoryExcept(keys)
    }

    // Beschneidet einen (als JSON übergebenen) Layout-Baum um alle Blätter, deren
    // sessionId in `idset` liegt; kollabiert leere/einelementige Splits. Für die NICHT
    // aktiven Windows (deren Baum als JSON im Window-Objekt liegt).
    function pruneTreeJson(json, idset) {
        let tree = null
        try { tree = JSON.parse(json) } catch (e) { return { json: "null", empty: true } }
        const prune = function(n) {
            if (!n) return null
            if (n.children === undefined) return (idset.indexOf(n.sessionId) >= 0) ? null : n
            const kept = []
            for (let i = 0; i < n.children.length; ++i) { const c = prune(n.children[i]); if (c) kept.push(c) }
            if (kept.length === 0) return null
            if (kept.length === 1) return kept[0]
            n.children = kept
            if (n.sizes && n.sizes.length !== kept.length) delete n.sizes
            return n
        }
        tree = prune(tree)
        return { json: JSON.stringify(tree), empty: !tree }
    }

    // Entfernte Sessions aus ALLEN Window-Bäumen tilgen; leer gewordene Windows schließen.
    function pruneSessionsFromWindows(ids) {
        if (!ids || ids.length === 0) return
        syncActiveTree()                       // aktives Window in sein layoutJson spiegeln
        const closeRows = []
        for (let i = 0; i < windows.count; ++i) {
            const w = windows.windowAt(i)
            const r = pruneTreeJson(w.layoutJson, ids)
            if (r.empty) closeRows.push(i)
            else w.layoutJson = r.json
        }
        let activeClosed = false
        for (let k = closeRows.length - 1; k >= 0; --k) {
            const w = windows.windowAt(closeRows[k])
            if (w && w.windowId === window.activeWindowId) activeClosed = true
            windows.closeWindow(closeRows[k])
        }
        // Verschwindet damit das LETZTE Fenster (z. B. `exit` in der einzigen Shell), ist
        // QTmux leer — dann beenden statt ein Geister-Fenster nachzuschieben (QTMUX-87).
        // Der Startup-Guard verhindert, dass ein kurzzeitig leerer Zustand während des
        // Restores die App beendet.
        if (windows.count === 0) {
            window.activeWindowId = -1
            window.layout = null
            if (!window._starting) window.requestQuit()
            return
        }
        if (activeClosed) {
            const nr = Math.max(0, Math.min(windows.activeRow >= 0 ? windows.activeRow : 0, windows.count - 1))
            const nw = windows.windowAt(nr)
            window.activeWindowId = -1
            window.loadWindow(nw.windowId)
        } else {
            // Aktives Window überlebte → Live-Baum aus dem (evtl. beschnittenen) layoutJson neu laden
            const w = activeWindowObj()
            let tree = null; try { tree = JSON.parse(w.layoutJson) } catch (e) { tree = null }
            window.layout = tree
            if (!findLeaf(window.activePaneId)) { const fl = firstLeaf(window.layout); window.activePaneId = fl ? fl.paneId : -1 }
            syncCurrentRow()
            window.windowsRevision++
            window.rebuildLayout()
        }
    }

    // Ein Window (Tab) samt seiner Sessions schließen.
    // Das LETZTE Fenster zu schließen beendet QTmux (Anwender-Entscheidung, QTMUX-87):
    // vorher entstand stattdessen sofort ein neues, leeres Fenster mit höherer ID — das
    // sah aus wie ein durchlaufender Zähler und wirkte, als sei nichts geschlossen worden.
    // requestQuit() nutzt die normale Beenden-Rückfrage (listet die offenen Sitzungen).
    function closeWindowRow(row) {
        const w = windows.windowAt(row)
        if (!w) return
        if (windows.count <= 1) { window.requestQuit(); return }
        const wasActive = (w.windowId === window.activeWindowId)
        const sids = w.sessionIds()            // C++ QList<int> -> JS-Array
        // Erst das Window aus dem Model nehmen, damit das Session-Pruning es nicht mehr sieht.
        windows.closeWindow(row)
        if (wasActive) {
            window.activeWindowId = -1
            window.layout = null
            if (windows.count > 0) {
                const nr = Math.max(0, Math.min(windows.activeRow >= 0 ? windows.activeRow : row, windows.count - 1))
                const nw = windows.windowAt(nr)
                window.loadWindow(nw.windowId)
            }
        }
        for (let i = 0; i < sids.length; ++i) {
            const rr = window.rowForSessionId(sids[i])
            if (rr >= 0) sessions.closeSession(rr)
        }
    }

    function rebuildLayout() {
        // Pane-Zoom (QTMUX-59): zeigt ein Zoom-Pane das nicht mehr existiert, Zoom aufheben.
        if (window.zoomedPane >= 0 && !findLeaf(window.zoomedPane)) window.zoomedPane = -1
        window.paneCount = leafCount()
        paneTreeLoader.sourceComponent = null
        paneTreeLoader.sourceComponent = paneTreeComp
        focusActivePane()
        persistLayout()
    }

    // Pane-Zoom (QTMUX-59): das aktive Pane maximieren, OHNE den Layout-Baum neu zu bauen.
    // `zoomedPane` hält die gezoomte paneId; SplitNode blendet die Zweige, deren Teilbaum
    // die Zoom-Pane NICHT enthält, per visible:false aus (SplitView schließt sie aus, der
    // sichtbare Zweig füllt). Kein Rebuild → Fokus, SplitView-Proportionen und die
    // TerminalItems bleiben unangetastet; beim Aufheben kehrt exakt das alte Layout zurück.
    property int zoomedPane: -1
    function subtreeHasPane(node, id) {
        if (!node) return false
        if (node.children === undefined) return node.paneId === id       // Blatt
        for (var i = 0; i < node.children.length; ++i)
            if (subtreeHasPane(node.children[i], id)) return true
        return false
    }
    function toggleZoom() {
        if (window.zoomedPane >= 0) { window.zoomedPane = -1; return }
        if (leafCount() <= 1) return                       // nichts zu zoomen
        if (window.activePaneId < 0 || !findLeaf(window.activePaneId)) return
        window.zoomedPane = window.activePaneId
    }
    function zoomPaneById(id) {                              // für MCP zoom_pane
        if (id < 0) { window.zoomedPane = -1; return true }
        if (!findLeaf(id)) return false
        window.zoomedPane = id
        return true
    }

    // Aktives Pane setzen (per paneId) und currentRow nachziehen.
    function setActivePaneById(id, term) {
        window.activePaneId = id
        if (term) window.activeTerminal = term
        const f = findLeaf(id)
        if (f && f.leaf) window.currentRow = window.rowForSessionId(f.leaf.sessionId)
        const w = activeWindowObj()
        if (w) w.activePaneId = id       // aktives Pane im Window-Objekt merken
    }
    // Aktives Pane zyklisch wechseln (dir = +1/-1) — das Tastatur-/Befehls-Pendant zum
    // Mausklick ins Pane. Reihenfolge = Blattreihenfolge des Layout-Baums (forEachLeaf).
    function cyclePane(dir) {
        if (window.paneCount <= 1) return
        const ids = []
        forEachLeaf(window.layout, function(l) { ids.push(l.paneId) })
        if (ids.length === 0) return
        let i = ids.indexOf(window.activePaneId)
        if (i < 0) i = 0
        const ni = (i + dir + ids.length) % ids.length
        const t = window.paneItems[ids[ni]]
        window.setActivePaneById(ids[ni], t)
        if (t) t.forceActiveFocus()
    }
    // Fokus (nach Item-Erzeugung) auf das aktive Pane legen.
    function focusActivePane() {
        Qt.callLater(function() {
            const t = window.paneItems[window.activePaneId]
            if (t) { window.activeTerminal = t; t.forceActiveFocus() }
        })
    }

    // Zeilenindex einer stabilen sessionId (sessions.rowForId ist nicht Q_INVOKABLE).
    function rowForSessionId(sid) {
        for (let i = 0; i < sessions.count; ++i) {
            const s = sessions.sessionAt(i)
            if (s && s.sessionId === sid) return i
        }
        return -1
    }
    // Window-Gruppen-Verschiebung (Palette/Menü/Drag). Reine Reihenfolge im WindowModel —
    // die Layout-Bäume bleiben unberührt (Blätter binden per sessionId).
    function moveGroupBy(name, dir)     { windows.moveGroup(name, dir) }
    function moveGroupToRow(name, row)  { windows.moveGroupToRow(name, row) }
    // Window per Drag umsortieren (übernimmt die Gruppe der neuen Nachbarschaft).
    function moveWindowRow(from, to)    { windows.moveWindow(from, to) }

    // Teilen: aktives Blatt im AKTIVEN Window durch einen Split [Blatt, neues Blatt]
    // ersetzen. Hat der Eltern-Split bereits dieselbe Orientierung, wird nur ein
    // Geschwister eingefügt — saubere verschachtelte H+V-Mischungen (QTMUX-3). Die neue
    // Session gehört demselben Window. Gibt die neue sessionId zurück (für MCP).
    function splitPane(orientation) {
        const row = sessions.createShellSession("", window.defaultShellProgram)
        const s = sessions.sessionAt(row)
        if (s) s.windowId = window.activeWindowId
        const sid = s ? s.sessionId : -1
        const newLeaf = { paneId: window.nextPaneId++, sessionId: sid }
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
        return sid
    }

    // Aktives Pane schließen. Ist es das letzte Pane des Windows, schließt das ganze
    // Window (Tab) — sonst nur dieses Pane samt seiner Session.
    function closePane() {
        if (leafCount() <= 1) { window.closeWindowRow(windows.rowForId(window.activeWindowId)); return }
        const f = findLeaf(window.activePaneId)
        if (!f || !f.parent) return
        const sid = f.leaf.sessionId
        const parent = f.parent
        parent.children.splice(f.idx, 1)
        collapseSplit(parent)        // Eltern-Split mit nur einem Kind kollabieren
        // Neues aktives Blatt wählen (irgendein verbleibendes).
        const fl = firstLeaf(window.layout)
        window.activePaneId = fl ? fl.paneId : -1
        syncCurrentRow()
        const r = window.rowForSessionId(sid)
        if (r >= 0) sessions.closeSession(r)   // -> onRowsRemoved beschneidet die Bäume
        rebuildLayout()
    }

    // Pane-Reorder (QTMUX-4): Inhalte (Session) zweier Blätter im aktiven Window tauschen.
    function swapPanes(idA, idB) {
        if (idA === idB) return
        const a = findLeaf(idA), b = findLeaf(idB)
        if (!a || !b) return
        const tmp = a.leaf.sessionId
        a.leaf.sessionId = b.leaf.sessionId
        b.leaf.sessionId = tmp
        window.activePaneId = idB
        syncCurrentRow()
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
        // Per-Window-Persistenz (Stufe 3): Windows + Layout + Proportionen + farbiger
        // Scrollback aus dem `windows`-Schema wiederherstellen (migriert einmalig ein
        // altes `sessions`-Profil). Kein sessions.restoreState() mehr — die Sessions
        // entstehen je Blatt aus dem gespeicherten cfg.
        window.restoreWindows()
        window._starting = false   // ab jetzt darf ein leerer Fensterstand beenden
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
        if (!window._persisted) { window.persistWindows(); window._persisted = true }   // Stufe 3
        sessions.shutdownAll()
    }

    // Schutz vor SIGTERM/SIGINT (main.cpp ruft QCoreApplication::quit()): dann feuert
    // onClosing NICHT (kein Fenster-Schließen), aber aboutToQuit — hier noch persistieren,
    // solange die Screens leben. Der _persisted-Guard verhindert Doppel-Speichern (das
    // zweite liefe nach shutdownAll auf toten Screens → leerer Scrollback).
    property bool _persisted: false
    Connections {
        target: Qt.application
        function onAboutToQuit() {
            // Im stillen Screenshot-Modus NICHT persistieren (frischer Zustand würde sonst
            // das gespeicherte Layout überschreiben).
            if (typeof qtmuxScreenshotMode !== "undefined" && qtmuxScreenshotMode) return
            if (!window._persisted) { window.persistWindows(); window._persisted = true; sessions.shutdownAll() }
        }
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
        property alias restoreSessionMode: window.restoreSessionMode
        property alias restoreAgents: window.restoreAgents
        property alias resumeAgentMode: window.resumeAgentMode
        property alias collapsedGroups: window.collapsedGroupsJson
        // (paneLayout entfällt: das Split-Layout lebt jetzt je Window; die per-Window-
        //  Persistenz über Neustarts kommt in Stufe 3 als eigenes `windows`-Schema.)
    }

    // --- Zentrale Aktionen: im Menü UND per Shortcut/Button nutzbar ----------
    Action {
        id: actNewSession
        text: qsTr("Neue Session")
        shortcut: Hotkeys.bindings["actNewSession"]
        enabled: !prefs.capturing
        onTriggered: window.newSession()
    }
    // Neue, unabhängige QTmux-Instanz (eigener Prozess, eigenes Profil, freier MCP-Port).
    Action {
        id: actNewInstance
        text: qsTr("Neues Fenster")
        shortcut: Hotkeys.bindings["actNewInstance"]
        enabled: !prefs.capturing
        onTriggered: {
            const port = App.openNewInstance()
            if (port < 0) window.notifyToast(qsTr("Kein freier MCP-Port für eine neue Instanz gefunden."))
            else window.notifyToast(qsTr("Neues Fenster gestartet (MCP-Port %1).").arg(port))
        }
    }
    Action {
        id: actCloseSession
        text: qsTr("Session schließen")
        shortcut: Hotkeys.bindings["actCloseSession"]
        enabled: window.currentRow >= 0 && !prefs.capturing
        onTriggered: window.closeCurrent()
    }
    Action {
        id: actToggleTheme
        text: Theme.dark ? qsTr("Helles Design") : qsTr("Dunkles Design")
        shortcut: Hotkeys.bindings["actToggleTheme"]
        enabled: !prefs.capturing
        onTriggered: Theme.toggle()
    }
    Action {
        id: actQuit
        text: qsTr("Beenden")
        shortcut: Hotkeys.bindings["actQuit"]
        enabled: !prefs.capturing
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
        enabled: !prefs.capturing
        onTriggered: prefs.open()
    }
    // Terminal-Zoom: Schriftgröße global vergrößern/verkleinern/zurücksetzen.
    Action {
        id: actZoomIn
        text: qsTr("Schrift vergrößern")
        shortcut: StandardKey.ZoomIn        // Cmd++/Strg++ (inkl. „=" ohne Shift)
        enabled: !prefs.capturing
        onTriggered: window.zoomTerminal(1)
    }
    Action {
        id: actZoomOut
        text: qsTr("Schrift verkleinern")
        shortcut: StandardKey.ZoomOut        // Cmd+-/Strg+-
        enabled: !prefs.capturing
        onTriggered: window.zoomTerminal(-1)
    }
    Action {
        id: actZoomReset
        text: qsTr("Schriftgröße zurücksetzen")
        shortcut: Hotkeys.bindings["actZoomReset"]
        enabled: !prefs.capturing
        onTriggered: window.resetTerminalZoom()
    }
    // Broadcast-Input umschalten: Eingabe an alle Sessions.
    Action {
        id: actBroadcast
        text: qsTr("Eingabe an alle Sessions")
        shortcut: Hotkeys.bindings["actBroadcast"]
        enabled: !prefs.capturing
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
        enabled: window.activeHasSelection && !prefs.capturing
        shortcut: Qt.platform.os === "osx" ? StandardKey.Copy : ""
        onTriggered: if (window.activeTerminal) window.activeTerminal.copy()
    }
    Action {
        id: actPaste
        text: qsTr("Einfügen")
        enabled: !prefs.capturing
        shortcut: Qt.platform.os === "osx" ? StandardKey.Paste : ""
        onTriggered: if (window.activeTerminal) window.activeTerminal.paste()
    }
    // Scrollback-Suche im aktiven Terminal öffnen (QTMUX-71). Die Find-Bar sitzt im Pane
    // (SplitNode.qml) und fokussiert sich selbst, sobald searchActive wird.
    Action {
        id: actFind
        text: qsTr("Suchen …")
        shortcut: Hotkeys.bindings["actFind"]
        enabled: window.activeTerminal && !prefs.capturing
        onTriggered: if (window.activeTerminal) window.activeTerminal.beginSearch()
    }
    // Split-Panes: nebeneinander / untereinander teilen, aktives Pane schließen.
    Action {
        id: actSplitH
        text: qsTr("Nebeneinander teilen")
        shortcut: Hotkeys.bindings["actSplitH"]
        enabled: !prefs.capturing
        onTriggered: window.splitPane(Qt.Horizontal)
    }
    Action {
        id: actSplitV
        text: qsTr("Untereinander teilen")
        shortcut: Hotkeys.bindings["actSplitV"]
        enabled: !prefs.capturing
        onTriggered: window.splitPane(Qt.Vertical)
    }
    Action {
        id: actClosePane
        text: qsTr("Pane schließen")
        shortcut: Hotkeys.bindings["actClosePane"]
        enabled: window.paneCount > 1 && !prefs.capturing
        onTriggered: window.closePane()
    }
    Action {
        id: actNextPane
        text: qsTr("Nächstes Pane")
        shortcut: Hotkeys.bindings["actNextPane"]
        enabled: window.paneCount > 1 && !prefs.capturing
        onTriggered: window.cyclePane(1)
    }
    Action {
        id: actPrevPane
        text: qsTr("Vorheriges Pane")
        shortcut: Hotkeys.bindings["actPrevPane"]
        enabled: window.paneCount > 1 && !prefs.capturing
        onTriggered: window.cyclePane(-1)
    }
    Action {
        id: actZoomPane
        text: window.zoomedPane >= 0 ? qsTr("Pane-Zoom aufheben") : qsTr("Pane zoomen")
        shortcut: Hotkeys.bindings["actZoomPane"]
        enabled: (window.paneCount > 1 || window.zoomedPane >= 0) && !prefs.capturing
        onTriggered: window.toggleZoom()
    }
    // Befehlspalette: fokussiert das dauerhafte Such-/Befehlsfeld in der Toolbar
    // (öffnet dadurch das Befehls-Popup) und markiert den Inhalt zum Überschreiben.
    Action {
        id: actCommandPalette
        text: qsTr("Befehlspalette …")
        shortcut: Hotkeys.bindings["actCommandPalette"]
        enabled: !prefs.capturing
        // Explizit öffnen (nicht nur über onActiveFocusChanged) — sonst bleibt die
        // Palette tot, wenn das Feld nach einem Befehl noch den Fokus hat.
        onTriggered: { cmdInput.forceActiveFocus(); cmdInput.selectAll(); cmdPopup.openFor() }
    }
    // Session-Navigation (nächste/vorige, mit Umlauf).
    Action {
        id: actNextSession
        text: qsTr("Nächste Session")
        shortcut: Hotkeys.bindings["actNextSession"]
        enabled: sessions.count > 1 && !prefs.capturing
        onTriggered: window.cycleSession(1)
    }
    Action {
        id: actPrevSession
        text: qsTr("Vorige Session")
        shortcut: Hotkeys.bindings["actPrevSession"]
        enabled: sessions.count > 1 && !prefs.capturing
        onTriggered: window.cycleSession(-1)
    }
    // Verbindungs-/Dialog-Aktionen (vorher nur Toolbar/Menü ohne Kürzel).
    Action {
        id: actNewSsh
        text: qsTr("Neue SSH-Verbindung …")
        shortcut: Hotkeys.bindings["actNewSsh"]
        enabled: !prefs.capturing
        onTriggered: sshDialog.open()
    }
    Action {
        id: actNewSerial
        text: qsTr("Neue serielle Verbindung …")
        shortcut: Hotkeys.bindings["actNewSerial"]
        enabled: !prefs.capturing
        onTriggered: serialDialog.openDialog()
    }
    Action {
        id: actConnections
        text: qsTr("Verbindungen verwalten …")
        shortcut: Hotkeys.bindings["actConnections"]
        enabled: !prefs.capturing
        onTriggered: prefs.open("verbindungen")
    }
    Action {
        id: actVault
        text: qsTr("Secrets-Vault …")
        shortcut: Hotkeys.bindings["actVault"]
        enabled: !prefs.capturing
        onTriggered: prefs.open("vault")
    }
    Action {
        id: actMcpToggle
        text: qsTr("MCP-Server umschalten")
        shortcut: Hotkeys.bindings["actMcpToggle"]
        enabled: !prefs.capturing
        onTriggered: mcp.listening ? mcp.stop() : mcp.start()
    }
    Action {
        id: actAbout
        text: qsTr("Über QTmux")
        shortcut: Hotkeys.bindings["actAbout"]
        enabled: !prefs.capturing
        onTriggered: aboutDialog.open()
    }
    // Direktsprung zu Session 1..9 (feste, nicht konfigurierbare Kürzel — sonst
    // würden 9 Einträge die Kürzel-Liste überladen). Ctrl+<N> lädt Session N.
    Shortcut { sequence: "Ctrl+1"; enabled: !prefs.capturing; onActivated: if (windows.count > 0) window.loadWindowRow(0) }
    Shortcut { sequence: "Ctrl+2"; enabled: !prefs.capturing; onActivated: if (windows.count > 1) window.loadWindowRow(1) }
    Shortcut { sequence: "Ctrl+3"; enabled: !prefs.capturing; onActivated: if (windows.count > 2) window.loadWindowRow(2) }
    Shortcut { sequence: "Ctrl+4"; enabled: !prefs.capturing; onActivated: if (windows.count > 3) window.loadWindowRow(3) }
    Shortcut { sequence: "Ctrl+5"; enabled: !prefs.capturing; onActivated: if (windows.count > 4) window.loadWindowRow(4) }
    Shortcut { sequence: "Ctrl+6"; enabled: !prefs.capturing; onActivated: if (windows.count > 5) window.loadWindowRow(5) }
    Shortcut { sequence: "Ctrl+7"; enabled: !prefs.capturing; onActivated: if (windows.count > 6) window.loadWindowRow(6) }
    Shortcut { sequence: "Ctrl+8"; enabled: !prefs.capturing; onActivated: if (windows.count > 7) window.loadWindowRow(7) }
    Shortcut { sequence: "Ctrl+9"; enabled: !prefs.capturing; onActivated: if (windows.count > 8) window.loadWindowRow(8) }

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
                onClicked: prefs.open("verbindungen")
            }
            IconToolButton {
                icon.source: window.icon("key")
                active: Vault.unlocked
                tip: qsTr("Secrets-Vault …")
                onClicked: prefs.open("vault")
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
                            { title: qsTr("Neues Fenster"),              sub: hk("actNewInstance"), icon: "terminal-window", run: function(){ actNewInstance.trigger() } },
                            { title: qsTr("Neue Session"),               sub: hk("actNewSession"), icon: "plus",            run: function(){ window.newSession() } },
                            { title: qsTr("Neue SSH-Verbindung …"),      sub: hk("actNewSsh"), icon: "plugs",           run: function(){ sshDialog.open() } },
                            { title: qsTr("Neue serielle Verbindung …"), sub: hk("actNewSerial"), icon: "usb",             run: function(){ serialDialog.openDialog() } },
                            { title: qsTr("Verbindungen verwalten …"),   sub: hk("actConnections"), icon: "bookmark",        run: function(){ prefs.open("verbindungen") } },
                            { title: qsTr("Secrets-Vault …"),            sub: hk("actVault"), icon: "key",             run: function(){ prefs.open("vault") } },
                            { title: qsTr("Session schließen"),          sub: hk("actCloseSession"), icon: "x",               run: function(){ window.closeCurrent() } },
                            { title: qsTr("Nebeneinander teilen"),       sub: hk("actSplitH"), icon: "split-h",         run: function(){ window.splitPane(Qt.Horizontal) } },
                            { title: qsTr("Untereinander teilen"),       sub: hk("actSplitV"), icon: "split-v",         run: function(){ window.splitPane(Qt.Vertical) } },
                            { title: qsTr("Pane schließen"),             sub: hk("actClosePane"), icon: "x",               run: function(){ window.closePane() } },
                            { title: qsTr("Schrift vergrößern"),         sub: "",             icon: "plus",            run: function(){ window.zoomTerminal(1) } },
                            { title: qsTr("Schrift verkleinern"),        sub: "",             icon: "x",               run: function(){ window.zoomTerminal(-1) } },
                            { title: qsTr("Schriftgröße zurücksetzen"),  sub: hk("actZoomReset"), icon: "gear",            run: function(){ window.resetTerminalZoom() } },
                            { title: qsTr("Eingabe an alle Sessions"),   sub: hk("actBroadcast"), icon: "broadcast-input", run: function(){ window.broadcastInput = !window.broadcastInput } },
                            { title: qsTr("Design umschalten"),          sub: hk("actToggleTheme"), icon: "moon",            run: function(){ Theme.toggle() } },
                            { title: qsTr("Einstellungen …"),            sub: hk("actSettings"), icon: "gear",            run: function(){ prefs.open() } },
                            { title: qsTr("MCP-Server umschalten"),      sub: hk("actMcpToggle"), icon: "broadcast",       run: function(){ mcp.listening ? mcp.stop() : mcp.start() } },
                            { title: qsTr("Kopieren"),                   sub: App.shortcutText("Ctrl+C"), icon: "copy",            run: function(){ if (window.activeTerminal) window.activeTerminal.copy() } },
                            { title: qsTr("Einfügen"),                   sub: App.shortcutText("Ctrl+V"), icon: "clipboard",       run: function(){ if (window.activeTerminal) window.activeTerminal.paste() } },
                            { title: qsTr("Suchen (Scrollback)"),        sub: hk("actFind"), icon: "eye",             run: function(){ if (window.activeTerminal) window.activeTerminal.beginSearch() } },
                            { title: qsTr("Nächste Session"),            sub: hk("actNextSession"), icon: "terminal-window", run: function(){ window.cycleSession(1) } },
                            { title: qsTr("Vorige Session"),             sub: hk("actPrevSession"), icon: "terminal-window", run: function(){ window.cycleSession(-1) } },
                            { title: qsTr("Nächstes Pane"),              sub: hk("actNextPane"), icon: "split-h",         run: function(){ window.cyclePane(1) } },
                            { title: qsTr("Vorheriges Pane"),            sub: hk("actPrevPane"), icon: "split-h",         run: function(){ window.cyclePane(-1) } },
                            { title: window.zoomedPane >= 0 ? qsTr("Pane-Zoom aufheben") : qsTr("Pane zoomen"), sub: hk("actZoomPane"), icon: "eye", run: function(){ window.toggleZoom() } },
                            { title: qsTr("Ligaturen umschalten"),       sub: "",             icon: "terminal-window", run: function(){ window.terminalLigatures = !window.terminalLigatures } },
                            { title: qsTr("GPU-Rendering umschalten"),   sub: "",             icon: "terminal-window", run: function(){ window.terminalGpuRendering = !window.terminalGpuRendering } },
                            { title: qsTr("Auswahl automatisch kopieren"), sub: "",           icon: "copy",            run: function(){ window.copyOnSelect = !window.copyOnSelect } },
                            { title: qsTr("Rechtsklick fügt ein"),       sub: "",             icon: "clipboard",       run: function(){ window.rightClickPaste = !window.rightClickPaste } },
                            { title: qsTr("Vor mehrzeiligem Einfügen warnen"), sub: "",       icon: "info",            run: function(){ window.pasteWarnMultiline = !window.pasteWarnMultiline } },
                            { title: qsTr("Vor dem Beenden nachfragen"),  sub: "",             icon: "info",            run: function(){ window.confirmQuit = !window.confirmQuit } },
                            { title: qsTr("Sessions wiederherstellen: gar nicht"), sub: "",    icon: "terminal-window", run: function(){ window.restoreSessionMode = 0 } },
                            { title: qsTr("Sessions wiederherstellen: ohne Verlauf"), sub: "", icon: "terminal-window", run: function(){ window.restoreSessionMode = 1 } },
                            { title: qsTr("Sessions wiederherstellen: alles"), sub: "",        icon: "terminal-window", run: function(){ window.restoreSessionMode = 2 } },
                            { title: qsTr("Agenten beim Start wiederherstellen"), sub: "",     icon: "robot",           run: function(){ window.restoreAgents = !window.restoreAgents } },
                            { title: qsTr("Unterhaltung fortsetzen: gar nicht"), sub: "",      icon: "robot",           run: function(){ window.resumeAgentMode = 0 } },
                            { title: qsTr("Unterhaltung fortsetzen: jüngste im Verzeichnis"), sub: "", icon: "robot",   run: function(){ window.resumeAgentMode = 1 } },
                            { title: qsTr("Unterhaltung fortsetzen: Auswahl beim Start"), sub: "", icon: "robot",       run: function(){ window.resumeAgentMode = 2 } },
                            { title: qsTr("Unterhaltung fortsetzen: gemeldete Sitzung"), sub: "", icon: "robot",        run: function(){ window.resumeAgentMode = 3 } },
                            { title: qsTr("Design: Wie System"),         sub: "",             icon: "gear",            run: function(){ Theme.mode = Theme.System } },
                            { title: qsTr("Design: Hell"),               sub: "",             icon: "sun",             run: function(){ Theme.mode = Theme.Light } },
                            { title: qsTr("Design: Dunkel"),             sub: "",             icon: "moon",            run: function(){ Theme.mode = Theme.Dark } },
                            { title: qsTr("Sprache: Deutsch"),           sub: "",             icon: "translate",       run: function(){ App.language = "de" } },
                            { title: qsTr("Sprache: English"),           sub: "",             icon: "translate",       run: function(){ App.language = "en" } },
                            { title: qsTr("Über QTmux"),                 sub: hk("actAbout"), icon: "info",            run: function(){ aboutDialog.open() } },
                            { title: qsTr("Beenden"),                    sub: hk("actQuit"), icon: "x",               run: function(){ window.requestQuit() } },
                        ]
                        // Quake-Modus ist nur auf macOS aktiv (globaler Carbon-Hotkey).
                        if (Qt.platform.os === "osx")
                            c.push({ title: qsTr("Quake-Modus umschalten"), sub: "", icon: "terminal-window",
                                     run: function(){ window.toggleQuake() } })
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
                            // SFTP war bisher NUR über einen Profil-Button tief in den
                            // Einstellungen erreichbar — hier je SSH-Profil (type 1) auffindbar.
                            if (profs[j].type === 1)
                                c.push({ title: qsTr("SFTP: %1").arg(profs[j].name),
                                         sub: window.profileSummary(profs[j]),
                                         icon: "bookmark",
                                         run: (function(p){ return function(){ window.openSftp(p) } })(profs[j]) })
                        }
                        // Window-Gruppen (QTMUX-83, Stufe 5): dieselben Operationen wie das
                        // Rechtsklick-Menü der Kachel bzw. des Gruppenkopfs. Wirken auf das
                        // AKTIVE Window (windows.activeRow wird ERST beim Ausführen gelesen).
                        if (windows.count > 0) {
                            var cw = window.activeWindowObj()
                            var cwTitle = cw ? window.windowTitle(cw) : qsTr("Aktives Fenster")
                            c.push({ title: qsTr("Fenster gruppieren …"), sub: cwTitle, icon: "bookmark",
                                     run: function(){ groupNameDialog.start(windows.activeRow) } })
                            var gs = windows.groups()
                            for (var gi = 0; gi < gs.length; ++gi) {
                                c.push({ title: qsTr("Fenster zu Gruppe: %1").arg(gs[gi]), sub: cwTitle,
                                         icon: "bookmark",
                                         run: (function(n){ return function(){ windows.setWindowGroup(windows.activeRow, n) } })(gs[gi]) })
                            }
                            c.push({ title: qsTr("Fenster aus Gruppe nehmen"), sub: cwTitle, icon: "x",
                                     run: function(){ windows.setWindowGroup(windows.activeRow, "") } })
                            for (var gj = 0; gj < gs.length; ++gj) {
                                c.push({ title: qsTr("Gruppe umbenennen: %1 …").arg(gs[gj]), sub: qsTr("Gruppe"),
                                         icon: "bookmark",
                                         run: (function(n){ return function(){ groupNameDialog.startRename(n) } })(gs[gj]) })
                                c.push({ title: qsTr("Gruppe auflösen: %1").arg(gs[gj]), sub: qsTr("Gruppe"),
                                         icon: "trash",
                                         run: (function(n){ return function(){ windows.renameGroup(n, "") } })(gs[gj]) })
                                c.push({ title: qsTr("Gruppe nach oben: %1").arg(gs[gj]), sub: qsTr("Gruppe"),
                                         icon: "bookmark",
                                         run: (function(n){ return function(){ window.moveGroupBy(n, -1) } })(gs[gj]) })
                                c.push({ title: qsTr("Gruppe nach unten: %1").arg(gs[gj]), sub: qsTr("Gruppe"),
                                         icon: "bookmark",
                                         run: (function(n){ return function(){ window.moveGroupBy(n, 1) } })(gs[gj]) })
                            }
                        }
                        for (var i = 0; i < windows.count; ++i) {
                            var w = windows.windowAt(i)
                            var t = window.windowTitle(w)
                            c.push({ title: qsTr("Wechseln zu: %1").arg(t), sub: qsTr("Fenster"),
                                     icon: "terminal-window",
                                     run: (function(id){ return function(){ window.loadWindow(id) } })(w.windowId) })
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
                onClicked: prefs.open()
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
            ShortcutMenuItem { action: actNewInstance; icon.source: window.icon("terminal-window"); icon.color: Theme.menuIcon; icon.width: 16; icon.height: 16 }
            MenuSeparator {}
            ShortcutMenuItem { action: actNewSession; icon.source: window.icon("plus"); icon.color: Theme.menuIcon; icon.width: 16; icon.height: 16 }
            ShortcutMenuItem { action: actNewSsh;     icon.source: window.icon("plugs");    icon.color: Theme.menuIcon; icon.width: 16; icon.height: 16 }
            ShortcutMenuItem { action: actNewSerial;  icon.source: window.icon("usb");      icon.color: Theme.menuIcon; icon.width: 16; icon.height: 16 }
            ShortcutMenuItem { action: actConnections; icon.source: window.icon("bookmark"); icon.color: Theme.menuIcon; icon.width: 16; icon.height: 16 }
            ShortcutMenuItem { action: actVault;      icon.source: window.icon("key");      icon.color: Theme.menuIcon; icon.width: 16; icon.height: 16 }
            ShortcutMenuItem { action: actCloseSession; icon.source: window.icon("x"); icon.color: Theme.menuIcon; icon.width: 16; icon.height: 16 }
            MenuSeparator {}
            ShortcutMenuItem { action: actNextSession }
            ShortcutMenuItem { action: actPrevSession }
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
            // Direkt über „Beenden“, weil die Option genau dessen Verhalten steuert
            // (QTMUX-46: stand vorher NUR im Einstellungsdialog, während die anderen
            // Komfort-Schalter überall erreichbar sind).
            ShortcutMenuItem {
                text: qsTr("Vor dem Beenden nachfragen")
                icon.source: window.icon("info"); icon.color: Theme.menuIcon; icon.width: 16; icon.height: 16
                checkable: true
                checked: window.confirmQuit
                onTriggered: window.confirmQuit = !window.confirmQuit
            }
            // Gegenstück dazu: was beim NÄCHSTEN Start zurückkommt (QTMUX-99).
            // Auch in den Einstellungen und in der Palette erreichbar (QTMUX-46).
            ThemedMenu {
                title: qsTr("Sessions beim Start wiederherstellen")
                Repeater {
                    model: [qsTr("Gar nicht"), qsTr("Ohne Verlauf"), qsTr("Alles")]
                    ShortcutMenuItem {
                        required property int index
                        required property string modelData
                        text: modelData
                        checkable: true
                        checked: window.restoreSessionMode === index
                        onTriggered: window.restoreSessionMode = index
                    }
                }
            }
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
            ShortcutMenuItem { action: actFind; icon.source: window.icon("eye"); icon.color: Theme.menuIcon; icon.width: 16; icon.height: 16 }
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
            ShortcutMenuItem { action: actNextPane; enabled: window.paneCount > 1 }
            ShortcutMenuItem { action: actPrevPane; enabled: window.paneCount > 1 }
            ShortcutMenuItem { action: actZoomPane; icon.source: window.icon("eye"); icon.color: Theme.menuIcon; icon.width: 16; icon.height: 16 }
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
            MenuSeparator {}
            // Auch in den Einstellungen und in der Palette erreichbar (QTMUX-46).
            ShortcutMenuItem {
                text: qsTr("Agenten beim Start wiederherstellen")
                checkable: true
                checked: window.restoreAgents
                onTriggered: window.restoreAgents = !window.restoreAgents
            }
            ThemedMenu {
                title: qsTr("Unterhaltung fortsetzen")
                enabled: window.restoreAgents
                Repeater {
                    model: [qsTr("Gar nicht"), qsTr("Jüngste im Verzeichnis"),
                            qsTr("Auswahl beim Start"), qsTr("Gemeldete Sitzung")]
                    ShortcutMenuItem {
                        required property int index
                        required property string modelData
                        text: modelData
                        checkable: true
                        checked: window.resumeAgentMode === index
                        onTriggered: window.resumeAgentMode = index
                    }
                }
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
                    // Sidebar = Windows (Tabs, QTMUX-83). Jede Kachel ist ein Window mit
                    // eigenem Split-Layout; Klick aktiviert das Window. Gruppen (Window-
                    // Gruppen, Stufe 5) über ListView-Sections — das Model hält sie als
                    // zusammenhängende Blöcke.
                    model: windows
                    currentIndex: windows.activeRow

                    section.property: "group"
                    section.criteria: ViewSection.FullString
                    section.delegate: Item {
                        id: groupHeader
                        required property string section
                        width: sessionList.width
                        height: section.length > 0 ? 26 : 0
                        visible: section.length > 0

                        readonly property bool collapsed: window.isGroupCollapsed(section)
                        z: hdrDrag.active ? 3 : 0
                        opacity: hdrDrag.active ? 0.85 : 1.0
                        property real dragDy: 0
                        transform: Translate { y: groupHeader.dragDy }

                        Rectangle {
                            anchors.fill: parent
                            anchors.topMargin: 4
                            anchors.bottomMargin: 2
                            radius: 6
                            color: hdrDrag.active ? Theme.sidebarSelected
                                 : hdrHover.hovered ? Theme.sidebarHover : "transparent"
                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 6
                                anchors.rightMargin: 6
                                spacing: 6
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
                                    text: (window.groupsRevision, window.groupSize(groupHeader.section))
                                    color: Theme.textDim
                                    font.pixelSize: 10
                                }
                            }
                            HoverHandler { id: hdrHover }
                            TapHandler { onTapped: window.toggleGroupCollapsed(groupHeader.section) }
                            TapHandler {
                                acceptedButtons: Qt.RightButton
                                onTapped: { groupMenu.groupName = groupHeader.section; groupMenu.popup() }
                            }
                            DragHandler {
                                id: hdrDrag
                                // Wie beim Kachel-Drag: nicht die Position des vom ListView
                                // verwalteten Elements anfassen, sondern nur eine Transform.
                                target: null
                                xAxis.enabled: false
                                yAxis.enabled: true
                                onActiveTranslationChanged: if (active) groupHeader.dragDy = activeTranslation.y
                                onActiveChanged: {
                                    if (active) { groupHeader.dragDy = 0; return }
                                    const cy = groupHeader.y + groupHeader.dragDy + groupHeader.height / 2
                                    groupHeader.dragDy = 0
                                    const target = window.rowNearestTo(cy, -1)
                                    if (target >= 0) window.moveGroupToRow(groupHeader.section, target)
                                    sessionList.forceLayout()
                                }
                            }
                        }
                    }

                    delegate: Rectangle {
                        id: tile
                        required property int index
                        required property int windowId
                        required property string group
                        required property var windowObject
                        readonly property var wobj: windowObject
                        // Aggregierte Anzeige aus den Panes/Sessions — in QML berechnet;
                        // die Revision-Anker (sessionsRevision/windowsRevision) halten die
                        // Bindungen live (analog groupsRevision).
                        readonly property string dispTitle: (window.sessionsRevision, window.windowsRevision, window.windowTitle(wobj))
                        readonly property int paneN: (window.windowsRevision, wobj ? wobj.sessionIds().length : 0)
                        readonly property int aggState: (window.sessionsRevision, window.windowRunState(wobj))
                        readonly property bool attention: (window.sessionsRevision, window.windowAttention(wobj))
                        readonly property bool controller: (window.sessionsRevision, window.windowController(wobj))
                        readonly property int activeSid: (window.sessionsRevision, window.windowsRevision, window.windowActiveSessionId(wobj))
                        readonly property bool selected: tile.index === windows.activeRow
                        // Eingeklappte Gruppen: Kachel verschwindet (Window läuft weiter).
                        readonly property bool hidden: group.length > 0 && window.isGroupCollapsed(group)
                        // Gruppierte Kacheln eingerückt (QTMUX-45-Muster): Zugehörigkeit an der
                        // Form erkennbar; die Farbmarke sitzt in der Einzugsspalte.
                        readonly property real groupIndent: group.length > 0 ? 12 : 0

                        width: ListView.view.width
                        visible: !hidden
                        height: hidden ? 0 : 52
                        color: "transparent"      // Kachel-Optik liegt im `card`

                        z: dragH.active ? 2 : 0
                        opacity: dragH.active ? 0.85 : 1.0
                        scale: dragH.active ? 1.02 : 1.0
                        // Optischer Versatz beim Ziehen. Bewusst eine Transform statt einer
                        // Positionsänderung: die Layout-Position gehört dem ListView (s. dragH).
                        property real dragDy: 0
                        transform: Translate { y: tile.dragDy }

                        HoverHandler { id: hover }
                        TapHandler { onTapped: window.loadWindowRow(tile.index) }
                        // Rechtsklick: Window-Kontextmenü (umbenennen/Gruppe/schließen).
                        TapHandler {
                            acceptedButtons: Qt.RightButton
                            onTapped: {
                                windowMenu.row = tile.index
                                windowMenu.windowId = tile.windowId
                                windowMenu.currentName = (tile.wobj && tile.wobj.name) ? tile.wobj.name : ""
                                windowMenu.currentGroup = tile.group
                                windowMenu.isController = tile.controller
                                windowMenu.groupList = windows.groups()
                                windowMenu.popup()
                            }
                        }
                        // Drag-to-Reorder: vertikal ziehen, Zielzeile aus der Position bestimmen.
                        DragHandler {
                            id: dragH
                            // ⚠️ `target` bleibt NULL — die Kachel wird optisch über eine
                            // Translate-Transform versetzt (s. `transform` oben), nicht über
                            // ihre Position. Ein ListView vergibt die `y` seiner Delegates
                            // selbst und leitet daraus die Ausdehnung des Inhalts ab; wird
                            // die LETZTE Kachel per `target` nach oben gezogen, schrumpft
                            // diese Ausdehnung, Flickable korrigiert `contentY` ins Negative
                            // und schiebt damit alle übrigen Kacheln nach unten. Die
                            // Korrektur verschiebt die gezogene Kachel erneut gegenüber dem
                            // Zeiger → nächste Korrektur: die Liste läuft weg, bis nichts
                            // mehr im Bild ist. Gemessen: contentY 0 → −6 → −52 → −100 →
                            // −163 → −213 …, und nur beim Ziehen der letzten Kachel.
                            // Eine Transform ist rein visuell und lässt das Layout in Ruhe.
                            target: null
                            xAxis.enabled: false
                            yAxis.enabled: true
                            onActiveTranslationChanged: if (active) tile.dragDy = activeTranslation.y
                            onActiveChanged: {
                                if (active) { tile.dragDy = 0; return }
                                const from = tile.index
                                // Zielzeile aus der GEZOGENEN Lage (Layout-y + optischer Versatz).
                                const ni = window.rowNearestTo(tile.y + tile.dragDy + tile.height / 2, from)
                                tile.dragDy = 0
                                if (ni >= 0 && ni !== from) window.moveWindowRow(from, ni)
                                sessionList.forceLayout()
                            }
                        }

                        // Kachelfläche (Auswahl/Hover) — respektiert den Einzug.
                        Rectangle {
                            id: card
                            anchors.fill: parent
                            anchors.leftMargin: tile.groupIndent
                            radius: 8
                            color: tile.selected ? Theme.sidebarSelected
                                 : hover.hovered ? Theme.sidebarHover : "transparent"
                        }

                        // Farbmarke der Gruppe (in der Einzugsspalte links vor der Kachel).
                        Rectangle {
                            visible: tile.group.length > 0
                            width: 3; radius: 1.5
                            color: window.groupColor(tile.group)
                            anchors.left: parent.left
                            anchors.leftMargin: 4
                            anchors.verticalCenter: parent.verticalCenter
                            height: parent.height - 14
                        }

                        // Roter Tab: irgendein Pane dieses Windows ist MCP-Controller.
                        Rectangle {
                            visible: tile.controller
                            width: 3; radius: 1.5
                            color: "#e5534b"
                            anchors.left: parent.left
                            anchors.leftMargin: tile.groupIndent
                            anchors.verticalCenter: parent.verticalCenter
                            height: parent.height - 14
                        }

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 10 + tile.groupIndent
                            anchors.rightMargin: 10
                            spacing: 10

                            // Aggregierter Status-Ring: Aufmerksamkeit (blau, pulsierend) hat
                            // Vorrang, sonst der dringlichste Pane-Zustand
                            // (0=Start 1=Läuft 2=WartetEingabe 3=Fehler 4=Zu).
                            Rectangle {
                                id: statusRing
                                width: 10; height: 10; radius: 5
                                color: tile.attention ? Theme.accent
                                     : tile.aggState === 1 ? "#46d369"
                                     : tile.aggState === 2 ? "#f5c451"
                                     : tile.aggState === 3 ? "#e5534b"
                                     : tile.aggState === 4 ? "#5a5d6a"
                                     : Theme.textDim
                                SequentialAnimation on opacity {
                                    running: tile.attention && !App.reduceMotion
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
                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 6
                                    Text {
                                        text: tile.dispTitle
                                        color: Theme.textBright
                                        font.pixelSize: 13
                                        elide: Text.ElideRight
                                        Layout.fillWidth: true
                                    }
                                    // Pane-Zahl (nur bei Splits) als kleines Badge.
                                    Text {
                                        visible: tile.paneN > 1
                                        text: "▦ " + tile.paneN
                                        color: Theme.textDim
                                        font.pixelSize: 10
                                        Layout.alignment: Qt.AlignVCenter
                                    }
                                    // Stabile SESSION-ID des aktiven Panes (QTMUX-44): genau
                                    // die Nummer für send_text/read_screen. Bewusst NICHT die
                                    // Window-ID — die ist ein internes, stetig wachsendes
                                    // Adress-Token und wirkte als Kachel-Nummer wie ein
                                    // durchlaufender Zähler (QTMUX-87).
                                    Text {
                                        text: tile.activeSid >= 0 ? "#" + tile.activeSid : ""
                                        color: Theme.textDim
                                        font.pixelSize: 10
                                        font.family: window.terminalFontFamily
                                        Layout.alignment: Qt.AlignVCenter
                                    }
                                }
                            }

                            // Schließen-Button (×) — schließt das ganze Window (alle Panes).
                            Rectangle {
                                Layout.preferredWidth: 20
                                Layout.preferredHeight: 20
                                radius: 4
                                visible: hover.hovered || tile.selected
                                color: closeHover.hovered ? Theme.border : "transparent"
                                Image {
                                    anchors.centerIn: parent
                                    source: window.icon("x")
                                    sourceSize.width: 12
                                    sourceSize.height: 12
                                    layer.enabled: true
                                    layer.effect: MultiEffect {
                                        brightness: 1.0
                                        colorization: 1.0
                                        colorizationColor: Theme.textDim
                                    }
                                }
                                HoverHandler { id: closeHover }
                                TapHandler { onTapped: window.closeWindowRow(tile.index) }
                            }
                        }

                        // Aufmerksamkeit: der ganze Tab pulsiert mit blauem Rahmen.
                        Rectangle {
                            anchors.fill: parent
                            radius: 8
                            color: "transparent"
                            visible: tile.attention
                            border.color: Theme.accent
                            border.width: 2
                            SequentialAnimation on opacity {
                                running: tile.attention && !App.reduceMotion
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
                    // Theme.selection/Theme.hover existieren nicht (waren undefiniert) →
                    // die echten Selektionsfarben (im Dark Mode dunkel, helle Schrift lesbar).
                    color: sftpList.currentIndex === index ? Theme.sidebarSelected
                                                           : (hov.hovered ? Theme.sidebarHover : "transparent")
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
                            // Monochromes SVG themegerecht tönen (sonst schwarz/dunkel im
                            // Dark Mode). brightness hebt es erst auf Weiß, dann colorize.
                            layer.enabled: true
                            layer.effect: MultiEffect {
                                brightness: 1.0
                                colorization: 1.0
                                colorizationColor: Theme.textBright
                            }
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

    // Toast-Overlay (window.notifyToast): kurzer Hinweis unten mittig, blendet sich weg.
    Rectangle {
        id: toast
        property string text: ""
        function restart() { opacity = 1; hideTimer.restart() }
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 24
        z: 1000
        visible: opacity > 0
        opacity: 0
        width: toastText.implicitWidth + 28
        height: toastText.implicitHeight + 18
        radius: 8
        color: Theme.bgElevated
        border.color: Theme.border
        border.width: 1
        Behavior on opacity { NumberAnimation { duration: 200 } }
        Text {
            id: toastText
            anchors.centerIn: parent
            text: toast.text
            color: Theme.textBright
            font.pixelSize: 12
        }
        Timer { id: hideTimer; interval: 2600; onTriggered: toast.opacity = 0 }
    }

    // --- Gruppen-Kontextmenüs (QTMUX-42) ------------------------------------
    // Rechtsklick auf eine Kachel: Gruppe zuordnen, wechseln oder verlassen.
    ThemedMenu {
        id: sessionMenu
        property int row: -1
        property string currentGroup: ""
        property bool isController: false

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
                id: groupItem
                required property string modelData
                text: modelData
                checkable: true
                // Klick auf die BEREITS angehakte Gruppe = daraus entfernen (""); jede andere
                // = dorthin verschieben. Vergleich über currentGroup (Modellwahrheit beim
                // Öffnen), NICHT über den getoggelten `checked`-Zustand — der wechselt beim
                // Klick und wäre hier schon invertiert. Ohne diese Fallunterscheidung rief das
                // Abwählen setSessionGroup(row, gleicheGruppe) → No-op (früher Return im Model),
                // der Haken verschwand, aber Farbmarke/Einrückung blieben stehen.
                onTriggered: sessions.setSessionGroup(sessionMenu.row,
                                 groupItem.modelData === sessionMenu.currentGroup ? "" : groupItem.modelData)
                // Haken BINDUNGSSICHER: ein Klick setzt `checked` imperativ und zerstört eine
                // gewöhnliche `checked:`-Bindung; zudem recycelt der Repeater Delegates gleicher
                // Länge (statt sie neu zu bauen), sodass der Haken sonst auf dem zuletzt
                // getoggelten Stand einfror (Gruppe angehakt selbst bei ungruppierten Shells).
                // Das Binding-Element erzwingt den Wert erneut — inklusive `sessionMenu.opened`,
                // damit es bei JEDEM Öffnen neu greift, auch wenn currentGroup unverändert bleibt.
                Binding {
                    target: groupItem
                    property: "checked"
                    value: sessionMenu.opened && groupItem.modelData === sessionMenu.currentGroup
                }
            }
        }
        AppMenuItem {
            text: qsTr("Neue Gruppe …")
            // In-Window-Kontextmenü → App-Theme-Tönung (AppMenuItem-Default Theme.textBright),
            // NICHT Theme.menuIcon: das folgt auf macOS dem System und wäre bei App=Dunkel/
            // System=Hell dunkel auf dunklem Grund.
            icon.source: window.icon("plus")
            onTriggered: groupNameDialog.start(sessionMenu.row)
        }
        AppMenuItem {
            text: qsTr("Aus Gruppe entfernen")
            enabled: sessionMenu.currentGroup.length > 0
            icon.source: window.icon("x")
            onTriggered: sessions.setSessionGroup(sessionMenu.row, "")
        }
        // Roter MCP-Controller-Tab lässt sich sonst vom Menschen nicht zurücksetzen
        // (attach_controller ist MCP-only) — bleibt ein steuernder Agent ohne Abmeldung
        // weg, hängt der Tab. Nur sichtbar, wenn diese Kachel gerade Controller ist.
        MenuSeparator { visible: sessionMenu.isController }
        AppMenuItem {
            text: qsTr("Controller-Markierung entfernen")
            visible: sessionMenu.isController
            height: visible ? implicitHeight : 0
            icon.source: window.icon("robot")
            onTriggered: sessions.clearMcpController(sessionMenu.row)
        }
    }

    // Rechtsklick auf eine Gruppen-Kopfzeile: umbenennen oder auflösen.
    ThemedMenu {
        id: groupMenu
        property string groupName: ""
        AppMenuItem {
            text: qsTr("Gruppe nach oben")
            onTriggered: window.moveGroupBy(groupMenu.groupName, -1)
        }
        AppMenuItem {
            text: qsTr("Gruppe nach unten")
            onTriggered: window.moveGroupBy(groupMenu.groupName, 1)
        }
        MenuSeparator {}
        AppMenuItem {
            text: qsTr("Gruppe umbenennen …")
            onTriggered: groupNameDialog.startRename(groupMenu.groupName)
        }
        AppMenuItem {
            text: qsTr("Gruppe auflösen")
            icon.source: window.icon("x")   // App-Theme-Tönung (s. sessionMenu oben)
            // Auflösen betrifft nur die Zuordnung — die Windows/Sessions laufen weiter.
            onTriggered: windows.renameGroup(groupMenu.groupName, "")
        }
    }

    // Name für eine neue Gruppe bzw. für das Umbenennen einer bestehenden.
    AppDialog {
        id: groupNameDialog
        width: 380
        property int row: -1          // >= 0: Window dieser Zeile zuordnen
        property string renaming: ""  // nicht leer: bestehende Gruppe umbenennen
        title: renaming.length > 0 ? qsTr("Gruppe umbenennen") : qsTr("Neue Gruppe")
        standardButtons: Dialog.Ok | Dialog.Cancel

        function start(r) { row = r; renaming = ""; groupNameField.text = ""; open() }
        function startRename(name) { row = -1; renaming = name; groupNameField.text = name; open() }

        onOpened: { groupNameField.forceActiveFocus(); groupNameField.selectAll() }
        onAccepted: {
            const name = groupNameField.text.trim()
            if (name.length === 0) return
            if (renaming.length > 0) windows.renameGroup(renaming, name)
            else if (row >= 0) windows.setWindowGroup(row, name)
        }
        ColumnLayout {
            spacing: 8
            Label {
                Layout.preferredWidth: 340
                wrapMode: Text.WordWrap
                color: Theme.textDim
                font.pixelSize: 11
                text: qsTr("Fenster einer Gruppe stehen in der Seitenleiste zusammen und lassen sich gemeinsam ein- und ausklappen.")
            }
            TextField {
                id: groupNameField
                Layout.preferredWidth: 340
                placeholderText: qsTr("z. B. Release 1.5")
                onAccepted: groupNameDialog.accept()
            }
        }
    }

    // --- Window-Kontextmenü (QTMUX-83) --------------------------------------
    // Rechtsklick auf eine Sidebar-Kachel (= Window): umbenennen oder schließen.
    ThemedMenu {
        id: windowMenu
        property int row: -1
        property int windowId: -1
        property string currentName: ""
        property string currentGroup: ""
        property var groupList: []
        property bool isController: false
        AppMenuItem {
            text: qsTr("Umbenennen …")
            icon.source: window.icon("terminal-window")
            onTriggered: windowRenameDialog.start(windowMenu.windowId, windowMenu.currentName)
        }
        AppMenuItem {
            text: qsTr("Automatischer Name")
            enabled: windowMenu.currentName.length > 0
            icon.source: window.icon("x")
            onTriggered: { const w = windows.windowById(windowMenu.windowId); if (w) w.name = "" }
        }
        MenuSeparator {}
        // --- Window-Gruppen (QTMUX-83, Stufe 5) ---
        MenuItem {
            enabled: false
            height: 26
            contentItem: Text {
                text: qsTr("Gruppe"); color: Theme.textDim
                font.pixelSize: 10; font.bold: true
                verticalAlignment: Text.AlignVCenter; leftPadding: 8
            }
        }
        Repeater {
            model: windowMenu.groupList
            delegate: AppMenuItem {
                id: wGroupItem
                required property string modelData
                text: modelData
                checkable: true
                onTriggered: windows.setWindowGroup(windowMenu.row,
                                 wGroupItem.modelData === windowMenu.currentGroup ? "" : wGroupItem.modelData)
                Binding {
                    target: wGroupItem
                    property: "checked"
                    value: windowMenu.opened && wGroupItem.modelData === windowMenu.currentGroup
                }
            }
        }
        AppMenuItem {
            text: qsTr("Neue Gruppe …")
            icon.source: window.icon("plus")
            onTriggered: groupNameDialog.start(windowMenu.row)
        }
        AppMenuItem {
            text: qsTr("Aus Gruppe entfernen")
            enabled: windowMenu.currentGroup.length > 0
            icon.source: window.icon("x")
            onTriggered: windows.setWindowGroup(windowMenu.row, "")
        }
        MenuSeparator {}
        AppMenuItem {
            text: qsTr("Fenster schließen")
            icon.source: window.icon("x")
            onTriggered: window.closeWindowRow(windowMenu.row)
        }
        // Roter MCP-Controller-Tab lässt sich sonst vom Menschen nicht zurücksetzen
        // (attach_controller ist MCP-only) — für alle Panes des Windows entfernen.
        MenuSeparator { visible: windowMenu.isController }
        AppMenuItem {
            text: qsTr("Controller-Markierung entfernen")
            visible: windowMenu.isController
            height: visible ? implicitHeight : 0
            icon.source: window.icon("robot")
            onTriggered: {
                const w = windows.windowById(windowMenu.windowId)
                if (!w) return
                const ids = w.sessionIds()
                for (let i = 0; i < ids.length; ++i) {
                    const r = window.rowForSessionId(ids[i])
                    if (r >= 0) sessions.clearMcpController(r)
                }
            }
        }
    }

    AppDialog {
        id: windowRenameDialog
        width: 380
        property int windowId: -1
        title: qsTr("Fenster umbenennen")
        standardButtons: Dialog.Ok | Dialog.Cancel
        function start(id, name) { windowId = id; windowRenameField.text = name; open() }
        onOpened: { windowRenameField.forceActiveFocus(); windowRenameField.selectAll() }
        onAccepted: {
            const w = windows.windowById(windowRenameDialog.windowId)
            if (w) w.name = windowRenameField.text.trim()
        }
        ColumnLayout {
            spacing: 8
            Label {
                Layout.preferredWidth: 340
                wrapMode: Text.WordWrap
                color: Theme.textDim
                font.pixelSize: 11
                text: qsTr("Leer lassen = automatischer Name (Titel des aktiven Panes).")
            }
            TextField {
                id: windowRenameField
                Layout.preferredWidth: 340
                placeholderText: qsTr("z. B. Build, Server, Logs")
                onAccepted: windowRenameDialog.accept()
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



}
