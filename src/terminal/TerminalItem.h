#pragma once

#include <QQuickPaintedItem>
#include <QFont>
#include <QColor>
#include <memory>

#include "ITerminalBackend.h"

namespace qtmux {

class VtScreen;

/// QML-Item, das eine Terminal-Session darstellt.
///
/// Verdrahtung: ITerminalBackend (PTY/SSH/…) -> VtScreen (libvterm) -> paint().
/// Rendering vorerst über QQuickPaintedItem/QPainter (GPU-getextured, robust);
/// eine GPU-Glyph-Atlas-Variante (QSGRenderNode) ist die spätere Performance-Stufe.
class TerminalItem : public QQuickPaintedItem {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(int pointSize READ pointSize WRITE setPointSize NOTIFY fontChanged)
public:
    explicit TerminalItem(QQuickItem *parent = nullptr);
    ~TerminalItem() override;

    int pointSize() const { return m_pointSize; }
    void setPointSize(int s);

    /// Startet eine lokale Shell-Session (Default-Backend für Phase 1).
    Q_INVOKABLE void startShell();

    void paint(QPainter *painter) override;

signals:
    void fontChanged();
    void titleChanged(const QString &title);

protected:
    void keyPressEvent(QKeyEvent *event) override;
    void geometryChange(const QRectF &newGeo, const QRectF &oldGeo) override;
    void mousePressEvent(QMouseEvent *event) override;

private:
    void recomputeGrid();
    QByteArray encodeKey(QKeyEvent *event) const;

    std::unique_ptr<ITerminalBackend> m_backend;
    std::unique_ptr<VtScreen> m_screen;

    QFont m_font;
    int m_pointSize = 13;
    qreal m_cellW = 8;
    qreal m_cellH = 16;
    qreal m_baseline = 12;
    int m_cols = 80;
    int m_rows = 24;

    QColor m_defaultFg{0xe6, 0xe7, 0xee};
    QColor m_defaultBg{0x1e, 0x1f, 0x29};
};

} // namespace qtmux
