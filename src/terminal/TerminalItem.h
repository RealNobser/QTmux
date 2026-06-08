#pragma once

#include <QQuickPaintedItem>
#include <QFont>
#include <QColor>
#include <QPointer>

QT_BEGIN_NAMESPACE
class QWheelEvent;
QT_END_NAMESPACE

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
    Q_PROPERTY(bool broadcast READ broadcast WRITE setBroadcast NOTIFY broadcastChanged)
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

    /// Broadcast-Modus: Eingabe geht nicht an die eigene Session, sondern wird
    /// per `inputForBroadcast`-Signal nach außen gereicht (QML → an alle Sessions).
    bool broadcast() const { return m_broadcast; }
    void setBroadcast(bool on);

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
    /// Cmd/Strg+Mausrad — QML passt die globale Schriftgröße an (delta +1/−1).
    void zoomRequested(int delta);
    /// Im Broadcast-Modus: getippte/eingefügte Bytes, die QML an ALLE Sessions verteilt.
    void inputForBroadcast(const QByteArray &data);
    void broadcastChanged();

protected:
    void keyPressEvent(QKeyEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void geometryChange(const QRectF &newGeo, const QRectF &oldGeo) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    void recomputeGrid();
    QByteArray encodeKey(QKeyEvent *event) const;
    /// Eingabe-Bytes zustellen: im Broadcast-Modus per Signal nach außen, sonst an
    /// die eigene Session. Zentral genutzt von Tastatur- und Paste-Eingabe.
    void sendInput(const QByteArray &bytes);
    VtScreen *screen() const;
    QPoint cellAt(const QPointF &pos) const;     // Pixel -> Zellkoordinate (col,row)
    QString selectedText() const;
    void clearSelection();
    void onDamaged();                            // Damage + Scroll-Anker nachführen
    int maxScrollOffset() const;                 // = Scrollback-Zeilenzahl
    void scrollByLines(int lines);               // + = in die Historie, - = zurück
    /// Quelle einer sichtbaren Zeile (0..rows-1): true + sbIndex = Scrollback-Zeile,
    /// false + liveRow = Live-Screen-Zeile. Berücksichtigt den Scroll-Offset.
    bool viewportSource(int screenRow, int &sbIndex, int &liveRow) const;

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
    bool m_broadcast = false;   // Eingabe an alle Sessions (siehe sendInput)

    // Scrollback-Ansicht: 0 = Live-Boden, >0 = so viele Zeilen in die Historie.
    int m_scrollOffset = 0;
    int m_lastSbCount = 0;   // letzter scrollbackCount() — für die Anker-Nachführung
};

} // namespace qtmux
