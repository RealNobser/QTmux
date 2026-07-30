#include "TerminalItem.h"
#include "KeyEncoding.h"
#include "TerminalGrid.h"
#include "Session.h"
#include "VtScreen.h"
#include "LinkDetector.h"

#include <QDesktopServices>
#include <QHoverEvent>
#include <QUrl>
#include <QPainter>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QFontDatabase>
#include <QFontMetricsF>
#include <QGlyphRun>
#include <QGuiApplication>
#include <QRawFont>
#include <QTextLayout>
#include <QTextLine>
#include <QClipboard>
#include <QQuickWindow>
#include <QSGGeometry>
#include <QSGGeometryNode>
#include <QSGVertexColorMaterial>
#include <QSGMaterial>
#include <QSGMaterialShader>
#include <QSGTexture>
#include <QSGSimpleTextureNode>
#include <QSGTransformNode>
#include <QMatrix4x4>
#include <cmath>
#include <algorithm>
#include <vector>

namespace qtmux {

// ============================================================================
//  GPU-Glyph-Rendering: eigenes Material (Atlas-Alpha × Vertex-Vordergrundfarbe)
//  + Vertex-Layout. Alles file-lokal (nur hier genutzt). (QTMUX-6)
// ============================================================================
namespace {

// Vertex des Glyph-Knotens: Position (logisch), Atlas-Texturkoordinate, Farbe.
struct GlyphVertex {
    float x, y;
    float u, v;
    unsigned char r, g, b, a;
};

const QSGGeometry::AttributeSet &glyphAttributes() {
    static QSGGeometry::Attribute attrs[] = {
        QSGGeometry::Attribute::createWithAttributeType(
            0, 2, QSGGeometry::FloatType, QSGGeometry::PositionAttribute),
        QSGGeometry::Attribute::createWithAttributeType(
            1, 2, QSGGeometry::FloatType, QSGGeometry::TexCoordAttribute),
        QSGGeometry::Attribute::createWithAttributeType(
            2, 4, QSGGeometry::UnsignedByteType, QSGGeometry::ColorAttribute),
    };
    static QSGGeometry::AttributeSet set = { 3, sizeof(GlyphVertex), attrs };
    return set;
}

// Shader, der die Atlas-Deckung (Alpha) mit der Per-Vertex-Farbe multipliziert.
class GlyphShader : public QSGMaterialShader {
public:
    GlyphShader() {
        setShaderFileName(VertexStage, QStringLiteral(":/shaders/glyph.vert.qsb"));
        setShaderFileName(FragmentStage, QStringLiteral(":/shaders/glyph.frag.qsb"));
    }

    bool updateUniformData(RenderState &state, QSGMaterial *, QSGMaterial *) override {
        QByteArray *buf = state.uniformData();
        Q_ASSERT(buf->size() >= 68);
        bool changed = false;
        if (state.isMatrixDirty()) {
            const QMatrix4x4 m = state.combinedMatrix();
            memcpy(buf->data(), m.constData(), 64);
            changed = true;
        }
        if (state.isOpacityDirty()) {
            const float o = float(state.opacity());
            memcpy(buf->data() + 64, &o, 4);
            changed = true;
        }
        return changed;
    }

    void updateSampledImage(RenderState &, int, QSGTexture **texture,
                            QSGMaterial *newMaterial, QSGMaterial *) override;
};

class GlyphMaterial : public QSGMaterial {
public:
    GlyphMaterial() { setFlag(Blending, true); }
    QSGMaterialType *type() const override { static QSGMaterialType t; return &t; }
    QSGMaterialShader *createShader(QSGRendererInterface::RenderMode) const override {
        return new GlyphShader;
    }
    int compare(const QSGMaterial *o) const override {
        auto *other = static_cast<const GlyphMaterial *>(o);
        if (m_texture == other->m_texture) return 0;
        return m_texture < other->m_texture ? -1 : 1;
    }
    void setTexture(QSGTexture *t) { m_texture = t; }
    QSGTexture *texture() const { return m_texture; }
private:
    QSGTexture *m_texture = nullptr;
};

void GlyphShader::updateSampledImage(RenderState &state, int, QSGTexture **texture,
                                     QSGMaterial *newMaterial, QSGMaterial *) {
    auto *m = static_cast<GlyphMaterial *>(newMaterial);
    if (QSGTexture *t = m->texture()) {
        t->setFiltering(QSGTexture::Linear);
        // Pixeldaten der Atlas-Textur tatsächlich auf die GPU laden. Bei eigenem
        // Material zwingend selbst anstoßen (anders als QSGSimpleTextureNode) —
        // sonst bleibt die Textur leer und die Glyphen unsichtbar.
        t->commitTextureOperations(state.rhi(), state.resourceUpdateBatch());
        *texture = t;
    }
}

// Wurzelknoten des GPU-Pfads: drei Geometrie-Kinder (Hintergrund, Glyphen, Overlay)
// in Zeichenreihenfolge + die selbst verwaltete Atlas-Textur.
// QSGTransformNode statt QSGNode: trägt die Sub-Pixel-Snapping-Matrix (s.
// updatePaintNode), damit das Zellraster auch bei fraktionaler Pane-Position der
// SplitView auf dem Geräte-Pixel-Raster landet.
class GpuRoot : public QSGTransformNode {
public:
    QSGGeometryNode *bg = nullptr;
    QSGGeometryNode *glyph = nullptr;
    QSGGeometryNode *underline = nullptr;
    QSGGeometryNode *overlay = nullptr;
    GlyphMaterial *glyphMat = nullptr;
    QSGTexture *atlasTex = nullptr;
    quint64 texGen = 0;
    ~GpuRoot() override { delete atlasTex; }
};

// Hilfen zum Füllen der Geometrie ------------------------------------------------

void uploadColored(QSGGeometryNode *node,
                   const std::vector<QSGGeometry::ColoredPoint2D> &verts) {
    const int n = int(verts.size());
    QSGGeometry *g = node->geometry();
    if (!g) {
        g = new QSGGeometry(QSGGeometry::defaultAttributes_ColoredPoint2D(), n);
        g->setDrawingMode(QSGGeometry::DrawTriangles);
        node->setGeometry(g);
        node->setFlag(QSGNode::OwnsGeometry, true);
    } else {
        g->allocate(n);
    }
    if (n > 0) memcpy(g->vertexData(), verts.data(), n * sizeof(QSGGeometry::ColoredPoint2D));
    node->markDirty(QSGNode::DirtyGeometry);
}

void uploadGlyph(QSGGeometryNode *node, const std::vector<GlyphVertex> &verts) {
    const int n = int(verts.size());
    QSGGeometry *g = node->geometry();
    if (!g) {
        g = new QSGGeometry(glyphAttributes(), n);
        g->setDrawingMode(QSGGeometry::DrawTriangles);
        node->setGeometry(g);
        node->setFlag(QSGNode::OwnsGeometry, true);
    } else {
        g->allocate(n);
    }
    if (n > 0) memcpy(g->vertexData(), verts.data(), n * sizeof(GlyphVertex));
    node->markDirty(QSGNode::DirtyGeometry);
}

} // namespace

TerminalItem::TerminalItem(QQuickItem *parent) : QQuickItem(parent) {
    m_font = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    m_font.setStyleHint(QFont::Monospace);
    m_font.setPointSize(m_pointSize);
    applyFontFeatures();   // Ligaturen initial aus (opt-in)

    // Notausstieg/Support-Schalter: QTMUX_NO_GPU=1 erzwingt den QPainter-Fallback.
    if (qEnvironmentVariableIsSet("QTMUX_NO_GPU")) m_gpu = false;

    // QTMUX-86: Entprellung der Session-Größe. 60 ms sind lang genug, damit die
    // Zwischenschritte eines Layout-Aufbaus (mehrere Frames) zusammenfallen, und kurz
    // genug, dass ein Ziehen am Splitter unmittelbar wirkt.
    m_resizeTimer.setSingleShot(true);
    m_resizeTimer.setInterval(60);
    connect(&m_resizeTimer, &QTimer::timeout, this, &TerminalItem::applyPendingResize);

    setFlag(ItemHasContents, true);
    setAcceptedMouseButtons(Qt::AllButtons);
    setAcceptHoverEvents(true);   // für Link-Hervorhebung unter der Maus
    setActiveFocusOnTab(true);
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
        // Kein Größen-Vorbehalt mehr an dieser Stelle: recomputeGrid() bricht selbst ab,
        // wenn die Größe noch nicht feststeht (gridFor), und reicht die Größe ohnehin nur
        // entprellt weiter (QTMUX-86). Sonst stünde dieselbe Regel an zwei Stellen — und
        // die zweite (geometryChange) hatte sie früher nicht.
        recomputeGrid();
        forceActiveFocus();
    }
    m_geomDirty = true;   // neuer Inhalt
    emit sessionChanged();
    update();
}

void TerminalItem::setBackgroundColor(const QColor &c) {
    if (c == m_defaultBg) return;
    m_defaultBg = c;
    m_geomDirty = true;
    emit colorsChanged();
    update();
}

void TerminalItem::setForegroundColor(const QColor &c) {
    if (c == m_defaultFg) return;
    m_defaultFg = c;
    m_geomDirty = true;
    emit colorsChanged();
    update();
}

void TerminalItem::setCursorColor(const QColor &c) {
    if (c == m_cursorColor) return;
    m_cursorColor = c;
    emit colorsChanged();
    update();
}

void TerminalItem::setBroadcast(bool on) {
    if (on == m_broadcast) return;
    m_broadcast = on;
    emit broadcastChanged();
}

void TerminalItem::sendInput(const QByteArray &bytes) {
    if (bytes.isEmpty()) return;
    if (m_broadcast)
        emit inputForBroadcast(bytes);   // QML verteilt an ALLE Sessions (inkl. dieser)
    else if (m_session)
        m_session->write(bytes);
}

void TerminalItem::setPointSize(int s) {
    if (s == m_pointSize || s <= 0) return;
    m_pointSize = s;
    m_font.setPointSize(s);
    recomputeGrid();
    emit fontChanged();
    update();
}

void TerminalItem::setFontFamily(const QString &family) {
    if (family.isEmpty() || family == m_font.family()) return;
    m_font.setFamily(family);
    recomputeGrid();
    emit fontChanged();
    update();
}

void TerminalItem::setLigatures(bool on) {
    if (on == m_ligatures) return;
    m_ligatures = on;
    applyFontFeatures();
    m_geomDirty = true;
    emit fontChanged();
    update();   // useGpu() ändert sich → Renderpfad wird in updatePaintNode getauscht
}

void TerminalItem::setGpuRendering(bool on) {
    if (on == m_gpu) return;
    m_gpu = on;
    m_geomDirty = true;
    emit gpuRenderingChanged();
    update();
}

// Programmier-Ligaturen über OpenType-Features schalten. Aus (Default) unterdrückt
// `liga`/`calt`/`dlig`; an überlässt sie dem Font (Run-Rendering im Fallback formt sie).
void TerminalItem::applyFontFeatures() {
    if (m_ligatures) {
        m_font.unsetFeature(QFont::Tag("liga"));
        m_font.unsetFeature(QFont::Tag("calt"));
        m_font.unsetFeature(QFont::Tag("dlig"));
    } else {
        m_font.setFeature(QFont::Tag("liga"), 0);
        m_font.setFeature(QFont::Tag("calt"), 0);
        m_font.setFeature(QFont::Tag("dlig"), 0);
    }
}

void TerminalItem::recomputeGrid() {
    QFontMetricsF fm(m_font);
    qreal cw = fm.horizontalAdvance(QChar('M'));
    if (cw <= 0) cw = fm.averageCharWidth();

    // Zellmaße auf GANZE Geräte-Pixel runden (Crispness): fraktionale Zellmaße legen die
    // Glyph-Quads bei col*cellW / row*cellH auf HALBE Geräte-Pixel — der Atlas wird dann
    // interpoliert (matschig) und die Grundlinie driftet Zeile für Zeile. Snappen wir
    // cellW/cellH/baseline auf ein Vielfaches von 1/dpr, liegt jede Spalte/Zeile auf einem
    // echten Geräte-Pixel und die Atlas-Kachel (ceil(cellW*dpr)) deckt sich exakt mit dem Quad.
    QQuickWindow *win = window();
    const qreal dpr = win ? win->effectiveDevicePixelRatio() : 1.0;
    const auto snap = [dpr](qreal v) { return dpr > 0 ? std::round(v * dpr) / dpr : v; };
    m_cellW = std::max(snap(cw), 1.0);
    m_cellH = std::max(snap(fm.height()), 1.0);
    m_baseline = snap(fm.ascent());
    m_gridDpr = dpr;

    m_geomDirty = true;   // Raster/Metrik geändert → Inhalt neu aufbauen

    // QTMUX-86, Teil 1: ohne belastbare Größe gar nichts ableiten. `geometryChange` feuert
    // auch für den Übergang auf 0×0, den ein Item beim Neuaufbau des Pane-Baums durchläuft;
    // früher wurde das Raster dann auf 1×1 geklemmt. Regel + Begründung: gridFor().
    const TerminalGrid g = gridFor(width(), height(), m_cellW, m_cellH);
    if (!g.valid) return;              // bisheriges Raster behalten, nichts anstoßen
    m_cols = g.cols;
    m_rows = g.rows;

    // QTMUX-86, Teil 2: die Session NICHT sofort resizen, sondern erst wenn das Layout zur
    // Ruhe gekommen ist (Begründung am Timer im Header). Das Rendering nutzt m_cols/m_rows
    // sofort weiter — für die paar Millisekunden zeichnet das Pane höchstens leere Zeilen.
    if (m_session) m_resizeTimer.start();
}

void TerminalItem::applyPendingResize() {
    if (m_session) m_session->resize(m_cols, m_rows);
}

QColor TerminalItem::effectiveFg(const Cell &c) const {
    QColor fg = c.fgDefault ? m_defaultFg : QColor::fromRgb(c.fg);
    if (c.faint) {
        // Faint/Dim (SGR 2): Vordergrund Richtung Hintergrund mischen (~45 %) —
        // so rendern Terminals gedimmten/Geister-Text (z. B. Claudes Vorschläge).
        const QColor bg = c.bgDefault ? m_defaultBg : QColor::fromRgb(c.bg);
        constexpr qreal t = 0.45;
        fg = QColor::fromRgbF(fg.redF()   + (bg.redF()   - fg.redF())   * t,
                              fg.greenF() + (bg.greenF() - fg.greenF()) * t,
                              fg.blueF()  + (bg.blueF()  - fg.blueF())  * t);
    }
    return fg;
}

// --- Gemeinsame Zeichenlogik (Fallback + Referenz) --------------------------
// Zeichnet den kompletten Inhalt in LOGISCHEN Koordinaten. Im Fallback-Pfad rendert
// dies in ein QImage; der GPU-Pfad baut dieselbe Bildbeschreibung als Geometrie.
void TerminalItem::paintContents(QPainter *painter) {
    painter->fillRect(boundingRect(), m_defaultBg);
    VtScreen *sc = screen();
    if (!sc) return;

    painter->setFont(m_font);

    // Selektion liegt in ABSOLUTEN Inhalts-Zeilen (Scrollback-Index, Live-Screen
    // dahinter) — so bleibt sie beim Scrollen am Text. selBase = absolute Zeile der
    // obersten sichtbaren Zeile; damit wird die Viewport-Zeile in den Selektionsraum
    // umgerechnet.
    int selLo = -1, selHi = -1;
    const int selBase = sc->scrollbackCount() - m_scrollOffset;
    if (m_hasSelection) {
        const int a = m_selAnchor.y() * m_cols + m_selAnchor.x();
        const int b = m_selCaret.y() * m_cols + m_selCaret.x();
        selLo = std::min(a, b);
        selHi = std::max(a, b);
    }
    const QColor selColor(0x5b, 0x8c, 0xff, 0x70);

    auto drawGlyph = [&](const QString &text, qreal x, qreal y, const QColor &fg,
                         bool bold, bool italic, bool underline) {
        painter->setPen(fg);
        if (bold || italic || underline) {
            QFont f = m_font;
            f.setBold(bold); f.setItalic(italic); f.setUnderline(underline);
            painter->setFont(f);
        }
        painter->drawText(QPointF(x, y + m_baseline), text);
        if (bold || italic || underline) painter->setFont(m_font);
    };

    for (int row = 0; row < m_rows; ++row) {
        const qreal y = row * m_cellH;
        int sbIndex = 0, liveRow = 0;
        const bool isSb = viewportSource(row, sbIndex, liveRow);
        const std::vector<Cell> *sbLine =
            isSb && sbIndex < sc->scrollbackCount() ? &sc->scrollbackLine(sbIndex) : nullptr;

        QString run;
        qreal runX = 0;
        QColor runFg;
        bool runBold = false, runItalic = false, runUnder = false;
        auto flush = [&]() {
            if (run.isEmpty()) return;
            drawGlyph(run, runX, y, runFg, runBold, runItalic, runUnder);
            run.clear();
        };

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
                const int idx = (selBase + row) * m_cols + col;
                if (idx >= selLo && idx <= selHi)
                    painter->fillRect(QRectF(x, y, m_cellW * c.width, m_cellH), selColor);
            }

            const bool visible = !c.text.isEmpty() && c.text != QStringLiteral(" ");
            if (!visible || c.width != 1) {
                flush();
                if (visible)
                    drawGlyph(c.text, x, y, effectiveFg(c),
                              c.bold, c.italic, c.underline);
                continue;
            }
            const QColor fg = effectiveFg(c);
            if (!run.isEmpty() && (fg != runFg || c.bold != runBold
                                   || c.italic != runItalic || c.underline != runUnder))
                flush();
            if (run.isEmpty()) {
                runX = x; runFg = fg;
                runBold = c.bold; runItalic = c.italic; runUnder = c.underline;
            }
            run += c.text;
        }
        flush();
    }

    if (m_scrollOffset == 0 && sc->cursorVisible()) {
        const QPoint cur = sc->cursor();
        QRectF r(cur.x() * m_cellW, cur.y() * m_cellH, m_cellW, m_cellH);
        QColor cc = m_cursorColor;
        cc.setAlpha(0x99);
        painter->fillRect(r, cc);
    }

    const int sb = sc->scrollbackCount();
    if (sb > 0 && m_rows > 0) {
        const qreal total = sb + m_rows;
        const qreal viewH = height();
        const qreal thumbH = std::max(viewH * m_rows / total, 24.0);
        const qreal topFrac = (sb - m_scrollOffset) / total;
        const qreal yTop = std::min(topFrac * viewH, viewH - thumbH);
        const QColor thumb(0xff, 0xff, 0xff, m_scrollOffset > 0 ? 0x66 : 0x2a);
        painter->fillRect(QRectF(width() - 4, yTop, 3, thumbH), thumb);
    }
}

// --- Scene-Graph: GPU-Pfad + Fallback-Pfad ----------------------------------
QSGNode *TerminalItem::updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *) {
    if (width() <= 0 || height() <= 0) { delete oldNode; return nullptr; }
    QQuickWindow *win = window();
    const qreal dpr = win ? win->effectiveDevicePixelRatio() : 1.0;

    // ---- Fallback: QPainter → QImage → QSGSimpleTextureNode ----------------
    if (!useGpu() || !win) {
        auto *tn = dynamic_cast<QSGSimpleTextureNode *>(oldNode);
        if (!tn) { delete oldNode; tn = new QSGSimpleTextureNode; tn->setOwnsTexture(true); }
        const QSize px(std::max(1, int(std::ceil(width() * dpr))),
                       std::max(1, int(std::ceil(height() * dpr))));
        QImage img(px, QImage::Format_ARGB32_Premultiplied);
        img.setDevicePixelRatio(dpr);
        img.fill(Qt::transparent);
        { QPainter p(&img); paintContents(&p); }
        QSGTexture *t = win ? win->createTextureFromImage(img, QQuickWindow::TextureHasAlphaChannel)
                            : nullptr;
        tn->setTexture(t);          // ownsTexture → alte Textur wird freigegeben
        tn->setRect(boundingRect());
        tn->setFiltering(QSGTexture::Linear);
        return tn;
    }

    // ---- GPU-Pfad: Glyph-Atlas + Geometrie ---------------------------------
    auto *root = dynamic_cast<GpuRoot *>(oldNode);
    bool firstBuild = false;
    if (!root) {
        delete oldNode;
        root = new GpuRoot;
        // Ein QSGGeometryNode muss VOR appendChildNode Material UND Geometry
        // besitzen (Debug-Assert in qsgnode.cpp) — daher hier sofort eine leere
        // Geometrie setzen; uploadColored/uploadGlyph allokieren später nur um.
        auto initColored = [](QSGGeometryNode *n) {
            auto *g = new QSGGeometry(QSGGeometry::defaultAttributes_ColoredPoint2D(), 0);
            g->setDrawingMode(QSGGeometry::DrawTriangles);
            n->setGeometry(g);
            n->setFlag(QSGNode::OwnsGeometry, true);
        };
        root->bg = new QSGGeometryNode;
        root->bg->setMaterial(new QSGVertexColorMaterial);
        root->bg->setFlag(QSGNode::OwnsMaterial, true);
        initColored(root->bg);
        root->glyph = new QSGGeometryNode;
        root->glyphMat = new GlyphMaterial;
        root->glyph->setMaterial(root->glyphMat);
        root->glyph->setFlag(QSGNode::OwnsMaterial, true);
        {
            auto *g = new QSGGeometry(glyphAttributes(), 0);
            g->setDrawingMode(QSGGeometry::DrawTriangles);
            root->glyph->setGeometry(g);
            root->glyph->setFlag(QSGNode::OwnsGeometry, true);
        }
        root->underline = new QSGGeometryNode;
        root->underline->setMaterial(new QSGVertexColorMaterial);
        root->underline->setFlag(QSGNode::OwnsMaterial, true);
        initColored(root->underline);
        root->overlay = new QSGGeometryNode;
        root->overlay->setMaterial(new QSGVertexColorMaterial);
        root->overlay->setFlag(QSGNode::OwnsMaterial, true);
        initColored(root->overlay);
        // Zeichenreihenfolge: Hintergrund → Glyphen → Unterstreichung → Overlay.
        // Unterstreichung gehört zum Inhalt (nur bei Damage neu gebaut), liegt aber
        // ÜBER den Glyphen; das Overlay (Selektion/Cursor/Scrollbalken) zuoberst.
        root->appendChildNode(root->bg);
        root->appendChildNode(root->glyph);
        root->appendChildNode(root->underline);
        root->appendChildNode(root->overlay);
        firstBuild = true;
    }

    auto pushColoredQuad = [](std::vector<QSGGeometry::ColoredPoint2D> &v,
                              qreal x, qreal y, qreal w, qreal h, const QColor &c) {
        if (w <= 0 || h <= 0) return;
        const int a = c.alpha();
        // Scene-Graph erwartet vormultiplizierte Vertexfarben.
        const uchar r = uchar(c.red() * a / 255);
        const uchar g = uchar(c.green() * a / 255);
        const uchar b = uchar(c.blue() * a / 255);
        const uchar A = uchar(a);
        const float x0 = float(x), y0 = float(y), x1 = float(x + w), y1 = float(y + h);
        QSGGeometry::ColoredPoint2D q[6];
        q[0].set(x0, y0, r, g, b, A); q[1].set(x1, y0, r, g, b, A); q[2].set(x0, y1, r, g, b, A);
        q[3].set(x1, y0, r, g, b, A); q[4].set(x1, y1, r, g, b, A); q[5].set(x0, y1, r, g, b, A);
        for (auto &pt : q) v.push_back(pt);
    };

    VtScreen *sc = screen();
    const QColor selColor(0x5b, 0x8c, 0xff, 0x70);

    // ---- Inhalt (Hintergrund + Glyphen + Unterstreichung) -------------------
    // Nur neu aufbauen, wenn sich der Inhalt geändert hat (Damage/Scroll/Resize/
    // Font/Farbe). Cursor-Bewegung und Maus-Selektion lassen m_geomDirty unberührt
    // und rebuilden nur das (billige) dynamische Overlay weiter unten.
    if (firstBuild || m_geomDirty) {
        m_atlas.configure(m_font, m_cellW, m_cellH, m_baseline, dpr);

        std::vector<QSGGeometry::ColoredPoint2D> bgVerts, ulVerts;
        std::vector<GlyphVertex> glVerts;
        // Glyph-Quads zunächst mit PIXEL-UV sammeln; nach der Schleife normalisieren
        // (der Atlas kann während der Schleife wachsen → Größe ändert sich).
        // `color` = Farb-Glyphe (Emoji): der Shader nutzt dann die Atlas-RGB direkt.
        struct GQ { float x, y, w, h; QRect uv; uchar r, g, b; bool color; };
        std::vector<GQ> glyphQuads;

        // Ganzflächiger Standard-Hintergrund.
        pushColoredQuad(bgVerts, 0, 0, width(), height(), m_defaultBg);

        // Shaping-Pfad (nur bei aktiven Ligaturen): eine Zell-Folge wird per
        // QTextLayout geformt (Ligaturen entstehen), die geformten Glyphen landen
        // einzeln als texturierte Quads über den Glyph-Index-Atlas. Der Atlas bleibt
        // dadurch durch die Glyph-Zahl des Fonts begrenzt (kein Run-Text-Cache).
        // Platzierung in LOGISCHEN Koordinaten; Baseline robust auf rowY+m_baseline
        // verankert (per-Glyph-Versatz aus den Shaping-Positionen relativ zur Ascent).
        auto emitRun = [&](const QString &runText, int startCol, qreal rowY,
                           const QColor &fg, bool bold, bool italic) {
            if (runText.isEmpty()) return;
            QFont f = m_font;
            if (bold) f.setBold(true);
            if (italic) f.setItalic(true);
            QTextLayout layout(runText, f);
            layout.setCacheEnabled(true);
            layout.beginLayout();
            QTextLine line = layout.createLine();
            if (!line.isValid()) { layout.endLayout(); return; }
            line.setLineWidth(1e7);
            layout.endLayout();

            const qreal runX = startCol * m_cellW;
            const qreal ascent = line.ascent();
            const uchar fr = uchar(fg.red()), fgc = uchar(fg.green()), fb = uchar(fg.blue());
            for (const QGlyphRun &gr : line.glyphRuns()) {
                const QRawFont rf = gr.rawFont();
                const QList<quint32> idx = gr.glyphIndexes();
                const QList<QPointF> pos = gr.positions();
                for (int i = 0; i < idx.size(); ++i) {
                    const GlyphAtlas::IndexedEntry &e = m_atlas.glyphByIndex(rf, idx[i], dpr);
                    if (!e.valid || e.empty) continue;
                    const float gx = float(runX + pos[i].x() + e.offset.x());
                    const float gy = float(rowY + m_baseline + (pos[i].y() - ascent) + e.offset.y());
                    glyphQuads.push_back({gx, gy, float(e.sizeLogical.width()),
                                          float(e.sizeLogical.height()), e.rect, fr, fgc, fb,
                                          e.color});
                }
            }
        };

        if (sc) {
            for (int row = 0; row < m_rows; ++row) {
                const qreal y = row * m_cellH;
                int sbIndex = 0, liveRow = 0;
                const bool isSb = viewportSource(row, sbIndex, liveRow);
                const std::vector<Cell> *sbLine =
                    isSb && sbIndex < sc->scrollbackCount() ? &sc->scrollbackLine(sbIndex) : nullptr;

                // Run-Akkumulator für den Ligatur-Pfad (sonst ungenutzt).
                QString run; int runStart = 0; QColor runFg; bool runBold = false, runItalic = false;
                auto flushRun = [&]() {
                    if (run.isEmpty()) return;
                    emitRun(run, runStart, y, runFg, runBold, runItalic);
                    run.clear();
                };

                for (int col = 0; col < m_cols; ++col) {
                    Cell c;
                    if (isSb) { if (sbLine && col < int(sbLine->size())) c = (*sbLine)[col]; }
                    else c = sc->cell(liveRow, col);
                    const qreal x = col * m_cellW;
                    const int wUnits = std::max(1, int(c.width));

                    if (!c.bgDefault)
                        pushColoredQuad(bgVerts, x, y, m_cellW * c.width, m_cellH,
                                        QColor::fromRgb(c.bg));

                    const bool visible = !c.text.isEmpty() && c.text != QStringLiteral(" ");
                    const QColor fg = effectiveFg(c);

                    if (visible && c.underline) {
                        const qreal uy = y + m_baseline + 1;
                        pushColoredQuad(ulVerts, x, uy, m_cellW * wUnits, std::max(1.0, dpr), fg);
                    }

                    if (!m_ligatures) {
                        // Zellweiser Atlas (Standardpfad, maximal cachebar).
                        if (visible) {
                            const GlyphAtlas::Entry &e = m_atlas.glyph(c.text, c.bold, c.italic, wUnits);
                            if (e.valid)
                                glyphQuads.push_back({float(x), float(y),
                                                      float(m_cellW * wUnits), float(m_cellH), e.rect,
                                                      uchar(fg.red()), uchar(fg.green()), uchar(fg.blue()),
                                                      e.color});
                        }
                        continue;
                    }

                    // Ligatur-Pfad: Run aus einbreiten, gleich attributierten Zellen bilden;
                    // Lücken/Breitzeichen/Attributwechsel brechen den Run (geformt separat).
                    if (!visible || c.width != 1) {
                        flushRun();
                        if (visible) emitRun(c.text, col, y, fg, c.bold, c.italic);
                        continue;
                    }
                    if (!run.isEmpty() && (fg != runFg || c.bold != runBold
                                           || c.italic != runItalic))
                        flushRun();
                    if (run.isEmpty()) {
                        runStart = col; runFg = fg; runBold = c.bold; runItalic = c.italic;
                    }
                    run += c.text;
                }
                flushRun();
            }
        }

        // Atlas-Textur bei Inhalts-/Größenänderung (neu) erzeugen.
        if (!root->atlasTex || root->texGen != m_atlas.generation() || m_atlas.contentDirty()) {
            delete root->atlasTex;
            root->atlasTex = win->createTextureFromImage(m_atlas.image(),
                                                         QQuickWindow::TextureHasAlphaChannel);
            root->texGen = m_atlas.generation();
            m_atlas.clearContentDirty();
        }
        root->glyphMat->setTexture(root->atlasTex);

        // Glyph-Quads jetzt mit endgültiger Atlas-Größe normalisieren.
        const float aw = std::max(1, m_atlas.width());
        const float ah = std::max(1, m_atlas.height());
        glVerts.reserve(glyphQuads.size() * 6);
        for (const GQ &q : glyphQuads) {
            const float u0 = q.uv.x() / aw, v0 = q.uv.y() / ah;
            const float u1 = (q.uv.x() + q.uv.width()) / aw, v1 = (q.uv.y() + q.uv.height()) / ah;
            const float x0 = q.x, y0 = q.y, x1 = q.x + q.w, y1 = q.y + q.h;
            // Vertex-Alpha ist der Shader-Selektor: 255 = Mono-Glyphe (per fg tönen),
            // 0 = Farb-Glyphe (Emoji, Atlas-RGB direkt). NICHT Deckung/Opazität.
            const uchar sel = q.color ? 0 : 255;
            GlyphVertex v[6] = {
                {x0, y0, u0, v0, q.r, q.g, q.b, sel}, {x1, y0, u1, v0, q.r, q.g, q.b, sel},
                {x0, y1, u0, v1, q.r, q.g, q.b, sel}, {x1, y0, u1, v0, q.r, q.g, q.b, sel},
                {x1, y1, u1, v1, q.r, q.g, q.b, sel}, {x0, y1, u0, v1, q.r, q.g, q.b, sel},
            };
            for (const auto &pt : v) glVerts.push_back(pt);
        }

        uploadColored(root->bg, bgVerts);
        uploadGlyph(root->glyph, glVerts);
        uploadColored(root->underline, ulVerts);
        m_geomDirty = false;
    }

    // ---- Dynamisches Overlay (Selektion + Cursor + Scrollbalken) ------------
    // Bei JEDEM Update neu — billig und zell-zugriffsfrei: Selektion als Zeilen-
    // Spans (eine Box je Zeile über den Spaltenbereich), Cursor + Scrollbalken
    // als einzelne Quads. So kosten Cursor-/Selektions-Updates keinen Glyph-Aufbau.
    std::vector<QSGGeometry::ColoredPoint2D> ovVerts;
    if (sc) {
        if (m_hasSelection) {
            // Selektion in ABSOLUTEN Inhalts-Zeilen; pro sichtbarer Zeile in
            // Viewport-Koordinaten umrechnen (selBase = absolute oberste Zeile),
            // damit sie beim Scrollen am Text bleibt und nur sichtbare Teile rendert.
            const int a = m_selAnchor.y() * m_cols + m_selAnchor.x();
            const int b = m_selCaret.y() * m_cols + m_selCaret.x();
            const int lo = std::min(a, b), hi = std::max(a, b);
            const int r0 = lo / m_cols, c0 = lo % m_cols;
            const int r1 = hi / m_cols, c1 = hi % m_cols;
            const int selBase = sc->scrollbackCount() - m_scrollOffset;
            for (int absRow = r0; absRow <= r1; ++absRow) {
                const int vr = absRow - selBase;          // sichtbare Zeile
                if (vr < 0 || vr >= m_rows) continue;
                const int from = (absRow == r0) ? c0 : 0;
                const int to   = (absRow == r1) ? c1 : m_cols - 1;
                pushColoredQuad(ovVerts, from * m_cellW, vr * m_cellH,
                                (to - from + 1) * m_cellW, m_cellH, selColor);
            }
        }

        if (m_scrollOffset == 0 && sc->cursorVisible()) {
            const QPoint cur = sc->cursor();
            QColor cc = m_cursorColor; cc.setAlpha(0x99);
            pushColoredQuad(ovVerts, cur.x() * m_cellW, cur.y() * m_cellH, m_cellW, m_cellH, cc);
        }

        // Link-Unterstreichung unter der Maus (schon beim einfachen Drüberfahren gesetzt,
        // zur Auffindbarkeit — der Klick zum Öffnen bleibt Cmd/Ctrl): dünne Linie an der
        // Zellunterkante über den Spaltenbereich des Treffers, in Viewport-Koordinaten
        // wie die Selektion umgerechnet.
        if (m_hoverRow >= 0 && m_hoverC1 >= m_hoverC0) {
            const int vr = m_hoverRow - (sc->scrollbackCount() - m_scrollOffset);
            if (vr >= 0 && vr < m_rows) {
                const QColor linkColor(0x4d, 0x9d, 0xff);   // Link-Blau, hell/dunkel tauglich
                pushColoredQuad(ovVerts, m_hoverC0 * m_cellW, (vr + 1) * m_cellH - 2,
                                (m_hoverC1 - m_hoverC0 + 1) * m_cellW, 2, linkColor);
            }
        }

        // Scrollback-Suchtreffer (QTMUX-71): alle Treffer halbtransparent amber (Text bleibt
        // lesbar), der aktuelle kräftiger. In Viewport-Zeilen umgerechnet wie die Selektion.
        if (m_searchActive && !m_matches.isEmpty()) {
            const int base = sc->scrollbackCount() - m_scrollOffset;
            for (int i = 0; i < m_matches.size(); ++i) {
                const int vr = m_matches[i].line - base;
                if (vr < 0 || vr >= m_rows) continue;
                const QColor hl(0xf5, 0xc4, 0x51, i == m_currentMatch ? 0xcc : 0x60);
                pushColoredQuad(ovVerts, m_matches[i].col * m_cellW, vr * m_cellH,
                                m_matches[i].length * m_cellW, m_cellH, hl);
            }
        }

        const int sb = sc->scrollbackCount();
        if (sb > 0 && m_rows > 0) {
            const qreal total = sb + m_rows;
            const qreal viewH = height();
            const qreal thumbH = std::max(viewH * m_rows / total, 24.0);
            const qreal topFrac = (sb - m_scrollOffset) / total;
            const qreal yTop = std::min(topFrac * viewH, viewH - thumbH);
            const QColor thumb(0xff, 0xff, 0xff, m_scrollOffset > 0 ? 0x66 : 0x2a);
            pushColoredQuad(ovVerts, width() - 4, yTop, 3, thumbH, thumb);
        }
    }
    uploadColored(root->overlay, ovVerts);

    // Sub-Pixel-Snapping (Crispness bei Split-Panes): das Zellraster ist relativ zur
    // Pane-Ecke ganzzahlig (recomputeGrid), aber die SplitView platziert die Pane an
    // fraktionaler Szenen-Position → das ganze Raster liegt sonst um <1 Geräte-Pixel
    // verschoben und wird interpoliert (matschig). Wir verschieben den gezeichneten
    // Inhalt um den Bruchteil der Szenen-Position (in Geräte-Pixeln, zum nächsten Pixel
    // gerundet) zurück, damit die ABSOLUTE Position auf dem Geräte-Pixel-Raster landet.
    // Während updatePaintNode ist der GUI-Thread blockiert → mapToScene ist hier sicher.
    const QPointF scenePos = mapToScene(QPointF(0, 0));
    const double dx = (scenePos.x() * dpr - std::round(scenePos.x() * dpr)) / dpr;
    const double dy = (scenePos.y() * dpr - std::round(scenePos.y() * dpr)) / dpr;
    QMatrix4x4 snap;
    snap.translate(float(-dx), float(-dy));
    root->setMatrix(snap);
    return root;
}

QByteArray TerminalItem::encodeKey(QKeyEvent *event) const {
    // Reine Übersetzungslogik lebt Gui-frei in qtmux_core (KeyEncoding.cpp),
    // damit sie ohne Quick-Aufbau unit-testbar ist (QTMUX-43).
    return encodeKeyBytes(event->key(), event->modifiers(), event->text());
}

void TerminalItem::keyPressEvent(QKeyEvent *event) {
    if (!m_session) { event->ignore(); return; }

    const Qt::KeyboardModifiers mods = event->modifiers();
#if defined(Q_OS_MACOS)
    const bool cpMod = (mods & Qt::ControlModifier) && !(mods & Qt::MetaModifier);
#else
    const bool cpMod = (mods & Qt::ControlModifier) && (mods & Qt::ShiftModifier);
#endif
    if (cpMod && event->key() == Qt::Key_C) { copy();  event->accept(); return; }
    if (cpMod && event->key() == Qt::Key_V) { paste(); event->accept(); return; }

#if !defined(Q_OS_MACOS)
    // Smart Ctrl+C/V (Windows-Terminal-Stil): Ctrl+C kopiert NUR, wenn Text markiert
    // ist, und löscht danach die Auswahl — ohne Auswahl fällt es durch zu encodeKey
    // (\x03 = SIGINT). Ctrl+V fügt ein. Ctrl+Shift+C/V bleiben zusätzlich erhalten.
    const bool plainCtrl = (mods & Qt::ControlModifier)
                           && !(mods & Qt::ShiftModifier) && !(mods & Qt::AltModifier);
    if (plainCtrl && event->key() == Qt::Key_C && m_hasSelection) {
        copy(); clearSelection(); event->accept(); return;
    }
    if (plainCtrl && event->key() == Qt::Key_V) { paste(); event->accept(); return; }
#endif

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
        if (m_hasSelection) clearSelection();
        if (m_scrollOffset != 0) { m_scrollOffset = 0; m_geomDirty = true; update(); }
        sendInput(bytes);
        event->accept();
    } else {
        QQuickItem::keyPressEvent(event);
    }
}

QPoint TerminalItem::cellAt(const QPointF &pos) const {
    int col = m_cellW > 0 ? static_cast<int>(pos.x() / m_cellW) : 0;
    int row = m_cellH > 0 ? static_cast<int>(pos.y() / m_cellH) : 0;
    col = std::clamp(col, 0, std::max(m_cols - 1, 0));
    row = std::clamp(row, 0, std::max(m_rows - 1, 0));
    return {col, row};
}

QPoint TerminalItem::absCellAt(const QPointF &pos) const {
    const QPoint vp = cellAt(pos);            // (col, sichtbare Zeile)
    VtScreen *sc = screen();
    const int base = (sc ? sc->scrollbackCount() : 0) - m_scrollOffset;
    return {vp.x(), base + vp.y()};           // (col, absolute Inhalts-Zeile)
}

bool TerminalItem::isLinkModifier(Qt::KeyboardModifiers mods) {
#if defined(Q_OS_MACOS)
    // macOS: Cmd = ControlModifier (physisches Ctrl = MetaModifier, das schließen wir aus).
    return (mods & Qt::ControlModifier) && !(mods & Qt::MetaModifier);
#else
    return (mods & Qt::ControlModifier) && !(mods & Qt::AltModifier);
#endif
}

QString TerminalItem::absLineText(int absRow) const {
    VtScreen *sc = screen();
    if (!sc || absRow < 0) return {};
    const int sb = sc->scrollbackCount();
    const std::vector<Cell> *sbLine =
        (absRow < sb) ? &sc->scrollbackLine(absRow) : nullptr;
    const int liveRow = absRow - sb;
    QString line;
    for (int col = 0; col < m_cols; ++col) {
        Cell c;
        if (sbLine) { if (col < static_cast<int>(sbLine->size())) c = (*sbLine)[col]; }
        else if (liveRow >= 0 && liveRow < m_rows) c = sc->cell(liveRow, col);
        line += c.text.isEmpty() ? QStringLiteral(" ") : c.text;
    }
    return line;
}

QString TerminalItem::sessionCwd() const {
    return m_session ? m_session->currentWorkingDirectory() : QString();
}

void TerminalItem::updateHoverLink(const QPointF &pos, Qt::KeyboardModifiers /*mods*/) {
    int newRow = -1, c0 = 0, c1 = -1;
    QString target;
    // Schon beim einfachen Drüberfahren nach Links suchen (Auffindbarkeit: unterstreichen
    // + Hand-Cursor + Tooltip, damit man den Cmd/Ctrl-Klick überhaupt entdeckt). Der Klick
    // zum Öffnen bleibt in mousePressEvent Cmd/Ctrl-gebunden. Das detect()-Ergebnis wird je
    // Zeile gecacht, damit das Fahren INNERHALB einer Zeile keine QFileInfo-Syscalls je
    // Pixel auslöst.
    if (screen() && pos.x() >= 0) {
        const QPoint ac = absCellAt(pos);
        const QString lineText = absLineText(ac.y());
        if (ac.y() != m_hoverDetectRow || lineText != m_hoverDetectText) {
            m_hoverDetectRow = ac.y();
            m_hoverDetectText = lineText;
            m_hoverSpans = LinkDetector::detect(lineText, sessionCwd());
        }
        for (const auto &s : m_hoverSpans) {
            if (ac.x() >= s.start && ac.x() < s.start + s.length) {
                newRow = ac.y(); c0 = s.start; c1 = s.start + s.length - 1;
                target = s.target;
                break;
            }
        }
    }
    if (newRow != m_hoverRow || c0 != m_hoverC0 || c1 != m_hoverC1) {
        const bool targetChanged = (target != m_hoverTarget);
        m_hoverRow = newRow; m_hoverC0 = c0; m_hoverC1 = c1; m_hoverTarget = target;
        setCursor(newRow >= 0 ? Qt::PointingHandCursor : Qt::ArrowCursor);
        if (targetChanged) emit hoverLinkChanged();
        update();
    }
}

bool TerminalItem::openLinkAt(const QPointF &pos) {
    if (!screen()) return false;
    const QPoint ac = absCellAt(pos);
    const auto spans = LinkDetector::detect(absLineText(ac.y()), sessionCwd());
    for (const auto &s : spans) {
        if (ac.x() < s.start || ac.x() >= s.start + s.length) continue;
        const QUrl url = (s.kind == LinkDetector::Span::FilePath)
            ? QUrl::fromLocalFile(s.target)
            : QUrl::fromUserInput(s.target);
        // Doppelte Sicherung: nur freigegebene Schemata an den OS-Handler geben.
        if (!url.scheme().isEmpty() && LinkDetector::isAllowedScheme(url.scheme()))
            QDesktopServices::openUrl(url);
        return true;   // Treffer verbraucht den Klick, auch wenn das Schema abgelehnt wurde
    }
    return false;
}

// --- Scrollback-Suche (QTMUX-71) -----------------------------------------------------
void TerminalItem::recomputeMatches() {
    m_matches.clear();
    VtScreen *sc = screen();
    if (!sc || m_searchQuery.isEmpty()) return;
    // Gesamten Inhalt (Scrollback + sichtbar) zu Zeilen zusammensetzen; Zeilenindex =
    // absolute Inhalts-Zeile. absLineText baut 1 Zeichen/Spalte (Spalten↔Zeichen 1:1,
    // solange keine Emoji davor — dieselbe Konvention wie die Link-Erkennung).
    const int total = sc->scrollbackCount() + m_rows;
    QStringList lines;
    lines.reserve(total);
    for (int r = 0; r < total; ++r) lines << absLineText(r);
    m_matches = TerminalSearch::find(lines, m_searchQuery, /*caseSensitive=*/false);
}

void TerminalItem::scrollToMatch(int idx) {
    if (idx < 0 || idx >= m_matches.size()) return;
    VtScreen *sc = screen();
    if (!sc) return;
    // m_scrollOffset so wählen, dass der Treffer etwa in Zeilenmitte sichtbar wird.
    const int off = std::clamp(sc->scrollbackCount() - m_matches[idx].line + m_rows / 2,
                               0, maxScrollOffset());
    if (off != m_scrollOffset) { m_scrollOffset = off; m_geomDirty = true; }
}

void TerminalItem::beginSearch() {
    if (m_searchActive) return;
    m_searchActive = true;
    emit searchChanged();
    update();
}

void TerminalItem::updateSearch(const QString &query) {
    m_searchActive = true;
    m_searchQuery = query;
    recomputeMatches();
    m_currentMatch = -1;
    if (!m_matches.isEmpty()) {
        VtScreen *sc = screen();
        const int topAbs = sc ? sc->scrollbackCount() - m_scrollOffset : 0;
        int pick = 0;                                   // ersten Treffer ab dem Sichtbereich
        for (int i = 0; i < m_matches.size(); ++i)
            if (m_matches[i].line >= topAbs) { pick = i; break; }
        m_currentMatch = pick;
        scrollToMatch(pick);
    }
    emit searchChanged();
    update();
}

void TerminalItem::searchNext() {
    if (m_matches.isEmpty()) return;
    m_currentMatch = (m_currentMatch + 1) % m_matches.size();
    scrollToMatch(m_currentMatch);
    emit searchChanged();
    update();
}

void TerminalItem::searchPrev() {
    if (m_matches.isEmpty()) return;
    m_currentMatch = (m_currentMatch - 1 + m_matches.size()) % m_matches.size();
    scrollToMatch(m_currentMatch);
    emit searchChanged();
    update();
}

void TerminalItem::endSearch() {
    if (!m_searchActive && m_matches.isEmpty()) return;
    m_searchActive = false;
    m_matches.clear();
    m_searchQuery.clear();
    m_currentMatch = -1;
    emit searchChanged();
    update();
}

void TerminalItem::clearSelection() {
    if (!m_hasSelection) return;
    m_hasSelection = false;
    m_selAnchor = m_selCaret = QPoint(-1, -1);
    emit selectionChanged();
    update();
}

void TerminalItem::selectAll() {
    VtScreen *sc = screen();
    if (!sc || m_cols <= 0 || m_rows <= 0) return;
    // Absolute Inhaltszeilen: 0 … scrollbackCount()-1 = Historie, dahinter der
    // Live-Screen. Anker auf den Anfang, Caret auf die letzte Zelle der letzten
    // Live-Zeile — damit liefert selectedText() genau das, was auch ein Drag über
    // alles liefern würde (inkl. Soft-Wrap-Behandlung).
    const int lastRow = sc->scrollbackCount() + m_rows - 1;
    if (lastRow < 0) return;
    m_selAnchor = QPoint(0, 0);
    m_selCaret = QPoint(m_cols - 1, lastRow);
    m_hasSelection = true;
    m_selecting = false;
    emit selectionChanged();
    if (m_copyOnSelect) copy();
    update();
}

QString TerminalItem::selectedText() const {
    VtScreen *sc = screen();
    if (!sc || !m_hasSelection) return {};
    // Selektion in ABSOLUTEN Inhalts-Zeilen: row < sb -> Scrollback-Zeile, sonst
    // Live-Screen-Zeile (row - sb). Unabhängig vom aktuellen Scroll-Offset, daher
    // liefert Kopieren denselben Text, egal wohin gerade gescrollt ist.
    int a = m_selAnchor.y() * m_cols + m_selAnchor.x();
    int b = m_selCaret.y() * m_cols + m_selCaret.x();
    if (a > b) std::swap(a, b);
    const QPoint start(a % m_cols, a / m_cols);
    const QPoint end(b % m_cols, b / m_cols);
    const int sb = sc->scrollbackCount();

    // Ist die absolute Zeile `row` ein weicher Umbruch (Autowrap-Fortsetzung) der
    // vorigen? Dann gehört sie zur SELBEN logischen Zeile → beim Kopieren KEIN \n
    // einfügen (sonst wird ein umbrochener Befehl fälschlich als mehrzeilig erkannt).
    auto isContinuation = [&](int absRow) -> bool {
        if (absRow < sb) return absRow >= 0 && sc->scrollbackContinuation(absRow);
        const int lr = absRow - sb;
        return lr >= 0 && lr < m_rows && sc->lineContinuation(lr);
    };

    QString out;
    for (int row = start.y(); row <= end.y(); ++row) {
        const int c0 = (row == start.y()) ? start.x() : 0;
        const int c1 = (row == end.y()) ? end.x() : m_cols - 1;
        const std::vector<Cell> *sbLine =
            (row < sb && row >= 0) ? &sc->scrollbackLine(row) : nullptr;
        const int liveRow = row - sb;
        QString line;
        for (int col = c0; col <= c1 && col < m_cols; ++col) {
            Cell c;
            if (sbLine) { if (col < static_cast<int>(sbLine->size())) c = (*sbLine)[col]; }
            else if (liveRow >= 0 && liveRow < m_rows) c = sc->cell(liveRow, col);
            line += c.text.isEmpty() ? QStringLiteral(" ") : c.text;
        }
        // Bei einer fortgesetzten (umbrochenen) Zeile rechte Leerzeichen NICHT
        // trimmen — der Text läuft ja in der nächsten Zeile weiter.
        if (!isContinuation(row + 1))
            while (line.endsWith(QLatin1Char(' '))) line.chop(1);
        if (row > start.y() && !isContinuation(row)) out += QLatin1Char('\n');
        out += line;
    }
    return out;
}

void TerminalItem::copy() {
    const QString t = selectedText();
    if (!t.isEmpty()) { QGuiApplication::clipboard()->setText(t); return; }
    // Diagnose (Copy-Bug): Kopieren angefordert, aber selectedText() ist leer. Verrät beim
    // nächsten Auftreten die Ursache im Log (Console.app) — reine Whitespace-Auswahl (wird
    // zu "" getrimmt), verlorene Auswahl, oder falsches/leeres Pane. Sichtbar nur im
    // Problemfall, kein Rauschen im Normalbetrieb.
    qWarning("QTmux copy(): nichts kopiert (hasSelection=%d, cols=%d, anchor=%d,%d caret=%d,%d)",
             m_hasSelection ? 1 : 0, m_cols,
             m_selAnchor.x(), m_selAnchor.y(), m_selCaret.x(), m_selCaret.y());
}

void TerminalItem::paste() {
    const QString t = QGuiApplication::clipboard()->text();
    if (t.isEmpty()) return;
    QByteArray data = t.toUtf8();
    data.replace("\r\n", "\r");
    data.replace('\n', '\r');
    if (!m_broadcast && m_pasteWarnMultiline && data.contains('\r')) {
        m_pendingPaste = data;
        emit multilinePasteWarning(static_cast<int>(data.count('\r')) + 1);
        return;
    }
    doPaste(data);
}

void TerminalItem::confirmPaste() {
    const QByteArray d = m_pendingPaste;
    m_pendingPaste.clear();
    doPaste(d);
}

void TerminalItem::cancelPaste() { m_pendingPaste.clear(); }

void TerminalItem::doPaste(const QByteArray &data) {
    if (data.isEmpty()) return;
    if (m_broadcast) { emit inputForBroadcast(data); return; }
    VtScreen *sc = screen();
    if (sc) sc->startPaste();
    if (m_session) m_session->write(data);
    if (sc) sc->endPaste();
}

void TerminalItem::geometryChange(const QRectF &newGeo, const QRectF &oldGeo) {
    QQuickItem::geometryChange(newGeo, oldGeo);
    if (newGeo.size() != oldGeo.size())
        recomputeGrid();
    // Immer neu zeichnen: bei Größenänderung wegen des Rasters, bei reinem
    // Positionswechsel wegen der Sub-Pixel-Snapping-Matrix (hängt an der Szenen-
    // Position). Der teure Inhaltsaufbau bleibt über m_geomDirty gated.
    if (newGeo != oldGeo)
        update();
}

void TerminalItem::itemChange(ItemChange change, const ItemChangeData &data) {
    QQuickItem::itemChange(change, data);
    // Monitor-/Skalierungswechsel: das Raster wurde auf die ALTE Geräte-Pixel-Dichte
    // gesnappt und wäre auf dem neuen Bildschirm wieder matschig — neu berechnen.
    if (change == ItemDevicePixelRatioHasChanged
        && !qFuzzyCompare(m_gridDpr, window() ? window()->effectiveDevicePixelRatio() : 1.0)) {
        recomputeGrid();
        update();
    }
}

// Qt-Maustaste → libvterm-Buttonnummer (1=links, 2=mitte, 3=rechts). 0 = ignoriert.
static int vtMouseButton(Qt::MouseButton b) {
    switch (b) {
    case Qt::LeftButton:   return 1;
    case Qt::MiddleButton: return 2;
    case Qt::RightButton:  return 3;
    default:               return 0;
    }
}

// Maus-/Rad-Events an die App melden nur, wenn Tracking aktiv **und** ein Vollbild-TUI
// (Alternate Screen) läuft (QTMUX-104). Die Alt-Screen-Bedingung schützt die Shell: Endet
// ein TUI unsauber (Crash, `kill`, SSH-Abbruch), bleibt das Tracking-Flag hängen und
// schickte sonst SGR-Codes in den zurückkehrenden Prompt — die zsh füllt sich mit
// „35;63;49M…". vim/htop/less/Agenten laufen im Alt-Screen, ihre Maus bleibt also intakt.
static bool appMouseActive(const VtScreen *sc) {
    return sc && sc->mouseTracking() != 0 && sc->altScreen();
}

void TerminalItem::mousePressEvent(QMouseEvent *event) {
    forceActiveFocus();
    // Cmd/Ctrl+Linksklick auf einen erkannten Link öffnet ihn im verknüpften Viewer —
    // lokal und VOR jeder App-Maus-Weiterleitung (analog dazu, wie Shift die Selektion
    // erzwingt). Der Modifier ist die bewusste Geste, die versehentliches Öffnen von
    // Agenten-Output verhindert.
    if (event->button() == Qt::LeftButton && isLinkModifier(event->modifiers())
            && openLinkAt(event->position())) {
        event->accept();
        return;
    }
    VtScreen *sc = screen();
    // App mit Maus-Tracking: Klicks an die App melden (vim/htop/tmux/…). Shift
    // erzwingt weiterhin die lokale Text-Selektion (übliche Terminal-Konvention).
    const int btn = vtMouseButton(event->button());
    if (appMouseActive(sc) && btn > 0
            && !(event->modifiers() & Qt::ShiftModifier)) {
        const QPoint c = cellAt(event->position());
        m_lastMouseCell = c;
        m_mouseReporting = true;
        sc->mouseButton(btn, true, c.y(), c.x(), event->modifiers());
        event->accept();
        return;
    }
    m_mouseReporting = false;
    if (event->button() == Qt::LeftButton) {
        m_selAnchor = m_selCaret = absCellAt(event->position());
        m_selecting = true;
        m_hasSelection = false;
        emit selectionChanged();
        update();
    } else if (event->button() == Qt::RightButton) {
        if (m_rightClickPaste)
            paste();
        else
            emit contextMenuRequested();
    }
    event->accept();
}

void TerminalItem::mouseMoveEvent(QMouseEvent *event) {
    VtScreen *sc = screen();
    // Drag an die App melden (nur wenn der Press bereits an sie ging). libvterm
    // gibt im Drag-Modus die Bewegung aus, im reinen Klick-Modus ist es ein No-op.
    if (m_mouseReporting && sc) {
        const QPoint c = cellAt(event->position());
        if (c != m_lastMouseCell) {
            m_lastMouseCell = c;
            sc->mouseMove(c.y(), c.x(), event->modifiers());
        }
        event->accept();
        return;
    }
    if (!m_selecting) { event->ignore(); return; }
    const QPoint cell = absCellAt(event->position());
    if (cell != m_selCaret) {
        m_selCaret = cell;
        m_hasSelection = (m_selCaret != m_selAnchor);
        emit selectionChanged();
        update();
    }
    event->accept();
}

void TerminalItem::mouseReleaseEvent(QMouseEvent *event) {
    VtScreen *sc = screen();
    if (m_mouseReporting && sc) {
        const int btn = vtMouseButton(event->button());
        if (btn > 0) {
            const QPoint c = cellAt(event->position());
            sc->mouseButton(btn, false, c.y(), c.x(), event->modifiers());
        }
        m_mouseReporting = false;
        event->accept();
        return;
    }
    if (m_selecting) {
        m_selecting = false;
        if (m_selCaret == m_selAnchor) clearSelection();
        else if (m_copyOnSelect && m_hasSelection) copy();
        event->accept();
        return;
    }
    event->ignore();
}

void TerminalItem::hoverMoveEvent(QHoverEvent *event) {
    m_lastHoverPos = event->position();
    updateHoverLink(m_lastHoverPos, event->modifiers());
    QQuickItem::hoverMoveEvent(event);
}

void TerminalItem::hoverLeaveEvent(QHoverEvent *event) {
    m_lastHoverPos = QPointF(-1, -1);
    if (m_hoverRow >= 0 || m_hoverC1 >= m_hoverC0) {
        const bool hadTarget = !m_hoverTarget.isEmpty();
        m_hoverRow = -1; m_hoverC1 = -1; m_hoverTarget.clear();
        setCursor(Qt::ArrowCursor);
        if (hadTarget) emit hoverLinkChanged();
        update();
    }
    QQuickItem::hoverLeaveEvent(event);
}

void TerminalItem::keyReleaseEvent(QKeyEvent *event) {
    // Die Link-Hervorhebung hängt nicht mehr am Modifier (sie erscheint schon beim
    // Drüberfahren) — hier ist daher keine Neubewertung mehr nötig.
    QQuickItem::keyReleaseEvent(event);
}

void TerminalItem::wheelEvent(QWheelEvent *event) {
    const int dy = event->angleDelta().y();
    if (dy == 0) { event->ignore(); return; }
    if (event->modifiers() & Qt::ControlModifier) {
        emit zoomRequested(dy > 0 ? 1 : -1);
        event->accept();
        return;
    }
    // App mit Maus-Tracking (z. B. Claude Code, vim, less, htop im Alternate Screen):
    // das Rad als Maus-Button 4/5 an die App melden, damit SIE ihren eigenen Renderer
    // scrollt. Ohne das täte das Rad hier nur den (im Alt-Screen leeren) lokalen
    // Scrollback bewegen → Rad scheint „tot".
    VtScreen *sc = screen();
    if (appMouseActive(sc)) {
        const QPoint c = cellAt(event->position());
        sc->mouseButton(dy > 0 ? 4 : 5, true, c.y(), c.x(), event->modifiers());
        event->accept();
        return;
    }
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
    m_geomDirty = true;   // andere Zeilen sichtbar
    // Selektion NICHT löschen: sie liegt in absoluten Inhalts-Zeilen und bleibt
    // beim Scrollen korrekt am Text (wird nur ein-/ausgeblendet, wenn sie den
    // sichtbaren Bereich verlässt/betritt).
    update();
}

bool TerminalItem::viewportSource(int screenRow, int &sbIndex, int &liveRow) const {
    VtScreen *sc = screen();
    const int sb = sc ? sc->scrollbackCount() : 0;
    const int v = sb - m_scrollOffset + screenRow;
    if (v < sb) { sbIndex = v < 0 ? 0 : v; return true; }
    liveRow = v - sb;
    return false;
}

void TerminalItem::onDamaged() {
    VtScreen *sc = screen();
    const int sb = sc ? sc->scrollbackCount() : 0;
    if (m_scrollOffset > 0) {
        const int delta = sb - m_lastSbCount;
        if (delta > 0) m_scrollOffset = std::min(m_scrollOffset + delta, sb);
    }
    m_lastSbCount = sb;
    m_geomDirty = true;   // Inhalt hat sich geändert
    update();
}

} // namespace qtmux
