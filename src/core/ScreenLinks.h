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
///
/// Zwei Quellen, in dieser Reihenfolge:
///  1. **Hyperlinks des Programms (OSC 8).** Das Programm nennt das Ziel selbst, jede Zelle
///     trägt es. Das ist der einzige sichere Weg für Links, die ein Programm über mehrere
///     Zeilen SELBST umbricht.
///  2. **Erkannter Text** (`LinkDetector`), über weiche Umbrüche verbunden. Hart umbrochene
///     Zeilen werden NICHT zusammengesetzt: Gemessen an Claude Code sieht eine hart umbrochene
///     Adresse genauso aus wie eine Adresse am Zeilenende mit einem neuen Wort darunter. Ein
///     Text-Link, der am Rand endet und darunter weitergehen könnte, ist darum kein Link
///     (`Blocked::MaybeCut`): lieber keiner als der falsche.
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
    int lastRow = -1;
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
    bool declared = false;       // Ziel stammt vom Programm (OSC 8), nicht aus dem Text

    /// Warum an dieser Stelle KEIN Link ist, obwohl einer dasteht. Für die Rückmeldung.
    enum class Blocked {
        None,
        MaybeCut,   // Text-Link endet am Rand und könnte in der nächsten Zeile weitergehen
        Refused,    // Hyperlink des Programms mit einem Ziel, das nicht geöffnet wird
    };
    Blocked blocked = Blocked::None;
    QString refused;             // bei Refused: das abgelehnte Ziel

    bool valid() const { return !target.isEmpty(); }
};

/// Hyperlink des Programms (OSC 8) an der Zelle. `blocked == Refused`, wenn dort einer steht,
/// dessen Ziel nicht durch `LinkDetector::declaredTarget` kommt. Leer und nicht blockiert,
/// wenn die Zelle keinen trägt oder sein Ziel aus der Tabelle gefallen ist.
Hit hyperlinkAt(const qtmux::VtScreen &screen, int absRow, int col);

/// Erkannter Text-Link an der Zelle (`absRow`, `col`) einer logischen Zeile. Ein Treffer, der
/// an einer abgeschnittenen Kette endet, zählt nicht: Sein Ziel wäre unvollständig. Dasselbe
/// gilt für einen Treffer, der am Rand endet, während die nächste Zeile ohne
/// Fortsetzungs-Marke weitergehen könnte (`Blocked::MaybeCut`).
Hit hitAt(const qtmux::VtScreen &screen, const Line &line,
          const QList<LinkDetector::Span> &spans, int absRow, int col);

/// Steht (`absRow`, `col`) im ersten Wort einer Zeile, über der ein am Rand endender
/// Text-Link steht? Dann ist die Stelle vielleicht dessen Fortsetzung. Nur für die
/// Rückmeldung nach einem Klick daneben; teurer als `hitAt`, darum nicht beim Überfahren.
bool continuesCutLink(const qtmux::VtScreen &screen, int absRow, int col);

/// Alles zusammen: Hyperlink des Programms, sonst erkannter Text.
Hit linkAt(const qtmux::VtScreen &screen, int absRow, int col, const QString &cwd = QString());

} // namespace ScreenLinks
