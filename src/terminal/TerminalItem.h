#pragma once

#include <QQuickItem>
#include <QFont>
#include <QColor>
#include <QImage>
#include <QPointer>
#include "GlyphAtlas.h"

QT_BEGIN_NAMESPACE
class QWheelEvent;
class QPainter;
class QSGNode;
QT_END_NAMESPACE

namespace qtmux {

class Session;
class VtScreen;

/// QML-Item, das eine zugewiesene Session darstellt.
///
/// Rendering über den Scene-Graph mit GPU-Glyph-Atlas (QTMUX-6): Hintergrund/Cursor/
/// Selektion als farbige Quads, Glyphen als texturierte Quads aus einem dynamischen
/// Atlas (siehe GlyphAtlas + Glyph-Material). Als Rückfallnetz existiert ein
/// QPainter-in-QImage-Pfad (`gpuRendering=false` bzw. automatisch bei aktiven
/// Ligaturen, die Run-Shaping brauchen).
/// Besitzt die Session NICHT — die gehört dem SessionModel (ermöglicht Split-Panes
/// und Session-Wechsel ohne Neustart).
class TerminalItem : public QQuickItem {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QObject *session READ session WRITE setSession NOTIFY sessionChanged)
    Q_PROPERTY(int pointSize READ pointSize WRITE setPointSize NOTIFY fontChanged)
    Q_PROPERTY(QString fontFamily READ fontFamily WRITE setFontFamily NOTIFY fontChanged)
    Q_PROPERTY(bool ligatures READ ligatures WRITE setLigatures NOTIFY fontChanged)
    Q_PROPERTY(bool gpuRendering READ gpuRendering WRITE setGpuRendering NOTIFY gpuRenderingChanged)
    Q_PROPERTY(QColor backgroundColor READ backgroundColor WRITE setBackgroundColor NOTIFY colorsChanged)
    Q_PROPERTY(QColor foregroundColor READ foregroundColor WRITE setForegroundColor NOTIFY colorsChanged)
    Q_PROPERTY(QColor cursorColor READ cursorColor WRITE setCursorColor NOTIFY colorsChanged)
    Q_PROPERTY(bool hasSelection READ hasSelection NOTIFY selectionChanged)
    Q_PROPERTY(bool broadcast READ broadcast WRITE setBroadcast NOTIFY broadcastChanged)
    Q_PROPERTY(bool copyOnSelect READ copyOnSelect WRITE setCopyOnSelect NOTIFY copyOnSelectChanged)
    Q_PROPERTY(bool rightClickPaste READ rightClickPaste WRITE setRightClickPaste NOTIFY rightClickPasteChanged)
    Q_PROPERTY(bool pasteWarnMultiline READ pasteWarnMultiline WRITE setPasteWarnMultiline NOTIFY pasteWarnMultilineChanged)
public:
    explicit TerminalItem(QQuickItem *parent = nullptr);
    ~TerminalItem() override;

    QObject *session() const;
    void setSession(QObject *session);

    int pointSize() const { return m_pointSize; }
    void setPointSize(int s);

    QString fontFamily() const { return m_font.family(); }
    void setFontFamily(const QString &family);
    bool ligatures() const { return m_ligatures; }
    void setLigatures(bool on);

    /// GPU-Glyph-Atlas-Rendering (Standard). Aus = QPainter-Fallback. Ligaturen werden
    /// im GPU-Pfad über einen Glyph-Index-Atlas + Run-Shaping unterstützt (QTMUX-6-
    /// Folgeoptimierung); nur bei `gpuRendering=false` greift der QPainter-Fallback.
    bool gpuRendering() const { return m_gpu; }
    void setGpuRendering(bool on);

    QColor backgroundColor() const { return m_defaultBg; }
    void setBackgroundColor(const QColor &c);
    QColor foregroundColor() const { return m_defaultFg; }
    void setForegroundColor(const QColor &c);
    QColor cursorColor() const { return m_cursorColor; }
    void setCursorColor(const QColor &c);

    /// Broadcast-Modus: Eingabe geht nicht an die eigene Session, sondern wird
    /// per `inputForBroadcast`-Signal nach außen gereicht (QML → an alle Sessions).
    bool broadcast() const { return m_broadcast; }
    void setBroadcast(bool on);

    // Komfortoptionen (PuTTY-Stil), via QML/Settings gesetzt.
    bool copyOnSelect() const { return m_copyOnSelect; }
    void setCopyOnSelect(bool b) { if (b != m_copyOnSelect) { m_copyOnSelect = b; emit copyOnSelectChanged(); } }
    bool rightClickPaste() const { return m_rightClickPaste; }
    void setRightClickPaste(bool b) { if (b != m_rightClickPaste) { m_rightClickPaste = b; emit rightClickPasteChanged(); } }
    bool pasteWarnMultiline() const { return m_pasteWarnMultiline; }
    void setPasteWarnMultiline(bool b) { if (b != m_pasteWarnMultiline) { m_pasteWarnMultiline = b; emit pasteWarnMultilineChanged(); } }

    QSGNode *updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *) override;

    /// Kopiert die aktuelle Maus-Selektion in die Zwischenablage (leer = nichts).
    Q_INVOKABLE void copy();
    /// Fügt den Zwischenablage-Text in die Session ein (Zeilenumbrüche -> CR).
    /// Bei mehrzeiligem Inhalt + aktiver Warnung wird stattdessen
    /// `multilinePasteWarning` ausgelöst (QML bestätigt via confirmPaste()).
    Q_INVOKABLE void paste();
    /// Führt eine zuvor zurückgehaltene (mehrzeilige) Einfügung aus.
    Q_INVOKABLE void confirmPaste();
    /// Verwirft eine zurückgehaltene Einfügung.
    Q_INVOKABLE void cancelPaste();
    /// Ob aktuell etwas selektiert ist (für Menü-Aktivierung).
    Q_INVOKABLE bool hasSelection() const { return m_hasSelection; }

signals:
    void sessionChanged();
    void fontChanged();
    void gpuRenderingChanged();
    void colorsChanged();
    void selectionChanged();
    /// Rechtsklick im Terminal — QML öffnet daraufhin das Kontextmenü (Kopieren/Einfügen).
    void contextMenuRequested();
    /// Cmd/Strg+Mausrad — QML passt die globale Schriftgröße an (delta +1/−1).
    void zoomRequested(int delta);
    /// Im Broadcast-Modus: getippte/eingefügte Bytes, die QML an ALLE Sessions verteilt.
    void inputForBroadcast(const QByteArray &data);
    void broadcastChanged();
    void copyOnSelectChanged();
    void rightClickPasteChanged();
    void pasteWarnMultilineChanged();
    /// Mehrzeilige Einfügung erkannt — QML fragt nach (lineCount = Zeilenzahl).
    void multilinePasteWarning(int lineCount);

protected:
    void keyPressEvent(QKeyEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void geometryChange(const QRectF &newGeo, const QRectF &oldGeo) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    void recomputeGrid();
    void applyFontFeatures();   // Ligaturen je nach m_ligatures (de)aktivieren
    /// Effektiver Renderpfad: GPU, wenn aktiviert. Ligaturen laufen seit dem
    /// Glyph-Index-Atlas + Run-Shaping (2d6c51b) ebenfalls im GPU-Pfad — nur
    /// gpuRendering=false erzwingt noch den QPainter-Fallback.
    bool useGpu() const { return m_gpu; }
    /// Zeichnet den kompletten Terminalinhalt in logischen Koordinaten (Fallback +
    /// gemeinsame Logik). `painter` muss bereits auf Hintergrund/Font gesetzt sein.
    void paintContents(QPainter *painter);
    QByteArray encodeKey(QKeyEvent *event) const;
    /// Eingabe-Bytes zustellen: im Broadcast-Modus per Signal nach außen, sonst an
    /// die eigene Session. Zentral genutzt von Tastatur- und Paste-Eingabe.
    void sendInput(const QByteArray &bytes);
    /// Führt die eigentliche Einfügung aus: bracketed (falls App DECSET 2004 aktiv)
    /// an die eigene Session, oder im Broadcast-Modus roh an alle.
    void doPaste(const QByteArray &data);
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
    bool m_ligatures = false;   // Programmier-Ligaturen (Run-Rendering) opt-in
    bool m_gpu = true;          // GPU-Glyph-Atlas (Standard) vs. QPainter-Fallback
    // GPU-Pfad: Inhalt (Hintergrund/Glyphen/Unterstreichung) nur neu aufbauen, wenn
    // sich der Inhalt geändert hat. Cursor-/Selektions-Updates rebuilden nur das
    // (billige) dynamische Overlay. Gesetzt von Damage/Scroll/Resize/Font/Farbe.
    bool m_geomDirty = true;
    GlyphAtlas m_atlas;         // genutzt vom GPU-Pfad (in updatePaintNode)
    qreal m_cellW = 8;
    qreal m_cellH = 16;
    qreal m_baseline = 12;
    int m_cols = 80;
    int m_rows = 24;

    QColor m_defaultFg{0xe6, 0xe7, 0xee};
    QColor m_defaultBg{0x1e, 0x1f, 0x29};
    QColor m_cursorColor{0xe6, 0xe7, 0xee};

    // Maus-Selektion (Zellkoordinaten col=x, row=y; Strom-/Zeilen-Selektion).
    QPoint m_selAnchor{-1, -1};
    QPoint m_selCaret{-1, -1};
    bool m_selecting = false;
    bool m_hasSelection = false;
    bool m_broadcast = false;          // Eingabe an alle Sessions (siehe sendInput)
    bool m_copyOnSelect = false;       // Auswahl automatisch kopieren (PuTTY-Stil)
    bool m_rightClickPaste = false;    // Rechtsklick fügt ein statt Kontextmenü
    bool m_pasteWarnMultiline = true;  // Vor mehrzeiligem Einfügen warnen
    QByteArray m_pendingPaste;         // zurückgehaltene Einfügung (Multiline-Warnung)

    // Scrollback-Ansicht: 0 = Live-Boden, >0 = so viele Zeilen in die Historie.
    int m_scrollOffset = 0;
    int m_lastSbCount = 0;   // letzter scrollbackCount() — für die Anker-Nachführung
};

} // namespace qtmux
