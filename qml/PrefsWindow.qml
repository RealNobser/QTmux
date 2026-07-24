import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QtQuick.Effects
import QtCore
import QTmux

// Nicht-modales Einstellungsfenster mit Kategorie-Rail links und View rechts
// (QTMUX-47, Umsetzungsanweisung Schritt 2, Variante 1c). Ersetzt den modalen
// settingsDialog; die Kategorie-Inhalte ziehen in Schritt 3 als qml/prefs/Cat*.qml ein.
Window {
    id: root
    // 1c: großzügiges Fenster mit Vorschau-Spalte.
    width: 1060
    height: 700
    minimumWidth: 820
    minimumHeight: 560
    flags: Qt.Window
    title: qsTr("Einstellungen")
    color: Theme.bgMain

    // Ein separates Window erbt die überschriebene palette des ApplicationWindow NICHT
    // (sonst wären Basic-Controls im Hell-Modus falsch gefärbt) — deshalb hier derselbe
    // palette.*-Block wie in Main.qml. Popups/Menüs brauchen zusätzlich ihre eigene
    // Palette + AppPopupBg (kommt mit den Kategorie-Seiten).
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

    // Aktuelle Kategorie (ID); steuert den View. Persistiert (ui/prefsCategory).
    property string category: "allgemein"
    // Ziel-Einstellung eines Sprungs (Suche/Deep-Link) — von der Kategorie-Seite gelesen
    // und nach dem Hervorheben zurückgesetzt. Wird in Schritt 7 genutzt.
    property string pendingSetting: ""

    // Icon-Pfad-Helfer (window.icon ist hier nicht erreichbar — eigenes Fenster).
    function iconSrc(name) { return "qrc:/icons/" + name + ".svg" }

    // Öffnet das Fenster, optional auf eine Kategorie + Einstellung. Ein zweiter Aufruf
    // öffnet KEIN zweites Fenster, sondern hebt das bestehende nach vorn.
    function open(categoryId, settingKey) {
        if (categoryId && categoryId.length > 0) category = categoryId
        if (settingKey && settingKey.length > 0) pendingSetting = settingKey
        if (!visible) {
            // Beim ersten Öffnen versetzt zum Hauptfenster platzieren, damit die Sidebar
            // sichtbar bleibt (Teil B). Nur wenn keine gespeicherte Position vorliegt.
            if (x <= 0 && y <= 0) { x = 120; y = 90 }
            show()
        }
        raise()
        requestActivate()
    }

    // Persistenz: Position/Größe und zuletzt gewählte Kategorie. Die Design-Referenz
    // nennt ui/prefsGeometry + ui/prefsCategory; Geometrie hier als Einzelkomponenten,
    // da QML-Settings keinen Rect an einen Schlüssel binden.
    Settings {
        id: prefsSettings
        category: "ui"
        property alias prefsX: root.x
        property alias prefsY: root.y
        property alias prefsWidth: root.width
        property alias prefsHeight: root.height
        property alias prefsCategory: root.category
    }

    // Kategorien und Zuordnung nach Tabelle A4 der Design-Referenz.
    readonly property var categories: [
        { id: "allgemein",       icon: "gear",            label: qsTr("Allgemein") },
        { id: "erscheinungsbild",icon: "moon",            label: qsTr("Erscheinungsbild") },
        { id: "terminal",        icon: "terminal-window", label: qsTr("Terminal") },
        { id: "eingabe",         icon: "clipboard",       label: qsTr("Eingabe") },
        { id: "agenten",         icon: "robot",           label: qsTr("Agenten & MCP") },
        { id: "hotkeys",         icon: "command",         label: qsTr("Tastenkürzel") },
        { id: "verbindungen",    icon: "bookmark",        label: qsTr("Verbindungen") },
        { id: "vault",           icon: "key",             label: qsTr("Secrets-Vault") },
        { id: "erweiterungen",   icon: "plugs",           label: qsTr("Erweiterungen") }
    ]

    // Badge je Kategorie (Statusanzeige, kein Selbstzweck). Die konkreten Werte werden
    // mit den Kategorie-Seiten in Schritt 3 gefüllt; hier zentral, damit die Rail eine
    // Stelle hat. Vorerst nur die statisch bekannten.
    function badgeFor(id) {
        return ""
    }

    // Kategorie wechseln (Rail-Klick / Tastatur).
    function selectCategory(id) { category = id }
    function selectDelta(delta) {
        let i = 0
        for (let k = 0; k < categories.length; ++k) if (categories[k].id === category) i = k
        i = Math.max(0, Math.min(categories.length - 1, i + delta))
        category = categories[i].id
    }

    // Esc und ⌘W/Strg+W schließen nur dieses Fenster (nicht die Session im Hauptfenster).
    Shortcut { sequences: ["Escape"]; onActivated: root.close() }
    Shortcut { sequences: [StandardKey.Close]; onActivated: root.close() }
    // ⌘F/Strg+F fokussiert die Suche (Funktion folgt in Schritt 7).
    Shortcut { sequences: [StandardKey.Find]; onActivated: searchField.forceActiveFocus() }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // Fensterkopf (1c): Titel + Suchfeld.
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 60
            color: Theme.bgSidebar
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 20
                anchors.rightMargin: 18
                spacing: 16
                Text {
                    text: qsTr("Einstellungen")
                    color: Theme.textBright
                    font.pixelSize: 17
                    font.bold: true
                }
                Item { Layout.fillWidth: true }
                Rectangle {
                    Layout.preferredWidth: 300
                    Layout.preferredHeight: 34
                    radius: 17
                    color: Theme.bgMain
                    border.color: searchField.activeFocus ? Theme.accent : Theme.border
                    border.width: 1
                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 14
                        anchors.rightMargin: 14
                        spacing: 9
                        TextField {
                            id: searchField
                            Layout.fillWidth: true
                            background: null
                            color: Theme.textBright
                            placeholderText: qsTr("Suchen — z. B. „Ligaturen“")
                            placeholderTextColor: Theme.textDim
                            font.pixelSize: 13
                            selectByMouse: true
                            // Suche wird in Schritt 7 verdrahtet; Esc leert dann/blurrt.
                        }
                        Text {
                            text: App.shortcutText("Ctrl+F")
                            color: Theme.textDim
                            font.pixelSize: 11
                        }
                    }
                }
            }
        }
        Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: Theme.border }

        // Körper: Rail + View.
        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            // --- Kategorie-Rail -------------------------------------------------
            Rectangle {
                Layout.preferredWidth: 232
                Layout.fillHeight: true
                color: Theme.bgSidebar
                focus: true
                Keys.onUpPressed: root.selectDelta(-1)
                Keys.onDownPressed: root.selectDelta(1)

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 12
                    spacing: 3

                    Repeater {
                        model: root.categories
                        delegate: Rectangle {
                            required property var modelData
                            Layout.fillWidth: true
                            Layout.preferredHeight: 40
                            radius: 10
                            readonly property bool current: root.category === modelData.id
                            color: current ? Theme.sidebarSelected
                                 : railHover.hovered ? Theme.sidebarHover : "transparent"
                            readonly property color ink: current ? Theme.textBright : Theme.textDim

                            HoverHandler { id: railHover }
                            TapHandler { onTapped: root.selectCategory(modelData.id) }

                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 11
                                anchors.rightMargin: 11
                                spacing: 11
                                // Icon: explizite MultiEffect-Form (layer.effect greift in
                                // Delegates nicht zuverlässig — s. Umsetzungsanweisung).
                                Item {
                                    Layout.preferredWidth: 17
                                    Layout.preferredHeight: 17
                                    Image {
                                        id: railIco
                                        anchors.fill: parent
                                        source: root.iconSrc(modelData.icon)
                                        sourceSize.width: 17
                                        sourceSize.height: 17
                                        visible: false
                                    }
                                    MultiEffect {
                                        anchors.fill: railIco
                                        source: railIco
                                        colorization: 1.0
                                        colorizationColor: parent.parent.ink
                                    }
                                }
                                Text {
                                    Layout.fillWidth: true
                                    text: modelData.label
                                    color: parent.parent.ink
                                    font.pixelSize: 13.5
                                    font.weight: Font.Medium
                                    elide: Text.ElideRight
                                }
                                Text {
                                    readonly property string badge: root.badgeFor(modelData.id)
                                    visible: badge.length > 0
                                    text: badge
                                    color: Theme.textDim
                                    font.pixelSize: 10.5
                                    leftPadding: 7; rightPadding: 7; topPadding: 2; bottomPadding: 2
                                    background: Rectangle { radius: 8; color: Theme.bgElevated }
                                }
                            }
                        }
                    }

                    Item { Layout.fillHeight: true }

                    Text {
                        Layout.fillWidth: true
                        Layout.leftMargin: 4
                        Layout.bottomMargin: 4
                        text: qsTr("Alles wirkt sofort.")
                        color: Theme.textDim
                        font.pixelSize: 11
                        wrapMode: Text.WordWrap
                    }
                }
            }
            Rectangle { Layout.preferredWidth: 1; Layout.fillHeight: true; color: Theme.border }

            // --- View (Kategorie-Inhalt) ---------------------------------------
            // Eigenes Flickable/Scroll je Seite; die Rail scrollt nie mit. In Schritt 3
            // lädt der Loader qml/prefs/Cat*.qml; bis dahin ein Platzhalter.
            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                color: Theme.bgMain
                Loader {
                    anchors.fill: parent
                    sourceComponent: placeholderView
                }
            }
        }
    }

    // Platzhalter bis Schritt 3: zeigt Titel der aktiven Kategorie.
    Component {
        id: placeholderView
        Item {
            ColumnLayout {
                anchors.left: parent.left
                anchors.top: parent.top
                anchors.margins: 28
                anchors.right: parent.right
                spacing: 6
                Text {
                    text: {
                        for (let i = 0; i < root.categories.length; ++i)
                            if (root.categories[i].id === root.category) return root.categories[i].label
                        return ""
                    }
                    color: Theme.textBright
                    font.pixelSize: 26
                    font.bold: true
                }
                Text {
                    text: qsTr("Inhalt zieht in Schritt 3 ein.")
                    color: Theme.textDim
                    font.pixelSize: 13
                }
            }
        }
    }
}
