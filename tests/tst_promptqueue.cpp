#include <QtTest>

#include "PromptQueue.h"
#include "Session.h"   // NUR für den Drift-Wächter unten — PromptQueue selbst kennt Session nicht

using namespace qtmux;

// ---------------------------------------------------------------------------------
// Drift-Wächter für die gespiegelten Aktivitätswerte.
//
// `ActivityCode` in [PromptQueue.h](../src/core/PromptQueue.h) bildet `Session::Activity`
// per **Zahl** ab, damit der Kern die Session nicht kennen muss (dieselbe Entkopplung wie
// bei `shouldPreventSleep(bool, QList<int>)`). Der Preis dieser Entkopplung ist, dass ein
// umsortiertes oder erweitertes `Session::Activity` hier lautlos durchrutschen würde: Aus
// „arbeitet gerade" würde stillschweigend „ist frei" — die Warteschlange schriebe dann
// mitten in die laufende Ausgabe, also genau der Fehler, den QTMUX-90 beheben soll. Ein
// Test würde das nicht merken, denn beide Seiten wären ja in sich schlüssig.
//
// Der Wächter gehört bewusst **hierher und nicht in den Kern**: In `PromptQueue.cpp` würde
// er `Session.h` erzwingen und damit die Entkopplung aufgeben, um die es geht. Der Test ist
// die erste Stelle, an der beide Seiten ohnehin sichtbar sind. Ein Verstoß bricht damit den
// **Build** des Tests, nicht erst seinen Lauf — schärfer als jede Laufzeitprüfung.
//
// (Wenn die Anbindung aus Teil 2 steht, darf derselbe Wächter dort zusätzlich stehen; er
// ist beidseitig unschädlich und an der Anbindungsstelle das direktere Dokument.)
static_assert(int(Session::Activity::Idle)    == ActivityIdle);
static_assert(int(Session::Activity::Running) == ActivityRunning);
static_assert(int(Session::Activity::Waiting) == ActivityWaiting);
static_assert(int(Session::Activity::Error)   == ActivityError);
static_assert(int(Session::Activity::Closed)  == ActivityClosed);

/// Tests für die Gui-freie Prompt-Warteschlange (QTMUX-90).
///
/// Zwei Dinge sind hier die eigentliche Substanz, alles andere ist Buchhaltung:
///  1. die **Abgabe-Bedingung** `mayDispatchNext` — sie darf bei einer Session ohne
///     Shell-Integration nicht für immer blockieren (dieselbe Falle wie QTMUX-89), und
///  2. die **Reihenfolge**, wenn während der Abgabe neu eingereiht wird.
class TestPromptQueue : public QObject {
    Q_OBJECT
private slots:

    // ---------------------------------------------------------------- Abgabe-Regel

    void nothingToDispatch() {
        // Leere Liste: nie abgeben, auch wenn die Session völlig frei ist.
        QVERIFY(!mayDispatchNext({ .queueNotEmpty = false, .activityReported = true,
                                   .activity = ActivityIdle }));
    }

    void reportedSessionDispatchesWhenNotRunning() {
        // Zweig 1: Die Session meldet selbst → ihrer Meldung wird geglaubt. Der
        // Ausgabestrom wird dabei gar nicht befragt (msSinceLastOutput bleibt -1,
        // also „noch nie Ausgabe" — in Zweig 2 wäre das ein hartes Nein).
        const auto ctx = [](int activity) {
            return DispatchContext{ .queueNotEmpty = true, .activityReported = true,
                                    .activity = activity, .msSinceLastOutput = -1 };
        };
        QVERIFY(!mayDispatchNext(ctx(ActivityRunning)));   // arbeitet → warten
        QVERIFY( mayDispatchNext(ctx(ActivityIdle)));      // Prompt zurück
        QVERIFY( mayDispatchNext(ctx(ActivityWaiting)));   // fragt einen Menschen
        QVERIFY( mayDispatchNext(ctx(ActivityError)));     // hat aufgehört zu arbeiten
        QVERIFY(!mayDispatchNext(ctx(ActivityClosed)));    // tot → nie
    }

    void unreportedSessionUsesQuietWindow() {
        // ⚠️ DER Kernfall (QTMUX-89-Falle): Eine Shell ohne Shell-Integration meldet nie
        // etwas, ihr Aktivitätswert steht deshalb für immer auf dem Startwert `Running`.
        // Würde die Regel hier auf „nicht Running" bestehen, bliebe die Warteschlange bei
        // JEDER gewöhnlichen Shell für immer stehen.
        DispatchContext ctx{ .queueNotEmpty = true, .activityReported = false,
                             .activity = ActivityRunning, .msSinceLastOutput = 900,
                             .quietMs = 500 };
        QVERIFY(mayDispatchNext(ctx));   // trotz `Running` — der Wert ist hier bedeutungslos

        // Es fließen noch Bytes → nicht hineinschreiben.
        ctx.msSinceLastOutput = 100;
        QVERIFY(!mayDispatchNext(ctx));

        // Genau auf der Schwelle wird abgegeben (>=, nicht >).
        ctx.msSinceLastOutput = 500;
        QVERIFY(mayDispatchNext(ctx));

        // Auch ein gemeldeter Ruhezustand ändert in Zweig 2 nichts — ohne Meldung ist
        // der Wert egal, es zählt allein die Ruhe.
        ctx.activity = ActivityIdle;
        ctx.msSinceLastOutput = 100;
        QVERIFY(!mayDispatchNext(ctx));
    }

    void unreportedSessionWithoutAnyOutputWaits() {
        // Noch nie eine Ausgabe gesehen heißt „noch nicht bereit", nicht „ruhig": der
        // Prompt der Shell kommt erst. Hineinzuschreiben ist die Login-Script-Falle.
        QVERIFY(!mayDispatchNext({ .queueNotEmpty = true, .activityReported = false,
                                   .activity = ActivityRunning, .msSinceLastOutput = -1 }));
    }

    void closedSessionNeverDispatches() {
        // Auch nach beliebig langer Ruhe und ohne jede Meldung: in eine tote Session
        // schreibt niemand mehr.
        QVERIFY(!mayDispatchNext({ .queueNotEmpty = true, .activityReported = false,
                                   .activity = ActivityClosed,
                                   .msSinceLastOutput = 100000, .quietMs = 500 }));
    }

    // ---------------------------------------------------------------- Einreihen

    void enqueueAppendsAndSignals() {
        PromptQueue q;
        QSignalSpy spy(&q, &PromptQueue::changed);
        QVERIFY(q.isEmpty());
        QVERIFY(q.enqueue("eins"));
        QVERIFY(q.enqueue("zwei"));
        QCOMPARE(q.count(), 2);
        QCOMPARE(spy.count(), 2);
        QCOMPARE(q.entries(), QStringList({ "eins", "zwei" }));
        QCOMPARE(q.peekNext(), QString("eins"));
        QVERIFY(!q.isEmpty());
    }

    void blankEntriesAreRejected() {
        // Ein leerer Eintrag ginge als blankes Enter ins PTY — in einem Agenten-TUI ist
        // das eine HANDLUNG (Claude Code bestätigt damit die vorausgewählte Option einer
        // Rückfrage). Abweisen ist die einzige folgenlose Antwort.
        PromptQueue q;
        QSignalSpy spy(&q, &PromptQueue::changed);
        QVERIFY(!q.enqueue(QString()));
        QVERIFY(!q.enqueue(""));
        QVERIFY(!q.enqueue("   "));
        QVERIFY(!q.enqueue("\t \n"));
        QVERIFY(!q.insert(0, "  "));
        QCOMPARE(q.count(), 0);
        QCOMPARE(spy.count(), 0);   // abgewiesen heißt: KEIN Signal, die Liste änderte sich nicht

        // Aber: Rand-Whitespace wird nur zum PRÜFEN getrimmt, nicht zum Speichern.
        // Führende Leerzeichen sind bedeutsam (eingerückter Code; in vielen Shells hält
        // ein führendes Leerzeichen den Befehl aus der History).
        QVERIFY(q.enqueue("  make test  "));
        QCOMPARE(q.at(0), QString("  make test  "));
    }

    void insertClampsIndex() {
        PromptQueue q;
        q.enqueue("b");
        // Klemmen statt abweisen: die Aufrufstelle ist eine Liste zum Umsortieren, ein
        // um eins verrutschter Index darf den Eintrag nicht verschlucken.
        QVERIFY(q.insert(-5, "a"));
        QVERIFY(q.insert(99, "c"));
        QCOMPARE(q.entries(), QStringList({ "a", "b", "c" }));
    }

    // ---------------------------------------------------------------- Bearbeiten

    void removeClearAndMove() {
        PromptQueue q;
        for (const QString &s : { "a", "b", "c" }) q.enqueue(s);

        QSignalSpy spy(&q, &PromptQueue::changed);
        QVERIFY(!q.removeAt(-1));
        QVERIFY(!q.removeAt(3));
        QCOMPARE(spy.count(), 0);          // ungültig → keine Änderung, kein Signal
        QVERIFY(q.removeAt(1));
        QCOMPARE(q.entries(), QStringList({ "a", "c" }));
        QCOMPARE(spy.count(), 1);

        QCOMPARE(q.at(-1), QString());     // ungültiger Index liefert leer statt zu crashen
        QCOMPARE(q.at(7), QString());

        q.enqueue("d");                    // a, c, d
        QVERIFY(!q.move(0, 0));            // gleiche Position ist keine Änderung
        QVERIFY(!q.move(0, 3));            // Ziel außerhalb: sichtbar scheitern, nicht klemmen
        QVERIFY(!q.move(-1, 0));
        QVERIFY(q.move(2, 0));
        QCOMPARE(q.entries(), QStringList({ "d", "a", "c" }));

        QCOMPARE(q.clear(), 3);
        QVERIFY(q.isEmpty());
        QCOMPARE(q.clear(), 0);            // schon leer → kein zweites Signal
    }

    void clearOnEmptyQueueIsSilent() {
        PromptQueue q;
        QSignalSpy spy(&q, &PromptQueue::changed);
        QCOMPARE(q.clear(), 0);
        QCOMPARE(spy.count(), 0);
    }

    // ---------------------------------------------------------------- Abgeben

    void takeNextIsFifo() {
        PromptQueue q;
        for (const QString &s : { "eins", "zwei" }) q.enqueue(s);
        QCOMPARE(q.takeNext(), QString("eins"));
        QCOMPARE(q.count(), 1);
        QCOMPARE(q.takeNext(), QString("zwei"));
        QVERIFY(q.isEmpty());
        QCOMPARE(q.takeNext(), QString());   // leere Liste → leer, kein Absturz
    }

    void enqueueDuringDispatchKeepsOrder() {
        // Die geforderte Eigenschaft: Wird WÄHREND der Abgabe neu eingereiht, muss der
        // neue Eintrag hinter den bereits wartenden landen. Das trägt nur, weil takeNext
        // zuerst entnimmt und erst danach `changed` meldet.
        PromptQueue q;
        for (const QString &s : { "eins", "zwei" }) q.enqueue(s);

        bool once = true;
        connect(&q, &PromptQueue::changed, this, [&] {
            if (!once) return;
            once = false;
            q.enqueue("spaet");          // reiht mitten in der Abgabe von "eins" ein
        });

        QCOMPARE(q.takeNext(), QString("eins"));
        QCOMPARE(q.entries(), QStringList({ "zwei", "spaet" }));   // NICHT { "spaet", "zwei" }
        QCOMPARE(q.takeNext(), QString("zwei"));
        QCOMPARE(q.takeNext(), QString("spaet"));
    }

    void nestedTakeNextIsBlocked() {
        // Ohne Reentranz-Wächter hätte der verschachtelte Aufruf "zwei" geholt und
        // abgesetzt, bevor der äußere Aufruf sein "eins" überhaupt zurückgegeben hat —
        // auf der Leitung stünde #2 vor #1.
        PromptQueue q;
        for (const QString &s : { "eins", "zwei", "drei" }) q.enqueue(s);

        QString ausSlot = "unberuehrt";
        bool once = true;
        connect(&q, &PromptQueue::changed, this, [&] {
            if (!once) return;
            once = false;
            ausSlot = q.takeNext();
        });

        QCOMPARE(q.takeNext(), QString("eins"));
        QCOMPARE(ausSlot, QString());                              // der innere Aufruf tat nichts
        QCOMPARE(q.entries(), QStringList({ "zwei", "drei" }));     // nichts verschluckt
        // Nach der Abgabe ist die Warteschlange wieder normal benutzbar (der Wächter
        // darf nicht hängen bleiben).
        QCOMPARE(q.takeNext(), QString("zwei"));
    }
};

QTEST_APPLESS_MAIN(TestPromptQueue)
#include "tst_promptqueue.moc"
