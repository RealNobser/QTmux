#include "TerminalItem.h"
#include "Session.h"
#include "VtScreen.h"

#include <QPainter>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QFontDatabase>
#include <QFontMetricsF>
#include <QGuiApplication>
#include <QClipboard>
#include <algorithm>

namespace qtmux {

TerminalItem::TerminalItem(QQuickItem *parent) : QQuickPaintedItem(parent) {
    m_font = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    m_font.setStyleHint(QFont::Monospace);
    m_font.setPointSize(m_pointSize);

    setFlag(ItemHasContents, true);
    setAcceptedMouseButtons(Qt::AllButtons);
    setActiveFocusOnTab(true);
    setFillColor(m_defaultBg);
    recomputeGrid();
}

TerminalItem::~TerminalItem() = default;

QObject *TerminalItem::session() const { return m_session.data(); }

VtScreen *TerminalItem::screen() const {
    return m_session ? m_session->screen() : nullptr;
}

void TerminalItem::setSession(QObject *session) {
    auto *s = qobject_cast<Session *>(session);
    if (s == m_session) return;

    if (m_session && m_session->screen()) {
        disconnect(m_session->screen(), nullptr, this, nullptr);
    }
    m_session = s;

    m_scrollOffset = 0;            // neue Session -> Live-Boden zeigen
    if (VtScreen *sc = screen()) {
        connect(sc, &VtScreen::damaged, this, [this](const QRect &) { onDamaged(); });
        connect(sc, &VtScreen::cursorMoved, this, [this]() { update(); });
        m_lastSbCount = sc->scrollbackCount();
        // Auf die aktuelle Item-Größe synchronisieren.
        recomputeGrid();
        forceActiveFocus();
    }
    emit sessionChanged();
    update();
}

void TerminalItem::setBackgroundColor(const QColor &c) {
    if (c == m_defaultBg) return;
    m_defaultBg = c;
    setFillColor(c);
    emit colorsChanged();
    update();
}

void TerminalItem::setForegroundColor(const QColor &c) {
    if (c == m_defaultFg) return;
    m_defaultFg = c;
    emit colorsChanged();
    update();
}

void TerminalItem::setPointSize(int s) {
    if (s == m_pointSize || s <= 0) return;
    m_pointSize = s;
    m_font.setPointSize(s);
    recomputeGrid();
    emit fontChanged();
    update();
}

void TerminalItem::recomputeGrid() {
    QFontMetricsF fm(m_font);
    m_cellW = fm.horizontalAdvance(QChar('M'));
    if (m_cellW <= 0) m_cellW = fm.averageCharWidth();
    m_cellH = fm.height();
    m_baseline = fm.ascent();

    const int cols = m_cellW > 0 ? static_cast<int>(width() / m_cellW) : m_cols;
    const int rows = m_cellH > 0 ? static_cast<int>(height() / m_cellH) : m_rows;
    m_cols = std::max(cols, 1);
    m_rows = std::max(rows, 1);

    if (m_session) m_session->resize(m_cols, m_rows);
}

void TerminalItem::paint(QPainter *painter) {
    painter->fillRect(boundingRect(), m_defaultBg);
    VtScreen *sc = screen();
    if (!sc) return;

    painter->setFont(m_font);

    // Selektionsbereich als lineare Zellindizes (lo..hi), falls vorhanden.
    int selLo = -1, selHi = -1;
    if (m_hasSelection) {
        const int a = m_selAnchor.y() * m_cols + m_selAnchor.x();
        const int b = m_selCaret.y() * m_cols + m_selCaret.x();
        selLo = std::min(a, b);
        selHi = std::max(a, b);
    }
    const QColor selColor(0x5b, 0x8c, 0xff, 0x70);

    for (int row = 0; row < m_rows; ++row) {
        const qreal y = row * m_cellH;
        // Quelle dieser sichtbaren Zeile bestimmen (Scrollback oder Live-Screen).
        int sbIndex = 0, liveRow = 0;
        const bool isSb = viewportSource(row, sbIndex, liveRow);
        const std::vector<Cell> *sbLine =
            isSb && sbIndex < sc->scrollbackCount() ? &sc->scrollbackLine(sbIndex) : nullptr;

        for (int col = 0; col < m_cols; ++col) {
            Cell c;
            if (isSb) {
                if (sbLine && col < static_cast<int>(sbLine->size())) c = (*sbLine)[col];
            } else {
                c = sc->cell(liveRow, col);
            }
            const qreal x = col * m_cellW;

            if (!c.bgDefault) {
                painter->fillRect(QRectF(x, y, m_cellW * c.width, m_cellH),
                                  QColor::fromRgb(c.bg));
            }
            if (selLo >= 0) {
                const int idx = row * m_cols + col;
                if (idx >= selLo && idx <= selHi)
                    painter->fillRect(QRectF(x, y, m_cellW * c.width, m_cellH), selColor);
            }
            if (!c.text.isEmpty() && c.text != QStringLiteral(" ")) {
                painter->setPen(c.fgDefault ? m_defaultFg : QColor::fromRgb(c.fg));
                if (c.bold || c.italic) {
                    QFont f = m_font;
                    f.setBold(c.bold);
                    f.setItalic(c.italic);
                    painter->setFont(f);
                }
                painter->drawText(QPointF(x, y + m_baseline), c.text);
                if (c.bold || c.italic) painter->setFont(m_font);
            }
        }
    }

    // Cursor nur im Live-Boden zeigen (in der Historie ergibt er keinen Sinn).
    if (m_scrollOffset == 0 && sc->cursorVisible()) {
        const QPoint cur = sc->cursor();
        QRectF r(cur.x() * m_cellW, cur.y() * m_cellH, m_cellW, m_cellH);
        painter->fillRect(r, QColor(0xe6, 0xe7, 0xee, 0x99));
    }

    // Dezenter Scrollbalken am rechten Rand, sobald Historie existiert.
    const int sb = sc->scrollbackCount();
    if (sb > 0 && m_rows > 0) {
        const qreal total = sb + m_rows;
        const qreal viewH = height();
        const qreal thumbH = std::max(viewH * m_rows / total, 24.0);
        const qreal topFrac = (sb - m_scrollOffset) / total;   // erste sichtbare virtuelle Zeile
        const qreal yTop = std::min(topFrac * viewH, viewH - thumbH);
        const QColor thumb(0xff, 0xff, 0xff, m_scrollOffset > 0 ? 0x66 : 0x2a);
        painter->fillRect(QRectF(width() - 4, yTop, 3, thumbH), thumb);
    }
}

QByteArray TerminalItem::encodeKey(QKeyEvent *event) const {
    switch (event->key()) {
    case Qt::Key_Return:
    case Qt::Key_Enter:    return "\r";
    case Qt::Key_Backspace:return "\x7f";
    case Qt::Key_Tab:
        // Shift+Tab kann (je nach Plattform) als Key_Tab+Shift ankommen -> Back-Tab.
        return (event->modifiers() & Qt::ShiftModifier) ? QByteArray("\x1b[Z")
                                                         : QByteArray("\t");
    case Qt::Key_Backtab:  return "\x1b[Z";   // Shift+Tab (Back-Tab, CSI Z) -> z. B. Claude-Mode
    case Qt::Key_Escape:   return "\x1b";
    case Qt::Key_Up:       return "\x1b[A";
    case Qt::Key_Down:     return "\x1b[B";
    case Qt::Key_Right:    return "\x1b[C";
    case Qt::Key_Left:     return "\x1b[D";
    case Qt::Key_Home:     return "\x1b[H";
    case Qt::Key_End:      return "\x1b[F";
    case Qt::Key_PageUp:   return "\x1b[5~";
    case Qt::Key_PageDown: return "\x1b[6~";
    case Qt::Key_Delete:   return "\x1b[3~";
    default:
        return event->text().toUtf8();
    }
}

void TerminalItem::keyPressEvent(QKeyEvent *event) {
    if (!m_session) { event->ignore(); return; }

    // Copy/Paste-Tasten abfangen — plattformkorrekt, ohne das Terminal-Ctrl+C
    // (SIGINT) zu kapern. macOS: Cmd (=ControlModifier) ohne physisches Ctrl
    // (=MetaModifier). Sonst: Ctrl+Shift.
    const Qt::KeyboardModifiers mods = event->modifiers();
#if defined(Q_OS_MACOS)
    const bool cpMod = (mods & Qt::ControlModifier) && !(mods & Qt::MetaModifier);
#else
    const bool cpMod = (mods & Qt::ControlModifier) && (mods & Qt::ShiftModifier);
#endif
    if (cpMod && event->key() == Qt::Key_C) { copy();  event->accept(); return; }
    if (cpMod && event->key() == Qt::Key_V) { paste(); event->accept(); return; }

    // Scrollback per Tastatur: Shift+PageUp/Down (seitenweise), Shift+Home/End (Anfang/Boden).
    if (mods & Qt::ShiftModifier) {
        const int page = std::max(m_rows - 1, 1);
        switch (event->key()) {
        case Qt::Key_PageUp:   scrollByLines(page);              event->accept(); return;
        case Qt::Key_PageDown: scrollByLines(-page);             event->accept(); return;
        case Qt::Key_Home:     scrollByLines(maxScrollOffset()); event->accept(); return;
        case Qt::Key_End:      scrollByLines(-maxScrollOffset());event->accept(); return;
        default: break;
        }
    }

    const QByteArray bytes = encodeKey(event);
    if (!bytes.isEmpty()) {
        if (m_hasSelection) clearSelection();   // Tippen hebt die Selektion auf
        if (m_scrollOffset != 0) { m_scrollOffset = 0; update(); }  // Eingabe -> zum Boden
        m_session->write(bytes);
        event->accept();
    } else {
        QQuickPaintedItem::keyPressEvent(event);
    }
}

QPoint TerminalItem::cellAt(const QPointF &pos) const {
    int col = m_cellW > 0 ? static_cast<int>(pos.x() / m_cellW) : 0;
    int row = m_cellH > 0 ? static_cast<int>(pos.y() / m_cellH) : 0;
    col = std::clamp(col, 0, std::max(m_cols - 1, 0));
    row = std::clamp(row, 0, std::max(m_rows - 1, 0));
    return {col, row};
}

void TerminalItem::clearSelection() {
    if (!m_hasSelection) return;
    m_hasSelection = false;
    m_selAnchor = m_selCaret = QPoint(-1, -1);
    emit selectionChanged();
    update();
}

QString TerminalItem::selectedText() const {
    VtScreen *sc = screen();
    if (!sc || !m_hasSelection) return {};
    int a = m_selAnchor.y() * m_cols + m_selAnchor.x();
    int b = m_selCaret.y() * m_cols + m_selCaret.x();
    if (a > b) std::swap(a, b);
    const QPoint start(a % m_cols, a / m_cols);
    const QPoint end(b % m_cols, b / m_cols);

    QString out;
    for (int row = start.y(); row <= end.y(); ++row) {
        const int c0 = (row == start.y()) ? start.x() : 0;
        const int c1 = (row == end.y()) ? end.x() : m_cols - 1;
        // Selektion ist in Bildschirm-Zeilen — Quelle (Historie/Live) berücksichtigen,
        // damit beim Scrollen der wirklich sichtbare Text kopiert wird.
        int sbIndex = 0, liveRow = 0;
        const bool isSb = viewportSource(row, sbIndex, liveRow);
        const std::vector<Cell> *sbLine =
            isSb && sbIndex < sc->scrollbackCount() ? &sc->scrollbackLine(sbIndex) : nullptr;
        QString line;
        for (int col = c0; col <= c1 && col < m_cols; ++col) {
            Cell c;
            if (isSb) { if (sbLine && col < static_cast<int>(sbLine->size())) c = (*sbLine)[col]; }
            else c = sc->cell(liveRow, col);
            line += c.text.isEmpty() ? QStringLiteral(" ") : c.text;
        }
        while (line.endsWith(QLatin1Char(' '))) line.chop(1);  // rechte Leerzeichen weg
        if (row > start.y()) out += QLatin1Char('\n');
        out += line;
    }
    return out;
}

void TerminalItem::copy() {
    const QString t = selectedText();
    if (!t.isEmpty()) QGuiApplication::clipboard()->setText(t);
}

void TerminalItem::paste() {
    if (!m_session) return;
    const QString t = QGuiApplication::clipboard()->text();
    if (t.isEmpty()) return;
    QByteArray data = t.toUtf8();
    data.replace("\r\n", "\r");   // Zeilenumbrüche -> CR (wie Enter im Terminal)
    data.replace('\n', '\r');
    m_session->write(data);
}

void TerminalItem::geometryChange(const QRectF &newGeo, const QRectF &oldGeo) {
    QQuickPaintedItem::geometryChange(newGeo, oldGeo);
    if (newGeo.size() != oldGeo.size()) {
        recomputeGrid();
        update();
    }
}

void TerminalItem::mousePressEvent(QMouseEvent *event) {
    forceActiveFocus();
    if (event->button() == Qt::LeftButton) {
        m_selAnchor = m_selCaret = cellAt(event->position());
        m_selecting = true;
        m_hasSelection = false;          // erst ab Ziehen sichtbar
        emit selectionChanged();
        update();
    } else if (event->button() == Qt::RightButton) {
        // Selektion bleibt erhalten (für „Kopieren"); QML öffnet das Kontextmenü.
        emit contextMenuRequested();
    }
    event->accept();
}

void TerminalItem::mouseMoveEvent(QMouseEvent *event) {
    if (!m_selecting) { event->ignore(); return; }
    const QPoint cell = cellAt(event->position());
    if (cell != m_selCaret) {
        m_selCaret = cell;
        m_hasSelection = (m_selCaret != m_selAnchor);
        emit selectionChanged();
        update();
    }
    event->accept();
}

void TerminalItem::mouseReleaseEvent(QMouseEvent *event) {
    if (m_selecting) {
        m_selecting = false;
        // Reiner Klick (keine Bewegung) hebt eine evtl. alte Selektion auf.
        if (m_selCaret == m_selAnchor) clearSelection();
        event->accept();
        return;
    }
    event->ignore();
}

void TerminalItem::wheelEvent(QWheelEvent *event) {
    const int dy = event->angleDelta().y();
    if (dy == 0) { event->ignore(); return; }
    // Cmd (macOS) bzw. Strg + Mausrad -> zoomen statt scrollen.
    if (event->modifiers() & Qt::ControlModifier) {
        emit zoomRequested(dy > 0 ? 1 : -1);
        event->accept();
        return;
    }
    // Nach oben (dy>0) = in die Historie; 3 Zeilen je „Klick".
    scrollByLines(dy > 0 ? 3 : -3);
    event->accept();
}

int TerminalItem::maxScrollOffset() const {
    VtScreen *sc = screen();
    return sc ? sc->scrollbackCount() : 0;
}

void TerminalItem::scrollByLines(int lines) {
    const int o = std::clamp(m_scrollOffset + lines, 0, maxScrollOffset());
    if (o == m_scrollOffset) return;
    m_scrollOffset = o;
    if (m_hasSelection) clearSelection();   // Inhalt verschiebt sich -> Selektion verwerfen
    update();
}

bool TerminalItem::viewportSource(int screenRow, int &sbIndex, int &liveRow) const {
    VtScreen *sc = screen();
    const int sb = sc ? sc->scrollbackCount() : 0;
    // Virtuelle Zeile: Scrollback [0..sb-1] gefolgt von Live-Zeilen [sb..sb+rows-1].
    const int v = sb - m_scrollOffset + screenRow;
    if (v < sb) { sbIndex = v < 0 ? 0 : v; return true; }
    liveRow = v - sb;
    return false;
}

void TerminalItem::onDamaged() {
    // Ist der Blick in der Historie, beim Nachrücken neuer Zeilen den Anker halten,
    // damit der betrachtete Ausschnitt nicht wegspringt; am Boden weiter mitlaufen.
    VtScreen *sc = screen();
    const int sb = sc ? sc->scrollbackCount() : 0;
    if (m_scrollOffset > 0) {
        const int delta = sb - m_lastSbCount;
        if (delta > 0) m_scrollOffset = std::min(m_scrollOffset + delta, sb);
    }
    m_lastSbCount = sb;
    update();
}

} // namespace qtmux
