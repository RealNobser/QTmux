#pragma once

#include <QString>
#include <QtGlobal>
#include <vector>

namespace qtmux {

class VtScreen;

/// Auswahl im Terminal — Gui-frei, damit ihre Regel an echten Bytes im `VtScreen`
/// prüfbar ist.
///
/// 🔑 **Die Zusage:** Eine Auswahl markiert genau den Text, den der Anwender markiert hat,
/// oder sie ist weg. Sie markiert nie fremden Text. Zwei Mechanismen tragen das:
///
/// 1. **Stabile Zeilennummern.** Zeilen werden seit Sitzungsbeginn durchgezählt
///    (`absolute Zeile + scrollbackDropped()`). Rollt Text in den Scrollback, bleibt seine
///    Nummer gleich, die Auswahl wandert also mit. Fällt an der Scrollback-Grenze (10000
///    Zeilen) die älteste Zeile heraus, verschiebt sich die absolute Zählung, die stabile
///    nicht. Ist markierter Text dabei verloren gegangen, verschwindet die Auswahl.
/// 2. **Inhaltsvergleich.** Ein Programm kann Text verschieben, ohne dass Zeilen in den
///    Scrollback wandern: Neumalen an Ort und Stelle, Scroll-Regionen, Alt-Screen, Reflow,
///    Bildschirm löschen. Daran ändert sich keine Zeilennummer. Deshalb merkt sich die
///    Auswahl den Text ihrer Zeilen im Live-Bereich und vergleicht ihn bei jeder Änderung.
///    Weicht er ab, verschwindet sie. Scrollback-Zeilen ändern sich nie, sie werden nach
///    dem letzten Vergleich vergessen — das hält die Prüfung bei höchstens einem
///    Bildschirm Zellen, auch bei sehr langen Auswahlen.
///
/// Gleicher Text an gleicher Stelle gilt als derselbe (ein Neumalen ohne Änderung lässt die
/// Auswahl stehen). Attribute zählen nicht, nur der Text.
///
/// Ausnahme „Alles auswählen": Das meint den GESAMTEN Inhalt, gleich welcher es gerade ist,
/// und kann darum keinen fremden Text treffen. Es bleibt bei Ausgabe bestehen und wächst mit.
class TerminalSelection {
public:
    /// Maustaste gedrückt: Anker setzen. Markiert ist noch nichts.
    void begin(const VtScreen &screen, int col, int absRow);
    /// Maus gezogen: Auswahl bis zu dieser Zelle (inklusive). false, wenn sich nichts geändert hat.
    bool extendTo(const VtScreen &screen, int col, int absRow);
    /// Gesamten Inhalt auswählen.
    void selectAll();
    void clear();

    /// Ist etwas markiert? (Ein bloßer Anker ohne Ziehen zählt nicht.)
    bool hasSelection() const { return m_all || (m_anchorSet && !isEmptyRange()); }
    bool anchorSet() const { return m_anchorSet || m_all; }

    /// Nach jeder Änderung am Bildschirm aufrufen. true = die Auswahl (oder der Anker) ist
    /// dadurch verschwunden; der Aufrufer muss neu zeichnen und das melden.
    bool revalidate(const VtScreen &screen);

    /// Markierter Text wie beim Kopieren: weiche Umbrüche ohne `\n`, rechte Leerzeichen
    /// am logischen Zeilenende entfernt, die Spacer-Zelle hinter breiten Zeichen übersprungen.
    QString text(const VtScreen &screen) const;

    /// Markierter Spaltenbereich einer ABSOLUTEN Zeile für den Renderer; false = nichts.
    /// Bewusst keine lineare Zellnummer `zeile*spalten+spalte`: die liefe nach einer
    /// Größenänderung in die Nachbarzeile über, sobald eine Spalte >= neue Breite ist.
    bool rowSpan(const VtScreen &screen, int absRow, int cols, int &from, int &to) const;

private:
    struct Pos {
        int col = 0;
        qint64 line = 0;   // stabile Zeilennummer
    };
    struct Snapshot {
        qint64 line = 0;
        QString text;      // markierter Teil der Zeile, rechts getrimmt
    };

    static bool before(const Pos &a, const Pos &b) {
        return a.line < b.line || (a.line == b.line && a.col < b.col);
    }
    bool isEmptyRange() const { return m_anchor.line == m_caret.line && m_anchor.col == m_caret.col; }
    void range(const VtScreen &screen, Pos &start, Pos &end) const;
    void takeSnapshot(const VtScreen &screen);
    /// Text von `line` zwischen den Spalten, die zur Auswahl gehören.
    QString markedPart(const VtScreen &screen, qint64 line, const Pos &start, const Pos &end) const;

    bool m_anchorSet = false;
    bool m_all = false;
    Pos m_anchor;
    Pos m_caret;
    std::vector<Snapshot> m_snapshot;   // nur Zeilen, die noch im Live-Bereich liegen
};

} // namespace qtmux
