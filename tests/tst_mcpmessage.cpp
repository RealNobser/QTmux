#include <QtTest>
#include <QSettings>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocalServer>
#include <QLocalSocket>
#include <QProcess>
#include <QTemporaryDir>

#include "ITerminalBackend.h"
#include "McpServer.h"
#include "Session.h"
#include "SessionModel.h"

// Vorrang der Eingabe des Menschen und Nachrichten zwischen Sessions, an der MCP-Schicht:
// Was antworten send_text/send_keys/queue_text/send_message, was landet wirklich im
// Programm, und wer steht als Absender da?
//
// Sessions: seriell auf einem nicht vorhandenen Port (kein Prozess, alle Plattformen),
// danach bekommt jede ein Mitschnitt-Backend — so ist „es wurde nichts geschrieben“ eine
// Messung am Ausgang der Session und keine Annahme über ihren Zustand.
namespace qtmux {

class CaptureBackend final : public ITerminalBackend {
public:
    bool start(int, int) override { setState(BackendState::Running); return true; }
    void write(const QByteArray &d) override { captured += d; }
    void resize(int, int) override {}
    void terminate() override { setState(BackendState::Closed); }
    qint64 processId() const override { return pid; }
    QByteArray captured;
    qint64 pid = -1;
};

class TestMcpMessage : public QObject {
    Q_OBJECT

    struct Rig {
        SessionModel sessions;
        McpServer mcp;
        Rig() { mcp.setSessions(&sessions); }
        // Legt eine Session an und gibt ihr ein Mitschnitt-Backend. Sie meldet sich als
        // frei: Ohne den Merker ginge ein eingereihter Text sofort hinaus.
        int add(CaptureBackend **out = nullptr) {
            const int row = sessions.createSerialSession(QStringLiteral("qtmux-test-kein-port"), 9600);
            auto *s = static_cast<Session *>(sessions.sessionAt(row));
            auto *b = new CaptureBackend;
            s->attachBackend(b, Session::Type::Shell, 80, 24);
            s->start(80, 24);
            s->requestActivity(QStringLiteral("idle"));
            if (out) *out = b;
            return s->id();
        }
        Session *session(int id) { return sessions.sessionById(id); }
        QJsonObject call(const QString &name, const QJsonObject &args, bool &isError, QString &text) {
            isError = false;
            text.clear();
            return mcp.callTool(name, args, isError, text);
        }
        QJsonObject callJson(const QString &name, const QJsonObject &args, bool &isError) {
            QString text;
            call(name, args, isError, text);
            return QJsonDocument::fromJson(text.toUtf8()).object();
        }
        // Wer ruft? Im Betrieb ordnet onReadyRead den verbindenden Prozess seiner Session
        // zu; der Test setzt das Ergebnis dieser Zuordnung.
        void setCaller(int sessionId) { mcp.m_callerSessionId = sessionId; }
    };

    // Ein Nachrichten-Eingang, den der Test selbst aufmacht.
    struct Inbox {
        QTemporaryDir dir;
        QLocalServer server;
        QByteArray received;
        QString path;
        Inbox() {
            path = dir.filePath(QStringLiteral("in.sock"));
            QObject::connect(&server, &QLocalServer::newConnection, &server, [this] {
                while (QLocalSocket *c = server.nextPendingConnection()) {
                    QObject::connect(c, &QLocalSocket::readyRead, c,
                                     [this, c] { received += c->readAll(); });
                    QObject::connect(c, &QLocalSocket::disconnected, c, [this, c] {
                        received += c->readAll();
                        c->deleteLater();
                    });
                }
            });
        }
    };

private slots:
    void initTestCase() {
        QCoreApplication::setOrganizationName(QStringLiteral("QTmux"));
        QCoreApplication::setApplicationName(QStringLiteral("QTmuxMcpMessageTest"));
    }
    void init() { QSettings().clear(); }

    // --- Vorrang: send_text / send_keys lehnen ab -----------------------------------
    void sendTextIsRefusedWhileTheHumanTypes() {
        Rig rig;
        CaptureBackend *b = nullptr;
        const int id = rig.add(&b);
        rig.session(id)->writeFromUser("halber Sat");
        b->captured.clear();

        bool isError = false;
        QString text;
        rig.call(QStringLiteral("send_text"), {{"id", id}, {"text", "ANGEHAENGT"}}, isError, text);
        QVERIFY(isError);
        QVERIFY2(text.contains(QStringLiteral("send_message")) && text.contains(QStringLiteral("queue_text"))
                     && text.contains(QStringLiteral("force")), qPrintable(text));
        QTest::qWait(150);                              // auch kein abgesetztes Enter
        QVERIFY2(b->captured.isEmpty(), b->captured.constData());
        QVERIFY(rig.session(id)->ownerTyping());

        // Gegenprobe: Ohne den Entwurf geht derselbe Aufruf durch.
        rig.session(id)->writeFromUser("\x03");
        b->captured.clear();
        rig.call(QStringLiteral("send_text"), {{"id", id}, {"text", "DURCH"}, {"enterDelayMs", 0}}, isError, text);
        QVERIFY2(!isError, qPrintable(text));
        QCOMPARE(b->captured, QByteArray("DURCH\r"));
    }

    void sendKeysIsRefusedWhileTheHumanTypes() {
        // Der eigentliche Schaden der „C-u-Salve“: Sie löschte den Entwurf des Menschen.
        Rig rig;
        CaptureBackend *b = nullptr;
        const int id = rig.add(&b);
        rig.session(id)->writeFromUser("x");
        b->captured.clear();
        bool isError = false;
        QString text;
        rig.call(QStringLiteral("send_keys"), {{"id", id}, {"keys", QJsonArray{"C-u", "Enter"}}}, isError, text);
        QVERIFY(isError);
        QTest::qWait(150);
        QVERIFY(b->captured.isEmpty());
    }

    void forceOverridesAndASubmitClearsTheFlag() {
        Rig rig;
        CaptureBackend *b = nullptr;
        const int id = rig.add(&b);
        rig.session(id)->writeFromUser("x");
        b->captured.clear();
        bool isError = false;
        QString text;
        // Ohne Enter: Der Text steht jetzt im Entwurf, der Merker bleibt.
        rig.call(QStringLiteral("send_text"),
                 {{"id", id}, {"text", "A"}, {"enter", false}, {"force", true}}, isError, text);
        QVERIFY2(!isError, qPrintable(text));
        QCOMPARE(b->captured, QByteArray("A"));
        QVERIFY(rig.session(id)->ownerTyping());
        // Mit Enter: Die Zeile ist samt Entwurf hinausgegangen, der Merker fällt.
        rig.call(QStringLiteral("send_text"),
                 {{"id", id}, {"text", "B"}, {"enterDelayMs", 0}, {"force", true}}, isError, text);
        QVERIFY2(!isError, qPrintable(text));
        QCOMPARE(b->captured, QByteArray("AB\r"));
        QVERIFY(!rig.session(id)->ownerTyping());
        // send_keys mit force schreibt ebenfalls.
        rig.session(id)->writeFromUser("y");
        b->captured.clear();
        rig.call(QStringLiteral("send_keys"), {{"id", id}, {"keys", QJsonArray{"C-u"}}, {"force", true}}, isError, text);
        QVERIFY2(!isError, qPrintable(text));
        QCOMPARE(b->captured, QByteArray("\x15"));
    }

    void broadcastIsRefusedIfTheHumanTypesAnywhere() {
        Rig rig;
        CaptureBackend *a = nullptr, *b = nullptr;
        const int idA = rig.add(&a);
        const int idB = rig.add(&b);
        rig.session(idB)->writeFromUser("x");
        a->captured.clear(); b->captured.clear();
        bool isError = false;
        QString text;
        rig.call(QStringLiteral("send_text"), {{"text", "RUNDRUF"}, {"broadcast", true}}, isError, text);
        QVERIFY(isError);
        QVERIFY2(text.contains(QString::number(idB)), qPrintable(text));
        QVERIFY(a->captured.isEmpty());                  // auch nicht an die freie Session
        QVERIFY(b->captured.isEmpty());
        Q_UNUSED(idA)
        rig.session(idB)->releaseOwnerDraft();
        rig.call(QStringLiteral("send_text"), {{"text", "RUNDRUF"}, {"broadcast", true}}, isError, text);
        QVERIFY2(!isError, qPrintable(text));
        QCOMPARE(a->captured, QByteArray("RUNDRUF\r"));
    }

    void broadcastTypingByTheHumanCountsInEverySession() {
        // Broadcast-Eingabe kommt aus einem Terminal, also vom Menschen: Jede Session
        // führt ihren Merker. Der maschinelle Rundruf (MCP) tut das nicht.
        Rig rig;
        const int idA = rig.add();
        const int idB = rig.add();
        rig.sessions.writeToAll("m");
        QVERIFY(!rig.session(idA)->ownerTyping());
        QVERIFY(!rig.session(idB)->ownerTyping());
        rig.sessions.writeToAllFromUser("x");
        QVERIFY(rig.session(idA)->ownerTyping());
        QVERIFY(rig.session(idB)->ownerTyping());
        QCOMPARE(rig.sessions.sessionsWithOwnerTyping().size(), 2);
    }

    // --- Warteschlange: wartet, nennt Absender ----------------------------------------
    void queueTextWaitsAndNamesTheSender() {
        Rig rig;
        CaptureBackend *b = nullptr;
        const int target = rig.add(&b);
        const int caller = rig.add();
        rig.session(target)->writeFromUser("Entwurf");
        b->captured.clear();
        rig.setCaller(caller);

        bool isError = false;
        QJsonObject r = rig.callJson(QStringLiteral("queue_text"), {{"id", target}, {"text", "MELDUNG"}}, isError);
        QVERIFY(!isError);
        QCOMPARE(r.value("queued").toBool(), true);
        QCOMPARE(r.value("count").toInt(), 1);
        QCOMPARE(r.value("ownerTyping").toBool(), true);
        QCOMPARE(r.value("entries").toArray(), QJsonArray{"MELDUNG"});   // wie bisher
        const QJsonObject item = r.value("items").toArray().at(0).toObject();
        QCOMPARE(item.value("text").toString(), QStringLiteral("MELDUNG"));
        const QString from = item.value("from").toString();
        QVERIFY2(from.startsWith(QStringLiteral("#%1").arg(caller)), qPrintable(from));
        QTest::qWait(400);
        QVERIFY(b->captured.isEmpty());

        // list_sessions zeigt denselben Zustand.
        QString text;
        rig.call(QStringLiteral("list_sessions"), {}, isError, text);
        QJsonObject info;
        for (const QJsonValue &v : QJsonDocument::fromJson(text.toUtf8()).array())
            if (v.toObject().value("id").toInt() == target) info = v.toObject();
        QCOMPARE(info.value("ownerTyping").toBool(), true);
        QVERIFY(info.contains("ownerTypingForMs"));
        QCOMPARE(info.value("queued").toInt(), 1);
        QCOMPARE(info.value("queuedFrom").toArray().at(0).toString(), from);

        // Der Mensch schickt ab → der Eintrag folgt, die Felder fallen zurück.
        rig.session(target)->writeFromUser("\r");
        QTRY_COMPARE(b->captured, QByteArray("\rMELDUNG\r"));
        r = rig.callJson(QStringLiteral("queue_text"), {{"id", target}}, isError);
        QCOMPARE(r.value("ownerTyping").toBool(), false);
        QCOMPARE(r.value("count").toInt(), 0);
    }

    void senderCannotBeClaimed() {
        // Den Absender setzt QTmux aus dem aufrufenden Prozess. Ein Aufrufer, der eine
        // fremde Session als `sessionId` (oder irgendein anderes Feld) angibt, ändert
        // daran nichts — sonst könnte jeder Agent als Orchestrator auftreten.
        Rig rig;
        const int target = rig.add();
        const int caller = rig.add();
        const int other = rig.add();
        rig.session(target)->writeFromUser("x");
        rig.setCaller(caller);
        bool isError = false;
        QJsonObject r = rig.callJson(QStringLiteral("queue_text"),
                                     {{"id", target}, {"text", "A"}, {"sessionId", other},
                                      {"from", "Owner"}, {"sender", "Owner"}}, isError);
        const QString from = r.value("items").toArray().at(0).toObject().value("from").toString();
        QVERIFY2(from.startsWith(QStringLiteral("#%1").arg(caller)), qPrintable(from));
        r = rig.callJson(QStringLiteral("send_message"),
                         {{"id", target}, {"text", "B"}, {"sessionId", other}, {"from", "Owner"}}, isError);
        QVERIFY2(r.value("from").toString().startsWith(QStringLiteral("#%1").arg(caller)),
                 qPrintable(r.value("from").toString()));
        // Unbekannter Aufrufer (kein Prozess einer Session) heißt so, auch mit Angabe.
        rig.setCaller(-1);
        r = rig.callJson(QStringLiteral("send_message"),
                         {{"id", target}, {"text", "C"}, {"sessionId", other}}, isError);
        QCOMPARE(r.value("from").toString(), QStringLiteral("unbekannt"));
    }

    // --- send_message --------------------------------------------------------------
    void sendMessageWithoutInboxUsesTheQueue() {
        Rig rig;
        CaptureBackend *b = nullptr;
        const int target = rig.add(&b);
        const int caller = rig.add();
        rig.setCaller(caller);
        rig.session(target)->writeFromUser("Entwurf");
        b->captured.clear();

        bool isError = false;
        const QJsonObject r = rig.callJson(QStringLiteral("send_message"), {{"id", target}, {"text", "HALLO"}}, isError);
        QVERIFY(!isError);
        QCOMPARE(r.value("via").toString(), QStringLiteral("queue"));
        QCOMPARE(r.value("status").toString(), QStringLiteral("queued"));
        QVERIFY(!r.value("reason").toString().isEmpty());
        QCOMPARE(r.value("ownerTyping").toBool(), true);
        QTest::qWait(300);
        QVERIFY(b->captured.isEmpty());                  // der Entwurf bleibt unberührt
        // Der eingereihte Text sagt dem Empfänger, dass er nicht vom Menschen stammt.
        const QString queued = rig.session(target)->promptQueue()->peekNext();
        QVERIFY2(queued.startsWith(QStringLiteral("[QTmux-Nachricht von Session #%1").arg(caller)), qPrintable(queued));
        QVERIFY(queued.contains(QStringLiteral("nicht vom Menschen")));
        QVERIFY(queued.endsWith(QStringLiteral("\nHALLO")));

        // Freie Session: geht sofort hinaus, die Antwort sagt das.
        rig.session(target)->writeFromUser("\x03");
        QTRY_COMPARE(rig.session(target)->queuedCount(), 0);
        const QJsonObject r2 = rig.callJson(QStringLiteral("send_message"), {{"id", target}, {"text", "ZWEI"}}, isError);
        QCOMPARE(r2.value("via").toString(), QStringLiteral("queue"));
        QCOMPARE(r2.value("status").toString(), QStringLiteral("dispatched"));
    }

    void sendMessageRejectsNonsense() {
        Rig rig;
        const int target = rig.add();
        bool isError = false;
        QString text;
        rig.call(QStringLiteral("send_message"), {{"id", 9999}, {"text", "x"}}, isError, text);
        QVERIFY(isError);
        rig.call(QStringLiteral("send_message"), {{"id", target}, {"text", "   "}}, isError, text);
        QVERIFY(isError);
        QCOMPARE(rig.session(target)->queuedCount(), 0);
    }

    void sendMessageGoesThroughTheInboxAndLeavesTheTerminalAlone() {
#if !defined(Q_OS_UNIX)
        QSKIP("Der Eingang ist nur unter Unix gebaut.");
#else
        Rig rig;
        CaptureBackend *b = nullptr;
        const int target = rig.add(&b);
        const int caller = rig.add();
        rig.setCaller(caller);
        // Am Eingang lauscht dieser Testprozess; die Session gibt ihn als ihren Prozess
        // aus — so wie im Betrieb der Agent im Prozessbaum der Session-Shell liegt.
        b->pid = QCoreApplication::applicationPid();
        Inbox inbox;
        QVERIFY2(inbox.server.listen(inbox.path), qPrintable(inbox.server.errorString()));

        bool isError = false;
        QString text;
        rig.setCaller(target);   // die Sitzung meldet ihren Eingang selbst
        rig.call(QStringLiteral("set_agent_inbox"),
                 {{"kind", "claude-code"}, {"address", inbox.path}, {"version", "2.1.323"}}, isError, text);
        QVERIFY2(!isError, qPrintable(text));
        QVERIFY(rig.session(target)->hasAgentInbox());
        rig.setCaller(caller);

        // Der Mensch tippt gerade — für den Eingang ist das gleichgültig.
        rig.session(target)->writeFromUser("Entwurf");
        b->captured.clear();
        const QJsonObject r = rig.callJson(QStringLiteral("send_message"),
                                           {{"id", target}, {"text", "Erste Zeile\nzweite"}}, isError);
        QVERIFY(!isError);
        QCOMPARE(r.value("via").toString(), QStringLiteral("inbox"));
        QCOMPARE(r.value("status").toString(), QStringLiteral("handed-over"));
        QVERIFY(!r.contains("note"));                    // gemeldete Fassung = belegte Fassung
        QTRY_VERIFY(inbox.received.endsWith('\n'));
        QCOMPARE(inbox.received.count('\n'), 1);
        const QJsonObject frame = QJsonDocument::fromJson(inbox.received).object();
        const QString content = frame.value("message").toObject().value("content").toString();
        QVERIFY2(content.startsWith(QStringLiteral("<cross-session-message from-name=\"QTmux #%1").arg(caller)),
                 qPrintable(content));
        QVERIFY(content.contains(QStringLiteral("\nErste Zeile\nzweite\n")));
        // Der Antwort-Hinweis: wohin, und ausdrücklich NICHT per SendMessage (gemessen
        // suchte der Empfänger sonst in seiner Sitzungsliste nach einem ähnlichen Namen).
        QVERIFY(content.contains(QStringLiteral("send_message, id %1.]").arg(caller)));
        QVERIFY(content.contains(QStringLiteral("nicht per SendMessage antworten")));
        QVERIFY(content.endsWith(QStringLiteral("]\n</cross-session-message>")));
        // Nichts im Terminal, nichts in der Warteschlange, der Entwurf steht.
        QTest::qWait(200);
        QVERIFY(b->captured.isEmpty());
        QCOMPARE(rig.session(target)->queuedCount(), 0);
        QVERIFY(rig.session(target)->ownerTyping());
        QVERIFY(rig.session(target)->lastNotification().contains(QStringLiteral("#%1").arg(caller)));

        // list_sessions nennt die gemeldete Gegenstelle.
        rig.call(QStringLiteral("list_sessions"), {}, isError, text);
        for (const QJsonValue &v : QJsonDocument::fromJson(text.toUtf8()).array())
            if (v.toObject().value("id").toInt() == target)
                QCOMPARE(v.toObject().value("agentInbox").toString(), QStringLiteral("claude-code"));
#endif
    }

    void unverifiedAgentVersionIsSaid() {
#if !defined(Q_OS_UNIX)
        QSKIP("Der Eingang ist nur unter Unix gebaut.");
#else
        // Das Rahmenformat ist nicht dokumentiert. Meldet der Agent eine andere Fassung als
        // die belegte, steht das in der Antwort — still hoffen wäre die falsche Bauart.
        Rig rig;
        CaptureBackend *b = nullptr;
        const int target = rig.add(&b);
        b->pid = QCoreApplication::applicationPid();
        Inbox inbox;
        QVERIFY(inbox.server.listen(inbox.path));
        rig.session(target)->setAgentInbox(QStringLiteral("claude-code"), inbox.path, QStringLiteral("2.2.0"));
        bool isError = false;
        QJsonObject r = rig.callJson(QStringLiteral("send_message"), {{"id", target}, {"text", "x"}}, isError);
        QCOMPARE(r.value("via").toString(), QStringLiteral("inbox"));
        QVERIFY2(r.value("note").toString().contains(QStringLiteral("2.2.0")), qPrintable(r.value("note").toString()));
        rig.session(target)->setAgentInbox(QStringLiteral("claude-code"), inbox.path, QString());
        r = rig.callJson(QStringLiteral("send_message"), {{"id", target}, {"text", "y"}}, isError);
        QVERIFY(!r.value("note").toString().isEmpty());
#endif
    }

    void failedInboxFallsBackToTheQueueAndSaysWhy() {
#if !defined(Q_OS_UNIX)
        QSKIP("Der Eingang ist nur unter Unix gebaut.");
#else
        Rig rig;
        CaptureBackend *b = nullptr;
        const int target = rig.add(&b);
        bool isError = false;

        // (1) Der Socket ist weg (Agent beendet).
        QTemporaryDir dir;
        rig.session(target)->setAgentInbox(QStringLiteral("claude-code"), dir.filePath("weg.sock"), QString());
        QJsonObject r = rig.callJson(QStringLiteral("send_message"), {{"id", target}, {"text", "EINS"}}, isError);
        QVERIFY(!isError);
        QCOMPARE(r.value("via").toString(), QStringLiteral("queue"));
        QVERIFY2(r.value("reason").toString().contains(QStringLiteral("gescheitert")), qPrintable(r.value("reason").toString()));
        QVERIFY(!rig.session(target)->hasAgentInbox());   // nicht wieder versuchen
        QTRY_VERIFY(b->captured.endsWith("EINS\r"));       // freie Session: kam über die Zeile an

        // Ab hier gibt die Session diesen Testprozess als ihre Shell aus. Damit dort „ein
        // Programm läuft“ (sonst s. bareShellIsNeverTypedInto), bekommt er ein Kind.
        QProcess child;
        child.start(QStringLiteral("/bin/sleep"), {QStringLiteral("30")});
        QVERIFY(child.waitForStarted());
        b->pid = QCoreApplication::applicationPid();

        // (2) Am Socket lauscht ein Prozess, der NICHT zu dieser Session gehört: Der
        // Eingang gehört einem Kind, das mit dieser Shell nichts zu tun hat — geprüft wird
        // die Abstammung des Lauschers von der Session-Shell. Hier lauscht der Testprozess
        // selbst; als „Shell“ gilt für diesen Teil das Kind, von dem er NICHT abstammt.
        Inbox inbox;
        QVERIFY(inbox.server.listen(inbox.path));
        b->pid = child.processId();
        rig.session(target)->setAgentInbox(QStringLiteral("claude-code"), inbox.path, QStringLiteral("2.1.323"));
        QString text;
        rig.call(QStringLiteral("send_message"), {{"id", target}, {"text", "ZWEI"}}, isError, text);
        QTest::qWait(150);
        QVERIFY(inbox.received.isEmpty());               // kein Byte an den fremden Prozess
        QVERIFY(!rig.session(target)->hasAgentInbox());
        // (Das Kind hat selbst keine Kinder: Die „Shell“ ist nackt, also wird auch nicht
        // eingereiht — der Aufruf scheitert laut.)
        QVERIFY2(isError, qPrintable(text));
        QVERIFY2(text.contains(QStringLiteral("nicht zu dieser Session")), qPrintable(text));

        // (3) Zu lang für den Eingang: Warteschlange, die Gegenstelle bleibt gemeldet.
        b->pid = QCoreApplication::applicationPid();
        rig.session(target)->setAgentInbox(QStringLiteral("claude-code"), inbox.path, QStringLiteral("2.1.323"));
        r = rig.callJson(QStringLiteral("send_message"),
                         {{"id", target}, {"text", QString(200001, QLatin1Char('x'))}}, isError);
        QVERIFY(!isError);
        QCOMPARE(r.value("via").toString(), QStringLiteral("queue"));
        QVERIFY(rig.session(target)->hasAgentInbox());
        QVERIFY(inbox.received.isEmpty());
        child.kill();
        child.waitForFinished();
#endif
    }

    void bareShellIsNeverTypedInto() {
#if !defined(Q_OS_UNIX)
        QSKIP("Braucht einen Kindprozess nach Unix-Art.");
#else
        // Der Agent ist beendet, in der Session steht nur noch die Shell: Über die
        // Eingabezeile würde die Nachricht dort als BEFEHL ausgeführt, Zeile für Zeile.
        // Dann lieber gar nicht zustellen — und das laut sagen.
        Rig rig;
        CaptureBackend *b = nullptr;
        const int target = rig.add(&b);
        b->pid = QCoreApplication::applicationPid();     // „Shell“ ohne Kindprozess
        QTemporaryDir dir;
        rig.session(target)->setAgentInbox(QStringLiteral("claude-code"), dir.filePath("weg.sock"), QString());
        bool isError = false;
        QString text;
        rig.call(QStringLiteral("send_message"), {{"id", target}, {"text", "rm -rf nichts"}}, isError, text);
        QVERIFY(isError);
        QVERIFY2(text.contains(QStringLiteral("Befehl")), qPrintable(text));
        QTest::qWait(300);
        QVERIFY(b->captured.isEmpty());
        QCOMPARE(rig.session(target)->queuedCount(), 0);

        // Gegenprobe: Läuft dort ein Programm, wird eingereiht wie gehabt.
        QProcess child;
        child.start(QStringLiteral("/bin/sleep"), {QStringLiteral("30")});
        QVERIFY(child.waitForStarted());
        const QJsonObject r = rig.callJson(QStringLiteral("send_message"), {{"id", target}, {"text", "HALLO"}}, isError);
        QVERIFY(!isError);
        QCOMPARE(r.value("via").toString(), QStringLiteral("queue"));
        QTRY_VERIFY(b->captured.endsWith("HALLO\r"));
        child.kill();
        child.waitForFinished();
#endif
    }

    void setAgentInboxValidatesAndClears() {
        Rig rig;
        const int self = rig.add();
        rig.setCaller(self);
        bool isError = false;
        QString text;
        rig.call(QStringLiteral("set_agent_inbox"), {{"kind", "claude-code"}, {"address", "/etc/passwd"}}, isError, text);
        QVERIFY(isError);
        QVERIFY(!rig.session(self)->hasAgentInbox());
        rig.call(QStringLiteral("set_agent_inbox"), {{"kind", "unbekannt"}, {"address", "/tmp/a.sock"}}, isError, text);
        QVERIFY(isError);
        rig.call(QStringLiteral("set_agent_inbox"), {{"kind", "claude-code"}, {"address", "/tmp/cc-socks/1.sock"}}, isError, text);
        QVERIFY2(!isError, qPrintable(text));
        QCOMPARE(rig.session(self)->agentInboxAddress(), QStringLiteral("/tmp/cc-socks/1.sock"));
        rig.call(QStringLiteral("set_agent_inbox"), {{"kind", "claude-code"}, {"address", ""}}, isError, text);
        QVERIFY2(!isError, qPrintable(text));
        QVERIFY(!rig.session(self)->hasAgentInbox());
        // Ohne eigene Session gibt es nichts zu melden.
        rig.setCaller(-1);
        rig.call(QStringLiteral("set_agent_inbox"), {{"kind", "claude-code"}, {"address", "/tmp/a.sock"}}, isError, text);
        QVERIFY(isError);
    }

    void schemaDocumentsTheNewContract() {
        // Die Werkzeug-Texte sind die einzige Doku, die ein Agent sicher liest.
        Rig rig;
        QHash<QString, QJsonObject> tools;
        for (const QJsonValue &v : rig.mcp.toolsList().value("tools").toArray())
            tools.insert(v.toObject().value("name").toString(), v.toObject());
        QVERIFY(tools.contains(QStringLiteral("send_message")));
        QVERIFY(tools.contains(QStringLiteral("set_agent_inbox")));
        for (const QString &t : {QStringLiteral("send_text"), QStringLiteral("send_keys")}) {
            const QJsonObject props = tools.value(t).value("inputSchema").toObject().value("properties").toObject();
            QVERIFY2(props.contains(QStringLiteral("force")), qPrintable(t));
            QVERIFY2(props.value("force").toObject().value("description").toString().contains(QStringLiteral("NOTFALL")), qPrintable(t));
            QVERIFY2(tools.value(t).value("description").toString().contains(QStringLiteral("ownerTyping")), qPrintable(t));
        }
        // send_message nimmt KEINEN Absender entgegen.
        const QJsonObject mp = tools.value(QStringLiteral("send_message")).value("inputSchema").toObject().value("properties").toObject();
        QCOMPARE(mp.keys(), QStringList({QStringLiteral("id"), QStringLiteral("text")}));
        QVERIFY(tools.value(QStringLiteral("list_sessions")).value("description").toString().contains(QStringLiteral("ownerTyping")));
    }
};

} // namespace qtmux

QTEST_MAIN(qtmux::TestMcpMessage)
#include "tst_mcpmessage.moc"
