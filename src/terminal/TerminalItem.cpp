#include "TerminalItem.h"
#include "Session.h"
#include "VtScreen.h"

#include <QPainter>
#include <QKeyEvent>
#include <QFontDatabase>
#include <QFontMetricsF>

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

    if (VtScreen *sc = screen()) {
        connect(sc, &VtScreen::damaged, this, [this](const QRect &) { update(); });
        connect(sc, &VtScreen::cursorMoved, this, [this]() { update(); });
        // Auf die aktuelle Item-Größe synchronisieren.
        recomputeGrid();
        forceActiveFocus();
    }
    emit sessionChanged();
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

    for (int row = 0; row < m_rows; ++row) {
        const qreal y = row * m_cellH;
        for (int col = 0; col < m_cols; ++col) {
            const Cell c = sc->cell(row, col);
            const qreal x = col * m_cellW;

            if (!c.bgDefault) {
                painter->fillRect(QRectF(x, y, m_cellW * c.width, m_cellH),
                                  QColor::fromRgb(c.bg));
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

    if (sc->cursorVisible()) {
        const QPoint cur = sc->cursor();
        QRectF r(cur.x() * m_cellW, cur.y() * m_cellH, m_cellW, m_cellH);
        painter->fillRect(r, QColor(0xe6, 0xe7, 0xee, 0x99));
    }
}

QByteArray TerminalItem::encodeKey(QKeyEvent *event) const {
    switch (event->key()) {
    case Qt::Key_Return:
    case Qt::Key_Enter:    return "\r";
    case Qt::Key_Backspace:return "\x7f";
    case Qt::Key_Tab:      return "\t";
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
    const QByteArray bytes = encodeKey(event);
    if (!bytes.isEmpty()) {
        m_session->write(bytes);
        event->accept();
    } else {
        QQuickPaintedItem::keyPressEvent(event);
    }
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
    event->accept();
}

} // namespace qtmux
