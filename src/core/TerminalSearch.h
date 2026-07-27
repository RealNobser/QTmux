#pragma once

#include <QList>
#include <QString>
#include <QStringList>

// Gui-freie Terminal-/Scrollback-Suche: findet ein Suchwort in bereits zu Text
// zusammengesetzten Zeilen. Bewusst in qtmux_core (nur Qt6::Core), damit ohne GUI
// testbar (wie LinkDetector) — TerminalItem liefert die Zeilen aus VtScreen und zeichnet
// die zurueckgegebenen Treffer als Overlay-Quads.
namespace TerminalSearch {

struct Match {
    int line   = 0;   // Index in der uebergebenen Zeilenliste (vom Aufrufer = absolute Zeile)
    int col    = 0;   // Zeichen-Spalte, an der der Treffer beginnt
    int length = 0;   // Laenge in Zeichen
};

// Alle NICHT-ueberlappenden Treffer von `needle` in `lines`, zeilenweise von oben nach
// unten. Leeres `needle` -> keine Treffer. caseSensitive=false (Default): Gross-/
// Kleinschreibung wird ignoriert.
QList<Match> find(const QStringList &lines, const QString &needle, bool caseSensitive = false);

} // namespace TerminalSearch
