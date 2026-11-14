#include "ScreenLinks.h"
#include "VtScreen.h"

#include <QRegularExpression>

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

// Rahmenzeichen (U+2500–257F). Sie beenden schon im Detektor einen Treffer; hier begrenzen
// sie zusätzlich eine Tabellenzelle.
bool isBox(QChar c) { return c.unicode() >= 0x2500 && c.unicode() <= 0x257F; }

// Kann mit diesem Zeichen ein Link weitergehen? Maßstab ist der Detektor: Leerraum,
// Rahmenzeichen und die Zeichen, an denen seine Muster enden, können es nicht.
bool canContinueALink(QChar c) {
    static const QString stops = QStringLiteral("\"'<>`");
    return !c.isSpace() && !isBox(c) && !stops.contains(c);
}

// Eine Bildschirmzeile, gleich ob im Scrollback oder sichtbar.
struct Row {
    const VtScreen &sc;
    const std::vector<Cell> *sbLine = nullptr;
    int live = 0;
    int cols = 0;

    Row(const VtScreen &screen, int absRow) : sc(screen) {
        const int sb = sc.scrollbackCount();
        if (absRow < sb) {
            sbLine = &sc.scrollbackLine(absRow);
            cols = static_cast<int>(sbLine->size());
        } else {
            live = absRow - sb;
            cols = sc.cols();
        }
    }
    Cell at(int col) const { return sbLine ? (*sbLine)[col] : sc.cell(live, col); }
    // Erstes Zeichen der Zelle; eine leere Zelle und die Spacer-Zelle zählen als Leerzeichen.
    QChar ch(int col) const {
        const Cell c = at(col);
        return c.text.isEmpty() ? QLatin1Char(' ') : c.text.at(0);
    }
    bool blank(int col) const { return ch(col).isSpace(); }
    // Erste beschriebene Spalte in [from, to), sonst -1.
    int firstInk(int from, int to) const {
        for (int col = std::max(0, from); col < std::min(to, cols); ++col)
            if (!blank(col)) return col;
        return -1;
    }
    int lastInk() const {
        for (int col = cols - 1; col >= 0; --col)
            if (!blank(col)) return col;
        return -1;
    }
};

void appendRow(const VtScreen &sc, int absRow, bool wideCharFollows, Line &out) {
    const Row row(sc, absRow);
    int prevWidth = 1;
    for (int col = 0; col < row.cols; ++col) {
        const Cell c = row.at(col);
        if (c.text.isEmpty()) {
            if (prevWidth == 2) { prevWidth = 1; continue; }   // Spacer hinter breitem Zeichen
            // Passt ein breites Zeichen nicht mehr in die letzte Spalte, bricht das Terminal
            // davor um und lässt die Spalte leer. Diese Lücke stand nie in der Ausgabe; als
            // Leerzeichen gelesen zerschnitte sie einen Link an dieser Stelle.
            if (wideCharFollows && col == row.cols - 1) continue;
            out.text += QLatin1Char(' ');
            out.map.push_back({absRow, col});
        } else {
            out.text += c.text;
            for (int i = 0; i < c.text.size(); ++i) out.map.push_back({absRow, col});
        }
        prevWidth = c.width;
    }
}

// So viele Spalten am rechten Rand gelten als „bis an den Rand geschrieben“.
constexpr int kEdgeColumns = 2;

// Ein Spaltenbereich einer Zeile.
struct Columns {
    int from = 0;
    int to = 0;   // ausschließlich
};

// Letzte Spalte, die das Zeichen an `ref` belegt (ein breites Zeichen belegt zwei).
int endColumn(const Row &row, const CellRef &ref) {
    return ref.col + (row.at(ref.col).width == 2 ? 1 : 0);
}

// Beginnt der Text an Spalte `col` mit einem Schema (`wort://` oder `mailto:`)? Dann steht
// dort ein eigener Link und keine Fortsetzung.
bool startsWithScheme(const Row &row, int col) {
    QString head;
    for (int c = col; c < row.cols && head.size() < 24 && !row.blank(c); ++c) head += row.at(c).text;
    static const QRegularExpression scheme(QStringLiteral(R"(^(\w[\w+.-]*://|mailto:))"));
    return scheme.match(head).hasMatch();
}

// Könnte das Wort, das an `endIdx` der logischen Zeile endet, in der nächsten Bildschirmzeile
// weitergehen, obwohl es dort nicht nahtlos weiterläuft? So sieht ein Link aus, den das
// PROGRAMM umbrochen hat (eigener Zeilenumbruch oder Cursor-Positionierung statt Autowrap).
//
// 🔑 Ob er wirklich weitergeht, ist am Bildschirm nicht zu entscheiden. An Claude Code
// gemessen (beide Renderer): Eine hart umbrochene Adresse und eine Adresse, die genau am Rand
// endet und der ein neues Wort folgt, ergeben dasselbe Bild. Darum wird hier nicht
// zusammengesetzt, sondern nur festgestellt, dass das Ziel unvollständig sein kann.
//
// Drei Formen:
//   Rand:    Das Wort endet in der letzten oder vorletzten Spalte, die nächste Zeile enthält
//            Text. Die vorletzte zählt mit: Claude Code bricht die eigene Eingabe des
//            Anwenders eine Spalte vor dem Rand um (gemessen), und ein breites Zeichen, das
//            nicht mehr passt, lässt die letzte Spalte ebenfalls leer.
//   Zelle:   Das Wort füllt eine Tabellenzelle bis an deren Rahmen, dieselbe Zelle der
//            nächsten Zeile enthält Text.
//   Block:   Das Wort endet am Zeilenende, und die nächste Zeile ist ein einziges Wort, das
//            in derselben Spalte endet (ein Programm, das schmaler als das Fenster umbricht).
//            Nur für Adressen, und nicht, wenn darunter erkennbar ein eigener Link beginnt:
//            Gleich lange Pfade und Adressen untereinander sind der Alltag.
//
// 🔑 Eine Fortsetzungs-Marke auf der nächsten Zeile entlastet nur, wo Marken etwas belegen.
// Läuft das Wort dort nahtlos weiter (Spalte 0), endet es gar nicht hier, und diese Frage
// stellt sich nicht. Steht dort erst Leerraum, sagt die Marke: Das Terminal hat umbrochen,
// die Zeile geht weiter, das Wort ist zu Ende. Unter ConPTY gilt das nicht: Dort trägt auch
// eine Zeile die Marke, die mit der vorigen nichts zu tun hat, sobald ein Programm die vorige
// bis in die letzte Spalte beschrieben und ohne Zeilenvorschub verlassen hat
// (`VtScreen::conPtyHost`; am Windows-Mitschnitt trägt fast jede Zeile von Claude Code eine).
// Dort zählt mit Marke noch die letzte Spalte: „Adresse, ein Leerzeichen, Umbruch“ bleibt
// Fließtext.
//
// `where` nennt die Spalten der nächsten Zeile, in denen die Fortsetzung stünde.
bool mayContinueBelow(const VtScreen &sc, const Line &line, qsizetype endIdx,
                      LinkDetector::Span::Kind kind, Columns *where = nullptr) {
    if (endIdx < 0 || endIdx >= static_cast<qsizetype>(line.map.size())) return false;
    const CellRef end = line.map[endIdx];
    const int next = end.row + 1;
    if (next >= sc.scrollbackCount() + sc.rows()) return false;
    const bool marked = rowContinues(sc, next);
    if (marked && !sc.conPtyHost()) return false;
    const Row cur(sc, end.row), below(sc, next);
    const int endCol = endColumn(cur, end);

    int after = endCol + 1;
    while (after < cur.cols && cur.blank(after)) ++after;
    const int gap = after - endCol - 1;
    const int belowFirst = below.firstInk(0, below.cols);
    if (belowFirst < 0) return false;                 // nächste Zeile leer: Der Link ist zu Ende
    if (where) *where = {0, below.cols};

    // Rand.
    if (after >= cur.cols && endCol >= cur.cols - (marked ? 1 : kEdgeColumns))
        return canContinueALink(below.ch(belowFirst));

    // Zelle: höchstens ein Leerzeichen bis zum Rahmen rechts.
    if (after < cur.cols && isBox(cur.ch(after)) && gap <= 1) {
        int left = end.col;
        while (left >= 0 && !isBox(cur.ch(left))) --left;      // -1: kein Rahmen links
        if (after >= below.cols || !isBox(below.ch(after))) return false;
        if (left >= 0 && !isBox(below.ch(left))) return false;
        const int first = below.firstInk(left + 1, after);
        if (where) *where = {left + 1, after};
        return first >= 0 && canContinueALink(below.ch(first));
    }

    // Block.
    if (after >= cur.cols && kind == LinkDetector::Span::Url) {
        if (below.lastInk() < 0 || endColumn(below, {next, below.lastInk()}) != endCol) return false;
        for (int col = belowFirst; col <= below.lastInk(); ++col) {
            const Cell c = below.at(col);
            const bool spacer = c.text.isEmpty() && col > 0 && below.at(col - 1).width == 2;
            if (!spacer && (c.text.isEmpty() || c.text.at(0).isSpace())) return false;
        }
        return canContinueALink(below.ch(belowFirst)) && !startsWithScheme(below, belowFirst);
    }
    return false;
}

// Index des letzten Zeichens des Wortes, das an `idx` endet oder dort weitergeht. Der
// Detektor kürzt Satzzeichen am Ende weg; maßgeblich ist, wo das Wort am Bildschirm endet.
qsizetype wordEnd(const QString &text, qsizetype idx) {
    qsizetype end = idx;
    while (end + 1 < text.size() && !text.at(end + 1).isSpace() && !isBox(text.at(end + 1))) ++end;
    return end;
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
    line.lastRow = last;
    for (int r = first; r <= last; ++r) {
        const bool wideCharFollows = r < last && Row(sc, r + 1).cols > 0
            && Row(sc, r + 1).at(0).width == 2;
        appendRow(sc, r, wideCharFollows, line);
    }
    return line;
}

Hit hyperlinkAt(const VtScreen &sc, int absRow, int col) {
    if (absRow < 0 || absRow >= sc.scrollbackCount() + sc.rows()) return {};
    const Row row(sc, absRow);
    if (col < 0 || col >= row.cols) return {};
    // Die rechte Hälfte eines breiten Zeichens gehört zu dessen Zelle.
    if (col > 0 && row.at(col).text.isEmpty() && row.at(col - 1).width == 2) --col;
    const quint32 id = row.at(col).link;
    if (!id) return {};
    const QString uri = sc.hyperlink(id);
    if (uri.isEmpty()) return {};          // Eintrag verdrängt: wie Text ohne Hyperlink

    Hit hit;
    hit.declared = true;
    hit.target = LinkDetector::declaredTarget(uri);
    if (hit.target.isEmpty()) {
        hit.blocked = Hit::Blocked::Refused;
        hit.refused = uri;
        return hit;
    }
    // Zusammenhängende Zellen derselben Kennung, auch in den Nachbarzeilen: So schreibt ein
    // Programm einen Link, den es selbst umbricht (je Zeile dieselbe Kennung).
    const auto has = [id](const Row &r, int c) {
        if (r.at(c).link == id) return true;
        return c > 0 && r.at(c).text.isEmpty() && r.at(c - 1).width == 2 && r.at(c - 1).link == id;
    };
    int c0 = col, c1 = col;
    while (c0 > 0 && has(row, c0 - 1)) --c0;
    while (c1 + 1 < row.cols && has(row, c1 + 1)) ++c1;
    hit.segments.append(Segment{absRow, c0, c1});
    const int total = sc.scrollbackCount() + sc.rows();
    for (int r = absRow - 1, n = 0; r >= 0 && n < kMaxChainRows; --r, ++n) {
        const Row up(sc, r);
        int e = up.cols - 1;
        while (e >= 0 && !has(up, e)) --e;
        if (e < 0) break;
        int s = e;
        while (s > 0 && has(up, s - 1)) --s;
        hit.segments.prepend(Segment{r, s, e});
    }
    for (int r = absRow + 1, n = 0; r < total && n < kMaxChainRows; ++r, ++n) {
        const Row down(sc, r);
        int s = 0;
        while (s < down.cols && !has(down, s)) ++s;
        if (s >= down.cols) break;
        int e = s;
        while (e + 1 < down.cols && has(down, e + 1)) ++e;
        hit.segments.append(Segment{r, s, e});
    }
    return hit;
}

Hit hitAt(const VtScreen &sc, const Line &line, const QList<LinkDetector::Span> &spans,
          int absRow, int col) {
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
        // Vom Programm umbrochen? Dann ist unklar, ob das Ziel hier endet.
        if (mayContinueBelow(sc, line, wordEnd(line.text, s.start + s.length - 1), s.kind)) {
            hit.blocked = Hit::Blocked::MaybeCut;
            return hit;
        }
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

bool continuesCutLink(const VtScreen &sc, int absRow, int col) {
    if (absRow < 0 || absRow >= sc.scrollbackCount() + sc.rows()) return false;
    // Nur das erste Wort der Zeile (oder ihrer Tabellenzelle) kann die Fortsetzung sein.
    const Row row(sc, absRow);
    if (col < 0 || col >= row.cols || row.blank(col) || isBox(row.ch(col))) return false;
    int start = col;
    while (start > 0 && !row.blank(start - 1) && !isBox(row.ch(start - 1))) --start;
    for (int c = start - 1; c >= 0 && !isBox(row.ch(c)); --c)
        if (!row.blank(c)) return false;

    // Aufwärts, Bildschirmzeile für Bildschirmzeile: Marken sind hier kein Maßstab (s.
    // mayContinueBelow), und die mittlere Zeile eines langen Links ist selbst nur ein Wort
    // bis an den Rand.
    for (int r = absRow, n = 0; r > 0 && n < kMaxChainRows; --r, ++n) {
        const Line above = logicalLine(sc, r - 1);
        if (above.firstRow < 0) return false;
        const auto endsInRowAbove = [&](qsizetype idx) {
            return idx >= 0 && idx < static_cast<qsizetype>(above.map.size())
                && above.map[idx].row == r - 1;
        };
        // Ohne Arbeitsverzeichnis: Adressen und absolute Pfade genügen für diese Frage.
        // Die Stelle muss dort stehen, wo die Fortsetzung stünde: bei einer Tabelle in derselben
        // Zelle, sonst irgendwo in der Zeile (dass es ihr erstes Wort ist, steht oben schon fest).
        Columns where;
        for (const auto &s : LinkDetector::detect(above.text)) {
            const qsizetype e = wordEnd(above.text, s.start + s.length - 1);
            if (endsInRowAbove(e) && mayContinueBelow(sc, above, e, s.kind, &where)
                && col >= where.from && col < where.to)
                return true;
        }
        const Row prev(sc, r - 1);
        const int first = prev.firstInk(0, prev.cols), last = prev.lastInk();
        if (first < 0) return false;
        for (int c = first; c <= last; ++c)
            if (prev.blank(c) && !(c > 0 && prev.at(c - 1).width == 2)) return false;
        qsizetype idx = -1;
        for (qsizetype i = static_cast<qsizetype>(above.map.size()) - 1; i >= 0; --i)
            if (above.map[i].row == r - 1 && above.map[i].col == last) { idx = i; break; }
        if (!endsInRowAbove(idx) || !mayContinueBelow(sc, above, idx, LinkDetector::Span::Url))
            return false;
    }
    return false;
}

Hit linkAt(const VtScreen &screen, int absRow, int col, const QString &cwd) {
    // Nennt das Programm das Ziel selbst, gilt das, auch wenn es abgelehnt wird: Der Text an
    // der Stelle ist dann nicht das Ziel.
    const Hit declared = hyperlinkAt(screen, absRow, col);
    if (declared.declared) return declared;
    const Line line = logicalLine(screen, absRow);
    return hitAt(screen, line, LinkDetector::detect(line.text, cwd), absRow, col);
}

} // namespace ScreenLinks
