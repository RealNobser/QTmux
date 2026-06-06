#pragma once

#include <QQuickPaintedItem>
#include <QFont>
#include <QColor>
#include <QPointer>

namespace qtmux {

class Session;
class VtScreen;

/// QML-Item, das eine zugewiesene Session darstellt.
///
/// Rendering vorerst über QQuickPaintedItem/QPainter (GPU-getextured, robust);
/// eine GPU-Glyph-Atlas-Variante (QSGRenderNode) ist die spätere Performance-Stufe.
/// Besitzt die Session NICHT — die gehört dem SessionModel (ermöglicht Split-Panes
/// und Session-Wechsel ohne Neustart).
class TerminalItem : public QQuickPaintedItem {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QObject *session READ session WRITE setSession NOTIFY sessionChanged)
    Q_PROPERTY(int pointSize READ pointSize WRITE setPointSize NOTIFY fontChanged)
    Q_PROPERTY(QColor backgroundColor READ backgroundColor WRITE setBackgroundColor NOTIFY colorsChanged)
    Q_PROPERTY(QColor foregroundColor READ foregroundColor WRITE setForegroundColor NOTIFY colorsChanged)
    Q_PROPERTY(bool hasSelection READ hasSelection NOTIFY selectionChanged)
public:
    explicit TerminalItem(QQuickItem *parent = nullptr);
    ~TerminalItem() override;

    QObject *session() const;
    void setSession(QObject *session);

    int pointSize() const { return m_pointSize; }
    void setPointSize(int s);

    QColor backgroundColor() const { return m_defaultBg; }
    void setBackgroundColor(const QColor &c);
    QColor foregroundColor() const { return m_defaultFg; }
    void setForegroundColor(const QColor &c);

    void paint(QPainter *painter) override;

    /// Kopiert die aktuelle Maus-Selektion in die Zwischenablage (leer = nichts).
    Q_INVOKABLE void copy();
    /// Fügt den Zwischenablage-Text in die Session ein (Zeilenumbrüche -> CR).
    Q_INVOKABLE void paste();
    /// Ob aktuell etwas selektiert ist (für Menü-Aktivierung).
    Q_INVOKABLE bool hasSelection() const { return m_hasSelection; }

signals:
    void sessionChanged();
    void fontChanged();
    void colorsChanged();
    void selectionChanged();
    /// Rechtsklick im Terminal — QML öffnet daraufhin das Kontextmenü (Kopieren/Einfügen).
    void contextMenuRequested();

protected:
    void keyPressEvent(QKeyEvent *event) override;
    void geometryChange(const QRectF &newGeo, const QRectF &oldGeo) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    void recomputeGrid();
    QByteArray encodeKey(QKeyEvent *event) const;
    VtScreen *screen() const;
    QPoint cellAt(const QPointF &pos) const;     // Pixel -> Zellkoordinate (col,row)
    QString selectedText() const;
    void clearSelection();

    QPointer<Session> m_session;

    QFont m_font;
    int m_pointSize = 13;
    qreal m_cellW = 8;
    qreal m_cellH = 16;
    qreal m_baseline = 12;
    int m_cols = 80;
    int m_rows = 24;

    QColor m_defaultFg{0xe6, 0xe7, 0xee};
    QColor m_defaultBg{0x1e, 0x1f, 0x29};

    // Maus-Selektion (Zellkoordinaten col=x, row=y; Strom-/Zeilen-Selektion).
    QPoint m_selAnchor{-1, -1};
    QPoint m_selCaret{-1, -1};
    bool m_selecting = false;
    bool m_hasSelection = false;
};

} // namespace qtmux
