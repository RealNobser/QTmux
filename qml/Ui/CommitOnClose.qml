import QtQuick

// Familienregel „Schließen und Beenden übernehmen das Feld mit Fokus; nichts geht
// still verloren“: Ein Textfeld speichert bei editingFinished (Enter, Tab, Fokuswechsel).
// Wer das Fenster schließt oder QTmux beendet, während er noch tippt, löst das nicht aus.
// Dieses Objekt holt es nach: beim Schließen des Fensters und bei aboutToQuit bekommt
// das Feld mit Fokus sein editingFinished. Wächter: tests/tst_updatesettingspage.cpp.
QtObject {
    id: c
    property var window

    function commit() {
        const f = c.window ? c.window.activeFocusItem : null
        if (f && typeof f.editingFinished === "function") f.editingFinished()
    }

    property Connections closing: Connections {
        target: c.window
        function onClosing() { c.commit() }
    }
    property Connections quitting: Connections {
        target: Qt.application
        function onAboutToQuit() { c.commit() }
    }
}
