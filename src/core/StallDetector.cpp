#include "StallDetector.h"

#include <QHash>

namespace qtmux {

bool screenShowsWorking(const QString &screen, const QStringList &markers, int tailLines) {
    if (markers.isEmpty() || tailLines <= 0) return false;
    // Von unten nach oben die letzten `tailLines` nicht-leeren Zeilen prüfen. Bewusst
    // ohne split(): der Bildschirm wird in jedem Takt abgetastet, die Zeilen weiter
    // oben interessieren nie.
    int end = screen.size();
    int seen = 0;
    while (end > 0 && seen < tailLines) {
        const int nl = screen.lastIndexOf(QLatin1Char('\n'), end - 1);
        const QStringView line = QStringView(screen).mid(nl + 1, end - nl - 1);
        end = nl < 0 ? 0 : nl;
        if (line.trimmed().isEmpty()) continue;
        ++seen;
        for (const QString &m : markers)
            if (!m.isEmpty() && line.contains(m)) return true;
    }
    return false;
}

quint64 screenFingerprint(const QString &screen) {
    return static_cast<quint64>(qHash(screen));
}

void StallDetector::setThresholdMs(qint64 ms) {
    if (ms == m_thresholdMs) return;
    m_thresholdMs = ms;
    // Neue Schwelle gilt ab der nächsten Abtastung; ein bestehender Stillstand wird
    // dort neu bewertet (bei Abschalten sofort aufgelöst).
}

void StallDetector::reset() {
    m_have = false;
    m_stalled = false;
    m_sinceMs = m_lastSampleMs = 0;
    m_fingerprint = 0;
}

bool StallDetector::sample(qint64 nowMs, quint64 fingerprint, bool working) {
    const bool before = m_stalled;
    const bool gap = m_have && m_maxGapMs > 0 && nowMs - m_lastSampleMs > m_maxGapMs;
    m_lastSampleMs = nowMs;

    if (m_thresholdMs <= 0 || !working || !m_have || gap || fingerprint != m_fingerprint) {
        // Neue Vergleichsbasis: abgeschaltet, Agent arbeitet nicht (wartet am Prompt —
        // das deckt das question/done-Ereignis ab), erste Abtastung, Beobachtungslücke
        // oder der Bildschirm hat sich bewegt.
        m_have = true;
        m_fingerprint = fingerprint;
        m_sinceMs = nowMs;
        m_stalled = false;
    } else if (nowMs - m_sinceMs >= m_thresholdMs) {
        m_stalled = true;
    }
    return m_stalled != before;
}

} // namespace qtmux
