#pragma once

#include <QtGlobal>

namespace qtmux {

/// Zellraster eines Terminals (Spalten × Zeilen), abgeleitet aus Item-Größe und Zellmaß.
struct TerminalGrid {
    int cols = 0;
    int rows = 0;
    bool valid = false;   ///< false = keine belastbare Größe (Item (noch) ungelayoutet)
};

/// Rechnet Item-Größe + Zellmaß in ein Zellraster um. Gui-frei, damit die Regel unten
/// unit-testbar ist (`test_terminalgrid`) — `TerminalItem::recomputeGrid` delegiert hierher.
///
/// ⚠️ **QTMUX-86:** Ist eine der vier Größen nicht positiv, ist das Ergebnis `valid=false`
/// und der Aufrufer darf die Session **nicht** in der Größe verändern. Grund: Beim Abbauen
/// und Neuaufbauen des Pane-Baums (Window-Wechsel, Pane-Reorder) meldet Qt für ein Item
/// transient die Größe 0×0 — und `geometryChange` feuert dafür. Ein auf 1 geklemmtes Raster
/// hätte `Session::resize(1, 1)` zur Folge: libvterm kürzt dann JEDE Zeile auf ihr erstes
/// Zeichen (aus `H:\_ClaudeWorkspace>` wird `H`) und schiebt alles in den Scrollback. Der
/// Bildschirm ist danach leer, das Pane sieht „nicht gerendert" aus, obwohl die Session lebt
/// und korrekt gezeichnet wird — der Inhalt ist unwiederbringlich verstümmelt.
/// Ohne gültige Größe behält der Aufrufer sein bisheriges Raster, bis eine echte Größe kommt.
TerminalGrid gridFor(qreal width, qreal height, qreal cellW, qreal cellH);

} // namespace qtmux
