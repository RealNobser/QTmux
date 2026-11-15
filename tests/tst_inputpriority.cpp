#include <QtTest>

#include "ITerminalBackend.h"
#include "Session.h"

using namespace qtmux;

// Fängt alles ab, was die Session Richtung Programm schreibt.
class CaptureBackend final : public ITerminalBackend {
public:
    bool start(int, int) override { setState(BackendState::Running); return true; }
    void write(const QByteArray &d) override { captured += d; }
    void resize(int, int) override {}
    void terminate() override { setState(BackendState::Closed); }
    void feed(const QByteArray &d) { emit dataReceived(d); }
    QByteArray captured;
};

/// Vorrang der Eingabe des Menschen, an der echten Session: Was er tippt, hält alles
/// zurück, was die Warteschlange hineinschreiben würde — bis er abgeschickt hat.
///
/// Der Owner-Befund (2026-11-15): Ein Agent hängte seine Nachricht an einen halb getippten
/// Satz des Menschen und schickte beides zusammen ab. Die Regel selbst prüft
/// tst_ownerdraft; hier geht es um die Verdrahtung in `Session` — welcher Eingang den
/// Merker führt, und dass die Abgabe ihn achtet.
class TestInputPriority : public QObject {
    Q_OBJECT

    struct Rig {
        Session sess;
        CaptureBackend *backend;   // Session besitzt ihn
        Rig() {
            backend = new CaptureBackend;
            sess.attachBackend(backend, Session::Type::Shell, 80, 24);
            sess.start(80, 24);
            // Die Session meldet sich als frei: Ohne den Merker ginge ein Eintrag SOFORT
            // hinaus. Damit ist jedes „wartet“ unten allein dem Merker geschuldet.
            sess.requestActivity(QStringLiteral("idle"));
            backend->captured.clear();
        }
        // Ein Tastendruck des Menschen, wie TerminalItem ihn schickt.
        void key(const QByteArray &bytes) { sess.writeFromUser(bytes); }
        // Lässt verzögerte Aufrufe und das abgesetzte Enter (QTMUX-31) durchlaufen.
        static void settle(int ms = 250) { QTest::qWait(ms); }
    };

private slots:
    // Die Abnahme in einem Fall: getippter Entwurf, eingereihter Text, keine Zustellung,
    // nach Enter Zustellung — und zwar NACH dem Enter des Menschen.
    void typedDraftHoldsTheQueueUntilEnter() {
        Rig rig;
        QSignalSpy typingSpy(&rig.sess, &Session::ownerTypingChanged);
        rig.key("h"); rig.key("a"); rig.key("l"); rig.key("b");
        QVERIFY(rig.sess.ownerTyping());
        QCOMPARE(typingSpy.count(), 1);                 // einmal gemeldet, nicht je Taste
        QVERIFY(rig.sess.ownerTypingSinceMs() > 0);

        QVERIFY(rig.sess.queueText(QStringLiteral("NACHRICHT"), QStringLiteral("#7 Worker")));
        Rig::settle(400);                               // mehrere Abgabe-Takte (150 ms)
        QCOMPARE(rig.backend->captured, QByteArray("halb"));   // der Entwurf steht allein
        QCOMPARE(rig.sess.queuedCount(), 1);
        QCOMPARE(rig.sess.queueNote(), QStringLiteral("Von #7 Worker — Nachricht wartet"));

        rig.key("\r");                                  // der Mensch schickt ab
        QVERIFY(!rig.sess.ownerTyping());
        QCOMPARE(typingSpy.count(), 2);
        QTRY_COMPARE(rig.sess.queuedCount(), 0);
        QTRY_COMPARE(rig.backend->captured, QByteArray("halb\rNACHRICHT\r"));
        QVERIFY(rig.sess.queueNote().isEmpty());
    }

    void withoutADraftTheQueueDispatchesAtOnce() {
        // Gegenprobe zum Fall oben: derselbe Aufbau ohne Tippen gibt sofort ab.
        Rig rig;
        QVERIFY(rig.sess.queueText(QStringLiteral("NACHRICHT")));
        QTRY_COMPARE(rig.backend->captured, QByteArray("NACHRICHT\r"));
        QVERIFY(rig.sess.queueNote().isEmpty());        // nichts wartete auf den Menschen
    }

    void machineInputNeverSetsTheFlag() {
        // MCP und Warteschlange nehmen write/writePasted/writeWithEnter — dieselben Bytes
        // wie eine Taste, aber nicht vom Menschen. Stünde der Merker danach, sperrte sich
        // ein Agent mit seinem eigenen send_text aus.
        Rig rig;
        rig.sess.write("abc");
        rig.sess.writePasted("eingefuegt");
        rig.sess.writeWithEnter("befehl", 0);
        rig.sess.writePastedWithEnter("text", 0);
        QVERIFY(!rig.sess.ownerTyping());
        rig.backend->captured.clear();
        QVERIFY(rig.sess.queueText(QStringLiteral("NACHRICHT")));
        QTRY_COMPARE(rig.backend->captured, QByteArray("NACHRICHT\r"));
    }

    void pasteByTheHumanHoldsTheQueue() {
        Rig rig;
        rig.sess.pasteFromUser("aus der Zwischenablage");
        QVERIFY(rig.sess.ownerTyping());
        rig.sess.queueText(QStringLiteral("NACHRICHT"));
        Rig::settle(400);
        QCOMPARE(rig.backend->captured, QByteArray("aus der Zwischenablage"));
    }

    void ctrlCAndCountedBackspaceRelease() {
        Rig rig;
        rig.key("a"); rig.key("b");
        rig.sess.queueText(QStringLiteral("EINS"));
        rig.key("\x7f");
        QVERIFY(rig.sess.ownerTyping());                // eines steht noch
        rig.key("\x7f");
        QVERIFY(!rig.sess.ownerTyping());               // nachweislich leer
        QTRY_COMPARE(rig.sess.queuedCount(), 0);
        QTRY_VERIFY(rig.backend->captured.endsWith("EINS\r"));

        rig.key("x");
        rig.sess.queueText(QStringLiteral("ZWEI"));
        Rig::settle(300);
        QCOMPARE(rig.sess.queuedCount(), 1);
        rig.key("\x03");                                // Strg-C verwirft
        QTRY_COMPARE(rig.sess.queuedCount(), 0);
        QTRY_VERIFY(rig.backend->captured.endsWith("\x03ZWEI\r"));
    }

    void explicitReleaseDispatches() {
        // Der Weg aus dem Dauer-Merker: Der Mensch sagt in der Oberfläche „meine Zeile
        // ist leer“ (z. B. nach einer Ziffer in einem Auswahldialog).
        Rig rig;
        rig.key("2");
        rig.sess.queueText(QStringLiteral("NACHRICHT"), QStringLiteral("#3 B"));
        Rig::settle(300);
        QCOMPARE(rig.sess.queuedCount(), 1);
        rig.sess.releaseOwnerDraft();
        QVERIFY(!rig.sess.ownerTyping());
        QTRY_COMPARE(rig.sess.queuedCount(), 0);
        QTRY_VERIFY(rig.backend->captured.endsWith("NACHRICHT\r"));
    }

    void queueNoteNamesCountAndFirstSender() {
        Rig rig;
        QSignalSpy noteSpy(&rig.sess, &Session::queueNoteChanged);
        rig.key("x");
        QVERIFY(rig.sess.queueNote().isEmpty());        // getippt, aber nichts wartet
        rig.sess.queueText(QStringLiteral("eins"), QStringLiteral("#7 Worker"));
        rig.sess.queueText(QStringLiteral("zwei"));
        QCOMPARE(rig.sess.queueNote(),
                 QStringLiteral("Von #7 Worker zuerst — 2 Nachrichten warten"));
        QVERIFY(noteSpy.count() >= 2);
        rig.sess.promptQueue()->removeAt(0);
        QCOMPARE(rig.sess.queueNote(),
                 QStringLiteral("Von unbekanntem Absender — Nachricht wartet"));
        rig.sess.promptQueue()->clear();
        QVERIFY(rig.sess.queueNote().isEmpty());
    }

    void quietClockRestartsWhenTheHumanSubmits() {
        // Session OHNE eigene Zustandsmeldung: Entschieden wird über Ruhe im
        // Ausgabestrom. Ihre letzte Ausgabe liegt lange zurück — ohne neu gestellte Uhr
        // fiele der Eintrag im selben Augenblick hinter das Enter des Menschen, bevor
        // das Programm darauf reagieren konnte.
        Session sess;
        auto *backend = new CaptureBackend;
        sess.attachBackend(backend, Session::Type::Shell, 80, 24);
        sess.start(80, 24);
        backend->feed("prompt> ");
        QTest::qWait(650);                              // Ruhefenster (500 ms) ist vorbei
        backend->captured.clear();

        sess.writeFromUser("x");
        sess.queueText(QStringLiteral("NACHRICHT"));
        sess.writeFromUser("\r");
        QTest::qWait(200);                              // ein Abgabe-Takt
        QCOMPARE(backend->captured, QByteArray("x\r"));   // noch nicht hinterhergefallen
        QTRY_COMPARE_WITH_TIMEOUT(backend->captured, QByteArray("x\rNACHRICHT\r"), 3000);
    }

    void forcedSubmitByAutomationClearsTheFlag() {
        // Erzwungenes send_text mit Enter: Der Entwurf ist mit hinausgegangen.
        Rig rig;
        rig.key("h");
        QVERIFY(rig.sess.ownerTyping());
        rig.sess.noteLineSubmittedByAutomation();
        QVERIFY(!rig.sess.ownerTyping());
        QCOMPARE(rig.sess.ownerTypingSinceMs(), qint64(0));
    }
};

QTEST_MAIN(TestInputPriority)
#include "tst_inputpriority.moc"
