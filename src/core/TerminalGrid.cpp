#include "TerminalGrid.h"

#include <algorithm>

namespace qtmux {

TerminalGrid gridFor(qreal width, qreal height, qreal cellW, qreal cellH) {
    TerminalGrid g;
    // Bewusst `!(x > 0)` statt `x <= 0`: fängt auch NaN mit ab (dort sind BEIDE
    // Vergleiche false, `<=` würde NaN also durchlassen).
    if (!(width > 0) || !(height > 0) || !(cellW > 0) || !(cellH > 0))
        return g;                      // valid bleibt false → Aufrufer resizt nicht

    g.cols = std::max(static_cast<int>(width / cellW), 1);
    g.rows = std::max(static_cast<int>(height / cellH), 1);
    g.valid = true;
    return g;
}

} // namespace qtmux
