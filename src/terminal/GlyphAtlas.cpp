#include "GlyphAtlas.h"

#include <QPainter>
#include <cmath>

namespace qtmux {

const GlyphAtlas::Entry GlyphAtlas::kInvalid{};

namespace {
constexpr int kAtlasWidth = 1024;       // feste Breite; gewachsen wird in der Höhe
constexpr int kInitialHeight = 256;
constexpr int kMaxHeight = 8192;
constexpr int kPad = 1;                  // 1 px Rand gegen Sampling-Bluten
} // namespace

void GlyphAtlas::configure(const QFont &font, qreal cellW, qreal cellH,
                           qreal baseline, qreal dpr) {
    const int tw = std::max(1, int(std::ceil(cellW * dpr)));
    const int th = std::max(1, int(std::ceil(cellH * dpr)));
    // Unverändert? Dann Cache behalten.
    if (m_generation != 0 && font == m_font && tw == m_tileW && th == m_tileH
        && qFuzzyCompare(m_dpr, dpr))
        return;

    m_font = font;
    m_cellW = cellW; m_cellH = cellH; m_baseline = baseline; m_dpr = dpr;
    m_tileW = tw; m_tileH = th;
    resetImage(kAtlasWidth, kInitialHeight);
}

void GlyphAtlas::resetImage(int w, int h) {
    m_image = QImage(w, h, QImage::Format_ARGB32_Premultiplied);
    m_image.setDevicePixelRatio(1.0);
    m_image.fill(Qt::transparent);
    m_cache.clear();
    m_penX = m_penY = m_rowH = 0;
    ++m_generation;
    m_contentDirty = true;
}

QString GlyphAtlas::keyFor(const QString &text, bool bold, bool italic) const {
    // Stil in zwei führende Steuerzeichen kodieren; danach der Text selbst.
    return QString(QChar(ushort((bold ? 2 : 0) | (italic ? 1 : 0)))) + text;
}

bool GlyphAtlas::ensureRow(int tileW, int tileH) {
    // Passt die Kachel in die aktuelle Zeile?
    if (m_penX + tileW + kPad <= m_image.width()) return true;
    // Neue Zeile beginnen.
    m_penX = 0;
    m_penY += m_rowH + kPad;
    m_rowH = 0;
    // Höhe reicht nicht → Bild verdoppeln und alten Inhalt erhalten (Pixel-Rechtecke
    // bleiben gültig, nur die Textur muss neu erzeugt werden → neue Generation).
    if (m_penY + tileH + kPad > m_image.height()) {
        const int newH = std::min(m_image.height() * 2, kMaxHeight);
        if (newH <= m_image.height()) return false;   // Maximalgröße erreicht
        QImage grown(m_image.width(), newH, QImage::Format_ARGB32_Premultiplied);
        grown.fill(Qt::transparent);
        {
            QPainter p(&grown);
            p.setCompositionMode(QPainter::CompositionMode_Source);
            p.drawImage(0, 0, m_image);
        }
        m_image = grown;
        ++m_generation;
        m_contentDirty = true;
    }
    return true;
}

const GlyphAtlas::Entry &GlyphAtlas::glyph(const QString &text, bool bold,
                                           bool italic, int cellWidthUnits) {
    if (text.isEmpty() || text == QStringLiteral(" ")) return kInvalid;

    const QString key = keyFor(text, bold, italic);
    auto it = m_cache.constFind(key);
    if (it != m_cache.constEnd()) return *it;

    const int tileW = m_tileW * std::max(1, cellWidthUnits);
    const int tileH = m_tileH;
    if (!ensureRow(tileW, tileH)) {
        // Atlas voll — als ungültig cachen, damit wir nicht in jedem Frame neu versuchen.
        return *m_cache.insert(key, kInvalid);
    }

    const QRect rect(m_penX, m_penY, tileW, tileH);

    // Glyphe weiß (volle Deckung) in die Kachel zeichnen; gefärbt wird im Shader.
    QFont f = m_font;
    if (bold) f.setBold(true);
    if (italic) f.setItalic(true);
    {
        QPainter p(&m_image);
        p.setRenderHint(QPainter::TextAntialiasing, true);
        p.setFont(f);
        p.setPen(Qt::white);
        // In Geräte-Pixeln zeichnen: Baseline relativ zur Kachel.
        p.translate(rect.topLeft());
        p.scale(m_dpr, m_dpr);
        p.drawText(QPointF(0, m_baseline), text);
    }

    m_penX += tileW + kPad;
    m_rowH = std::max(m_rowH, tileH);
    m_contentDirty = true;

    Entry e;
    e.rect = rect;
    e.valid = true;
    return *m_cache.insert(key, e);
}

} // namespace qtmux
