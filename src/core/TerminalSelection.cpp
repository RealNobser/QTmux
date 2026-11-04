#include "TerminalSelection.h"
#include "VtScreen.h"

#include <algorithm>
#include <climits>

namespace qtmux {

namespace {

// Zeichen einer Zeile im Spaltenbereich [from, to]. Die Spacer-Zelle hinter einem breiten
// Zeichen hat keinen Text und bekommt auch kein Leerzeichen, sonst stünde beim Kopieren
// hinter jedem Emoji eines, das nie jemand ausgegeben hat.
QString lineChars(const VtScreen &sc, int absRow, int from, int to) {
    QString out;
    const int sb = sc.scrollbackCount();
    if (absRow < 0) return out;
    if (absRow < sb) {
        const std::vector<Cell> &cells = sc.scrollbackLine(absRow);
        const int last = std::min(to, static_cast<int>(cells.size()) - 1);
        for (int col = std::max(from, 0); col <= last; ++col) {
            const Cell &c = cells[col];
            if (c.text.isEmpty()) {
                if (col > 0 && cells[col - 1].width == 2) continue;
                out += QLatin1Char(' ');
            } else {
                out += c.text;
            }
        }
        return out;
    }
    const int live = absRow - sb;
    if (live >= sc.rows()) return out;
    const int last = std::min(to, sc.cols() - 1);
    for (int col = std::max(from, 0); col <= last; ++col) {
        const Cell c = sc.cell(live, col);
        if (c.text.isEmpty()) {
            if (col > 0 && sc.cell(live, col - 1).width == 2) continue;
            out += QLatin1Char(' ');
        } else {
            out += c.text;
        }
    }
    return out;
}

void chopTrailingSpaces(QString &s) {
    while (s.endsWith(QLatin1Char(' '))) s.chop(1);
}

bool rowIsContinuation(const VtScreen &sc, int absRow) {
    const int sb = sc.scrollbackCount();
    if (absRow < 0) return false;
    if (absRow < sb) return sc.scrollbackContinuation(absRow);
    const int live = absRow - sb;
    return live < sc.rows() && sc.lineContinuation(live);
}

} // namespace

void TerminalSelection::begin(const VtScreen &screen, int col, int absRow) {
    m_all = false;
    m_anchorSet = true;
    m_anchor = Pos{col, absRow + screen.scrollbackDropped()};
    m_caret = m_anchor;
    m_snapshot.clear();
}

bool TerminalSelection::extendTo(const VtScreen &screen, int col, int absRow) {
    if (!m_anchorSet) return false;
    const Pos caret{col, absRow + screen.scrollbackDropped()};
    if (caret.col == m_caret.col && caret.line == m_caret.line) return false;
    m_caret = caret;
    takeSnapshot(screen);
    return true;
}

void TerminalSelection::selectAll() {
    m_all = true;
    m_anchorSet = false;
    m_snapshot.clear();
}

void TerminalSelection::clear() {
    m_all = false;
    m_anchorSet = false;
    m_snapshot.clear();
}

void TerminalSelection::range(const VtScreen &screen, Pos &start, Pos &end) const {
    if (m_all) {
        const qint64 dropped = screen.scrollbackDropped();
        start = Pos{0, dropped};
        end = Pos{std::max(screen.cols() - 1, 0),
                  dropped + screen.scrollbackCount() + screen.rows() - 1};
        return;
    }
    start = before(m_caret, m_anchor) ? m_caret : m_anchor;
    end = before(m_caret, m_anchor) ? m_anchor : m_caret;
}

QString TerminalSelection::markedPart(const VtScreen &screen, qint64 line, const Pos &start,
                                      const Pos &end) const {
    const int absRow = static_cast<int>(line - screen.scrollbackDropped());
    const int from = (line == start.line) ? start.col : 0;
    const int to = (line == end.line) ? end.col : INT_MAX;
    QString part = lineChars(screen, absRow, from, to);
    chopTrailingSpaces(part);
    return part;
}

void TerminalSelection::takeSnapshot(const VtScreen &screen) {
    m_snapshot.clear();
    if (isEmptyRange()) return;
    Pos start, end;
    range(screen, start, end);
    const qint64 firstLive = screen.scrollbackDropped() + screen.scrollbackCount();
    for (qint64 line = std::max(start.line, firstLive); line <= end.line; ++line)
        m_snapshot.push_back(Snapshot{line, markedPart(screen, line, start, end)});
}

bool TerminalSelection::revalidate(const VtScreen &screen) {
    if (m_all || !m_anchorSet) return false;
    const qint64 dropped = screen.scrollbackDropped();
    Pos start, end;
    range(screen, start, end);
    // Der Anfang der Auswahl ist aus dem Scrollback gefallen: Dieser Text existiert nicht
    // mehr. Beschneiden würde etwas anderes markieren, als der Anwender markiert hat.
    if (start.line < dropped) {
        clear();
        return true;
    }
    const qint64 firstLive = dropped + screen.scrollbackCount();
    size_t keep = 0;
    for (size_t i = 0; i < m_snapshot.size(); ++i) {
        const Snapshot &s = m_snapshot[i];
        if (markedPart(screen, s.line, start, end) != s.text) {
            clear();
            return true;
        }
        // Inzwischen im Scrollback und noch gleich: Das ändert sich nie mehr.
        if (s.line >= firstLive) {
            if (keep != i) m_snapshot[keep] = m_snapshot[i];
            ++keep;
        }
    }
    m_snapshot.resize(keep);
    return false;
}

QString TerminalSelection::text(const VtScreen &screen) const {
    if (!hasSelection()) return {};
    Pos start, end;
    range(screen, start, end);
    const qint64 dropped = screen.scrollbackDropped();
    QString out;
    for (qint64 line = start.line; line <= end.line; ++line) {
        const int absRow = static_cast<int>(line - dropped);
        const int from = (line == start.line) ? start.col : 0;
        const int to = (line == end.line) ? end.col : INT_MAX;
        QString part = lineChars(screen, absRow, from, to);
        // Bei einer fortgesetzten (umbrochenen) Zeile rechte Leerzeichen NICHT trimmen,
        // der Text läuft ja in der nächsten Zeile weiter. An weichen Umbrüchen kein `\n`,
        // sonst wird ein umbrochener Befehl beim Einfügen fälschlich mehrzeilig.
        if (!rowIsContinuation(screen, absRow + 1)) chopTrailingSpaces(part);
        if (line > start.line && !rowIsContinuation(screen, absRow)) out += QLatin1Char('\n');
        out += part;
    }
    return out;
}

bool TerminalSelection::rowSpan(const VtScreen &screen, int absRow, int cols, int &from,
                                int &to) const {
    if (!hasSelection() || cols <= 0) return false;
    Pos start, end;
    range(screen, start, end);
    const qint64 line = absRow + screen.scrollbackDropped();
    if (line < start.line || line > end.line) return false;
    from = (line == start.line) ? start.col : 0;
    to = (line == end.line) ? std::min(end.col, cols - 1) : cols - 1;
    return from <= to;
}

} // namespace qtmux
