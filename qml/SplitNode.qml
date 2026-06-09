import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QTmux

// Ein Knoten des Split-Baums — rekursiv:
//   Blatt:  { paneId, sessionRow }                       -> Terminal-Pane
//   Split:  { orientation, children: [node, node, ...] } -> verschachtelter SplitView
// Erlaubt beliebige H+V-Mischungen (QTMUX-3). Der Knoten redet ausschließlich mit
// `win` (der ApplicationWindow aus Main.qml) — keine direkten Modell-Zugriffe.
Item {
    id: root
    property var node                                   // Layout-Knoten (Blatt oder Split)
    property var win                                    // ApplicationWindow (Globals/Callbacks)
    readonly property bool isLeaf: node && node.children === undefined

    // Greifen, sobald dieser Knoten direktes Kind eines Eltern-SplitView ist.
    SplitView.fillWidth: true
    SplitView.fillHeight: true
    SplitView.minimumWidth: 140
    SplitView.minimumHeight: 90

    Loader {
        anchors.fill: parent
        // Nichts laden, solange noch kein Knoten gesetzt ist (vermeidet Null-Zugriffe).
        sourceComponent: !root.node ? null : (root.isLeaf ? leafComp : splitComp)
    }

    // --- Verschachtelter Split (rekursiv) --------------------------------
    Component {
        id: splitComp
        SplitView {
            orientation: (root.node && root.node.orientation !== undefined)
                         ? root.node.orientation : Qt.Horizontal
            handle: Rectangle {
                implicitWidth: 6
                implicitHeight: 6
                color: SplitHandle.pressed ? Theme.accent
                     : SplitHandle.hovered ? Theme.border : Theme.bgMain
            }
            Repeater {
                model: (root.node && root.node.children) ? root.node.children.length : 0
                // Rekursion über einen Loader mit source-URL: QML verbietet die
                // statische Selbst-Instanziierung eines Typs, dynamisches Laden umgeht das.
                // Wichtig: node/win per setSource VOR dem Laden setzen — sonst evaluieren
                // die inneren Bindungen (session: win.sessionObject(...)) kurz mit win===undefined
                // und brechen dauerhaft (weißer Header, kein Inhalt).
                delegate: Loader {
                    required property int index
                    SplitView.fillWidth: true
                    SplitView.fillHeight: true
                    SplitView.minimumWidth: 140
                    SplitView.minimumHeight: 90
                    Component.onCompleted: setSource(Qt.resolvedUrl("SplitNode.qml"),
                                                     { node: root.node.children[index], win: root.win })
                }
            }
        }
    }

    // --- Blatt: Terminal-Pane --------------------------------------------
    Component {
        id: leafComp
        Rectangle {
            id: pane
            readonly property int paneId: root.node.paneId
            readonly property int sessionRow: root.node.sessionRow
            property alias term: paneTerm

            color: Theme.bgMain
            radius: win.paneCount > 1 ? 6 : 0
            // Aktives Pane bei Mehrfach-Layout durch Akzentrahmen markieren.
            border.width: win.paneCount > 1 && paneId === win.activePaneId ? 2 : 0
            border.color: Theme.accent

            // Registrierung (Fokus nach Baum-Rebuild) + aktives Pane fokussieren.
            Component.onCompleted: {
                win.registerPane(paneId, paneTerm)
                if (paneId === win.activePaneId) {
                    win.activeTerminal = paneTerm
                    paneTerm.forceActiveFocus()
                }
            }
            Component.onDestruction: win.unregisterPane(paneId)

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 6
                spacing: 0

                // Pane-Kopf nur im Mehrfach-Layout: Greifpunkt (Reorder), Titel, Schließen.
                Rectangle {
                    id: header
                    visible: win.paneCount > 1
                    Layout.fillWidth: true
                    Layout.maximumHeight: visible ? 22 : 0
                    implicitHeight: visible ? 22 : 0
                    radius: 4
                    color: paneId === win.activePaneId
                           ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.18)
                           : Theme.bgElevated

                    // Greifpunkt (sechs Punkte) — Pane-Reorder per Drag (QTMUX-4).
                    // Deterministischer DragHandler + Hit-Test in window.paneIdAt:
                    // beim Loslassen tauscht das überfahrene Pane den Inhalt.
                    Rectangle {
                        id: grip
                        width: 22; height: 18; radius: 4
                        anchors.left: parent.left
                        anchors.leftMargin: 2
                        anchors.verticalCenter: parent.verticalCenter
                        color: dragGrip.active ? Theme.accent
                             : gripHover.hovered ? Theme.sidebarHover : "transparent"
                        z: 100

                        Grid {
                            anchors.centerIn: parent
                            columns: 2
                            rowSpacing: 3
                            columnSpacing: 3
                            Repeater {
                                model: 6
                                delegate: Rectangle {
                                    width: 2; height: 2; radius: 1
                                    color: dragGrip.active ? "#ffffff" : Theme.textDim
                                }
                            }
                        }

                        HoverHandler { id: gripHover; cursorShape: Qt.OpenHandCursor }
                        DragHandler {
                            id: dragGrip
                            target: null                  // nichts bewegen — nur Geste verfolgen
                            onActiveChanged: {
                                if (active) win.beginPaneDrag(pane.paneId)
                                else win.endPaneDrag()
                            }
                            onCentroidChanged: if (active) win.updatePaneDrag(centroid.scenePosition)
                        }
                    }

                    Text {
                        anchors.centerIn: parent
                        width: parent.width - 64
                        horizontalAlignment: Text.AlignHCenter
                        elide: Text.ElideMiddle
                        text: paneTerm.session ? paneTerm.session.title : ""
                        color: paneId === win.activePaneId ? Theme.textBright : Theme.textDim
                        font.pixelSize: 11
                        font.bold: paneId === win.activePaneId
                    }

                    // Schließen-Knopf des Panes.
                    Rectangle {
                        anchors.right: parent.right
                        anchors.rightMargin: 2
                        anchors.verticalCenter: parent.verticalCenter
                        width: 18; height: 18; radius: 4
                        color: closeMA.containsMouse ? Theme.sidebarHover : "transparent"
                        Text {
                            anchors.centerIn: parent
                            text: "×"
                            color: Theme.textDim
                            font.pixelSize: 14
                        }
                        MouseArea {
                            id: closeMA
                            anchors.fill: parent
                            hoverEnabled: true
                            onClicked: { win.setActivePaneById(pane.paneId, paneTerm); win.closePane() }
                        }
                    }
                }

                TerminalItem {
                    id: paneTerm
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    pointSize: win.terminalFontSize          // globaler Zoom
                    fontFamily: win.terminalFontFamily
                    ligatures: win.terminalLigatures
                    backgroundColor: Theme.terminalBg
                    foregroundColor: Theme.terminalFg
                    cursorColor: Theme.terminalCursor
                    session: win.sessionObject(pane.sessionRow)
                    // Broadcast-Modus: Eingabe an ALLE Sessions verteilen.
                    broadcast: win.broadcastInput
                    onInputForBroadcast: (data) => win.broadcastWrite(data)
                    // Komfortoptionen (PuTTY-Stil) + Multiline-Paste-Warnung.
                    copyOnSelect: win.copyOnSelect
                    rightClickPaste: win.rightClickPaste
                    pasteWarnMultiline: win.pasteWarnMultiline
                    onMultilinePasteWarning: (lines) => win.askMultilinePaste(paneTerm, lines)
                    // Fokus (Klick/Tab) macht dieses Pane aktiv.
                    onActiveFocusChanged: if (activeFocus) win.setActivePaneById(pane.paneId, paneTerm)
                    // Cmd/Strg+Mausrad -> global zoomen.
                    onZoomRequested: (delta) => win.zoomTerminal(delta)
                    // Rechtsklick -> erst Pane aktivieren, dann Kontextmenü.
                    onContextMenuRequested: {
                        win.setActivePaneById(pane.paneId, paneTerm)
                        win.popupTermContextMenu(paneTerm)
                    }
                }
            }

            // Hervorhebung, solange ein gezogenes Pane über diesem schwebt (Tausch-Ziel).
            Rectangle {
                anchors.fill: parent
                visible: win.dragPaneId >= 0 && win.dragOverPaneId === pane.paneId
                         && win.dragPaneId !== pane.paneId
                radius: pane.radius
                color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.15)
                border.width: 2
                border.color: Theme.accent
            }
        }
    }
}
