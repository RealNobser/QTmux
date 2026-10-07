#include <QtTest>

#include "AgentRegistry.h"
#include "ITerminalBackend.h"
#include "Session.h"
#include "StallDetector.h"

using namespace qtmux;

// Minimal-Backend: die Testseite „spricht" als Agent zur Session.
class FeedBackend final : public ITerminalBackend {
public:
    bool start(int, int) override { setState(BackendState::Running); return true; }
    void write(const QByteArray &) override {}
    void resize(int, int) override {}
    void terminate() override { setState(BackendState::Closed); }
    void feed(const QByteArray &d) { emit dataReceived(d); }
};

/// Stillstand-Erkennung (QTMUX-139). Die Zeit kommt als Argument herein — kein Test
/// wartet echte Minuten. Gliederung: (1) die reine Zustandsmaschine `StallDetector`,
/// (2) das Merkmal „arbeitet" am Bildschirmtext, (3) die Verdrahtung in `Session`
/// (needsAttention, Selbstauflösung, Text, Fremd-Aufmerksamkeit bleibt stehen).
class TestStallDetector : public QObject {
    Q_OBJECT

    static constexpr qint64 kMin = 60 * 1000;
    static constexpr qint64 kTick = 5000;

    // Echte Bildschirme, am lebenden Claude Code abgelesen (2026-10-07), gekürzt.
    static QString busyScreen(const QString &clock) {
        return QStringLiteral("⏺ Running 1 shell command · 9s…\n"
                              "\n"
                              "✽ Hullaballooing… (%1 · ↓ 6.8k tokens)\n"
                              "────────────────────────────────\n"
                              "❯ \n"
                              "────────────────────────────────\n"
                              "  ⏵⏵ auto mode on (shift+tab to cycle) · esc to interrupt · ← 1 agent\n"
                              "\n").arg(clock);
    }
    static QString idleScreen() {
        return QStringLiteral("✻ Cooked for 55s · done 8:35 PM · 1 shell still running\n"
                              "────────────────────────────────\n"
                              "❯ \n"
                              "────────────────────────────────\n"
                              "  ⏵⏵ auto mode on · 1 shell · ← 1 agent · ↓ to manage\n");
    }
    static QStringList claudeMarkers() { return AgentRegistry::workingMarkersFor(QStringLiteral("claude")); }

    // Tastet `det` von `from` bis einschließlich `to` im Takt ab; `fp(t)` liefert den
    // Fingerabdruck je Zeitpunkt. Liefert den Zeitpunkt der ersten Stillstands-Meldung
    // (oder -1).
    template <typename Fp>
    static qint64 run(StallDetector &det, qint64 from, qint64 to, bool working, Fp fp) {
        qint64 first = -1;
        for (qint64 t = from; t <= to; t += kTick) {
            det.sample(t, fp(t), working);
            if (det.stalled() && first < 0) first = t;
        }
        return first;
    }

    // moc verlangt Hilfstypen außerhalb von `private slots`.
    struct Rig {
        Session sess;
        FeedBackend *backend;
        Rig() {
            backend = new FeedBackend;
            sess.attachBackend(backend, Session::Type::Shell, 100, 12);
            sess.start(100, 12);
            sess.setRestoredAgent(QStringLiteral("claude"), QStringLiteral("claude"));
            sess.setStallThresholdMs(5 * kMin);
            sess.setStallMaxGapMs(3 * kTick);
        }
        void draw(const QString &screen) {
            backend->feed(QByteArray("\x1b[2J\x1b[H")
                          + screen.toUtf8().replace("\n", "\r\n"));
        }
        void sampleRange(qint64 from, qint64 to) {
            for (qint64 t = from; t <= to; t += kTick) sess.sampleStall(t);
        }
    };

private slots:
    // --- (1) Zustandsmaschine ---------------------------------------------------

    void workingAndUnchangedStallsExactlyAtThreshold() {
        StallDetector det;
        det.setThresholdMs(5 * kMin);
        det.setMaxGapMs(3 * kTick);
        const qint64 first = run(det, 0, 6 * kMin, true, [](qint64) { return 42u; });
        QCOMPARE(first, 5 * kMin);   // nicht früher, nicht später als die Schwelle
        QCOMPARE(det.unchangedSinceMs(), qint64(0));
    }

    void tickingClockNeverStalls() {
        // Die Spinner-Uhr läuft: jede Abtastung sieht einen neuen Bildschirm.
        StallDetector det;
        det.setThresholdMs(5 * kMin);
        const qint64 first = run(det, 0, 60 * kMin, true,
                                 [](qint64 t) { return quint64(qHash(busyScreen(QString::number(t / 1000) + "s"))); });
        QCOMPARE(first, qint64(-1));
    }

    void waitingAtPromptNeverStalls() {
        // Unveränderter Bildschirm, aber der Agent arbeitet nicht (wartet auf Eingabe).
        StallDetector det;
        det.setThresholdMs(5 * kMin);
        QCOMPARE(run(det, 0, 60 * kMin, false, [](qint64) { return 7u; }), qint64(-1));
    }

    void changeResolvesAndRestartsTheClock() {
        StallDetector det;
        det.setThresholdMs(5 * kMin);
        run(det, 0, 6 * kMin, true, [](qint64) { return 1u; });
        QVERIFY(det.stalled());
        QVERIFY(det.sample(6 * kMin + kTick, 2u, true));   // Wechsel wird gemeldet
        QVERIFY(!det.stalled());
        // Neue Strecke beginnt beim Wechsel — die alte Dauer zählt nicht weiter.
        QVERIFY(!det.sample(6 * kMin + 2 * kTick, 2u, true));
        QVERIFY(!det.stalled());
        QCOMPARE(run(det, 6 * kMin + 3 * kTick, 12 * kMin, true, [](qint64) { return 2u; }),
                 6 * kMin + kTick + 5 * kMin);
    }

    void stopWorkingResolves() {
        // Agent kehrt an den Prompt zurück (Fußzeile weg): kein Stillstand mehr.
        StallDetector det;
        det.setThresholdMs(5 * kMin);
        run(det, 0, 6 * kMin, true, [](qint64) { return 1u; });
        QVERIFY(det.stalled());
        det.sample(6 * kMin + kTick, 1u, false);
        QVERIFY(!det.stalled());
    }

    void disabledNeverStalls() {
        StallDetector det;   // Vorgabe 0 = aus
        QCOMPARE(run(det, 0, 600 * kMin, true, [](qint64) { return 1u; }), qint64(-1));
        det.setThresholdMs(-1);
        QCOMPARE(run(det, 0, 600 * kMin, true, [](qint64) { return 1u; }), qint64(-1));
    }

    void switchingOffResolvesARunningStall() {
        StallDetector det;
        det.setThresholdMs(5 * kMin);
        run(det, 0, 6 * kMin, true, [](qint64) { return 1u; });
        QVERIFY(det.stalled());
        det.setThresholdMs(0);
        QVERIFY(det.sample(6 * kMin + kTick, 1u, true));
        QVERIFY(!det.stalled());
    }

    void observationGapRestartsTheClock() {
        // Rechner schlief 2 h: danach nicht sofort „steht seit 2 h", sondern neu ansetzen.
        StallDetector det;
        det.setThresholdMs(5 * kMin);
        det.setMaxGapMs(3 * kTick);
        run(det, 0, 2 * kMin, true, [](qint64) { return 1u; });
        det.sample(2 * kMin + 120 * kMin, 1u, true);
        QVERIFY(!det.stalled());
        QCOMPARE(det.unchangedSinceMs(), 122 * kMin);
        // Ohne Lückenregel wäre derselbe Ablauf ein Stillstand gewesen (Gegenprobe).
        StallDetector noGap;
        noGap.setThresholdMs(5 * kMin);
        run(noGap, 0, 2 * kMin, true, [](qint64) { return 1u; });
        noGap.sample(2 * kMin + 120 * kMin, 1u, true);
        QVERIFY(noGap.stalled());
    }

    // --- (2) Merkmal „arbeitet" -----------------------------------------------

    void claudeMarkerIsRegistered() {
        QVERIFY(claudeMarkers().contains(QStringLiteral("esc to interrupt")));
        // Unbelegte Agenten bleiben leer = keine Erkennung (Registry-Linie: nie raten).
        QVERIFY(AgentRegistry::workingMarkersFor(QStringLiteral("aider")).isEmpty());
        QVERIFY(AgentRegistry::workingMarkersFor(QString()).isEmpty());
    }

    void busyFooterCountsAsWorking() {
        QVERIFY(screenShowsWorking(busyScreen(QStringLiteral("9s")), claudeMarkers()));
    }

    void idleFooterIsNotWorking() {
        QVERIFY(!screenShowsWorking(idleScreen(), claudeMarkers()));
    }

    void markerFarAboveTheBottomIsNotWorking() {
        // Agent beendet, darunter wartet ein Shell-Prompt: die alte Fußzeile steht noch
        // im Bild, gilt aber nicht mehr.
        const QString s = busyScreen(QStringLiteral("9s"))
                        + QStringLiteral("zsh: suspended  claude\n\n$ ls\na b c\nd e f\n$ \n");
        QVERIFY(!screenShowsWorking(s, claudeMarkers()));
    }

    void noMarkersMeansNeverWorking() {
        QVERIFY(!screenShowsWorking(busyScreen(QStringLiteral("9s")), {}));
        QVERIFY(!screenShowsWorking(QString(), claudeMarkers()));
    }

    // --- (3) Verdrahtung in Session (Prüfstand `Rig` im private-Teil) ------------

    void sessionStallRaisesAttentionWithOwnText() {
        Rig r;
        QSignalSpy att(&r.sess, &Session::attentionChanged);
        r.draw(busyScreen(QStringLiteral("9s")));
        r.sampleRange(0, 4 * kMin);
        QVERIFY(!r.sess.stalled());
        QVERIFY(!r.sess.needsAttention());
        r.sampleRange(4 * kMin + kTick, 5 * kMin);
        QVERIFY(r.sess.stalled());
        QVERIFY(r.sess.needsAttention());
        QCOMPARE(att.count(), 1);
        QVERIFY(r.sess.stallNote().contains(QStringLiteral("5 min")));
        QCOMPARE(r.sess.stalledForMs(), 5 * kMin);
        // Die Minuten zählen mit.
        r.sampleRange(5 * kMin + kTick, 12 * kMin);
        QVERIFY(r.sess.stallNote().contains(QStringLiteral("12 min")));
    }

    void sessionStallFlagsEvenTheFocusedSession() {
        Rig r;
        r.sess.setActive(true);
        r.draw(busyScreen(QStringLiteral("9s")));
        r.sampleRange(0, 5 * kMin);
        QVERIFY(r.sess.stalled());
        QVERIFY(r.sess.needsAttention());
    }

    void sessionOutputResolvesItself() {
        Rig r;
        r.draw(busyScreen(QStringLiteral("9s")));
        r.sampleRange(0, 6 * kMin);
        QVERIFY(r.sess.needsAttention());
        r.draw(busyScreen(QStringLiteral("10s")));   // Uhr läuft wieder
        r.sess.sampleStall(6 * kMin + kTick);
        QVERIFY(!r.sess.stalled());
        QVERIFY(!r.sess.needsAttention());
        QVERIFY(r.sess.stallNote().isEmpty());
    }

    void sessionPromptWaitingNeverStalls() {
        Rig r;
        r.draw(idleScreen());
        r.sampleRange(0, 60 * kMin);
        QVERIFY(!r.sess.stalled());
        QVERIFY(!r.sess.needsAttention());
    }

    void sessionWithoutAgentNeverStalls() {
        FeedBackend *b = new FeedBackend;
        Session s;
        s.attachBackend(b, Session::Type::Shell, 100, 12);
        s.start(100, 12);
        s.setStallThresholdMs(5 * kMin);
        b->feed(busyScreen(QStringLiteral("9s")).toUtf8().replace("\n", "\r\n"));
        for (qint64 t = 0; t <= 60 * kMin; t += kTick) s.sampleStall(t);
        QVERIFY(!s.stalled());
    }

    void sessionResolutionKeepsForeignAttention() {
        // Kam während des Stillstands ein echter Grund hinzu (Rückfrage des Agenten),
        // darf die Auflösung die Markierung NICHT mitnehmen.
        Rig r;
        r.draw(busyScreen(QStringLiteral("9s")));
        r.sampleRange(0, 6 * kMin);
        QVERIFY(r.sess.needsAttention());
        r.sess.flagAttention(QStringLiteral("Rückfrage"));
        r.draw(idleScreen());
        r.sess.sampleStall(6 * kMin + kTick);
        QVERIFY(!r.sess.stalled());
        QVERIFY(r.sess.needsAttention());
    }

    void sessionDisabledNeverStalls() {
        Rig r;
        r.sess.setStallThresholdMs(0);
        r.draw(busyScreen(QStringLiteral("9s")));
        r.sampleRange(0, 60 * kMin);
        QVERIFY(!r.sess.stalled());
        QVERIFY(!r.sess.needsAttention());
    }
};

QTEST_GUILESS_MAIN(TestStallDetector)
#include "tst_stalldetector.moc"
