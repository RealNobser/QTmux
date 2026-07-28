pragma ComponentBehavior: Bound

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

    // Läuft gerade eine Kürzel-Inline-Aufnahme (Schritt 5)? Solange true, deaktiviert
    // Main.qml alle globalen App-Shortcuts, damit die gedrückten Tasten aufgenommen
    // statt ausgeführt werden. Wird von der Hotkeys-Seite gesetzt/zurückgesetzt.
    property bool capturing: false

    // --- Brücken zu Main.qml ------------------------------------------------
    // Ein eigenes Window sieht die IDs aus Main.qml (window, sessions, mcp, die
    // modalen Editier-Dialoge) NICHT — sie werden hier als Handles hereingereicht
    // und von den Kategorie-Seiten über `host.*` genutzt. Globale Singletons
    // (Theme, App, ColorSchemes, Profiles, Hotkeys, Vault, AgentEvents, Plugins)
    // sind dagegen Context-Properties und überall direkt verfügbar.
    property var app                 // ApplicationWindow (Terminal-Einstellungen, Helfer)
    property var sessions            // SessionModel
    property var mcp                 // McpServer
    property var profileEditDialog   // Profil anlegen/bearbeiten (bleibt modal)
    property var secretEditDialog    // Secret anlegen/bearbeiten (bleibt modal)
    property var masterPwDialog      // Master-Passwort ändern (bleibt modal)
    property var schemeFileDialog    // Farbschema-Import-Dateidialog

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

    // Badge je Kategorie (Statusanzeige, kein Selbstzweck — s. Design-Referenz A4):
    // zeigt nur, was man von außen wissen will (aktives Schema, Anzahl Abos/Profile/
    // Plugins, geänderte Kürzel, Vault-Sperrzustand). `badgeRev` erzwingt die
    // Neuauswertung bei Registry-Änderungen, deren Quelle eine Methode statt einer
    // beobachtbaren Property ist (AgentEvents.subscriptions()).
    property int badgeRev: 0
    Connections { target: AgentEvents; function onSubscriptionsChanged() { root.badgeRev++ } }
    Connections { target: Hotkeys;     function onChanged()             { root.badgeRev++ } }
    function badgeFor(id) {
        root.badgeRev   // Abhängigkeit erzwingen
        switch (id) {
        case "erscheinungsbild":
            return ColorSchemes.current
        case "agenten": {
            const n = AgentEvents.subscriptions().length
            return n > 0 ? qsTr("%1 Abos").arg(n) : ""
        }
        case "hotkeys": {
            let n = 0
            const ids = Hotkeys.actionIds()
            for (let i = 0; i < ids.length; ++i)
                if (Hotkeys.bindings[ids[i]] !== Hotkeys.defaultSequence(ids[i])) n++
            return n > 0 ? qsTr("%1 geändert").arg(n) : ""
        }
        case "verbindungen": {
            const n = Profiles.profiles.length
            return n > 0 ? String(n) : ""
        }
        case "vault":
            return Vault.unlocked ? "" : qsTr("gesperrt")
        case "erweiterungen": {
            const n = Plugins.backendTypes.length
            return n > 0 ? String(n) : ""
        }
        }
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

    // --- Suche (Schritt 7) --------------------------------------------------
    function categoryLabel(id) {
        for (let i = 0; i < categories.length; ++i)
            if (categories[i].id === id) return categories[i].label
        return id
    }
    // Durchsuchbare Einstellungen: Label + Stichworte je Eintrag, Schlüssel = Sektions-
    // Anker (mehrere Einträge dürfen denselben Anker teilen). Kürzel werden dynamisch aus
    // der Registry angehängt (Sprung in die Kürzel-Kategorie).
    readonly property var searchEntries: {
        const e = [
            { cat: "allgemein",        key: "allgemein.general",     label: qsTr("Design"),                            keywords: "design darstellung hell dunkel system theme modus" },
            { cat: "allgemein",        key: "allgemein.general",     label: qsTr("Sprache"),                           keywords: "sprache language deutsch englisch locale" },
            { cat: "allgemein",        key: "allgemein.window",      label: qsTr("Vor dem Beenden nachfragen"),        keywords: "beenden quit schließen rückfrage confirm warnung" },
            { cat: "allgemein",        key: "allgemein.window",      label: qsTr("Quake-Modus"),                       keywords: "quake hotkey einblenden ausblenden global" },
            { cat: "erscheinungsbild", key: "erscheinung.schemes",   label: qsTr("Farbschema (Dunkel)"),               keywords: "farbschema schema dunkel ansi farben iterm ghostty xresources import" },
            { cat: "erscheinungsbild", key: "erscheinung.schemes",   label: qsTr("Farbschema (Hell)"),                 keywords: "farbschema schema hell ansi farben import" },
            { cat: "terminal",         key: "terminal.options",      label: qsTr("Schriftart"),                        keywords: "schrift font monospace terminal" },
            { cat: "terminal",         key: "terminal.options",      label: qsTr("Schriftgröße"),                      keywords: "schriftgröße größe font size zoom" },
            { cat: "terminal",         key: "terminal.options",      label: qsTr("Ligaturen"),                         keywords: "ligaturen firacode glyph programmier calt liga" },
            { cat: "terminal",         key: "terminal.options",      label: qsTr("GPU-Glyph-Atlas"),                   keywords: "gpu rendering glyph atlas qpainter beschleunigung" },
            { cat: "terminal",         key: "terminal.options",      label: qsTr("Standard-Shell"),                    keywords: "shell zsh bash powershell cmd standard" },
            { cat: "eingabe",          key: "eingabe.clipboard",     label: qsTr("Auswahl automatisch kopieren"),      keywords: "kopieren auswahl copy select zwischenablage" },
            { cat: "eingabe",          key: "eingabe.clipboard",     label: qsTr("Rechtsklick fügt ein"),              keywords: "rechtsklick einfügen paste zwischenablage" },
            { cat: "eingabe",          key: "eingabe.clipboard",     label: qsTr("Vor mehrzeiligem Einfügen warnen"),  keywords: "einfügen paste warnung mehrzeilig multiline" },
            { cat: "agenten",          key: "agenten.restore",       label: qsTr("Agenten beim Start wiederherstellen"), keywords: "agent wiederherstellen neustart restore claude codex starten sitzung" },
            { cat: "agenten",          key: "agenten.restore",       label: qsTr("Unterhaltung fortsetzen"),           keywords: "fortsetzen continue resume unterhaltung konversation agent auswahl gemeldet juengste" },
            { cat: "agenten",          key: "agenten.notifications", label: qsTr("Benachrichtigungen"),                keywords: "agent benachrichtigung abo matrix ereignis subscribe" },
            { cat: "agenten",          key: "agenten.mcp",           label: qsTr("MCP-Server"),                        keywords: "mcp server port agenten steuerung 127.0.0.1" },
            { cat: "verbindungen",     key: "verbindungen.list",     label: qsTr("Verbindungsprofile"),                keywords: "verbindung profil ssh seriell sftp profile" },
            { cat: "vault",            key: "vault.section",         label: qsTr("Secrets-Vault"),                     keywords: "vault secret passwort geheimnis master token" },
            { cat: "erweiterungen",    key: "erweiterungen.list",    label: qsTr("Erweiterungen"),                     keywords: "plugin erweiterung backend echo macpcan can" }
        ]
        if (app) {
            const ids = Hotkeys.actionIds()
            for (let i = 0; i < ids.length; ++i)
                e.push({ cat: "hotkeys", key: "hotkeys.list", label: app.hotkeyLabel(ids[i]),
                         keywords: "kürzel shortcut taste " + ids[i] + " " + (Hotkeys.bindings[ids[i]] || "") })
        }
        return e
    }
    readonly property var searchResults: {
        const q = searchQuery.trim().toLowerCase()
        if (q.length === 0) return []
        const out = []
        for (let i = 0; i < searchEntries.length && out.length < 8; ++i) {
            const e = searchEntries[i]
            const hay = (e.label + " " + e.keywords + " " + categoryLabel(e.cat)).toLowerCase()
            if (hay.indexOf(q) >= 0) out.push(e)
        }
        return out
    }
    // Von der Suche gesetzter Text (getrennt vom TextField, damit die Ergebnisliste nicht
    // vom Fokus abhängt).
    property string searchQuery: ""
    readonly property bool searchActive: searchQuery.length > 0

    // Springt zu einem Treffer: Kategorie wechseln, Sektion markieren (pendingSetting →
    // PrefAnchor blendet auf + scrollt), Suche schließen.
    function jumpTo(entry) {
        category = entry.cat
        pendingSetting = entry.key
        searchQuery = ""
        searchField.text = ""
    }

    // Esc und ⌘W/Strg+W schließen nur dieses Fenster (nicht die Session im Hauptfenster).
    // Während einer Kürzel-Aufnahme abgeschaltet, damit Esc/⌘W als Kürzel erfassbar sind
    // (die Hotkeys-Seite fängt sie dann selbst ab); bei aktiver Suche fängt Esc erst die
    // Suche ab (leert sie), statt das Fenster zu schließen.
    Shortcut { sequences: ["Escape"]; enabled: !root.capturing && !root.searchActive; onActivated: root.close() }
    Shortcut { sequences: [StandardKey.Close]; enabled: !root.capturing; onActivated: root.close() }
    // ⌘F/Strg+F fokussiert die Suche (Funktion folgt in Schritt 7).
    Shortcut { sequences: [StandardKey.Find]; enabled: !root.capturing; onActivated: searchField.forceActiveFocus() }

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
                            onTextChanged: root.searchQuery = text
                            // Esc leert die Suche (bzw. blurrt, wenn schon leer).
                            Keys.onEscapePressed: (event) => {
                                if (text.length > 0) { text = ""; root.searchQuery = "" }
                                else focus = false
                                event.accepted = true
                            }
                            // Enter übernimmt den ersten Treffer.
                            Keys.onReturnPressed: (event) => {
                                if (root.searchResults.length > 0) { root.jumpTo(root.searchResults[0]); event.accepted = true }
                            }
                        }
                        Text {
                            text: App.shortcutText("Ctrl+F")
                            color: Theme.textDim
                            font.pixelSize: 11
                        }
                    }

                    // Ergebnisliste unter dem Suchfeld (Kategorie · Einstellung).
                    Popup {
                        id: searchPopup
                        y: parent.height + 4
                        x: 0
                        width: parent.width
                        padding: 4
                        visible: root.searchActive && root.searchResults.length > 0
                        closePolicy: Popup.NoAutoClose
                        background: Rectangle {
                            color: Theme.bgElevated
                            border.color: Theme.border
                            border.width: 1
                            radius: 8
                        }
                        contentItem: ColumnLayout {
                            spacing: 1
                            Repeater {
                                model: root.searchResults
                                delegate: Rectangle {
                                    id: resRow
                                    required property var modelData
                                    Layout.fillWidth: true
                                    implicitHeight: 38
                                    radius: 6
                                    color: resHover.hovered ? Theme.sidebarHover : "transparent"
                                    HoverHandler { id: resHover }
                                    TapHandler { onTapped: root.jumpTo(resRow.modelData) }
                                    ColumnLayout {
                                        anchors.fill: parent
                                        anchors.leftMargin: 10
                                        anchors.rightMargin: 10
                                        spacing: 0
                                        Text {
                                            text: resRow.modelData.label
                                            color: Theme.textBright
                                            font.pixelSize: 13
                                            elide: Text.ElideRight
                                            Layout.fillWidth: true
                                        }
                                        Text {
                                            text: root.categoryLabel(resRow.modelData.cat)
                                            color: Theme.textDim
                                            font.pixelSize: 11
                                            elide: Text.ElideRight
                                            Layout.fillWidth: true
                                        }
                                    }
                                }
                            }
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
                            id: railTile
                            required property var modelData
                            Layout.fillWidth: true
                            Layout.preferredHeight: 40
                            radius: 10
                            readonly property bool current: root.category === modelData.id
                            color: current ? Theme.sidebarSelected
                                 : railHover.hovered ? Theme.sidebarHover : "transparent"
                            readonly property color ink: current ? Theme.textBright : Theme.textDim
                            readonly property string badge: root.badgeFor(modelData.id)

                            HoverHandler { id: railHover }
                            TapHandler { onTapped: root.selectCategory(railTile.modelData.id) }

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
                                        source: root.iconSrc(railTile.modelData.icon)
                                        sourceSize.width: 17
                                        sourceSize.height: 17
                                        visible: false
                                    }
                                    MultiEffect {
                                        anchors.fill: railIco
                                        source: railIco
                                        // Schwarzes SVG (fill="currentColor") erst auf Weiß
                                        // heben, dann colorize — sonst gewichtet die Colorization
                                        // mit der Quell-Luminanz ~0 und das Icon bleibt dunkel.
                                        brightness: 1.0
                                        colorization: 1.0
                                        colorizationColor: railTile.ink
                                    }
                                }
                                Text {
                                    Layout.fillWidth: true
                                    text: railTile.modelData.label
                                    color: railTile.ink
                                    font.pixelSize: 13
                                    font.weight: Font.Medium
                                    elide: Text.ElideRight
                                }
                                // Badge als Pille (Text hat kein background — deshalb ein
                                // Rectangle mit Text darin).
                                Rectangle {
                                    visible: railTile.badge.length > 0
                                    radius: 8
                                    color: Theme.bgElevated
                                    implicitWidth: badgeText.implicitWidth + 14
                                    implicitHeight: badgeText.implicitHeight + 4
                                    Text {
                                        id: badgeText
                                        anchors.centerIn: parent
                                        text: railTile.badge
                                        color: Theme.textDim
                                        font.pixelSize: 11
                                    }
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
            // Jede Kategorie ist eine eigene Datei qml/prefs/Cat*.qml (ein CatPage,
            // das selbst scrollt). Der Loader wählt sie nach der aktiven Kategorie;
            // die inline-Components reichen `host: root` durch, damit die Seiten über
            // host.app/host.sessions/host.mcp/host.*Dialog auf Main.qml zugreifen.
            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                color: Theme.bgMain
                Loader {
                    anchors.fill: parent
                    sourceComponent: root.viewFor(root.category)
                }
            }
        }
    }

    // Kategorie → Inhalts-Component. Der Vorschlag der Design-Referenz ist eine Datei
    // je Kategorie; hier zusammengeführt, damit `host` sauber gebunden wird.
    function viewFor(id) {
        switch (id) {
        case "allgemein":        return cAllgemein
        case "erscheinungsbild": return cErscheinungsbild
        case "terminal":         return cTerminal
        case "eingabe":          return cEingabe
        case "agenten":          return cAgenten
        case "hotkeys":          return cHotkeys
        case "verbindungen":     return cVerbindungen
        case "vault":            return cVault
        case "erweiterungen":    return cErweiterungen
        }
        return cAllgemein
    }

    Component { id: cAllgemein;        CatAllgemein        { host: root } }
    Component { id: cErscheinungsbild; CatErscheinungsbild { host: root } }
    Component { id: cTerminal;         CatTerminal         { host: root } }
    Component { id: cEingabe;          CatEingabe          { host: root } }
    Component { id: cAgenten;          CatAgenten          { host: root } }
    Component { id: cHotkeys;          CatHotkeys          { host: root } }
    Component { id: cVerbindungen;     CatVerbindungen     { host: root } }
    Component { id: cVault;            CatVault            { host: root } }
    Component { id: cErweiterungen;    CatErweiterungen    { host: root } }
}
