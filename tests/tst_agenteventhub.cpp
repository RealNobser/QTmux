#include <QtTest>
#include <QSignalSpy>

#include "AgentEventHub.h"

using namespace qtmux;
using Kind = AgentEventHub::Kind;

/// Tests für den Inter-Agenten-Ereignis-Bus (Gui-frei). Reine Logik: postEvent/seq,
/// Ringpuffer-Cap, Abo-Filter (Quelle/Art/„nicht sich selbst"), eventsFor-Cursor.
class TestAgentEventHub : public QObject {
    Q_OBJECT
private slots:
    void init() {
        // Frischer Zustand je Testfall: alle Abos der genutzten IDs entfernen.
        auto *h = AgentEventHub::instance();
        for (int id = 1; id <= 50; ++id) h->unsubscribe(id);
    }

    void kindRoundtrip() {
        QCOMPARE(AgentEventHub::kindFromString(QStringLiteral("done")), Kind::Done);
        QCOMPARE(AgentEventHub::kindFromString(QStringLiteral("QUESTION")), Kind::Question);
        QCOMPARE(AgentEventHub::kindFromString(QStringLiteral("error")), Kind::Error);
        QCOMPARE(AgentEventHub::kindFromString(QStringLiteral("info")), Kind::Info);
        QCOMPARE(AgentEventHub::kindFromString(QStringLiteral("quatsch")), Kind::Info); // unbekannt → Info
        QCOMPARE(AgentEventHub::kindName(Kind::Done), QStringLiteral("done"));
        QCOMPARE(AgentEventHub::kindName(Kind::Question), QStringLiteral("question"));
    }

    void seqMonotonAndSignal() {
        auto *h = AgentEventHub::instance();
        QSignalSpy spy(h, &AgentEventHub::eventPosted);
        const quint64 a = h->postEvent(1, Kind::Done, QStringLiteral("a"));
        const quint64 b = h->postEvent(1, Kind::Info, QStringLiteral("b"));
        QVERIFY(b > a);                 // streng monoton
        QCOMPARE(spy.count(), 2);       // je Ereignis ein Signal
        QCOMPARE(h->lastSeq(), b);
    }

    void subscribeAllSources() {
        auto *h = AgentEventHub::instance();
        const quint64 cursor = h->lastSeq();
        h->subscribe(10, {}, {});       // alle Quellen, alle Arten
        h->postEvent(3, Kind::Done, QStringLiteral("x"));
        h->postEvent(4, Kind::Question, QStringLiteral("y"));
        const auto evs = h->eventsFor(10, cursor);
        QCOMPARE(evs.size(), 2);
        QCOMPARE(evs.at(0).sourceSessionId, 3);
        QCOMPARE(evs.at(1).kind, Kind::Question);
    }

    void filterBySource() {
        auto *h = AgentEventHub::instance();
        const quint64 cursor = h->lastSeq();
        h->subscribe(11, QVariantList{5}, {});   // nur Quelle 5
        h->postEvent(5, Kind::Done, QStringLiteral("ja"));
        h->postEvent(6, Kind::Done, QStringLiteral("nein"));
        const auto evs = h->eventsFor(11, cursor);
        QCOMPARE(evs.size(), 1);
        QCOMPARE(evs.at(0).sourceSessionId, 5);
    }

    void filterByKind() {
        auto *h = AgentEventHub::instance();
        const quint64 cursor = h->lastSeq();
        h->subscribe(12, {}, QStringList{QStringLiteral("question")});
        h->postEvent(7, Kind::Done, QStringLiteral("egal"));
        h->postEvent(7, Kind::Question, QStringLiteral("frage"));
        const auto evs = h->eventsFor(12, cursor);
        QCOMPARE(evs.size(), 1);
        QCOMPARE(evs.at(0).kind, Kind::Question);
    }

    void notSelf() {
        auto *h = AgentEventHub::instance();
        const quint64 cursor = h->lastSeq();
        h->subscribe(13, {}, {});
        h->postEvent(13, Kind::Done, QStringLiteral("eigenes"));  // eigene Quelle
        h->postEvent(14, Kind::Done, QStringLiteral("fremdes"));
        const auto evs = h->eventsFor(13, cursor);
        QCOMPARE(evs.size(), 1);                 // nur das fremde
        QCOMPARE(evs.at(0).sourceSessionId, 14);
    }

    void cursorAdvances() {
        auto *h = AgentEventHub::instance();
        h->subscribe(15, {}, {});
        const quint64 c0 = h->lastSeq();
        const quint64 s1 = h->postEvent(8, Kind::Done, QStringLiteral("eins"));
        auto first = h->eventsFor(15, c0);
        QCOMPARE(first.size(), 1);
        // Mit dem Cursor des gelesenen Ereignisses gibt es nichts Neues.
        QVERIFY(h->eventsFor(15, s1).isEmpty());
        h->postEvent(8, Kind::Done, QStringLiteral("zwei"));
        QCOMPARE(h->eventsFor(15, s1).size(), 1); // nur das neue
    }

    void noSubscriptionNoEvents() {
        auto *h = AgentEventHub::instance();
        const quint64 cursor = h->lastSeq();
        h->postEvent(9, Kind::Done, QStringLiteral("z"));
        QVERIFY(h->eventsFor(99, cursor).isEmpty());   // 99 hat kein Abo
    }

    void unsubscribeStops() {
        auto *h = AgentEventHub::instance();
        h->subscribe(16, {}, {});
        QVERIFY(h->hasSubscription(16));
        h->unsubscribe(16);
        QVERIFY(!h->hasSubscription(16));
        const quint64 cursor = h->lastSeq();
        h->postEvent(2, Kind::Done, QStringLiteral("nach abmelden"));
        QVERIFY(h->eventsFor(16, cursor).isEmpty());
    }

    void latestFrom() {
        auto *h = AgentEventHub::instance();
        h->postEvent(20, Kind::Done, QStringLiteral("alt"));
        h->postEvent(20, Kind::Error, QStringLiteral("neu"));
        const auto ev = h->latestFrom(20);
        QCOMPARE(ev.kind, Kind::Error);
        QCOMPARE(ev.text, QStringLiteral("neu"));
        QCOMPARE(h->latestFrom(9999).seq, quint64(0));  // keines → seq 0
    }

    void ringBufferCap() {
        auto *h = AgentEventHub::instance();
        h->subscribe(17, {}, {});
        // Mehr als die Kapazität (256) einspeisen; älteste werden verworfen.
        for (int i = 0; i < 400; ++i) h->postEvent(30, Kind::Info, QString::number(i));
        const auto evs = h->eventsFor(17, 0);  // ab Anfang
        QVERIFY(evs.size() <= 256);
        // Das jüngste Ereignis ist erhalten.
        QCOMPARE(evs.last().text, QStringLiteral("399"));
    }
};

QTEST_MAIN(TestAgentEventHub)
#include "tst_agenteventhub.moc"
