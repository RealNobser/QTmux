#pragma once

#include <QFont>
#include <QHash>
#include <QImage>
#include <QRect>
#include <QString>

namespace qtmux {

/// Dynamischer Glyph-Atlas für den GPU-Renderer (QTMUX-6).
///
/// Rastert jede genutzte Glyphe (Zeichen × bold/italic) genau einmal per QPainter
/// als Alpha-Maske in eine gemeinsame Textur und merkt sich ihr Pixel-Rechteck.
/// Der Renderer baut daraus texturierte Quads; die Vordergrundfarbe kommt per
/// Vertex-Farbe aus dem Shader (ein Atlas → beliebige Farben).
///
/// Bewusst **zellweise** (nicht run-/ligaturbasiert): das ist der übliche, maximal
/// cachebare Ansatz für GPU-Terminals. Programmier-Ligaturen brauchen Run-Shaping
/// und bleiben daher dem QPainter-Fallback-Pfad vorbehalten.
class GlyphAtlas {
public:
    /// Pixel-Rechteck einer Glyphe in der Atlas-Textur (Geräte-Pixel).
    struct Entry {
        QRect rect;        ///< Kachel in der Atlas-Textur
        bool valid = false;
    };

    GlyphAtlas() = default;

    /// Setzt Font + Zellmetrik (logisch) + Device-Pixel-Ratio. Leert den Cache,
    /// wenn sich etwas geändert hat (neue Generation). Muss vor glyph() stehen.
    void configure(const QFont &font, qreal cellW, qreal cellH, qreal baseline, qreal dpr);

    /// Liefert (und rastert bei Bedarf) die Kachel für ein Zeichen mit gegebenem Stil.
    /// `cellWidthUnits` = 1 (normal) oder 2 (Doppelbreite/CJK). Leere Rückgabe-Kachel
    /// (valid=false) bedeutet „nichts zu zeichnen" (Leerzeichen).
    const Entry &glyph(const QString &text, bool bold, bool italic, int cellWidthUnits);

    const QImage &image() const { return m_image; }
    int width() const { return m_image.width(); }
    int height() const { return m_image.height(); }

    /// Generation der Textur — erhöht sich bei (Neu-)Allokation des Bildpuffers.
    /// Der Renderer erkennt daran, dass die QSGTexture neu erzeugt werden muss.
    quint64 generation() const { return m_generation; }
    /// Wurde seit dem letzten clearContentDirty() in das Bild geschrieben?
    bool contentDirty() const { return m_contentDirty; }
    void clearContentDirty() { m_contentDirty = false; }

private:
    QString keyFor(const QString &text, bool bold, bool italic) const;
    bool ensureRow(int tileW, int tileH);     ///< Platz für eine Kachel schaffen (ggf. wachsen)
    void resetImage(int w, int h);

    QFont m_font;
    qreal m_cellW = 8, m_cellH = 16, m_baseline = 12, m_dpr = 1;
    int m_tileW = 8, m_tileH = 16;             ///< Kachelgröße in Geräte-Pixeln (1 Zelle)

    QImage m_image;
    QHash<QString, Entry> m_cache;
    int m_penX = 0, m_penY = 0, m_rowH = 0;    ///< Shelf-Packer-Zustand
    quint64 m_generation = 0;
    bool m_contentDirty = false;

    static const Entry kInvalid;
};

} // namespace qtmux
