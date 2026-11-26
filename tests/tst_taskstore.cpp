#include <QtTest>
#include <QJsonArray>
#include <QRegularExpression>
#include <QSet>

#include "TaskStore.h"

// Speicher der MCP-Tasks (QTMUX-162), Gui-frei und mit gestellter Uhr: Grenzen,
// Aufbewahrung und Lebendfenster lassen sich so ohne Warten prüfen.
namespace qtmux {

class TestTaskStore : public QObject {
    Q_OBJECT

    qint64 m_now = 1'800'000'000'000LL;
    TaskStore make(TaskStore::Limits l = {}) {
        return TaskStore(l, [this] { return m_now; });
    }
    void advance(qint64 ms) { m_now += ms; }

private slots:
    void init() { m_now = 1'800'000'000'000LL; }

    void idsAreRandomAndWellFormed() {
        TaskStore st = make();
        QSet<QString> seen;
        const QRegularExpression re(QStringLiteral("^qt-[0-9a-f]{32}$"));
        for (int i = 0; i < 200; ++i) {
            const auto r = st.create(TaskStore::Kind::AwaitEvents, 1000 + i, true);
            QVERIFY2(!r.id.isEmpty(), qPrintable(r.error));
            QVERIFY(re.match(r.id).hasMatch());
            QVERIFY(TaskStore::isWellFormedId(r.id));
            QVERIFY(!seen.contains(r.id));
            seen.insert(r.id);
        }
    }

    void malformedIdsAreRejectedWithoutLookup() {
        QVERIFY(!TaskStore::isWellFormedId(QString()));
        QVERIFY(!TaskStore::isWellFormedId(QStringLiteral("QT-ABC")));
        QVERIFY(!TaskStore::isWellFormedId(QStringLiteral("qt-../x")));
        QVERIFY(!TaskStore::isWellFormedId(QStringLiteral("qt-ä")));
        QVERIFY(!TaskStore::isWellFormedId(QStringLiteral("qt- 1")));
        QVERIFY(!TaskStore::isWellFormedId(QString(65, QLatin1Char('a'))));
        QVERIFY(TaskStore::isWellFormedId(QString(64, QLatin1Char('a'))));
        TaskStore st = make();
        QCOMPARE(st.find(QStringLiteral("qt-0000")), nullptr);
    }

    void wireFormFollowsTheExtension() {
        TaskStore st = make();
        const QString id = st.create(TaskStore::Kind::AwaitMessage, 7, true).id;
        QJsonObject w = st.wire(*st.find(id));
        QCOMPARE(w.value("taskId").toString(), id);
        QCOMPARE(w.value("status").toString(), QStringLiteral("working"));
        QVERIFY(w.value("ttlMs").isNull());
        QCOMPARE(w.value("pollIntervalMs").toInt(), 2000);
        QVERIFY(w.value("createdAt").toString().endsWith(QLatin1Char('Z')));
        QVERIFY(!w.contains("result"));

        advance(5000);
        QVERIFY(st.complete(id, QJsonObject{{"content", QJsonArray{}}, {"isError", false}}, "fertig"));
        w = st.wire(*st.find(id));
        QCOMPARE(w.value("status").toString(), QStringLiteral("completed"));
        QCOMPARE(w.value("ttlMs").toDouble(), 5000.0 + 3600000.0);
        QVERIFY(w.contains("result"));
        QCOMPARE(w.value("statusMessage").toString(), QStringLiteral("fertig"));
        // Beendet bleibt beendet.
        QVERIFY(!st.complete(id, {}));
        QVERIFY(!st.cancel(id, "x"));
    }

    void pollIntervalSlowsAfterAMinute() {
        TaskStore st = make();
        const QString id = st.create(TaskStore::Kind::AwaitEvents, 1, true).id;
        QCOMPARE(st.pollIntervalMs(*st.find(id)), 2000);
        advance(60 * 1000 + 1);
        QCOMPARE(st.pollIntervalMs(*st.find(id)), 5000);
    }

    void newInboxReplacesTheOpenOne() {
        TaskStore st = make();
        const QString a = st.create(TaskStore::Kind::AwaitMessage, 3, true).id;
        const auto r = st.create(TaskStore::Kind::AwaitMessage, 3, true);
        QCOMPARE(r.replacedId, a);
        QCOMPARE(st.find(a)->status, TaskStore::Status::Cancelled);
        QCOMPARE(st.liveInboxOf(3), r.id);
        // Eine andere Session ist davon unberührt.
        const QString other = st.create(TaskStore::Kind::AwaitMessage, 4, true).id;
        QCOMPARE(st.find(r.id)->status, TaskStore::Status::Working);
        QCOMPARE(st.liveInboxOf(4), other);
    }

    void limitPerOwnerEvictsOnlyIdleTasks() {
        TaskStore st = make();
        QStringList ids;
        for (int i = 0; i < 8; ++i) ids << st.create(TaskStore::Kind::AwaitEvents, 5, true).id;
        // Alle leben (gerade angelegt): der neunte wird abgelehnt.
        auto r = st.create(TaskStore::Kind::AwaitEvents, 5, true);
        QVERIFY(r.id.isEmpty());
        QVERIFY(!r.error.isEmpty());
        // Nach 31 s ohne Abfrage leben sie nicht mehr; der älteste muss weichen.
        advance(31 * 1000);
        for (int i = 1; i < 8; ++i) st.notePolled(ids.at(i));
        r = st.create(TaskStore::Kind::AwaitEvents, 5, true);
        QVERIFY2(!r.id.isEmpty(), qPrintable(r.error));
        QCOMPARE(r.evictedId, ids.first());
        QCOMPARE(st.find(ids.first())->status, TaskStore::Status::Cancelled);
    }

    void limitTotalDropsFinishedFirst() {
        TaskStore::Limits l;
        l.maxTotal = 4;
        TaskStore st = make(l);
        const QString done = st.create(TaskStore::Kind::AwaitEvents, 1, true).id;
        st.complete(done, {});
        for (int i = 0; i < 3; ++i) QVERIFY(!st.create(TaskStore::Kind::AwaitEvents, 10 + i, true).id.isEmpty());
        QVERIFY(!st.create(TaskStore::Kind::AwaitEvents, 20, true).id.isEmpty());
        QCOMPARE(st.find(done), nullptr);
        // Nur noch offene: abgelehnt statt etwas Offenes zu verwerfen.
        QVERIFY(st.create(TaskStore::Kind::AwaitEvents, 21, true).id.isEmpty());
    }

    void finishedTasksStayAnHour() {
        TaskStore st = make();
        const QString id = st.create(TaskStore::Kind::AwaitEvents, 1, true).id;
        st.complete(id, {});
        advance(60LL * 60 * 1000);
        st.sweep();
        QVERIFY(st.find(id) != nullptr);
        advance(1);
        st.sweep();
        QCOMPARE(st.find(id), nullptr);
    }

    void openTaskWithoutPollsIsCancelledAfterADay() {
        TaskStore st = make();
        const QString id = st.create(TaskStore::Kind::AwaitMessage, 1, true).id;
        advance(24LL * 60 * 60 * 1000);
        st.sweep();
        QCOMPARE(st.find(id)->status, TaskStore::Status::Working);
        advance(1);
        st.sweep();
        QCOMPARE(st.find(id)->status, TaskStore::Status::Cancelled);
    }

    void liveWindowAndRecentInbox() {
        TaskStore st = make();
        const QString id = st.create(TaskStore::Kind::AwaitMessage, 9, true).id;
        QCOMPARE(st.liveInboxOf(9), id);
        advance(6 * 1000 + 1);
        QVERIFY(st.liveInboxOf(9).isEmpty());
        QVERIFY(st.inboxRecentlySeen(9));
        st.notePolled(id);
        QCOMPARE(st.liveInboxOf(9), id);
        // Nach Abschluss: 60 s lang „kürzlich gesehen“, dann nicht mehr.
        st.complete(id, {});
        QVERIFY(st.liveInboxOf(9).isEmpty());
        advance(60 * 1000);
        QVERIFY(st.inboxRecentlySeen(9));
        advance(1);
        QVERIFY(!st.inboxRecentlySeen(9));
        QVERIFY(!st.inboxRecentlySeen(10));
    }

    void fetchedIsSetOnlyByDeliveringTheEndState() {
        TaskStore st = make();
        const QString id = st.create(TaskStore::Kind::AwaitMessage, 2, true).id;
        const QString m = st.recordMessage(1, 2, "t", "b", "#1 x");
        st.notePolled(id);   // noch offen: kein Beleg
        QCOMPARE(st.find(id)->fetchedAt, qint64(0));
        st.complete(id, {}, {}, {m});
        QCOMPARE(st.message(m)->state, TaskStore::MessageState::InTask);
        QCOMPARE(TaskStore::stateName(st.message(m)->state), QStringLiteral("completed"));
        advance(10);
        st.notePolled(id);
        QCOMPARE(st.find(id)->fetchedAt, m_now);
        QCOMPARE(st.message(m)->state, TaskStore::MessageState::Fetched);
    }

    void bufferHasCountAndByteLimits() {
        TaskStore st = make();
        for (int i = 0; i < 32; ++i)
            QVERIFY(st.buffer(st.recordMessage(1, 2, QStringLiteral("m%1").arg(i), "b", "f")));
        QVERIFY(!st.buffer(st.recordMessage(1, 2, "zu viel", "b", "f")));
        const QList<TaskStore::Message> got = st.takeBuffered(2);
        QCOMPARE(got.size(), 32);
        QCOMPARE(got.first().text, QStringLiteral("m0"));
        QCOMPARE(got.last().text, QStringLiteral("m31"));
        QVERIFY(!st.hasBuffered(2));

        const QString big(200 * 1024, QLatin1Char('x'));
        QVERIFY(st.buffer(st.recordMessage(1, 3, big, "b", "f")));
        QVERIFY(!st.buffer(st.recordMessage(1, 3, big, "b", "f")));
    }

    void expiredBuffersAfterGrace() {
        TaskStore st = make();
        st.buffer(st.recordMessage(1, 2, "a", "a", "f"));
        advance(60 * 1000);
        QVERIFY(st.expiredBuffers().isEmpty());
        advance(1);
        QCOMPARE(st.expiredBuffers(), QList<int>{2});
    }

    void closingTheOwnerCancelsAndFailsBuffered() {
        TaskStore st = make();
        const QString id = st.create(TaskStore::Kind::AwaitMessage, 4, true).id;
        const QString m = st.recordMessage(1, 4, "a", "a", "f");
        st.buffer(m);
        st.ownerClosed(4);
        QCOMPARE(st.find(id)->status, TaskStore::Status::Cancelled);
        QCOMPARE(st.message(m)->state, TaskStore::MessageState::Failed);
        QVERIFY(!st.hasBuffered(4));
    }

    // Durchsicht 2026-11-26, Befund 1: Ein Beleg trägt den Text nur, solange er ihn braucht
    // (im Puffer). Sonst wüchse der Speicher mit jeder abgelehnten Riesen-Nachricht.
    void receiptsDropTheirTextOnceDecided() {
        TaskStore st = make();
        const QString big(100000, QLatin1Char('x'));
        const QString direct = st.recordMessage(1, 2, big, big, "f");
        st.markDirect(direct, "queue", "queued", false);
        QVERIFY(st.message(direct)->text.isEmpty());
        QVERIFY(st.message(direct)->body.isEmpty());
        const QString failed = st.recordMessage(1, 2, big, big, "f");
        st.markFailed(failed, "nein");
        QVERIFY(st.message(failed)->text.isEmpty());
        const QString buffered = st.recordMessage(1, 3, big, big, "f");
        QVERIFY(st.buffer(buffered));
        QCOMPARE(st.message(buffered)->body.size(), big.size());   // der Rückfall braucht ihn
        const QString id = st.create(TaskStore::Kind::AwaitMessage, 3, true).id;
        st.complete(id, {}, {}, {buffered});
        QVERIFY(st.message(buffered)->text.isEmpty());   // steht jetzt im Task-Ergebnis
    }

    // Befund 3: Ein Abbruch liefert keinen Endzustand aus, ist also kein Zustellbeleg.
    void cancelIsNoReceipt() {
        TaskStore st = make();
        const QString id = st.create(TaskStore::Kind::AwaitMessage, 2, true).id;
        const QString m = st.recordMessage(1, 2, "t", "b", "f");
        st.complete(id, {}, {}, {m});
        st.notePolled(id, /*deliveredEndState*/ false);
        QCOMPARE(st.find(id)->fetchedAt, qint64(0));
        QCOMPARE(st.message(m)->state, TaskStore::MessageState::InTask);
    }

    // Befund 5: Ein abgelehntes Anlegen darf den bestehenden Posteingang nicht wegnehmen.
    void rejectedCreateKeepsTheOldInbox() {
        TaskStore::Limits l;
        l.maxTotal = 2;
        TaskStore st = make(l);
        const QString inbox = st.create(TaskStore::Kind::AwaitMessage, 1, true).id;
        QVERIFY(!st.create(TaskStore::Kind::AwaitEvents, 2, true).id.isEmpty());
        // Gesamtgrenze erreicht, alles offen: Ein neuer Posteingang für Session 3 wird
        // abgelehnt und lässt alles, wie es war.
        QVERIFY(st.create(TaskStore::Kind::AwaitMessage, 3, true).id.isEmpty());
        QCOMPARE(st.find(inbox)->status, TaskStore::Status::Working);
        // Der Ersatz für den EIGENEN Posteingang geht durch: Der alte wird dabei frei.
        const auto r = st.create(TaskStore::Kind::AwaitMessage, 1, true);
        QVERIFY2(!r.id.isEmpty(), qPrintable(r.error));
        QCOMPARE(r.replacedId, inbox);
    }

    // Lebendschwelle aus der Messung (2026-11-26): Claude Code fragte im Takt von
    // pollIntervalMs nach, auch während langer Züge und /compact (größter Abstand 2,04 s
    // bzw. 5,01 s). „Lebt“ heißt darum: letzte Abfrage höchstens 3 Takte alt.
    void liveWindowFollowsThePollInterval() {
        TaskStore st = make();
        const QString id = st.create(TaskStore::Kind::AwaitMessage, 1, true).id;
        advance(6000);
        QCOMPARE(st.liveInboxOf(1), id);
        advance(1);
        QVERIFY(st.liveInboxOf(1).isEmpty());
        // Nach der ersten Minute gilt der langsame Takt: 15 s.
        advance(60 * 1000);
        st.notePolled(id);
        advance(15000);
        QCOMPARE(st.liveInboxOf(1), id);
        advance(1);
        QVERIFY(st.liveInboxOf(1).isEmpty());
    }

    // Befund 2: Ein beendeter Posteingang, den binnen 3 Takten niemand abholt, gibt seine
    // Nachrichten für den Rückfall frei.
    void unfetchedInboxIsReported() {
        TaskStore st = make();
        const QString id = st.create(TaskStore::Kind::AwaitMessage, 1, true).id;
        const QString m = st.recordMessage(5, 1, "t", "b", "f");
        st.complete(id, {}, {}, {m});
        QVERIFY(st.unfetchedInboxes().isEmpty());
        advance(6001);
        QCOMPARE(st.unfetchedInboxes(), QStringList{id});
        st.notePolled(id, true);
        QVERIFY(st.unfetchedInboxes().isEmpty());
    }

    void messageReceiptsExpire() {
        TaskStore st = make();
        const QString m = st.recordMessage(1, 2, "a", "a", "f");
        st.markDirect(m, "queue", "queued", false);
        advance(60LL * 60 * 1000 + 1);
        st.sweep();
        QCOMPARE(st.message(m), nullptr);
    }
};

} // namespace qtmux

QTEST_GUILESS_MAIN(qtmux::TestTaskStore)
#include "tst_taskstore.moc"
