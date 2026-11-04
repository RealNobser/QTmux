#pragma once

#include "FileManagerReveal.h"
#include "LinkDetector.h"

#include <QList>
#include <QString>
#include <Qt>
#include <vector>

namespace qtmux {
class VtScreen;
}

/// Klickbare Links im Bildschirminhalt — die Brücke zwischen Zellen und `LinkDetector`.
/// Gui-frei, damit an echten Bytes im `VtScreen` prüfbar. Eine Stelle für Hover, Klick und
/// Kontextmenü: Was unterstrichen ist, ist auch das, was geöffnet wird.
namespace ScreenLinks {

/// Maus-Geste auf einem Link. Auf jedem Host für alle drei Plattformen prüfbar.
enum class Gesture { None, Open, Reveal };

/// macOS: ⌘ öffnet, ⌘⌥ zeigt im Finder (physisches Ctrl ist Qt-`MetaModifier` und löst nichts
/// aus; Cmd ist Qt-`ControlModifier`). Windows/Linux: Strg öffnet, Strg+Umschalt zeigt.
/// Strg+Alt ist KEINE Geste: unter Windows ist das AltGr.
Gesture gestureFor(Qt::KeyboardModifiers mods, FileManagerReveal::Platform platform);

struct CellRef {
    int row = 0;   // absolute Inhalts-Zeile
    int col = 0;
};

/// Eine LOGISCHE Zeile: die über weiche Umbrüche verbundenen Bildschirmzeilen als ein Text.
/// `map[i]` nennt die Zelle des QChar `i`. 🔑 Spalten und Zeichen sind NICHT 1:1 — ein Emoji
/// belegt zwei Spalten und zwei QChar, seine Spacer-Zelle keines; ein Zeichen außerhalb der
/// BMP in einer Zelle zwei QChar. Wer gleichsetzt, unterstreicht neben dem Link.
struct Line {
    int firstRow = -1;
    QString text;
    std::vector<CellRef> map;
    bool cutTop = false;      // Kette an der Obergrenze abgeschnitten
    bool cutBottom = false;
};

/// Baut die logische Zeile um `absRow`. Die Kette ist auf `kMaxChainRows` Zeilen je Richtung
/// begrenzt: Ein 1-MB-Einzeiler soll nicht bei jeder Mausbewegung ganz durch den Detektor.
Line logicalLine(const qtmux::VtScreen &screen, int absRow);
constexpr int kMaxChainRows = 16;

/// Spalten eines Link-Abschnitts auf einer Zeile.
struct Segment {
    int row = 0;
    int c0 = 0;
    int c1 = 0;
    bool operator==(const Segment &o) const { return row == o.row && c0 == o.c0 && c1 == o.c1; }
};

struct Hit {
    QString target;
    LinkDetector::Span::Kind kind = LinkDetector::Span::Url;
    QList<Segment> segments;     // je Bildschirmzeile einer
    bool valid() const { return !target.isEmpty(); }
};

/// Link an der Zelle (`absRow`, `col`) einer logischen Zeile. Ein Treffer, der an einer
/// abgeschnittenen Kette endet, zählt nicht: Sein Ziel wäre unvollständig.
Hit hitAt(const Line &line, const QList<LinkDetector::Span> &spans, int absRow, int col);

/// Bequemlichkeit für Tests: logische Zeile bauen, erkennen, treffen.
Hit linkAt(const qtmux::VtScreen &screen, int absRow, int col, const QString &cwd = QString());

} // namespace ScreenLinks
