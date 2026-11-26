#include <QtTest>
#include <QEventLoop>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>
#include <QTcpServer>
#include <QTcpSocket>

#include "AgentEventHub.h"
#include "ITerminalBackend.h"
#include "McpHttp.h"
#include "McpServer.h"
#include "Session.h"
#include "SessionModel.h"
#include "TaskStore.h"
#include "qtmux_version.h"

// Der MCP-Server über die HTTP-Leitung (QTMUX-162): Kopfzeilen, Statuscodes und Rumpf je
// Fall, alter Weg und Revision 2026-07-28 nebeneinander, Tasks samt Posteingang.
//
// Der Client läuft im selben Prozess; wer anruft, setzt der Test über die Naht
// m_callerResolverForTest (im Betrieb ordnet QTmux den Client-Prozess seiner Session zu).
// Sessions: seriell auf einem nicht vorhandenen Port mit Mitschnitt-Backend, wie in
// tst_mcpmessage — kein Prozess, alle Plattformen.
namespace qtmux {

class HttpCaptureBackend final : public ITerminalBackend {
public:
    bool start(int, int) override { setState(BackendState::Running); return true; }
    void write(const QByteArray &d) override { captured += d; }
    void resize(int, int) override {}
    void terminate() override { setState(BackendState::Closed); }
    qint64 processId() const override { return -1; }
    QByteArray captured;
};

struct HttpReply {
    int status = 0;
    QByteArray head;
    QByteArray body;
    QJsonObject json() const { return QJsonDocument::fromJson(body).object(); }
    QJsonArray jsonArray() const { return QJsonDocument::fromJson(body).array(); }
    QByteArray header(const QByteArray &lowerName) const {
        for (const QByteArray &l : head.split('\n')) {
            const int c = l.indexOf(':');
            if (c > 0 && l.left(c).trimmed().toLower() == lowerName) return l.mid(c + 1).trimmed();
        }
        return {};
    }
};

class TestMcpHttp : public QObject {
    Q_OBJECT

    quint16 m_port = 0;
    qint64 m_now = 1'800'000'000'000LL;
    int m_caller = -1;
    bool m_fromNetwork = false;
    SessionModel *m_sessions = nullptr;
    McpServer *m_mcp = nullptr;
    QHash<int, HttpCaptureBackend *> m_backends;

    int addSession() {
        const int row = m_sessions->createSerialSession(QStringLiteral("qtmux-test-kein-port"), 9600);
        auto *s = static_cast<Session *>(m_sessions->sessionAt(row));
        auto *b = new HttpCaptureBackend;
        s->attachBackend(b, Session::Type::Shell, 80, 24);
        s->start(80, 24);
        s->requestActivity(QStringLiteral("idle"));
        m_backends.insert(s->id(), b);
        return s->id();
    }

    HttpReply raw(const QByteArray &request, int timeoutMs = 5000) {
        QTcpSocket sock;
        sock.connectToHost(QHostAddress::LocalHost, m_port);
        if (!sock.waitForConnected(2000)) return {};
        HttpReply r;
        QByteArray all;
        QEventLoop loop;
        QTimer guard;
        guard.setSingleShot(true);
        connect(&guard, &QTimer::timeout, &loop, &QEventLoop::quit);
        connect(&sock, &QTcpSocket::readyRead, &loop, [&] { all += sock.readAll(); });
        connect(&sock, &QTcpSocket::disconnected, &loop, &QEventLoop::quit);
        sock.write(request);
        guard.start(timeoutMs);
        loop.exec();
        all += sock.readAll();
        const int he = all.indexOf("\r\n\r\n");
        if (he < 0) return r;
        r.head = all.left(he);
        r.body = all.mid(he + 4);
        const QList<QByteArray> first = r.head.split('\n').first().split(' ');
        if (first.size() >= 2) r.status = first.at(1).toInt();
        return r;
    }

    static QByteArray httpPost(const QByteArray &path, const QByteArray &body,
                               const QList<QPair<QByteArray, QByteArray>> &headers = {}) {
        QByteArray req = "POST " + path + " HTTP/1.1\r\nHost: 127.0.0.1\r\nContent-Type: application/json\r\n"
                         "Accept: application/json, text/event-stream\r\n";
        for (const auto &h : headers) req += h.first + ": " + h.second + "\r\n";
        req += "Content-Length: " + QByteArray::number(body.size()) + "\r\n\r\n" + body;
        return req;
    }

    HttpReply legacy(const QString &method, const QJsonObject &params = {}, const QJsonValue &id = 1) {
        QJsonObject o{{"jsonrpc", "2.0"}, {"method", method}, {"params", params}};
        if (!id.isUndefined()) o.insert("id", id);
        return raw(httpPost("/mcp", QJsonDocument(o).toJson(QJsonDocument::Compact)));
    }

    static QJsonObject meta(bool tasks = true, const QString &version = QLatin1String(mcphttp::kModernVersion)) {
        QJsonObject ext;
        if (tasks) ext.insert(QLatin1String(mcphttp::kTasksExtension), QJsonObject{});
        return QJsonObject{
            {QLatin1String(mcphttp::kMetaProtocolVersion), version},
            {"io.modelcontextprotocol/clientInfo", QJsonObject{{"name", "test"}, {"version", "1"}}},
            {QLatin1String(mcphttp::kMetaClientCapabilities), QJsonObject{{"extensions", ext}}}};
    }

    // Anfrage der neuen Fassung, Kopfzeilen wie Claude Code sie setzt. `over` ersetzt
    // bzw. entfernt (leerer Wert) einzelne Kopfzeilen.
    HttpReply modern(const QString &method, QJsonObject params = {}, bool tasks = true,
                     const QHash<QByteArray, QByteArray> &over = {}, const QJsonValue &id = 7) {
        params.insert("_meta", meta(tasks));
        QHash<QByteArray, QByteArray> h{{"MCP-Protocol-Version", mcphttp::kModernVersion},
                                        {"Mcp-Method", method.toUtf8()}};
        if (method == QLatin1String("tools/call")) h.insert("Mcp-Name", params.value("name").toString().toUtf8());
        if (method.startsWith(QLatin1String("tasks/")))
            h.insert("Mcp-Name", params.value("taskId").toString().toUtf8());
        for (auto it = over.constBegin(); it != over.constEnd(); ++it) h.insert(it.key(), it.value());
        QList<QPair<QByteArray, QByteArray>> list;
        for (auto it = h.constBegin(); it != h.constEnd(); ++it)
            if (!it.value().isEmpty()) list.append({it.key(), it.value()});
        QJsonObject o{{"jsonrpc", "2.0"}, {"method", method}, {"params", params}};
        if (!id.isUndefined()) o.insert("id", id);
        return raw(httpPost("/mcp", QJsonDocument(o).toJson(QJsonDocument::Compact), list));
    }

    HttpReply modernCall(const QString &tool, const QJsonObject &args = {}, bool tasks = true) {
        return modern(QStringLiteral("tools/call"), QJsonObject{{"name", tool}, {"arguments", args}}, tasks);
    }
    HttpReply taskGet(const QString &id, const QString &m = QStringLiteral("tasks/get")) {
        return modern(m, QJsonObject{{"taskId", id}});
    }
    static QString toolText(const HttpReply &r) {
        return r.json().value("result").toObject().value("content").toArray().first().toObject()
            .value("text").toString();
    }
    static QJsonObject toolJson(const HttpReply &r) {
        return QJsonDocument::fromJson(toolText(r).toUtf8()).object();
    }
    // Werkzeug über den alten Weg aufrufen (Absender = aktueller m_caller).
    QJsonObject legacyTool(const QString &name, const QJsonObject &args) {
        return toolJson(legacy(QStringLiteral("tools/call"), QJsonObject{{"name", name}, {"arguments", args}}));
    }

    static QByteArray readFile(const QString &name) {
        QFile f(QFINDTESTDATA(QStringLiteral("data/mcp/") + name));
        if (!f.open(QIODevice::ReadOnly)) return {};
        return f.readAll();
    }

private slots:
    void initTestCase() {
        QCoreApplication::setOrganizationName(QStringLiteral("QTmux"));
        QCoreApplication::setApplicationName(QStringLiteral("QTmuxMcpHttpTest"));
        qunsetenv("QTMUX_MCP_BIND");
        qunsetenv("QTMUX_MCP_TOKEN");
    }

    void init() {
        QSettings().clear();
        // Freien Port bestimmen und per Umgebung setzen (setPort schriebe die Einstellung).
        QTcpServer probe;
        QVERIFY(probe.listen(QHostAddress::LocalHost, 0));
        m_port = probe.serverPort();
        probe.close();
        qputenv("QTMUX_MCP_PORT", QByteArray::number(m_port));
        m_now = 1'800'000'000'000LL;
        m_caller = -1;
        m_sessions = new SessionModel;
        m_mcp = new McpServer;
        m_mcp->setSessions(m_sessions);
        m_mcp->m_callerResolverForTest = [this](const QTcpSocket *) { return m_caller; };
        m_mcp->m_ownerCheckForTest = [this](const QTcpSocket *, int owner) { return owner == m_caller; };
        m_mcp->m_peerIsNetworkForTest = [this](const QTcpSocket *) { return m_fromNetwork; };
        m_mcp->m_taskStore = TaskStore(TaskStore::Limits{}, [this] { return m_now; });
        m_fromNetwork = false;
        QVERIFY(m_mcp->start());
        QCOMPARE(m_mcp->port(), int(m_port));
    }

    void cleanup() {
        delete m_mcp;
        m_mcp = nullptr;
        delete m_sessions;
        m_sessions = nullptr;
        m_backends.clear();
    }

    // --- alter Weg: unverändert -------------------------------------------------

    void legacyInitializeIsByteIdentical() {
        QByteArray expected = readFile(QStringLiteral("legacy-initialize-61ddc5a.json"));
        QVERIFY(!expected.isEmpty());
        expected.replace("\"1.10.1\"", "\"" QTMUX_VERSION_STRING "\"");
        const HttpReply r = raw(httpPost("/mcp", R"({"jsonrpc":"2.0","id":1,"method":"initialize","params":{}})"));
        QCOMPARE(r.status, 200);
        QCOMPARE(r.body, expected);
    }

    // Abnahmekriterium 2: Werkzeugliste des alten Wegs gleich dem Stand 1.10.1, bis auf
    // die ausdrücklich begründeten Zusätze.
    void legacyToolsListOnlyJustifiedChanges() {
        const QJsonArray before = QJsonDocument::fromJson(readFile(QStringLiteral("legacy-tools-list-61ddc5a.json")))
                                      .object().value("result").toObject().value("tools").toArray();
        QVERIFY(before.size() > 30);
        const HttpReply r = legacy(QStringLiteral("tools/list"));
        QCOMPARE(r.status, 200);
        const QJsonArray after = r.json().value("result").toObject().value("tools").toArray();
        // Begründet: message_status (Zustellbeleg, auch für alte Absender) am Ende,
        // send_message mit neuer Beschreibung (Rangfolge). Sonst nichts.
        QCOMPARE(after.size(), before.size() + 1);
        for (int i = 0; i < before.size(); ++i) {
            QJsonObject b = before.at(i).toObject();
            QJsonObject a = after.at(i).toObject();
            QCOMPARE(a.value("name").toString(), b.value("name").toString());
            if (b.value("name").toString() == QLatin1String("send_message")) {
                QVERIFY(a.value("description").toString() != b.value("description").toString());
                a.remove("description");
                b.remove("description");
            }
            QVERIFY2(a == b, qPrintable(b.value("name").toString()));
        }
        QCOMPARE(after.last().toObject().value("name").toString(), QStringLiteral("message_status"));
        for (const QJsonValue &t : after)
            QVERIFY(!t.toObject().value("name").toString().startsWith(QLatin1String("await_")));
    }

    void legacyBatchNotificationUnknownMethod() {
        HttpReply r = raw(httpPost("/mcp", R"([{"jsonrpc":"2.0","method":"notifications/initialized"},{"jsonrpc":"2.0","id":5,"method":"ping"}])"));
        QCOMPARE(r.status, 200);
        QCOMPARE(r.jsonArray().size(), 1);
        QCOMPARE(r.jsonArray().first().toObject().value("id").toInt(), 5);
        r = raw(httpPost("/mcp", R"({"jsonrpc":"2.0","method":"notifications/initialized"})"));
        QCOMPARE(r.status, 202);
        QVERIFY(r.body.isEmpty());
        // Unbekannte Methode auf dem alten Weg: weiterhin HTTP 200 (daran erkennt Claude
        // Code einen alten Server und fällt auf initialize zurück).
        r = raw(httpPost("/mcp", R"({"jsonrpc":"2.0","id":2,"method":"server/discover"})"));
        QCOMPARE(r.status, 200);
        QCOMPARE(r.json().value("error").toObject().value("code").toInt(), -32601);
    }

    void legacyLongPollOverTheWire() {
        const int a = addSession();
        const int b = addSession();
        m_caller = a;
        QVERIFY(legacyTool(QStringLiteral("subscribe_events"), {}).value("ok").toBool());
        // Ein Ereignis während des Wartens beendet den Long-Poll.
        QTimer::singleShot(200, this, [b] {
            AgentEventHub::instance()->postEvent(b, AgentEventHub::Kind::Done, QStringLiteral("fertig"));
        });
        QElapsedTimer t;
        t.start();
        const HttpReply r = legacy(QStringLiteral("tools/call"),
                                   QJsonObject{{"name", "wait_for_events"},
                                               {"arguments", QJsonObject{{"timeoutMs", 5000}}}});
        QVERIFY(t.elapsed() < 4000);
        QCOMPARE(r.status, 200);
        const QJsonArray evs = toolJson(r).value("events").toArray();
        QCOMPARE(evs.size(), 1);
        QCOMPARE(evs.first().toObject().value("sourceSessionId").toInt(), b);
        QVERIFY(!r.json().value("result").toObject().contains("resultType"));
    }

    // --- HTTP-Ebene ---------------------------------------------------------------

    void getOnMcpPathIs405StatusStaysOnRoot() {
        HttpReply r = raw("GET /mcp HTTP/1.1\r\nHost: x\r\nAccept: text/event-stream\r\n\r\n");
        QCOMPARE(r.status, 405);
        QCOMPARE(r.header("allow"), QByteArray("POST"));
        r = raw("GET /mcp HTTP/1.1\r\nHost: x\r\nAccept: */*\r\n\r\n");
        QCOMPARE(r.status, 405);
        r = raw("GET / HTTP/1.1\r\nHost: x\r\nAccept: text/event-stream\r\n\r\n");
        QCOMPARE(r.status, 405);
        r = raw("GET / HTTP/1.1\r\nHost: x\r\n\r\n");
        QCOMPARE(r.status, 200);
        QCOMPARE(r.json().value("server").toString(), QStringLiteral("QTmux MCP"));
        r = raw("DELETE /mcp HTTP/1.1\r\nHost: x\r\n\r\n");
        QCOMPARE(r.status, 405);
    }

    void postOnlyOnMcpPathAndRoot() {
        const QByteArray init = R"({"jsonrpc":"2.0","id":1,"method":"initialize","params":{}})";
        QCOMPARE(raw(httpPost("/mcp", init)).status, 200);
        QCOMPARE(raw(httpPost("/mcp/", init)).status, 200);
        QCOMPARE(raw(httpPost("/", init)).status, 200);
        QCOMPARE(raw(httpPost("/mcp?x=1", init)).status, 200);
        QCOMPARE(raw(httpPost("/other", init)).status, 404);
        QCOMPARE(raw(httpPost("/mcpx", init)).status, 404);
    }

    void originIsChecked() {
        const QByteArray init = R"({"jsonrpc":"2.0","id":1,"method":"initialize","params":{}})";
        QCOMPARE(raw(httpPost("/mcp", init, {{"Origin", "http://evil.example"}})).status, 403);
        QCOMPARE(raw(httpPost("/mcp", init, {{"Origin", "null"}})).status, 403);
        QCOMPARE(raw(httpPost("/mcp", init, {{"Origin", "http://localhost.evil.example"}})).status, 403);
        QCOMPARE(raw(httpPost("/mcp", init, {{"Origin", "http://localhost:6274"}})).status, 200);
        QCOMPARE(raw(httpPost("/mcp", init, {{"Origin", "http://127.0.0.1"}})).status, 200);
        QCOMPARE(raw("GET / HTTP/1.1\r\nHost: x\r\nOrigin: https://evil.example\r\n\r\n").status, 403);
    }

    void originRuleUnit() {
        QVERIFY(mcphttp::originAllowed({}));
        QVERIFY(mcphttp::originAllowed("http://[::1]:8080"));
        QVERIFY(mcphttp::originAllowed("https://localhost"));
        QVERIFY(!mcphttp::originAllowed("file://"));
        QVERIFY(!mcphttp::originAllowed("http://localhost/pfad"));
        QVERIFY(!mcphttp::originAllowed("http://user@localhost"));
        QVERIFY(!mcphttp::originAllowed("http://127.0.0.1.nip.io"));
        QVERIFY(!mcphttp::originAllowed("http://loc\x01" "alhost"));
    }

    void headerLimits() {
        QByteArray big = "POST /mcp HTTP/1.1\r\nX-Big: " + QByteArray(17 * 1024, 'a') + "\r\n\r\n";
        QCOMPARE(raw(big).status, 431);
        // Kopfteil ohne Ende, der weiter wächst: ebenfalls 431 statt endlos zu puffern.
        QCOMPARE(raw("POST /mcp HTTP/1.1\r\nX: " + QByteArray(20 * 1024, 'b')).status, 431);
        QByteArray many = "POST /mcp HTTP/1.1\r\n";
        for (int i = 0; i < 70; ++i) many += "X-" + QByteArray::number(i) + ": 1\r\n";
        QCOMPARE(raw(many + "\r\n").status, 431);
        QCOMPARE(raw("POST /mcp HTTP/1.1\r\nX: a\x01" "b\r\nContent-Length: 0\r\n\r\n").status, 400);
        QCOMPARE(raw("POST /mcp HTTP/1.1\r\nContent-Length: 2\r\nContent-Length: 2\r\n\r\n{}").status, 400);
        QCOMPARE(raw("POST /mcp HTTP/1.1\r\nContent-Length: -1\r\n\r\n").status, 400);
        QCOMPARE(raw("POST /mcp HTTP/1.1\r\nContent-Length: 99999999\r\n\r\n").status, 413);
        QCOMPARE(raw("POST /mcp HTTP/1.1\r\nTransfer-Encoding: chunked\r\n\r\n").status, 501);
        QCOMPARE(raw("POST mcp HTTP/1.1\r\n\r\n").status, 400);
        QCOMPARE(raw("PO ST /mcp HTTP/1.1\r\n\r\n").status, 400);
    }

    // --- Revision 2026-07-28 --------------------------------------------------------

    void modernDiscover() {
        const HttpReply r = modern(QStringLiteral("server/discover"));
        QCOMPARE(r.status, 200);
        const QJsonObject res = r.json().value("result").toObject();
        QCOMPARE(res.value("supportedVersions").toArray(), QJsonArray{QStringLiteral("2026-07-28")});
        QVERIFY(res.value("capabilities").toObject().value("extensions").toObject()
                    .contains(QLatin1String(mcphttp::kTasksExtension)));
        QVERIFY(res.value("capabilities").toObject().contains("tools"));
        QCOMPARE(res.value("resultType").toString(), QStringLiteral("complete"));
        QVERIFY(res.value("ttlMs").toDouble() >= 0);
        QCOMPARE(res.value("cacheScope").toString(), QStringLiteral("public"));
        QCOMPARE(res.value("_meta").toObject().value(QLatin1String(mcphttp::kMetaServerInfo)).toObject()
                     .value("name").toString(), QStringLiteral("QTmux"));
    }

    void modernToolsListHasTaskTools() {
        const HttpReply r = modern(QStringLiteral("tools/list"));
        QCOMPARE(r.status, 200);
        const QJsonObject res = r.json().value("result").toObject();
        QStringList names;
        for (const QJsonValue &t : res.value("tools").toArray()) names << t.toObject().value("name").toString();
        QVERIFY(names.contains("await_events"));
        QVERIFY(names.contains("await_message"));
        QVERIFY(names.contains("message_status"));
        QCOMPARE(res.value("resultType").toString(), QStringLiteral("complete"));
        QCOMPARE(res.value("cacheScope").toString(), QStringLiteral("public"));
        QVERIFY(res.contains("ttlMs"));
        QCOMPARE(modern(QStringLiteral("tools/list"), QJsonObject{{"cursor", "x"}}).json()
                     .value("error").toObject().value("code").toInt(), -32602);
    }

    void modernPlainToolCall() {
        const HttpReply r = modernCall(QStringLiteral("get_server_info"));
        QCOMPARE(r.status, 200);
        const QJsonObject res = r.json().value("result").toObject();
        QCOMPARE(res.value("resultType").toString(), QStringLiteral("complete"));
        QVERIFY(!res.value("isError").toBool());
        QVERIFY(res.value("_meta").toObject().contains(QLatin1String(mcphttp::kMetaServerInfo)));
        QCOMPARE(toolJson(r).value("server").toString(), QStringLiteral("QTmux MCP"));
    }

    void modernErrors_data() {
        QTest::addColumn<QByteArray>("hdrName");
        QTest::addColumn<QByteArray>("hdrValue");
        QTest::addColumn<int>("status");
        QTest::addColumn<int>("code");
        QTest::newRow("version header missing") << QByteArray("MCP-Protocol-Version") << QByteArray() << 400 << -32020;
        QTest::newRow("version header differs") << QByteArray("MCP-Protocol-Version") << QByteArray("2026-08-01") << 400 << -32020;
        QTest::newRow("method header missing") << QByteArray("Mcp-Method") << QByteArray() << 400 << -32020;
        QTest::newRow("method header differs") << QByteArray("Mcp-Method") << QByteArray("tools/list") << 400 << -32020;
        QTest::newRow("name header missing") << QByteArray("Mcp-Name") << QByteArray() << 400 << -32020;
        QTest::newRow("name header differs") << QByteArray("Mcp-Name") << QByteArray("send_text") << 400 << -32020;
        QTest::newRow("name header bad base64") << QByteArray("Mcp-Name") << QByteArray("=?base64?***?=") << 400 << -32020;
        QTest::newRow("name header base64 ok") << QByteArray("Mcp-Name") << QByteArray("=?base64?Z2V0X3NlcnZlcl9pbmZv?=") << 200 << 0;
    }
    void modernErrors() {
        QFETCH(QByteArray, hdrName);
        QFETCH(QByteArray, hdrValue);
        QFETCH(int, status);
        QFETCH(int, code);
        const HttpReply r = modern(QStringLiteral("tools/call"),
                                   QJsonObject{{"name", "get_server_info"}, {"arguments", QJsonObject{}}}, true,
                                   {{hdrName, hdrValue}});
        QCOMPARE(r.status, status);
        QCOMPARE(r.json().value("error").toObject().value("code").toInt(), code);
        if (code) QCOMPARE(r.json().value("id").toInt(), 7);
    }

    void modernUnsupportedVersion() {
        QJsonObject params{{"_meta", meta(true, QStringLiteral("2027-01-01"))}};
        const QJsonObject o{{"jsonrpc", "2.0"}, {"id", 3}, {"method", "server/discover"}, {"params", params}};
        const HttpReply r = raw(httpPost("/mcp", QJsonDocument(o).toJson(QJsonDocument::Compact),
                                         {{"MCP-Protocol-Version", "2027-01-01"}, {"Mcp-Method", "server/discover"}}));
        QCOMPARE(r.status, 400);
        const QJsonObject err = r.json().value("error").toObject();
        QCOMPARE(err.value("code").toInt(), -32022);
        QCOMPARE(err.value("data").toObject().value("supported").toArray(), QJsonArray{QStringLiteral("2026-07-28")});
        QCOMPARE(err.value("data").toObject().value("requested").toString(), QStringLiteral("2027-01-01"));
    }

    void modernBatchUnknownMethodNotification() {
        const QByteArray batch = "[" + QJsonDocument(QJsonObject{{"jsonrpc", "2.0"}, {"id", 1}, {"method", "tools/list"},
                                                                 {"params", QJsonObject{{"_meta", meta()}}}})
                                           .toJson(QJsonDocument::Compact) + "]";
        HttpReply r = raw(httpPost("/mcp", batch, {{"MCP-Protocol-Version", "2026-07-28"}, {"Mcp-Method", "tools/list"}}));
        QCOMPARE(r.status, 400);
        QCOMPARE(r.json().value("error").toObject().value("code").toInt(), -32600);
        // Die eigene Meldung, nicht nur der Code: Fehlte die Stapel-Prüfung, finge die
        // allgemeine „kein Objekt“-Prüfung den Fall mit demselben Code ab (Mutationsprobe).
        QVERIFY(r.json().value("error").toObject().value("message").toString().contains(QStringLiteral("Stapel")));
        r = modern(QStringLiteral("ping"));
        QCOMPARE(r.status, 404);
        QCOMPARE(r.json().value("error").toObject().value("code").toInt(), -32601);
        r = modern(QStringLiteral("subscriptions/listen"));
        QCOMPARE(r.status, 404);
        r = modern(QStringLiteral("notifications/whatever"), {}, true, {}, QJsonValue::Undefined);
        QCOMPARE(r.status, 202);
    }

    // --- Tasks -------------------------------------------------------------------

    void taskToolsNeedTheExtension() {
        const int a = addSession();
        m_caller = a;
        HttpReply r = modernCall(QStringLiteral("await_message"), {}, /*tasks*/ false);
        QCOMPARE(r.status, 400);
        QJsonObject err = r.json().value("error").toObject();
        QCOMPARE(err.value("code").toInt(), -32021);
        QVERIFY(err.value("data").toObject().value("requiredCapabilities").toObject().value("extensions")
                    .toObject().contains(QLatin1String(mcphttp::kTasksExtension)));
        r = modern(QStringLiteral("tasks/get"), QJsonObject{{"taskId", "qt-00"}}, false);
        QCOMPARE(r.status, 400);
        QCOMPARE(r.json().value("error").toObject().value("code").toInt(), -32021);
        // Der alte Weg kennt sie nicht und sagt das als Werkzeugfehler.
        r = legacy(QStringLiteral("tools/call"), QJsonObject{{"name", "await_message"}, {"arguments", QJsonObject{}}});
        QVERIFY(r.json().value("result").toObject().value("isError").toBool());
    }

    void inboxTaskDeliversMessageWithSender() {
        const int a = addSession();
        const int b = addSession();
        m_caller = a;
        HttpReply r = modernCall(QStringLiteral("await_message"));
        QCOMPARE(r.status, 200);
        QJsonObject task = r.json().value("result").toObject();
        QCOMPARE(task.value("resultType").toString(), QStringLiteral("task"));
        QCOMPARE(task.value("status").toString(), QStringLiteral("working"));
        QVERIFY(task.value("ttlMs").isNull());
        QCOMPARE(task.value("pollIntervalMs").toInt(), 2000);
        QVERIFY(!task.value("statusMessage").toString().isEmpty());
        const QString tid = task.value("taskId").toString();
        QVERIFY(TaskStore::isWellFormedId(tid));

        r = taskGet(tid);
        QCOMPARE(r.json().value("result").toObject().value("status").toString(), QStringLiteral("working"));
        QCOMPARE(r.json().value("result").toObject().value("resultType").toString(), QStringLiteral("complete"));

        m_caller = b;
        const QJsonObject sent = legacyTool(QStringLiteral("send_message"), {{"id", a}, {"text", "Hallo A"}});
        QCOMPARE(sent.value("via").toString(), QStringLiteral("task"));
        QCOMPARE(sent.value("status").toString(), QStringLiteral("completed"));
        const QString mid = sent.value("messageId").toString();
        QVERIFY(!mid.isEmpty());
        QVERIFY(m_backends.value(a)->captured.isEmpty());   // nicht über die Eingabezeile
        QCOMPARE(legacyTool(QStringLiteral("message_status"), {{"messageId", mid}}).value("state").toString(),
                 QStringLiteral("completed"));

        m_caller = a;
        r = taskGet(tid);
        const QJsonObject done = r.json().value("result").toObject();
        QCOMPARE(done.value("status").toString(), QStringLiteral("completed"));
        const QString text = done.value("result").toObject().value("content").toArray().first().toObject()
                                 .value("text").toString();
        QVERIFY(text.contains(QStringLiteral("Hallo A")));
        QVERIFY(text.contains(QStringLiteral("QTmux-Session #%1").arg(b)));
        QVERIFY(text.contains(QStringLiteral("send_message, id %1").arg(b)));
        QVERIFY(!done.value("ttlMs").isNull());

        m_caller = b;
        QCOMPARE(legacyTool(QStringLiteral("message_status"), {{"messageId", mid}}).value("state").toString(),
                 QStringLiteral("fetched"));
        // Nur der Absender bekommt Auskunft.
        m_caller = a;
        QCOMPARE(legacyTool(QStringLiteral("message_status"), {{"messageId", mid}}).value("state").toString(),
                 QStringLiteral("unknown"));
    }

    void foreignOrUnknownTaskIsNotFound() {
        const int a = addSession();
        const int b = addSession();
        m_caller = a;
        const QString tid = modernCall(QStringLiteral("await_message")).json().value("result").toObject()
                                .value("taskId").toString();
        m_caller = b;
        for (const QString &m : {QStringLiteral("tasks/get"), QStringLiteral("tasks/cancel"), QStringLiteral("tasks/update")}) {
            const HttpReply r = taskGet(tid, m);
            QCOMPARE(r.json().value("error").toObject().value("code").toInt(), -32602);
        }
        m_caller = a;
        QCOMPARE(taskGet(tid).json().value("result").toObject().value("status").toString(), QStringLiteral("working"));
        for (const QString &bad : {QStringLiteral("qt-0000"), QStringLiteral("../x"),
                                   QString(5000, QLatin1Char('a'))})
            QCOMPARE(taskGet(bad).json().value("error").toObject().value("code").toInt(), -32602);
        // Leere Kennung: Die Pflicht-Kopfzeile Mcp-Name fehlt dann schon (-32020).
        QCOMPARE(taskGet(QString()).json().value("error").toObject().value("code").toInt(), -32020);
    }

    void restartForgetsTasks() {
        const int a = addSession();
        m_caller = a;
        const QString tid = modernCall(QStringLiteral("await_message")).json().value("result").toObject()
                                .value("taskId").toString();
        QVERIFY(!tid.isEmpty());
        // „Neustart“: ein frischer Server ohne Speicher.
        m_mcp->stop();
        delete m_mcp;
        m_mcp = new McpServer;
        m_mcp->setSessions(m_sessions);
        m_mcp->m_callerResolverForTest = [this](const QTcpSocket *) { return m_caller; };
        m_mcp->m_ownerCheckForTest = [this](const QTcpSocket *, int owner) { return owner == m_caller; };
        QVERIFY(m_mcp->start());
        const HttpReply r = taskGet(tid);
        QCOMPARE(r.status, 200);
        QCOMPARE(r.json().value("error").toObject().value("code").toInt(), -32602);
    }

    void cancelAndUpdate() {
        const int a = addSession();
        m_caller = a;
        const QString tid = modernCall(QStringLiteral("await_message")).json().value("result").toObject()
                                .value("taskId").toString();
        HttpReply r = modern(QStringLiteral("tasks/update"), QJsonObject{{"taskId", tid}, {"inputResponses", QJsonObject{{"k", 1}}}});
        QCOMPARE(r.json().value("result").toObject().value("resultType").toString(), QStringLiteral("complete"));
        QCOMPARE(taskGet(tid).json().value("result").toObject().value("status").toString(), QStringLiteral("working"));
        r = taskGet(tid, QStringLiteral("tasks/cancel"));
        QCOMPARE(r.status, 200);
        QCOMPARE(r.json().value("result").toObject().value("resultType").toString(), QStringLiteral("complete"));
        QCOMPARE(taskGet(tid).json().value("result").toObject().value("status").toString(), QStringLiteral("cancelled"));
        // Abbrechen eines beendeten Tasks: Quittung, Zustand bleibt.
        QCOMPARE(taskGet(tid, QStringLiteral("tasks/cancel")).status, 200);
    }

    void bufferKeepsOrderWhileInboxIsRearmed() {
        const int a = addSession();
        const int b = addSession();
        m_caller = a;
        const QString t1 = modernCall(QStringLiteral("await_message")).json().value("result").toObject()
                               .value("taskId").toString();
        m_caller = b;
        QCOMPARE(legacyTool(QStringLiteral("send_message"), {{"id", a}, {"text", "eins"}}).value("via").toString(),
                 QStringLiteral("task"));
        m_caller = a;
        taskGet(t1);   // abgeholt; das Modell stellt den Posteingang erst gleich neu
        m_now += 5000;
        m_caller = b;
        QJsonObject s2 = legacyTool(QStringLiteral("send_message"), {{"id", a}, {"text", "zwei"}});
        QJsonObject s3 = legacyTool(QStringLiteral("send_message"), {{"id", a}, {"text", "drei"}});
        QCOMPARE(s2.value("via").toString(), QStringLiteral("task-buffer"));
        QCOMPARE(s3.value("via").toString(), QStringLiteral("task-buffer"));
        QCOMPARE(legacyTool(QStringLiteral("message_status"), {{"messageId", s2.value("messageId")}}).value("state").toString(),
                 QStringLiteral("buffered"));
        m_caller = a;
        const QString t2 = modernCall(QStringLiteral("await_message")).json().value("result").toObject()
                               .value("taskId").toString();
        const QJsonArray content = taskGet(t2).json().value("result").toObject().value("result").toObject()
                                       .value("content").toArray();
        QCOMPARE(content.size(), 2);
        QVERIFY(content.at(0).toObject().value("text").toString().contains("zwei"));
        QVERIFY(content.at(1).toObject().value("text").toString().contains("drei"));
        QVERIFY(m_backends.value(a)->captured.isEmpty());
    }

    void bufferFallsBackInOrderAfterGrace() {
        const int a = addSession();
        const int b = addSession();
        m_caller = a;
        const QString t1 = modernCall(QStringLiteral("await_message")).json().value("result").toObject()
                               .value("taskId").toString();
        m_caller = b;
        legacyTool(QStringLiteral("send_message"), {{"id", a}, {"text", "eins"}});
        m_caller = a;
        taskGet(t1);
        m_caller = b;
        const QString m2 = legacyTool(QStringLiteral("send_message"), {{"id", a}, {"text", "zwei"}}).value("messageId").toString();
        legacyTool(QStringLiteral("send_message"), {{"id", a}, {"text", "drei"}});
        m_now += 61 * 1000;
        m_mcp->onTaskTimer();
        const QByteArray got = m_backends.value(a)->captured;
        QVERIFY2(got.contains("zwei") && got.contains("drei"), got.constData());
        QVERIFY(got.indexOf("zwei") < got.indexOf("drei"));
        const QJsonObject st = legacyTool(QStringLiteral("message_status"), {{"messageId", m2}});
        QCOMPARE(st.value("state").toString(), QStringLiteral("fell-back"));
        QCOMPARE(st.value("via").toString(), QStringLiteral("queue"));
    }

    void withoutInboxTheOldPathIsTaken() {
        const int a = addSession();
        const int b = addSession();
        m_caller = b;
        const QJsonObject s = legacyTool(QStringLiteral("send_message"), {{"id", a}, {"text", "alt"}});
        QCOMPARE(s.value("via").toString(), QStringLiteral("queue"));
        QVERIFY(!s.value("messageId").toString().isEmpty());
        QVERIFY(m_backends.value(a)->captured.contains("alt"));
        QCOMPARE(legacyTool(QStringLiteral("message_status"), {{"messageId", s.value("messageId")}}).value("state").toString(),
                 QStringLiteral("direct"));
    }

    void staleInboxIsNotUsed() {
        const int a = addSession();
        const int b = addSession();
        m_caller = a;
        modernCall(QStringLiteral("await_message"));
        // Client fragt nicht mehr (z. B. nach /exit): nach 30 s kein lebender Task mehr,
        // nach 60 s auch kein Puffer mehr — dann der alte Weg.
        m_now += 61 * 1000;
        m_caller = b;
        const QJsonObject s = legacyTool(QStringLiteral("send_message"), {{"id", a}, {"text", "spät"}});
        QCOMPARE(s.value("via").toString(), QStringLiteral("queue"));
    }

    void inboxNeedsAnOwnSession() {
        const int a = addSession();
        m_caller = -1;   // lokaler Prozess außerhalb jeder Session
        HttpReply r = modernCall(QStringLiteral("await_message"), {{"sessionId", a}});
        QCOMPARE(r.status, 200);
        QVERIFY(r.json().value("result").toObject().value("isError").toBool());
        QVERIFY(r.json().value("result").toObject().value("resultType").toString() != QLatin1String("task"));
    }

    void awaitEventsLifecycle() {
        const int a = addSession();
        const int b = addSession();
        m_caller = a;
        HttpReply r = modernCall(QStringLiteral("await_events"));
        QVERIFY(r.json().value("result").toObject().value("isError").toBool());   // ohne Abo
        QVERIFY(toolText(r).contains(QStringLiteral("subscribe_events")));
        QVERIFY(legacyTool(QStringLiteral("subscribe_events"), {}).value("ok").toBool());
        r = modernCall(QStringLiteral("await_events"));
        const QJsonObject task = r.json().value("result").toObject();
        QCOMPARE(task.value("resultType").toString(), QStringLiteral("task"));
        const QString tid = task.value("taskId").toString();
        QCOMPARE(taskGet(tid).json().value("result").toObject().value("status").toString(), QStringLiteral("working"));
        AgentEventHub::instance()->postEvent(b, AgentEventHub::Kind::Question, QStringLiteral("Frage"));
        const QJsonObject done = taskGet(tid).json().value("result").toObject();
        QCOMPARE(done.value("status").toString(), QStringLiteral("completed"));
        const QJsonObject res = QJsonDocument::fromJson(done.value("result").toObject().value("content").toArray()
                                                            .first().toObject().value("text").toString().toUtf8()).object();
        QCOMPARE(res.value("events").toArray().size(), 1);
        QCOMPARE(res.value("events").toArray().first().toObject().value("kind").toString(), QStringLiteral("question"));
        QVERIFY(res.value("nextSeq").toDouble() > 0);
        AgentEventHub::instance()->unsubscribe(a);
    }

    // --- Befunde der Durchsicht 2026-11-26 -----------------------------------------

    // Befund 2: Ein Posteingang, den nach dem Abschluss niemand abholt (Client weg), gibt
    // seine Nachricht nach 3 Takten an den Rückfall; holt der Client später doch ab, steht
    // dort nur der Hinweis, kein zweites Exemplar.
    void unfetchedInboxFallsBack() {
        const int a = addSession();
        const int b = addSession();
        m_caller = a;
        const QString tid = modernCall(QStringLiteral("await_message")).json().value("result").toObject()
                                .value("taskId").toString();
        m_caller = b;
        const QString mid = legacyTool(QStringLiteral("send_message"), {{"id", a}, {"text", "verwaist"}})
                                .value("messageId").toString();
        m_now += 6001;
        m_mcp->onTaskTimer();
        QVERIFY(m_backends.value(a)->captured.contains("verwaist"));
        const QJsonObject st = legacyTool(QStringLiteral("message_status"), {{"messageId", mid}});
        QCOMPARE(st.value("state").toString(), QStringLiteral("fell-back"));
        m_caller = a;
        const QString text = taskGet(tid).json().value("result").toObject().value("result").toObject()
                                 .value("content").toArray().first().toObject().value("text").toString();
        QVERIFY(!text.contains(QStringLiteral("verwaist")));
        QVERIFY(text.contains(QStringLiteral("queue")));
    }

    // Befund 3 über die Leitung: tasks/cancel ist kein Zustellbeleg.
    void cancelIsNoReceiptOverTheWire() {
        const int a = addSession();
        const int b = addSession();
        m_caller = a;
        const QString tid = modernCall(QStringLiteral("await_message")).json().value("result").toObject()
                                .value("taskId").toString();
        m_caller = b;
        const QString mid = legacyTool(QStringLiteral("send_message"), {{"id", a}, {"text", "x"}})
                                .value("messageId").toString();
        m_caller = a;
        taskGet(tid, QStringLiteral("tasks/cancel"));
        m_caller = b;
        QCOMPARE(legacyTool(QStringLiteral("message_status"), {{"messageId", mid}}).value("state").toString(),
                 QStringLiteral("completed"));
    }

    // Befund 4: Eine Verbindung trägt genau eine Anfrage (der Server antwortet mit
    // Connection: close). Weitere Bytes auf derselben Verbindung werden nicht bearbeitet.
    void oneRequestPerConnection() {
        const int a = addSession();
        m_caller = a;
        legacyTool(QStringLiteral("subscribe_events"), {});
        const QByteArray one = httpPost("/mcp", R"({"jsonrpc":"2.0","id":1,"method":"tools/call","params":{"name":"wait_for_events","arguments":{"timeoutMs":1000}}})");
        const QByteArray two = httpPost("/mcp", R"({"jsonrpc":"2.0","id":2,"method":"tools/call","params":{"name":"wait_for_events","arguments":{"timeoutMs":1000}}})");
        QTcpSocket sock;
        sock.connectToHost(QHostAddress::LocalHost, m_port);
        QVERIFY(sock.waitForConnected(2000));
        sock.write(one);
        sock.flush();
        QTest::qWait(100);
        sock.write(two);
        sock.flush();
        QTest::qWait(100);
        QCOMPARE(m_mcp->m_pendingPolls.size(), 1);
        AgentEventHub::instance()->postEvent(addSession(), AgentEventHub::Kind::Done, QStringLiteral("x"));
        QTRY_VERIFY_WITH_TIMEOUT(sock.state() != QAbstractSocket::ConnectedState, 3000);
        QCOMPARE(m_mcp->m_pendingPolls.size(), 0);
        AgentEventHub::instance()->unsubscribe(a);
    }

    // Befund 6: Ist der Puffer voll, überholt die neue Nachricht die gepufferten nicht.
    void fullBufferKeepsOrder() {
        const int a = addSession();
        const int b = addSession();
        m_caller = a;
        const QString tid = modernCall(QStringLiteral("await_message")).json().value("result").toObject()
                                .value("taskId").toString();
        m_caller = b;
        legacyTool(QStringLiteral("send_message"), {{"id", a}, {"text", "m00"}});
        m_caller = a;
        taskGet(tid);
        m_caller = b;
        for (int i = 1; i <= 32; ++i)
            QCOMPARE(legacyTool(QStringLiteral("send_message"), {{"id", a}, {"text", QStringLiteral("m%1").arg(i, 2, 10, QLatin1Char('0'))}})
                         .value("via").toString(), QStringLiteral("task-buffer"));
        const QJsonObject over = legacyTool(QStringLiteral("send_message"), {{"id", a}, {"text", "m33"}});
        QCOMPARE(over.value("via").toString(), QStringLiteral("queue"));
        const QByteArray got = m_backends.value(a)->captured;
        QVERIFY(got.contains("m01") && got.contains("m32") && got.contains("m33"));
        QVERIFY(got.indexOf("m01") < got.indexOf("m32"));
        QVERIFY(got.indexOf("m32") < got.indexOf("m33"));
    }

    // Befund 7: Ein Client aus dem Netz darf mit sessionId keinen gebundenen, lebenden
    // Posteingang der echten Session verdrängen.
    void networkCannotReplaceABoundInbox() {
        const int a = addSession();
        m_caller = a;
        const QString tid = modernCall(QStringLiteral("await_message")).json().value("result").toObject()
                                .value("taskId").toString();
        m_caller = -1;
        m_fromNetwork = true;
        HttpReply r = modernCall(QStringLiteral("await_message"), {{"sessionId", a}});
        QVERIFY(r.json().value("result").toObject().value("isError").toBool());
        m_caller = a;
        m_fromNetwork = false;
        QCOMPARE(taskGet(tid).json().value("result").toObject().value("status").toString(), QStringLiteral("working"));
        // Ohne eigenen Posteingang der Session ist es erlaubt (Netz-Client mit Token).
        const int c = addSession();
        m_caller = -1;
        m_fromNetwork = true;
        r = modernCall(QStringLiteral("await_message"), {{"sessionId", c}});
        QCOMPARE(r.json().value("result").toObject().value("resultType").toString(), QStringLiteral("task"));
    }

    // Spezifikation: unbekanntes Werkzeug ist in 2026-07-28 ein Protokollfehler.
    void modernUnknownToolIsInvalidParams() {
        const HttpReply r = modernCall(QStringLiteral("gibtsnicht"));
        QCOMPARE(r.json().value("error").toObject().value("code").toInt(), -32602);
        // Alter Weg unverändert: Werkzeugfehler im Ergebnis.
        QVERIFY(legacy(QStringLiteral("tools/call"), QJsonObject{{"name", "gibtsnicht"}, {"arguments", QJsonObject{}}})
                    .json().value("result").toObject().value("isError").toBool());
    }

    // Spezifikation: Für Benachrichtigungen sind keine Pflicht-Kopfzeilen definiert.
    void modernNotificationWithoutHeaders() {
        QJsonObject params{{"_meta", meta()}};
        const QJsonObject o{{"jsonrpc", "2.0"}, {"method", "notifications/cancelled"}, {"params", params}};
        const HttpReply r = raw(httpPost("/mcp", QJsonDocument(o).toJson(QJsonDocument::Compact)));
        QCOMPARE(r.status, 202);
    }

    // Alter Weg: Ziel in absoluter Form (RFC 9112) und Entwurfsfassungen in der Kopfzeile.
    void legacyAbsoluteFormAndDraftVersion() {
        const QByteArray init = R"({"jsonrpc":"2.0","id":1,"method":"initialize","params":{}})";
        QByteArray req = httpPost("/mcp", init);
        req.replace("POST /mcp ", "POST http://127.0.0.1/mcp ");
        QCOMPARE(raw(req).status, 200);
        const HttpReply r = raw(httpPost("/mcp", R"({"jsonrpc":"2.0","id":1,"method":"tools/list"})",
                                         {{"MCP-Protocol-Version", "2025-12-01"}}));
        QCOMPARE(r.status, 200);
        QVERIFY(r.json().value("result").toObject().contains("tools"));
    }

    void modernLongPollCarriesResultType() {
        const int a = addSession();
        m_caller = a;
        legacyTool(QStringLiteral("subscribe_events"), {});
        const HttpReply r = modernCall(QStringLiteral("wait_for_events"), {{"timeoutMs", 1000}});
        QCOMPARE(r.status, 200);
        QCOMPARE(r.json().value("result").toObject().value("resultType").toString(), QStringLiteral("complete"));
        AgentEventHub::instance()->unsubscribe(a);
    }
};

} // namespace qtmux

QTEST_GUILESS_MAIN(qtmux::TestMcpHttp)
#include "tst_mcphttp.moc"
