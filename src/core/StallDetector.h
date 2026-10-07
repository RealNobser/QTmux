#pragma once

#include <QString>
#include <QStringList>
#include <QtGlobal>

namespace qtmux {

/// Zeigt der sichtbare Bildschirm an, dass der Agent gerade ARBEITET? (Stillstand-
/// Erkennung, QTMUX-139.) Gesucht wird jedes Merkmal aus `markers` als wörtliche
/// Teilzeichenkette in den letzten `tailLines` NICHT-leeren Zeilen von `screen`.
///
/// 🔑 Warum nur am unteren Rand: Claude Code schreibt „esc to interrupt" in seine
/// Fußzeile, und die ist die unterste Zeile des Bildschirms (am lebenden Objekt
/// gelesen, 2026-10-07). Weiter oben stünde dieselbe Zeichenkette auch dann noch, wenn
/// der Agent längst beendet ist und darunter ein Shell-Prompt wartet — oder schlicht
/// als Text im Verlauf (z. B. in genau dieser Doku, von einem Agenten zitiert).
/// Leere `markers` heißt: für diesen Agenten ist kein Merkmal belegt → nie „arbeitet".
bool screenShowsWorking(const QString &screen, const QStringList &markers, int tailLines = 3);

/// Billiger Fingerabdruck des sichtbaren Bildschirms (nur Vergleich auf Gleichheit,
/// kein kryptografischer Anspruch).
quint64 screenFingerprint(const QString &screen);

/// Erkennt eine Agent-Session, die sichtbar ARBEITET, deren Bildschirm sich aber seit
/// `thresholdMs` nicht mehr verändert hat (QTMUX-139).
///
/// 🔑 Das Signal ist das EINFRIEREN, nicht die Stille: Ein lebender Agent zählt in
/// seiner Arbeitszeile eine Sekundenuhr hoch — auch während ein Befehl ohne jede
/// Ausgabe läuft (gemessen 2026-10-07: „Running 1 shell command · 3s…" → „· 9s…",
/// „(3m 7s" → „(3m 13s"). Ein Bildschirm, der „arbeitet" zeigt und sich trotzdem
/// minutenlang nicht ändert, heißt also: Der Agent selbst kommt nicht mehr dazu, seine
/// Oberfläche zu zeichnen (Anlass: 4 h 48 min in einem unsichtbaren macOS-TCC-Dialog).
///
/// Rein, ohne Timer und ohne Uhr — die Zeit kommt als Argument herein, damit Tests
/// ohne echte Minuten auskommen. Der Aufrufer tastet in einem festen Takt ab.
class StallDetector {
public:
    /// `thresholdMs <= 0` schaltet die Erkennung ab.
    void setThresholdMs(qint64 ms);
    qint64 thresholdMs() const { return m_thresholdMs; }

    /// Größte Lücke zwischen zwei Abtastungen, die noch als durchgehende Beobachtung
    /// gilt (`<= 0` = keine Grenze). Darüber wird die Uhr neu angesetzt: Nach einem
    /// Ruhezustand des Rechners stünde der Bildschirm sonst „seit Stunden" still, nur
    /// weil niemand hingesehen hat — und der Agent hätte noch keine Gelegenheit gehabt,
    /// nach dem Aufwachen neu zu zeichnen.
    void setMaxGapMs(qint64 ms) { m_maxGapMs = ms; }

    /// Eine Abtastung. Liefert true, wenn sich `stalled()` dadurch geändert hat.
    bool sample(qint64 nowMs, quint64 fingerprint, bool working);

    /// Alles vergessen (z. B. wenn der Agent wechselt).
    void reset();

    bool stalled() const { return m_stalled; }
    /// Seit wann steht der Bildschirm unverändert (Beginn der Beobachtung); nur
    /// sinnvoll, solange `stalled()`.
    qint64 unchangedSinceMs() const { return m_sinceMs; }

private:
    qint64 m_thresholdMs = 0;
    qint64 m_maxGapMs = 0;
    bool m_have = false;        // gibt es eine Vergleichsbasis?
    quint64 m_fingerprint = 0;
    qint64 m_sinceMs = 0;       // Beginn der aktuellen unveränderten Strecke
    qint64 m_lastSampleMs = 0;
    bool m_stalled = false;
};

} // namespace qtmux
