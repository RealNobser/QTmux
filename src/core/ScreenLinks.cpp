#include "ScreenLinks.h"
#include "VtScreen.h"

#include <algorithm>

namespace ScreenLinks {

using qtmux::Cell;
using qtmux::VtScreen;

Gesture gestureFor(Qt::KeyboardModifiers mods, FileManagerReveal::Platform platform) {
    const bool ctrl = mods & Qt::ControlModifier;
    const bool alt = mods & Qt::AltModifier;
    const bool shift = mods & Qt::ShiftModifier;
    if (platform == FileManagerReveal::Platform::MacOS) {
        // Cmd = ControlModifier. Physisches Ctrl = MetaModifier; Ctrl+Klick ist auf dem Mac
        // der Rechtsklick und darf keinen Link öffnen.
        if (!ctrl || (mods & Qt::MetaModifier)) return Gesture::None;
        return alt ? Gesture::Reveal : Gesture::Open;
    }
    if (!ctrl || alt) return Gesture::None;
    return shift ? Gesture::Reveal : Gesture::Open;
}

namespace {

bool rowContinues(const VtScreen &sc, int absRow) {
    const int sb = sc.scrollbackCount();
    if (absRow < 0) return false;
    if (absRow < sb) return sc.scrollbackContinuation(absRow);
    const int live = absRow - sb;
    return live < sc.rows() && sc.lineContinuation(live);
}

void appendRow(const VtScreen &sc, int absRow, Line &out) {
    const int sb = sc.scrollbackCount();
    const std::vector<Cell> *sbLine = absRow < sb ? &sc.scrollbackLine(absRow) : nullptr;
    const int live = absRow - sb;
    const int cols = sbLine ? static_cast<int>(sbLine->size()) : sc.cols();
    int prevWidth = 1;
    for (int col = 0; col < cols; ++col) {
        const Cell c = sbLine ? (*sbLine)[col] : sc.cell(live, col);
        if (c.text.isEmpty()) {
            if (prevWidth == 2) { prevWidth = 1; continue; }   // Spacer hinter breitem Zeichen
            out.text += QLatin1Char(' ');
            out.map.push_back({absRow, col});
        } else {
            out.text += c.text;
            for (int i = 0; i < c.text.size(); ++i) out.map.push_back({absRow, col});
        }
        prevWidth = c.width;
    }
}

} // namespace

Line logicalLine(const VtScreen &sc, int absRow) {
    Line line;
    const int total = sc.scrollbackCount() + sc.rows();
    if (absRow < 0 || absRow >= total) return line;
    int first = absRow;
    // Zeile 0 kann selbst eine Fortsetzung sein: An der Scrollback-Grenze fällt der Anfang einer
    // langen Zeile heraus, ihr Rest bleibt. Dort endet die Kette (nie Zeile -1) und gilt als
    // abgeschnitten, denn ihr Anfang fehlt.
    for (int n = 0; n < kMaxChainRows && first > 0 && rowContinues(sc, first); ++n) --first;
    line.cutTop = rowContinues(sc, first);
    int last = absRow;
    for (int n = 0; n < kMaxChainRows && last + 1 < total && rowContinues(sc, last + 1); ++n) ++last;
    line.cutBottom = last + 1 < total && rowContinues(sc, last + 1);
    line.firstRow = first;
    for (int r = first; r <= last; ++r) appendRow(sc, r, line);
    return line;
}

Hit hitAt(const Line &line, const QList<LinkDetector::Span> &spans, int absRow, int col) {
    const auto atOrAfter = [](const CellRef &a, int row, int c) {
        return row > a.row || (row == a.row && c >= a.col);
    };
    const int mapSize = static_cast<int>(line.map.size());
    // Ein abgeschnittenes Ziel wäre ein anderes Ziel: lieber kein Link als der falsche. Maßgeblich
    // ist das ganze Wort am Schnitt, nicht das Textende. Der Detektor kürzt Satzzeichen am Ende
    // weg, und ohne den Anfang fände er in `https://a/?u=https://b` nur noch das innere Ziel.
    const auto isSpace = [](QChar c) { return c.isSpace(); };
    const qsizetype firstBlank =
        std::find_if(line.text.cbegin(), line.text.cend(), isSpace) - line.text.cbegin();
    const qsizetype lastBlank =
        line.text.crend() - std::find_if(line.text.crbegin(), line.text.crend(), isSpace) - 1;
    for (const auto &s : spans) {
        if (s.length <= 0 || s.start < 0 || s.start + s.length > mapSize) continue;
        if (line.cutTop && s.start < firstBlank) continue;
        if (line.cutBottom && s.start + s.length > lastBlank) continue;
        const CellRef &first = line.map[s.start];
        const CellRef &last = line.map[s.start + s.length - 1];
        if (!atOrAfter(first, absRow, col) || !atOrAfter({absRow, col}, last.row, last.col)) continue;

        Hit hit;
        hit.target = s.target;
        hit.kind = s.kind;
        for (int i = s.start; i < s.start + s.length; ++i) {
            const CellRef &ref = line.map[i];
            if (hit.segments.isEmpty() || hit.segments.last().row != ref.row)
                hit.segments.append(Segment{ref.row, ref.col, ref.col});
            else
                hit.segments.last().c1 = ref.col;
        }
        return hit;
    }
    return {};
}

Hit linkAt(const VtScreen &screen, int absRow, int col, const QString &cwd) {
    const Line line = logicalLine(screen, absRow);
    return hitAt(line, LinkDetector::detect(line.text, cwd), absRow, col);
}

} // namespace ScreenLinks
