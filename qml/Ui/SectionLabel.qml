import QtQuick
import QtQuick.Layouts
import QTmux

// Abschnittsüberschrift in den Einstellungen. Aus dem settingsDialog herausgezogen
// (QTMUX-47, Schritt 1), damit die Kategorie-Seiten des Einstellungsfensters sie nutzen.
Text {
    color: Theme.textDim
    font.pixelSize: 11
    font.bold: true
    Layout.topMargin: 6
}
