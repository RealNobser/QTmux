#include "McpServer.h"
#include "SessionModel.h"
#include "Session.h"
#include "ProcessInfo.h"
#include "PluginHost.h"
#include "AgentEventHub.h"
#include "ConnectionProfile.h"

#include "qtmux_version.h"   // generiert aus cmake/Version.h.in (PROJECT_VERSION)

#include <QTcpServer>
#include <QTcpSocket>
#include <QHostAddress>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonValue>
#include <QTimer>
#include <QChar>
#include <QSettings>

namespace qtmux {

static const char *kProtocolVersion = "2024-11-05";

// QTMUX-30: Der Ereignis-Kanal transportiert nur, was Quellen aktiv melden. Dass das
// nicht selbsterklärend ist, hat im echten Betrieb einen Controller seinen ganzen
// Arbeitsablauf auf ein Aufwachen ausrichten lassen, das nie kam.
static const char *kNoSourceHint =
    "Bislang hat keine der abonnierten Quell-Sessions je ein Ereignis gemeldet. "
    "Ereignisse entstehen NUR, wenn die Quelle sie selbst sendet — per MCP-Werkzeug "
    "post_event oder per OSC 777;qtmux-event. QTmux leitet nichts aus Bildschirminhalt "
    "oder Prozesszustand ab. Ein Claude-Code-Worker meldet von sich aus nichts; dafür "
    "braucht er einen Stop-Hook (fertiges Beispiel in docs/MCP.md, Abschnitt "
    "'Worker ereignisfähig machen'). Ohne das bleibt nur das Abfragen per read_screen.";

int McpServer::defaultPort() {
    // Reihenfolge: Umgebungsvariable > gespeicherte Einstellung > Vorgabe 7345.
    // Die Env-Variable ist der Weg, eine ZWEITE Instanz zum Testen zu starten, ohne
    // der produktiven ins Gehege zu kommen (zusammen mit QTMUX_PROFILE, s. main.cpp).
    bool ok = false;
    const int fromEnv = qEnvironmentVariableIntValue("QTMUX_MCP_PORT", &ok);
    if (ok && fromEnv > 0 && fromEnv < 65536) return fromEnv;
    const int fromSettings = QSettings().value(QStringLiteral("mcp/port"), 0).toInt();
    if (fromSettings > 0 && fromSettings < 65536) return fromSettings;
    return 7345;
}

McpServer::McpServer(QObject *parent) : QObject(parent), m_port(defaultPort()) {
    // Auf neue Agenten-Ereignisse horchen → wartende Long-Polls wecken.
    connect(AgentEventHub::instance(), &AgentEventHub::eventPosted,
            this, &McpServer::onHubEvent);
}
McpServer::~McpServer() { stop(); }

// QML-Handler eines *Requested-Signals meldet hierüber synchron sein Ergebnis
// (Layout-JSON, Session-ID oder Fehlermeldung) an den laufenden Tool-Aufruf zurück.
void McpServer::provideResult(bool ok, const QString &text) {
    m_bridgeSet = true;
    m_bridgeOk = ok;
    m_bridgeText = text;
}

bool McpServer::listening() const { return m_server && m_server->isListening(); }

void McpServer::setSessions(SessionModel *m) {
    if (m == m_sessions) return;
    m_sessions = m;
    emit sessionsChanged();
}

void McpServer::setPort(int p) {
    if (p == m_port) return;
    m_port = p;
    // QTMUX-46: Der Port war als Einstellung `mcp/port` dokumentiert, aber nur lesend
    // implementiert (defaultPort()) — er ließ sich also nur per Hand in den
    // Einstellungen bzw. per QTMUX_MCP_PORT ändern. Wer ihn setzt, will ihn behalten,
    // deshalb hier schreiben. Der Konstruktor geht NICHT über setPort, ein reiner
    // Programmstart schreibt die Einstellung also nicht (Env-Wert bleibt flüchtig).
    QSettings().setValue(QStringLiteral("mcp/port"), p);
    emit portChanged();
}

void McpServer::reloadPort() {
    const int p = defaultPort();
    if (p == m_port) return;
    const bool wasListening = listening();
    if (wasListening) stop();
    m_port = p;
    emit portChanged();
    if (wasListening) start();
}

bool McpServer::start() {
    if (listening()) return true;
    if (!m_server) {
        m_server = new QTcpServer(this);
        connect(m_server, &QTcpServer::newConnection, this, [this]() {
            while (QTcpSocket *sock = m_server->nextPendingConnection()) {
                m_buffers.insert(sock, QByteArray());
                connect(sock, &QTcpSocket::readyRead, this, [this, sock]() { onReadyRead(sock); });
                connect(sock, &QTcpSocket::disconnected, this, [this, sock]() {
                    m_buffers.remove(sock);
                    removePollsForSocket(sock);   // wartenden Long-Poll abräumen
                    sock->deleteLater();
                });
            }
        });
    }
    // Bewusst nur localhost — das ist die Sicherheitsgrenze.
    const bool ok = m_server->listen(QHostAddress::LocalHost, static_cast<quint16>(m_port));
    emit listeningChanged();
    return ok;
}

void McpServer::stop() {
    if (m_server) {
        m_server->close();
        emit listeningChanged();
    }
}

void McpServer::onReadyRead(QTcpSocket *sock) {
    QByteArray &buf = m_buffers[sock];
    buf += sock->readAll();

    const int headerEnd = buf.indexOf("\r\n\r\n");
    if (headerEnd < 0) return;  // Header noch unvollständig

    const QByteArray header = buf.left(headerEnd);
    const bool isPost = header.startsWith("POST");

    // Content-Length ermitteln.
    int contentLength = 0;
    for (const QByteArray &line : header.split('\n')) {
        const QByteArray l = line.trimmed().toLower();
        if (l.startsWith("content-length:")) {
            contentLength = l.mid(15).trimmed().toInt();
            break;
        }
    }

    const int bodyStart = headerEnd + 4;
    if (buf.size() - bodyStart < contentLength) return;  // Body noch unvollständig

    const QByteArray body = buf.mid(bodyStart, contentLength);
    buf.clear();

    if (!isPost) {
        sendHttpJson(sock, R"({"server":"QTmux MCP","transport":"streamable-http"})");
        return;
    }

    const QJsonDocument doc = QJsonDocument::fromJson(body);
    if (doc.isObject()) {
        // Den verbindenden Agentenprozess seiner Session zuordnen — beim Handshake
        // UND bei Tool-Aufrufen (manche Clients halten/wechseln Verbindungen anders).
        const QString m = doc.object().value("method").toString();
        // Den verbindenden Agentenprozess seiner Session zuordnen (einmal pro Request):
        // markiert die Controller-Session (roter Tab) UND merkt die Caller-Session als
        // Fallback für post_event/subscribe_events ohne explizites sessionId-Argument.
        m_callerSessionId = -1;
        if (m == QLatin1String("initialize") || m == QLatin1String("tools/call")) {
            m_callerSessionId = sessionIdForClientPort(static_cast<quint16>(sock->peerPort()));
            if (m_callerSessionId >= 0 && m_sessions)
                if (Session *s = m_sessions->sessionById(m_callerSessionId))
                    s->setMcpController(true);
        }
        // Long-Poll wait_for_events: aufgeschobene Antwort — VOR dem synchronen
        // handleRpc abzweigen, weil der Socket hier (anders als in callTool) greifbar ist
        // und offen bleiben muss, bis ein Ereignis vorliegt oder der Timeout greift.
        const QJsonObject reqObj = doc.object();
        if (m == QLatin1String("tools/call")) {
            const QJsonObject p = reqObj.value("params").toObject();
            if (p.value("name").toString() == QLatin1String("wait_for_events")) {
                beginLongPoll(sock, reqObj.value("id"), p.value("arguments").toObject());
                return;   // Antwort folgt asynchron (completePoll)
            }
        }
        bool isNotification = false;
        const QJsonObject resp = handleRpc(doc.object(), isNotification);
        if (isNotification) {
            sendHttpJson(sock, QByteArray(), 202);  // Notification: kein Body
        } else {
            sendHttpJson(sock, QJsonDocument(resp).toJson(QJsonDocument::Compact));
        }
    } else if (doc.isArray()) {  // JSON-RPC-Batch
        QJsonArray out;
        for (const QJsonValue &v : doc.array()) {
            bool isNotification = false;
            const QJsonObject resp = handleRpc(v.toObject(), isNotification);
            if (!isNotification) out.append(resp);
        }
        sendHttpJson(sock, QJsonDocument(out).toJson(QJsonDocument::Compact));
    } else {
        sendHttpJson(sock, R"({"jsonrpc":"2.0","id":null,"error":{"code":-32700,"message":"Parse error"}})");
    }
}

int McpServer::sessionIdForClientPort(quint16 clientPort) const {
    if (!m_sessions) return -1;
    // Welcher lokale Prozess hält die Verbindung zu unserem Port?
    const qint64 pid = procinfo::pidOfTcpClient(clientPort, static_cast<quint16>(m_port));
    if (pid <= 0) return -1;
    // Den Client seiner Session zuordnen: seine Vorfahrenkette enthält die
    // Shell-PID genau einer Session (Agent läuft als Kind dieser Shell).
    const QList<qint64> chain = procinfo::ancestorPids(pid);
    for (Session *s : m_sessions->sessions()) {
        const qint64 spid = s->processId();
        if (spid > 0 && chain.contains(spid)) return s->id();
    }
    return -1;
}

// --- Inter-Agenten-Benachrichtigung: Long-Poll ------------------------------

int McpServer::subscriberSessionId(const QJsonObject &args, quint16 clientPort) const {
    // Primär: explizites sessionId-Argument (Agent liest $QTMUX_SESSION_ID).
    const int explicitId = args.value("sessionId").toInt(0);
    if (explicitId > 0) return explicitId;
    // Fallback: Vorfahrenketten-Heuristik über den verbindenden Client-Prozess.
    return sessionIdForClientPort(clientPort);
}

QJsonObject McpServer::pollResult(int subscriberSessionId, quint64 afterSeq) const {
    QJsonArray arr;
    quint64 maxSeq = afterSeq;
    const QList<AgentEventHub::Event> evs =
        AgentEventHub::instance()->eventsFor(subscriberSessionId, afterSeq);
    for (const AgentEventHub::Event &e : evs) {
        arr.append(QJsonObject{
            {"sourceSessionId", e.sourceSessionId},
            {"kind", AgentEventHub::kindName(e.kind)},
            {"text", e.text},
            {"timestamp", static_cast<double>(e.timestampMs)},
            {"seq", static_cast<double>(e.seq)},
        });
        if (e.seq > maxSeq) maxSeq = e.seq;
    }
    QJsonObject out{{"events", arr}, {"nextSeq", static_cast<double>(maxSeq)}};
    // QTMUX-30: Leerlauf einordnen statt bloß Stille zu liefern. Hat noch KEINE der
    // abonnierten Quellen je etwas gemeldet, liegt es fast immer daran, dass auf der
    // Quellseite niemand sendet — nicht daran, dass gerade nichts passiert.
    if (arr.isEmpty() && eventCapableSources(subscriberSessionId) == 0)
        out.insert(QStringLiteral("hinweis"), kNoSourceHint);
    return out;
}

// Wie viele Quellen dieses Abos haben bislang überhaupt je ein Ereignis gemeldet?
// Das ist der einzige ehrlich belegbare Indikator für „ereignisfähig" — QTmux kann
// einer Session nicht ansehen, ob in ihr jemand post_event aufrufen WIRD.
int McpServer::eventCapableSources(int subscriberSessionId) const {
    auto *hub = AgentEventHub::instance();
    QList<int> sources;
    for (const QVariant &v : hub->subscriptions()) {
        const QVariantMap m = v.toMap();
        if (m.value(QStringLiteral("subscriberSessionId")).toInt() != subscriberSessionId)
            continue;
        for (const QVariant &s : m.value(QStringLiteral("sources")).toList())
            sources << s.toInt();
        break;
    }
    if (sources.isEmpty() && m_sessions) {   // kein Filter = alle anderen Sessions
        for (Session *o : m_sessions->sessions())
            if (o->id() != subscriberSessionId) sources << o->id();
    }
    int capable = 0;
    for (int sid : sources)
        if (hub->latestFrom(sid).seq > 0) ++capable;
    return capable;
}

void McpServer::beginLongPoll(QTcpSocket *sock, const QJsonValue &rpcId,
                              const QJsonObject &args) {
    auto *hub = AgentEventHub::instance();
    const int subId = subscriberSessionId(args, static_cast<quint16>(sock->peerPort()));

    // afterSeq: expliziter Cursor, sonst „ab jetzt" (nur künftige Ereignisse).
    const bool hasCursor = args.contains(QStringLiteral("afterSeq"));
    const quint64 afterSeq = hasCursor
        ? static_cast<quint64>(args.value("afterSeq").toDouble(0))
        : hub->lastSeq();

    auto reply = [this, sock, rpcId](const QJsonObject &result) {
        const QJsonObject resp{{"jsonrpc", "2.0"}, {"id", rpcId},
            {"result", QJsonObject{
                {"content", QJsonArray{QJsonObject{{"type", "text"},
                    {"text", QString::fromUtf8(QJsonDocument(result).toJson(QJsonDocument::Compact))}}}},
                {"isError", false}}}};
        sendHttpJson(sock, QJsonDocument(resp).toJson(QJsonDocument::Compact));
    };

    // Ohne gültige Subscriber-Session: leer + Fehlerhinweis (kein Warten).
    if (subId <= 0) {
        reply(QJsonObject{{"events", QJsonArray{}}, {"nextSeq", static_cast<double>(afterSeq)},
                          {"error", "Keine Subscriber-Session (sessionId fehlt/unbekannt)."}});
        return;
    }

    // QTMUX-30: Ohne Abo NICHT die volle Wartezeit stumm absitzen — das war der Grund,
    // warum ein Controller erst nach 55 s Stille merkte, dass hier nie etwas kommt.
    if (!hub->hasSubscription(subId)) {
        reply(QJsonObject{{"events", QJsonArray{}}, {"nextSeq", static_cast<double>(afterSeq)},
                          {"error", QStringLiteral("Kein Abo für Session %1 — zuerst "
                                                   "subscribe_events aufrufen.").arg(subId)}});
        return;
    }

    // Liegen bereits passende Ereignisse vor → sofort antworten (kein Aufschub).
    const QJsonObject immediate = pollResult(subId, afterSeq);
    if (!immediate.value("events").toArray().isEmpty()) {
        reply(immediate);
        return;
    }

    // Sonst Poll vormerken und Socket offen lassen (Timeout default 25 s, Deckel 55 s).
    int timeoutMs = args.value("timeoutMs").toInt(25000);
    timeoutMs = qBound(1000, timeoutMs, 55000);

    PendingPoll poll;
    poll.sock = sock;
    poll.rpcId = rpcId;
    poll.subscriberSessionId = subId;
    poll.afterSeq = afterSeq;
    poll.deadline = new QTimer(this);
    poll.deadline->setSingleShot(true);
    poll.deadline->setInterval(timeoutMs);
    connect(poll.deadline, &QTimer::timeout, this, [this, sock]() {
        // Timeout: Poll mit seinem (leeren) Ergebnis beantworten — completePoll liefert
        // events:[] zurück, wenn nichts vorliegt.
        for (int i = 0; i < m_pendingPolls.size(); ++i)
            if (m_pendingPolls.at(i).sock == sock) { completePoll(i); break; }
    });
    m_pendingPolls.append(poll);
    poll.deadline->start();
}

void McpServer::onHubEvent() {
    // Ein Ereignis kam — alle wartenden Polls prüfen (rückwärts wg. Entfernen).
    for (int i = m_pendingPolls.size() - 1; i >= 0; --i) {
        const PendingPoll &p = m_pendingPolls.at(i);
        if (!AgentEventHub::instance()->eventsFor(p.subscriberSessionId, p.afterSeq).isEmpty())
            completePoll(i);
    }
}

void McpServer::completePoll(int index) {
    if (index < 0 || index >= m_pendingPolls.size()) return;
    const PendingPoll p = m_pendingPolls.at(index);
    m_pendingPolls.removeAt(index);
    if (p.deadline) { p.deadline->stop(); p.deadline->deleteLater(); }
    const QJsonObject resp{{"jsonrpc", "2.0"}, {"id", p.rpcId},
        {"result", QJsonObject{
            {"content", QJsonArray{QJsonObject{{"type", "text"},
                {"text", QString::fromUtf8(QJsonDocument(
                    pollResult(p.subscriberSessionId, p.afterSeq)).toJson(QJsonDocument::Compact))}}}},
            {"isError", false}}}};
    sendHttpJson(p.sock, QJsonDocument(resp).toJson(QJsonDocument::Compact));
}

void McpServer::removePollsForSocket(QTcpSocket *sock) {
    for (int i = m_pendingPolls.size() - 1; i >= 0; --i) {
        if (m_pendingPolls.at(i).sock == sock) {
            if (m_pendingPolls.at(i).deadline) {
                m_pendingPolls.at(i).deadline->stop();
                m_pendingPolls.at(i).deadline->deleteLater();
            }
            m_pendingPolls.removeAt(i);
        }
    }
}

void McpServer::sendHttpJson(QTcpSocket *sock, const QByteArray &json, int status) {
    const QByteArray reason = (status == 200) ? "OK" : (status == 202) ? "Accepted" : "Bad Request";
    QByteArray resp = "HTTP/1.1 " + QByteArray::number(status) + " " + reason + "\r\n";
    resp += "Content-Type: application/json\r\n";
    resp += "Content-Length: " + QByteArray::number(json.size()) + "\r\n";
    resp += "Connection: close\r\n\r\n";
    resp += json;
    sock->write(resp);
    sock->flush();
    sock->disconnectFromHost();
}

// --- JSON-RPC / MCP ---------------------------------------------------------

QJsonObject McpServer::handleRpc(const QJsonObject &req, bool &isNotification) {
    const QString method = req.value("method").toString();
    const QJsonObject params = req.value("params").toObject();
    const QJsonValue id = req.value("id");
    isNotification = id.isUndefined() || id.isNull();

    bool ok = true;
    QString errMsg;
    const QJsonObject result = dispatchMethod(method, params, ok, errMsg);

    if (isNotification) return {};

    QJsonObject resp{{"jsonrpc", "2.0"}, {"id", id}};
    if (ok) {
        resp.insert("result", result);
    } else {
        resp.insert("error", QJsonObject{{"code", -32601}, {"message", errMsg}});
    }
    return resp;
}

QJsonObject McpServer::dispatchMethod(const QString &method, const QJsonObject &params,
                                      bool &ok, QString &errMsg) {
    ok = true;
    if (method == "initialize") {
        return QJsonObject{
            {"protocolVersion", kProtocolVersion},
            {"capabilities", QJsonObject{{"tools", QJsonObject{}}}},
            // Version generiert aus PROJECT_VERSION (cmake/Version.h.in): genau
            // dieses Feld liest ein Agent aus, ein vergessener Bump log ihn an.
            {"serverInfo", QJsonObject{{"name", "QTmux"},
                                       {"version", QTMUX_VERSION_STRING}}},
        };
    }
    if (method == "tools/list") {
        return toolsList();
    }
    if (method == "tools/call") {
        const QString name = params.value("name").toString();
        const QJsonObject args = params.value("arguments").toObject();
        bool isError = false;
        QString text;
        callTool(name, args, isError, text);
        return QJsonObject{
            {"content", QJsonArray{QJsonObject{{"type", "text"}, {"text", text}}}},
            {"isError", isError},
        };
    }
    if (method == "ping") {
        return QJsonObject{};
    }
    ok = false;
    errMsg = QStringLiteral("Method not found: %1").arg(method);
    return {};
}

// Hilfs-Schema-Bausteine.
static QJsonObject strProp(const QString &desc) {
    return QJsonObject{{"type", "string"}, {"description", desc}};
}
static QJsonObject intProp(const QString &desc) {
    return QJsonObject{{"type", "integer"}, {"description", desc}};
}
static QJsonObject boolProp(const QString &desc) {
    return QJsonObject{{"type", "boolean"}, {"description", desc}};
}
static QJsonObject arrProp(const QString &itemType, const QString &desc) {
    return QJsonObject{{"type", "array"}, {"items", QJsonObject{{"type", itemType}}},
                       {"description", desc}};
}
static QJsonObject tool(const QString &name, const QString &desc,
                        const QJsonObject &props, const QJsonArray &required) {
    return QJsonObject{
        {"name", name},
        {"description", desc},
        {"inputSchema", QJsonObject{
            {"type", "object"},
            {"properties", props},
            {"required", required},
        }},
    };
}

QJsonObject McpServer::toolsList() const {
    QJsonArray tools;
    tools.append(tool("list_sessions",
                      "Listet alle offenen Sessions mit Status (id, title, type, activity, "
                      "agentId, needsAttention, lastNotification, mcpController, workingDir, group, "
                      "progress).",
                      {}, {}));
    tools.append(tool("create_session",
                      "Erstellt eine Session in einem NEUEN Window (Tab) und aktiviert es. "
                      "type: 'shell' (Standard), 'ssh', 'serial' oder 'plugin'. Je nach Typ die "
                      "passenden Felder füllen (siehe unten). Gibt die neue Session-ID zurück. "
                      "(Für ein Pane IM aktiven Window: split_pane.)",
                      QJsonObject{{"type", strProp("'shell' | 'ssh' | 'serial' | 'plugin'")},
                                  // Shell
                                  {"program", strProp("Shell: Programm/Kommandozeile (optional, leer = Standard-Shell). Siehe list_shells.")},
                                  {"cwd", strProp("Shell: Startverzeichnis (optional)")},
                                  // SSH
                                  {"host", strProp("ssh: Zielhost")},
                                  {"user", strProp("ssh: Benutzername (optional)")},
                                  {"identity", strProp("ssh: Identity-/Key-Datei (optional)")},
                                  // Serial
                                  {"port", strProp("serial: Portname (z. B. COM3). ssh: Port (Zahl als String). Siehe list_serial_ports.")},
                                  {"baud", intProp("serial: Baudrate (Standard 115200)")},
                                  // Plugin
                                  {"pluginId", strProp("plugin: Plugin-ID (siehe list_plugins)")},
                                  {"typeId", strProp("plugin: Backend-Typ-ID des Plugins (siehe list_plugins)")},
                                  // Gemeinsam
                                  {"loginScript", strProp("optional: Befehle nach Verbindungsaufbau (eine Zeile = ein Befehl)")}},
                      {}));
    tools.append(tool("close_session", "Schließt eine Session per ID.",
                      QJsonObject{{"id", intProp("Session-ID")}}, QJsonArray{"id"}));
    tools.append(tool("set_session_group",
                      "Ordnet das WINDOW dieser Session einer Sidebar-Gruppe zu (Window-Modell, "
                      "QTMUX-83) — Windows einer Gruppe stehen zusammen und lassen sich gemeinsam "
                      "einklappen. Leerer/fehlender 'group'-Wert nimmt es aus der Gruppe. "
                      "(Direkt per Window: set_window_group.)",
                      QJsonObject{{"id", intProp("Session-ID")},
                                  {"group", strProp("Gruppenname (leer = ohne Gruppe)")}},
                      QJsonArray{"id"}));
    tools.append(tool("set_window_group",
                      "Ordnet ein Window (Tab) einer Sidebar-Gruppe zu (QTMUX-83). Leerer/"
                      "fehlender 'group'-Wert nimmt es aus der Gruppe.",
                      QJsonObject{{"windowId", intProp("Window-ID (siehe list_windows)")},
                                  {"group", strProp("Gruppenname (leer = ohne Gruppe)")}},
                      QJsonArray{"windowId"}));
    tools.append(tool("rename_group",
                      "Benennt eine bestehende Window-Gruppe um (alle Mitglieder ziehen mit) "
                      "oder löst sie auf (leeres 'to').",
                      QJsonObject{{"from", strProp("bestehender Gruppenname")},
                                  {"to", strProp("neuer Name (leer = Gruppe auflösen)")}},
                      QJsonArray{"from"}));
    tools.append(tool("move_group",
                      "Verschiebt eine ganze Window-Gruppe als Block in der Sidebar-Reihenfolge. "
                      "Sie springt über die benachbarte Sektion (andere Gruppe oder Lauf "
                      "gruppenloser Windows); die Mitglieder bleiben zusammenhängend.",
                      QJsonObject{{"name", strProp("Gruppenname")},
                                  {"direction", strProp("'up' = nach oben | 'down' = nach unten")}},
                      QJsonArray{"name", "direction"}));
    tools.append(tool("focus_session",
                      "Aktiviert das Window, in dem diese Session als Pane liegt (Window-Modell, "
                      "QTMUX-83). Die Session selbst bleibt an ihrem Pane.",
                      QJsonObject{{"id", intProp("Session-ID")}}, QJsonArray{"id"}));
    tools.append(tool("attach_controller",
                      "Markiert die Session mit dieser ID als steuernde MCP-Controller-Session "
                      "(roter Tab in der Sidebar). Üblich: der Agent liest $QTMUX_SESSION_ID "
                      "und ruft dieses Tool mit diesem Wert auf.",
                      QJsonObject{{"id", intProp("Session-ID des steuernden Agenten")}},
                      QJsonArray{"id"}));
    tools.append(tool("send_text",
                      "Sendet Text an eine Session (optional mit Enter). Mit broadcast=true "
                      "geht der Text an ALLE Sessions (id wird dann ignoriert). Das Enter geht "
                      "kurz NACH dem Text raus (Standard 60 ms) — TUI-Anwendungen wie Claude "
                      "Code werten sonst einen in einem Rutsch ankommenden Block als "
                      "Einfügevorgang und das Enter landet als Zeilenumbruch im Eingabefeld, "
                      "statt abzuschicken. Bei sehr trägen Oberflächen enterDelayMs erhöhen.",
                      QJsonObject{{"id", intProp("Session-ID (entfällt bei broadcast)")},
                                  {"text", strProp("zu sendender Text")},
                                  {"enter", boolProp("Enter anhängen (Standard true)")},
                                  {"enterDelayMs", intProp("Verzögerung vor dem Enter in ms (Standard 60; 0 = sofort, im selben Block)")},
                                  {"broadcast", boolProp("an alle Sessions senden (Standard false)")}},
                      QJsonArray{"text"}));
    tools.append(tool("queue_text",
                      "Reiht Text in die Warteschlange einer Session ein (QTMUX-90) statt ihn "
                      "sofort zu senden. Ist die Session gerade frei, geht er unmittelbar raus; "
                      "arbeitet sie noch, wartet er, bis sie fertig ist — so landet ein Auftrag "
                      "nicht mitten in der Ausgabe eines laufenden Agenten. Ohne `text` liefert "
                      "der Aufruf nur den aktuellen Stand der Warteschlange; mit clear=true wird "
                      "sie geleert.",
                      QJsonObject{{"id", intProp("Session-ID")},
                                  {"text", strProp("einzureihender Text (weglassen = nur abfragen)")},
                                  {"clear", boolProp("Warteschlange vorher leeren (Standard false)")}},
                      QJsonArray{"id"}));
    tools.append(tool("read_screen",
                      "Liest den Bildschirm einer Session als Text. Mit scrollback=true zusätzlich "
                      "die gesamte Historie (Scrollback) vor dem sichtbaren Bereich.",
                      QJsonObject{{"id", intProp("Session-ID")},
                                  {"scrollback", boolProp("Scrollback-Historie mitliefern (Standard false)")}},
                      QJsonArray{"id"}));
    tools.append(tool("set_theme", "Setzt das Design: 'system', 'light' oder 'dark'.",
                      QJsonObject{{"mode", strProp("'system' | 'light' | 'dark'")}},
                      QJsonArray{"mode"}));
    // Discovery (read-only): was kann create_session erzeugen?
    tools.append(tool("list_shells",
                      "Listet die auf dieser Plattform verfügbaren Shells ({program, name}) "
                      "für create_session type=shell.", {}, {}));
    tools.append(tool("list_serial_ports",
                      "Listet die verfügbaren seriellen Ports für create_session type=serial.",
                      {}, {}));
    tools.append(tool("list_plugins",
                      "Listet die von geladenen Plugins angebotenen Backend-Typen "
                      "({pluginId, typeId, name, description}) für create_session type=plugin.",
                      {}, {}));
    // Layout-/Pane-Steuerung (QTMUX-29): dieselben Operationen wie die Split-Menüs
    // der GUI — für AI-Companions, die Sessions nebeneinander anordnen wollen.
    tools.append(tool("get_layout",
                      "Liefert das Pane-Layout des AKTIVEN Windows als JSON: {layout, windowId, "
                      "activePaneId, sessions}. 'layout' ist der Baum (Blatt: {paneId, sessionId, "
                      "active}; Split: {orientation:'h'|'v', children:[…]}). 'sessions' listet ALLE "
                      "Sessions mit ihrer Pane-Zuordnung im aktiven Window — paneId=null/"
                      "visible=false heißt: läuft in einem ANDEREN Window (nicht sichtbar). Mit "
                      "'windowId' das Layout eines bestimmten Windows abfragen (siehe list_windows).",
                      QJsonObject{{"windowId", intProp("Window-ID (optional, sonst aktives Window)")}},
                      {}));
    tools.append(tool("split_pane",
                      "Teilt das aktive Pane IM AKTIVEN WINDOW (wie die GUI-Splits): erzeugt eine "
                      "neue Shell-Session im neuen Pane (wird aktiv) und liefert deren Session-ID. "
                      "Anders als create_session (das ein neues Window öffnet) bleibt der Split im "
                      "selben Window.",
                      QJsonObject{{"orientation", strProp("'h' = nebeneinander | 'v' = untereinander")}},
                      QJsonArray{"orientation"}));
    tools.append(tool("close_pane",
                      "Schließt ein Pane MITSAMT seiner Session (GUI-Semantik). Ohne paneId "
                      "das aktive Pane; beim letzten Pane wird nur die Session geschlossen.",
                      QJsonObject{{"paneId", intProp("Pane-ID aus get_layout (optional, sonst aktives Pane)")}},
                      {}));
    tools.append(tool("focus_pane",
                      "Setzt ein bestehendes Pane aktiv (reiner Fokuswechsel, ohne die "
                      "Session zu ändern) — das GUI-Pendant zum Klick in ein Pane, das "
                      "es bisher nur per Maus gab.",
                      QJsonObject{{"paneId", intProp("Pane-ID aus get_layout")}},
                      QJsonArray{"paneId"}));
    tools.append(tool("zoom_pane",
                      "Maximiert ein Pane temporaer ('zoomt') bzw. hebt den Zoom auf, ohne "
                      "das Layout zu veraendern. Mit paneId das genannte Pane; ohne/-1 wird "
                      "der Zoom aufgehoben.",
                      QJsonObject{{"paneId", intProp("Pane-ID aus get_layout (fehlend/-1 = Zoom aus)")}},
                      {}));
    // Window-Steuerung (QTMUX-83, Stufe 4): die Sidebar listet Windows (Tabs), jedes mit
    // eigenem Split-Layout. Sessions bleiben per Session-ID adressierbar (send_text/…).
    tools.append(tool("list_windows",
                      "Listet alle Windows (Tabs) mit ihrem aggregierten Status: {windowId, "
                      "title, group, paneCount, active, sessionIds}. Jedes Window hat sein "
                      "eigenes Split-Layout (siehe get_layout windowId).",
                      {}, {}));
    tools.append(tool("focus_window",
                      "Aktiviert ein Window (schaltet das ganze Layout auf dessen Panes um).",
                      QJsonObject{{"windowId", intProp("Window-ID (siehe list_windows)")}},
                      QJsonArray{"windowId"}));
    tools.append(tool("new_window",
                      "Öffnet ein neues Window (Tab) mit einer Shell-Session und aktiviert es. "
                      "Liefert die Session-ID der neuen Shell.",
                      {}, {}));
    tools.append(tool("rename_window",
                      "Benennt ein Window um. Leerer/fehlender 'name' stellt den automatischen "
                      "Titel (Titel des aktiven Panes) wieder her.",
                      QJsonObject{{"windowId", intProp("Window-ID")},
                                  {"name", strProp("neuer Name (leer = automatisch)")}},
                      QJsonArray{"windowId"}));
    tools.append(tool("close_window",
                      "Schließt ein Window (Tab) samt ALLER seiner Sessions/Panes.",
                      QJsonObject{{"windowId", intProp("Window-ID")}},
                      QJsonArray{"windowId"}));
    tools.append(tool("assign_session",
                      "VERALTET (Window-Modell, QTMUX-83): Es gibt kein „Session in ein Pane "
                      "laden“ mehr — jede Session gehört fest zu einem Window/Pane. Nutze "
                      "focus_session (Window der Session aktivieren) bzw. focus_window. Der "
                      "Aufruf meldet nur noch einen Hinweis.",
                      QJsonObject{{"id", intProp("Session-ID")},
                                  {"paneId", intProp("(ignoriert)")}},
                      QJsonArray{"id"}));
    // Verbindungsprofile (QTMUX-29): gespeicherte Verbindungen nutzbar machen.
    // Ein Vault-Passwort wird beim Verbinden INTERN aufgelöst (gleicher Weg wie der
    // GUI-Klick) und nie über MCP ausgegeben.
    tools.append(tool("list_profiles",
                      "Listet die gespeicherten Verbindungsprofile ({name, type:'shell'|'ssh'|"
                      "'serial', …typspezifische Felder, hasPasswordSecret, hasLoginScript}). "
                      "Geheimniswerte werden nie ausgegeben.",
                      {}, {}));
    tools.append(tool("connect_profile",
                      "Verbindet ein gespeichertes Profil (wie der Verbinden-Klick im "
                      "Connection-Manager, inkl. interner Vault-Passwort-Auflösung) und "
                      "liefert die neue Session-ID.",
                      QJsonObject{{"name", strProp("Profilname (siehe list_profiles)")}},
                      QJsonArray{"name"}));
    // Inter-Agenten-Benachrichtigung: ein Agent in einer Session wird benachrichtigt,
    // wenn ein Agent in einer ANDEREN Session fertig ist oder eine Frage hat.
    tools.append(tool("wait_for_events",
                      "Long-Poll: blockiert, bis ein abonniertes Agenten-Ereignis vorliegt "
                      "oder der Timeout greift. Liefert {events:[{sourceSessionId, kind, text, "
                      "timestamp, seq}], nextSeq}. Mit nextSeq als afterSeq weiterpollen "
                      "(keine Lücken/Doppel). sourceSessionId ist die Quell-Session, in der "
                      "per send_text/read_screen/focus_session weitergearbeitet werden kann. "
                      "Voraussetzung: zuvor subscribe_events. WICHTIG: Es kommt nur an, was "
                      "eine Quell-Session selbst per post_event oder OSC 777;qtmux-event "
                      "meldet — aus Bildschirminhalt oder Prozesszustand wird NICHTS "
                      "abgeleitet. Ein Claude-Code-Worker meldet ohne eingerichteten "
                      "Stop-Hook nichts; dann bleibt nur Abfragen per read_screen "
                      "(s. docs/MCP.md). Kommt nichts, sagt das Feld 'hinweis' warum.",
                      QJsonObject{{"sessionId", intProp("eigene Session-ID (sonst $QTMUX_SESSION_ID; Fallback Prozess-Heuristik)")},
                                  {"afterSeq", intProp("nur Ereignisse mit seq > afterSeq (Standard: ab jetzt)")},
                                  {"timeoutMs", intProp("max. Wartezeit in ms (Standard 25000, Deckel 55000)")}},
                      {}));
    tools.append(tool("post_event",
                      "Meldet ein Agenten-Ereignis aus DIESER Session (fertig/Frage/Fehler/Info). "
                      "Abonnierende Sessions werden benachrichtigt; der Text erscheint in der "
                      "Sidebar. Bei 'question'/'error' pulst zusaetzlich die Kachel (Aufmerksamkeit), "
                      "sofern sie nicht fokussiert ist. Alternative zum Shell-Hook 'qtmux-event'.",
                      QJsonObject{{"kind", strProp("'done' | 'question' | 'error' | 'info'")},
                                  {"text", strProp("Beschreibungstext")},
                                  {"sessionId", intProp("Quell-Session-ID (sonst $QTMUX_SESSION_ID; Fallback Prozess-Heuristik)")}},
                      QJsonArray{"kind"}));
    tools.append(tool("needs_attention",
                      "Fordert explizit Aufmerksamkeit fuer DIESE Session an: die Sidebar-Kachel "
                      "pulst (sofern nicht fokussiert). Fuer 'ich bin blockiert / brauche einen "
                      "Menschen', entkoppelt vom Ereignis-Bus. Optionaler Text landet als "
                      "Sidebar-Notiz.",
                      QJsonObject{{"text", strProp("optionaler Hinweistext")},
                                  {"sessionId", intProp("eigene Session-ID (sonst $QTMUX_SESSION_ID; Fallback Prozess-Heuristik)")}},
                      {}));
    tools.append(tool("clear_attention",
                      "Loescht die Aufmerksamkeits-Markierung DIESER Session wieder (Gegenstueck "
                      "zu needs_attention; sonst erlischt sie erst beim Fokussieren der Kachel).",
                      QJsonObject{{"sessionId", intProp("eigene Session-ID (sonst $QTMUX_SESSION_ID; Fallback Prozess-Heuristik)")}},
                      {}));
    tools.append(tool("set_activity",
                      "Setzt den Dauer-Zustand DIESER Session, der den Sidebar-Ring faerbt: "
                      "'idle' (dim), 'busy' (gruen), 'waiting' (amber = wartet/blockiert), "
                      "'error' (rot). Fuer Agenten-TUIs, die keine OSC-133-Marker senden - so "
                      "zeigt der Punkt echten Zustand statt Dauergruen. Der Aufmerksamkeits-Puls "
                      "(needs_attention/question/error) liegt weiterhin darueber.",
                      QJsonObject{{"state", strProp("'idle' | 'busy' | 'waiting' | 'error'")},
                                  {"sessionId", intProp("eigene Session-ID (sonst $QTMUX_SESSION_ID; Fallback Prozess-Heuristik)")}},
                      QJsonArray{"state"}));
    tools.append(tool("set_agent_session",
                      "Meldet die Kennung DEINER laufenden Unterhaltung (z. B. die eigene "
                      "Session-/Conversation-ID). QTmux legt sie an deinem Pane ab und startet "
                      "dich beim naechsten Programmstart mit genau dieser Unterhaltung wieder - "
                      "sofern der Anwender 'Unterhaltung fortsetzen: gemeldete Sitzung' gewaehlt "
                      "hat. QTmux kann die ID NICHT selbst ermitteln (du bist ein fremdes "
                      "Programm im PTY), und sie aendert sich bei /resume oder /clear: melde sie "
                      "darum beim Start und nach jedem Wechsel erneut. Leerer Wert loescht sie.",
                      QJsonObject{{"ref", strProp("Kennung der Unterhaltung (undurchsichtige Zeichenkette)")},
                                  {"sessionId", intProp("eigene Session-ID (sonst $QTMUX_SESSION_ID; Fallback Prozess-Heuristik)")}},
                      QJsonArray{"ref"}));
    tools.append(tool("subscribe_events",
                      "Abonniert Agenten-Ereignisse für DIESE Session. Ohne Filter werden alle "
                      "Ereignisse aller anderen Sessions empfangen; optional auf Quell-Sessions "
                      "und/oder Ereignisarten einschränken. Danach mit wait_for_events abholen. "
                      "Die Antwort nennt je Quelle, ob sie existiert und ob sie bisher je ein "
                      "Ereignis gemeldet hat (hasPostedEvents) — denn geliefert wird nur, was "
                      "Quellen SELBST senden (post_event / OSC 777;qtmux-event). Steht "
                      "sourcesWithEventsSoFar auf 0, wird wait_for_events aller Voraussicht "
                      "nach nichts bringen; dann erst die Quellseite einrichten (docs/MCP.md).",
                      QJsonObject{{"sessionId", intProp("eigene Session-ID (sonst $QTMUX_SESSION_ID; Fallback Prozess-Heuristik)")},
                                  {"sources", arrProp("integer", "nur diese Quell-Session-IDs (leer/fehlt = alle)")},
                                  {"kinds", arrProp("string", "nur diese Arten: done/question/error/info (leer = alle)")}},
                      {}));
    tools.append(tool("unsubscribe_events",
                      "Hebt das Ereignis-Abo DIESER Session wieder auf.",
                      QJsonObject{{"sessionId", intProp("eigene Session-ID (sonst $QTMUX_SESSION_ID; Fallback Prozess-Heuristik)")}},
                      {}));
    tools.append(tool("list_subscriptions",
                      "Listet alle aktiven Ereignis-Abos ({subscriberSessionId, sources, kinds}).",
                      {}, {}));
    return QJsonObject{{"tools", tools}};
}

QJsonObject McpServer::callTool(const QString &name, const QJsonObject &args,
                                bool &isError, QString &text) {
    isError = false;
    if (!m_sessions) {
        isError = true;
        text = QStringLiteral("Keine SessionModel-Instanz verbunden.");
        return {};
    }

    auto sessionInfo = [](Session *s) {
        QJsonObject o{
            {"id", s->id()},
            {"title", s->title()},
            {"type", static_cast<int>(s->type())},
            {"activity", s->activityInt()},
            {"agentId", s->agentId()},
            {"needsAttention", s->needsAttention()},
            {"lastNotification", s->lastNotification()},
            {"mcpController", s->mcpController()},
            {"workingDir", s->workingDirectory()},
            {"group", s->group()},
            {"progressActive", s->progressActive()},
            {"progressState", s->progressState()},
            {"progressValue", s->progressValue()},
        };
        // Jüngstes Agenten-Ereignis dieser Session als Quelle (für Polling-Clients).
        const AgentEventHub::Event ev = AgentEventHub::instance()->latestFrom(s->id());
        if (ev.seq > 0) {
            o.insert("lastAgentEventKind", AgentEventHub::kindName(ev.kind));
            o.insert("lastAgentEventText", ev.text);
            o.insert("lastAgentEventSeq", static_cast<double>(ev.seq));
        }
        return o;
    };

    // Inter-Agenten-Benachrichtigung: Caller-Session (explizit oder per Vorfahren-Fallback).
    auto callerId = [this, &args]() {
        const int explicitId = args.value("sessionId").toInt(0);
        return explicitId > 0 ? explicitId : m_callerSessionId;
    };

    if (name == "post_event") {
        const int srcId = callerId();
        if (srcId <= 0) { isError = true; text = QStringLiteral("Keine Quell-Session (sessionId fehlt/unbekannt)."); return {}; }
        const QString kind = args.value("kind").toString(QStringLiteral("info"));
        const QString evText = args.value("text").toString();
        // Ueber die Quell-Session melden: das speist den Ereignis-Bus, spiegelt die Notiz in
        // die Sidebar UND weckt bei question/error den Aufmerksamkeits-Puls (frueher fehlte
        // Letzteres beim MCP-Weg — Ereignis kam an, aber die Kachel pulste nicht). Fallback
        // auf den Hub direkt, falls die Session nicht (mehr) existiert.
        if (Session *s = m_sessions ? m_sessions->sessionById(srcId) : nullptr)
            s->reportAgentEvent(kind, evText);
        else
            AgentEventHub::instance()->postEvent(srcId, AgentEventHub::kindFromString(kind), evText);
        text = QStringLiteral("ok");
        return {};
    }
    if (name == "needs_attention") {
        const int srcId = callerId();
        if (srcId <= 0) { isError = true; text = QStringLiteral("Keine Quell-Session (sessionId fehlt/unbekannt)."); return {}; }
        Session *s = m_sessions ? m_sessions->sessionById(srcId) : nullptr;
        if (!s) { isError = true; text = QStringLiteral("Unbekannte Session-ID: %1.").arg(srcId); return {}; }
        s->flagAttention(args.value("text").toString());
        text = QStringLiteral("ok");
        return {};
    }
    if (name == "clear_attention") {
        const int srcId = callerId();
        if (srcId <= 0) { isError = true; text = QStringLiteral("Keine Quell-Session (sessionId fehlt/unbekannt)."); return {}; }
        Session *s = m_sessions ? m_sessions->sessionById(srcId) : nullptr;
        if (!s) { isError = true; text = QStringLiteral("Unbekannte Session-ID: %1.").arg(srcId); return {}; }
        s->clearAttention();
        text = QStringLiteral("ok");
        return {};
    }
    if (name == "set_agent_session") {
        const int srcId = callerId();
        if (srcId <= 0) { isError = true; text = QStringLiteral("Keine Quell-Session (sessionId fehlt/unbekannt)."); return {}; }
        const int row = m_sessions ? m_sessions->rowForId(srcId) : -1;
        if (row < 0) { isError = true; text = QStringLiteral("Unbekannte Session-ID: %1.").arg(srcId); return {}; }
        m_sessions->setAgentSessionRef(row, args.value("ref").toString());
        text = QStringLiteral("ok");
        return {};
    }
    if (name == "set_activity") {
        const int srcId = callerId();
        if (srcId <= 0) { isError = true; text = QStringLiteral("Keine Quell-Session (sessionId fehlt/unbekannt)."); return {}; }
        const QString st = args.value("state").toString().toLower();
        if (st != QLatin1String("idle") && st != QLatin1String("busy")
            && st != QLatin1String("waiting") && st != QLatin1String("error")) {
            isError = true; text = QStringLiteral("state muss 'idle', 'busy', 'waiting' oder 'error' sein.");
            return {};
        }
        Session *s = m_sessions ? m_sessions->sessionById(srcId) : nullptr;
        if (!s) { isError = true; text = QStringLiteral("Unbekannte Session-ID: %1.").arg(srcId); return {}; }
        s->requestActivity(st);
        text = QStringLiteral("ok");
        return {};
    }
    if (name == "subscribe_events") {
        const int subId = callerId();
        if (subId <= 0) { isError = true; text = QStringLiteral("Keine Subscriber-Session (sessionId fehlt/unbekannt)."); return {}; }
        QStringList kinds;
        for (const QJsonValue &v : args.value("kinds").toArray()) kinds << v.toString();
        const QJsonArray sources = args.value("sources").toArray();
        AgentEventHub::instance()->subscribe(subId, sources.toVariantList(), kinds);

        // QTMUX-30: Statt bloß „ok" auch sagen, WORAUF man da wartet. Ob eine Quelle
        // je Ereignisse senden wird, kann QTmux nicht wissen — belegbar ist nur, ob sie
        // es bisher getan hat. Genau diese Auskunft fehlte, als ein Controller seinen
        // Arbeitsablauf auf ein Aufwachen ausrichtete, das nie kam.
        QJsonArray srcInfo;
        auto describe = [this](int sid) {
            return QJsonObject{
                {"sessionId", sid},
                {"exists", m_sessions->sessionById(sid) != nullptr},
                {"hasPostedEvents", AgentEventHub::instance()->latestFrom(sid).seq > 0}};
        };
        if (sources.isEmpty()) {
            for (Session *o : m_sessions->sessions())
                if (o->id() != subId) srcInfo.append(describe(o->id()));
        } else {
            for (const QJsonValue &v : sources) srcInfo.append(describe(v.toInt()));
        }
        const int capable = eventCapableSources(subId);
        QJsonObject res{{"ok", true},
                        {"subscriberSessionId", subId},
                        {"sources", srcInfo},
                        {"sourcesWithEventsSoFar", capable}};
        if (capable == 0) res.insert(QStringLiteral("hinweis"), kNoSourceHint);
        text = QString::fromUtf8(QJsonDocument(res).toJson(QJsonDocument::Compact));
        return {};
    }
    if (name == "unsubscribe_events") {
        const int subId = callerId();
        if (subId <= 0) { isError = true; text = QStringLiteral("Keine Subscriber-Session (sessionId fehlt/unbekannt)."); return {}; }
        AgentEventHub::instance()->unsubscribe(subId);
        text = QStringLiteral("ok");
        return {};
    }
    if (name == "list_subscriptions") {
        text = QString::fromUtf8(QJsonDocument(
            QJsonArray::fromVariantList(AgentEventHub::instance()->subscriptions()))
                                     .toJson(QJsonDocument::Compact));
        return {};
    }

    if (name == "list_sessions") {
        QJsonArray arr;
        for (Session *s : m_sessions->sessions()) arr.append(sessionInfo(s));
        text = QString::fromUtf8(QJsonDocument(arr).toJson(QJsonDocument::Compact));
        return {};
    }
    if (name == "list_shells") {
        text = QString::fromUtf8(QJsonDocument(
            QJsonArray::fromVariantList(m_sessions->availableShells())).toJson(QJsonDocument::Compact));
        return {};
    }
    if (name == "list_serial_ports") {
        QJsonArray arr;
        for (const QString &p : m_sessions->availableSerialPorts()) arr.append(p);
        text = QString::fromUtf8(QJsonDocument(arr).toJson(QJsonDocument::Compact));
        return {};
    }
    if (name == "list_plugins") {
        text = QString::fromUtf8(QJsonDocument(
            QJsonArray::fromVariantList(PluginHost::instance().backendTypesVariant()))
                                     .toJson(QJsonDocument::Compact));
        return {};
    }
    if (name == "create_session") {
        const QString type = args.value("type").toString(QStringLiteral("shell"));
        const QString loginScript = args.value("loginScript").toString();
        int row = -1;
        if (type == "serial") {
            row = m_sessions->createSerialSession(args.value("port").toString(),
                                                  args.value("baud").toInt(115200),
                                                  loginScript);
        } else if (type == "ssh") {
            // 'port' kann als Zahl oder String kommen; default 22.
            const int sshPort = args.value("port").isDouble()
                                    ? args.value("port").toInt(22)
                                    : args.value("port").toString().toInt();
            row = m_sessions->createSshSession(args.value("host").toString(),
                                               sshPort > 0 ? sshPort : 22,
                                               args.value("user").toString(),
                                               args.value("identity").toString(),
                                               loginScript);
        } else if (type == "plugin") {
            row = m_sessions->createPluginSession(args.value("pluginId").toString(),
                                                  args.value("typeId").toString());
        } else {
            row = m_sessions->createShellSession(args.value("cwd").toString(),
                                                 args.value("program").toString(),
                                                 loginScript);
        }
        if (row < 0) { isError = true; text = QStringLiteral("Erstellung fehlgeschlagen."); return {}; }
        auto *s = static_cast<Session *>(m_sessions->sessionAt(row));
        emit focusRequested(row);
        text = QString::number(s ? s->id() : -1);
        return {};
    }
    if (name == "send_text" && args.value("broadcast").toBool(false)) {
        // Broadcast: an alle Sessions, ohne id (Sync-Input-Modus, QTMUX-21).
        QByteArray data = args.value("text").toString().toUtf8();
        if (args.value("enter").toBool(true)) data += '\r';
        m_sessions->writeToAll(data);
        text = QStringLiteral("ok");
        return {};
    }

    // Ab hier braucht's eine gültige Session-ID.
    const int id = args.value("id").toInt();
    Session *s = m_sessions->sessionById(id);

    // QTMUX-32: „Unbekannte ID." schickte Aufrufer auf die falsche Fährte, wenn in
    // Wahrheit nur der Parametername danebenlag. Die ANTWORTEN von list_sessions und
    // get_layout heißen sessionId/paneId, die EINGABE heißt schlicht `id` — dieser
    // Verwechslung antworten wir jetzt mit Klartext statt mit „gibt es nicht".
    auto idProblem = [this, &args, id]() -> QString {
        if (!args.contains(QStringLiteral("id"))) {
            static const char *aliases[] = {"sessionId", "paneId", "session", "sessionID", "ID"};
            for (const char *a : aliases) {
                if (args.contains(QLatin1String(a)))
                    return QStringLiteral(
                               "Parameter 'id' fehlt (übergeben wurde '%1'). Alle "
                               "sitzungsbezogenen Werkzeuge erwarten 'id' — auch wenn die "
                               "Antwortfelder von list_sessions/get_layout 'sessionId' bzw. "
                               "'paneId' heißen.")
                        .arg(QLatin1String(a));
            }
            return QStringLiteral("Parameter 'id' fehlt.");
        }
        QStringList known;
        for (Session *o : m_sessions->sessions()) known << QString::number(o->id());
        return QStringLiteral("Unbekannte Session-ID: %1. Vorhanden: %2.")
            .arg(id)
            .arg(known.isEmpty() ? QStringLiteral("keine") : known.join(QStringLiteral(", ")));
    };
    // Gruppen sind im Window-Modell (QTMUX-83) WINDOW-Gruppen. set_session_group wirkt
    // deshalb auf das Window der genannten Session (Kompatibilität); set_window_group
    // adressiert das Window direkt. rename_group/move_group operieren auf Window-Gruppen.
    if (name == "set_session_group") {
        if (!m_windows) { isError = true; text = QStringLiteral("UI nicht verbunden."); return {}; }
        Session *sess = m_sessions->sessionById(id);
        if (!sess) { isError = true; text = idProblem(); return {}; }
        const int wrow = m_windows->rowForId(sess->windowId());
        if (wrow < 0) { isError = true; text = QStringLiteral("Session gehört zu keinem Window."); return {}; }
        m_windows->setWindowGroup(wrow, args.value("group").toString());
        text = QStringLiteral("ok");
        return {};
    }
    if (name == "set_window_group") {
        if (!m_windows) { isError = true; text = QStringLiteral("UI nicht verbunden."); return {}; }
        const int wid = args.value("windowId").toInt(-1);
        const int wrow = m_windows->rowForId(wid);
        if (wrow < 0) { isError = true; text = QStringLiteral("Unbekannte windowId: %1.").arg(wid); return {}; }
        m_windows->setWindowGroup(wrow, args.value("group").toString());
        text = QStringLiteral("ok");
        return {};
    }
    if (name == "rename_group") {
        if (!m_windows) { isError = true; text = QStringLiteral("UI nicht verbunden."); return {}; }
        const QString from = args.value("from").toString().trimmed();
        const QString to = args.value("to").toString();
        if (from.isEmpty()) {
            isError = true;
            text = QStringLiteral("'from' (bestehender Gruppenname) ist erforderlich.");
            return {};
        }
        if (m_windows->groupSize(from) == 0) {
            isError = true;
            text = QStringLiteral("Unbekannte Gruppe: %1.").arg(from);
            return {};
        }
        m_windows->renameGroup(from, to);
        text = to.trimmed().isEmpty() ? QStringLiteral("aufgelöst") : QStringLiteral("ok");
        return {};
    }
    if (name == "move_group") {
        if (!m_windows) { isError = true; text = QStringLiteral("UI nicht verbunden."); return {}; }
        const QString gname = args.value("name").toString().trimmed();
        const QString dir = args.value("direction").toString().toLower();
        if (gname.isEmpty()) {
            isError = true; text = QStringLiteral("'name' (Gruppenname) ist erforderlich."); return {};
        }
        if (dir != QLatin1String("up") && dir != QLatin1String("down")) {
            isError = true; text = QStringLiteral("'direction' muss 'up' oder 'down' sein."); return {};
        }
        if (m_windows->groupSize(gname) == 0) {
            isError = true; text = QStringLiteral("Unbekannte Gruppe: %1.").arg(gname); return {};
        }
        m_windows->moveGroup(gname, dir == QLatin1String("up") ? -1 : 1);
        text = QStringLiteral("ok");
        return {};
    }
    if (name == "close_session") {
        const int row = m_sessions->rowForId(id);
        if (row < 0) { isError = true; text = idProblem(); return {}; }
        m_sessions->closeSession(row);
        text = QStringLiteral("ok");
        return {};
    }
    if (name == "focus_session") {
        const int row = m_sessions->rowForId(id);
        if (row < 0) { isError = true; text = idProblem(); return {}; }
        emit focusRequested(row);
        text = QStringLiteral("ok");
        return {};
    }
    if (name == "attach_controller") {
        if (!s) { isError = true; text = idProblem(); return {}; }
        s->setMcpController(true);   // roter Tab; gilt bis zum Schließen der Session
        text = QStringLiteral("ok");
        return {};
    }
    if (name == "send_text") {
        if (!s) { isError = true; text = idProblem(); return {}; }
        const QByteArray data = args.value("text").toString().toUtf8();
        if (args.value("enter").toBool(true)) {
            // QTMUX-31: Enter zeitlich abgesetzt — sonst schlucken TUI-Anwendungen es
            // als Teil eines vermeintlichen Einfügevorgangs (s. Session::writeWithEnter).
            const int delay = args.contains(QStringLiteral("enterDelayMs"))
                                  ? args.value("enterDelayMs").toInt(Session::kDefaultEnterDelayMs)
                                  : Session::kDefaultEnterDelayMs;
            s->writeWithEnter(data, delay);
        } else if (!data.isEmpty()) {
            s->write(data);
        }
        text = QStringLiteral("ok");
        return {};
    }
    if (name == "queue_text") {
        if (!s) { isError = true; text = idProblem(); return {}; }
        int removed = 0;
        if (args.value("clear").toBool(false)) removed = s->promptQueue()->clear();
        bool queued = false;
        if (args.contains(QStringLiteral("text"))) {
            const QString t = args.value("text").toString();
            queued = s->queueText(t);
            if (!queued) {
                // Nur leerer/whitespace-Text wird abgewiesen. Das ist kein Randfall,
                // sondern Absicht: ein blankes Enter ist in einem Agenten-TUI eine
                // HANDLUNG (es bestätigt die vorausgewählte Option einer Rückfrage).
                isError = true;
                text = QStringLiteral("Leerer Text wird nicht eingereiht.");
                return {};
            }
        }
        QJsonObject r{{"queued", queued},
                      {"count", s->queuedCount()},
                      {"cleared", removed}};
        QJsonArray entries;
        for (const QString &e : s->promptQueue()->entries()) entries.append(e);
        r.insert(QStringLiteral("entries"), entries);
        text = QString::fromUtf8(QJsonDocument(r).toJson(QJsonDocument::Compact));
        return {};
    }
    if (name == "read_screen") {
        if (!s) { isError = true; text = idProblem(); return {}; }
        if (args.value("scrollback").toBool(false)) {
            const QString sb = s->scrollbackText();
            text = sb.isEmpty() ? s->screenText() : sb + QChar('\n') + s->screenText();
        } else {
            text = s->screenText();
        }
        return {};
    }
    if (name == "set_theme") {
        const QString mode = args.value("mode").toString();
        const int m = (mode == "light") ? 1 : (mode == "dark") ? 2 : 0;
        emit setThemeRequested(m);
        text = QStringLiteral("ok");
        return {};
    }

    // --- Layout-/Pane-Steuerung (QTMUX-29) ---------------------------------
    // Der Layout-Baum lebt in QML (window.layout); die Signale laufen synchron in
    // die dortigen Handler, das Ergebnis kommt über die provideResult-Brücke zurück.
    if (name == "get_layout") {
        const int windowId = args.value("windowId").toInt(-1);
        bridgedCall([this, windowId] { emit layoutRequested(windowId); }, isError, text);
        return {};
    }
    // --- Window-Steuerung (QTMUX-83, Stufe 4) ------------------------------
    if (name == "list_windows") {
        bridgedCall([this] { emit listWindowsRequested(); }, isError, text);
        return {};
    }
    if (name == "focus_window") {
        const int windowId = args.value("windowId").toInt(-1);
        if (windowId < 0) { isError = true; text = QStringLiteral("windowId (>= 0) ist erforderlich; list_windows liefert die IDs."); return {}; }
        bridgedCall([this, windowId] { emit focusWindowRequested(windowId); }, isError, text);
        return {};
    }
    if (name == "new_window") {
        bridgedCall([this] { emit newWindowRequested(); }, isError, text);
        return {};
    }
    if (name == "rename_window") {
        const int windowId = args.value("windowId").toInt(-1);
        if (windowId < 0) { isError = true; text = QStringLiteral("windowId (>= 0) ist erforderlich."); return {}; }
        const QString wname = args.value("name").toString();
        bridgedCall([this, windowId, &wname] { emit renameWindowRequested(windowId, wname); }, isError, text);
        return {};
    }
    if (name == "close_window") {
        const int windowId = args.value("windowId").toInt(-1);
        if (windowId < 0) { isError = true; text = QStringLiteral("windowId (>= 0) ist erforderlich."); return {}; }
        bridgedCall([this, windowId] { emit closeWindowRequested(windowId); }, isError, text);
        return {};
    }
    if (name == "split_pane") {
        const QString o = args.value("orientation").toString().toLower();
        if (o != QLatin1String("h") && o != QLatin1String("v")) {
            isError = true;
            text = QStringLiteral("orientation muss 'h' oder 'v' sein.");
            return {};
        }
        bridgedCall([this, &o] { emit splitPaneRequested(o); }, isError, text);
        return {};
    }
    if (name == "close_pane") {
        const int paneId = args.value("paneId").toInt(-1);
        bridgedCall([this, paneId] { emit closePaneRequested(paneId); }, isError, text);
        return {};
    }
    if (name == "focus_pane") {
        const int paneId = args.value("paneId").toInt(-1);
        if (paneId < 0) {
            isError = true;
            text = QStringLiteral("paneId (>= 0) ist erforderlich; get_layout liefert die IDs.");
            return {};
        }
        bridgedCall([this, paneId] { emit focusPaneRequested(paneId); }, isError, text);
        return {};
    }
    if (name == "zoom_pane") {
        // paneId >= 0: dieses Pane maximieren; fehlend/-1: Zoom aufheben.
        const int paneId = args.value("paneId").toInt(-1);
        bridgedCall([this, paneId] { emit zoomPaneRequested(paneId); }, isError, text);
        return {};
    }
    if (name == "assign_session") {
        const int row = m_sessions->rowForId(id);
        if (row < 0) { isError = true; text = idProblem(); return {}; }
        const int paneId = args.value("paneId").toInt(-1);
        bridgedCall([this, row, paneId] { emit assignPaneRequested(row, paneId); },
                    isError, text);
        return {};
    }

    // --- Verbindungsprofile (QTMUX-29) -------------------------------------
    if (name == "list_profiles") {
        QJsonArray arr;
        const auto profiles = ConnectionProfileRegistry::instance()->profiles();
        for (const ConnectionProfile &p : profiles) {
            QJsonObject o{{"name", p.name},
                          {"type", p.type == 1 ? QStringLiteral("ssh")
                                 : p.type == 2 ? QStringLiteral("serial")
                                               : QStringLiteral("shell")},
                          // Nur FLAGS — Geheimniswert/Loginscript-Inhalt bleiben intern.
                          {"hasPasswordSecret", !p.passwordSecret.isEmpty()},
                          {"hasLoginScript", !p.loginScript.isEmpty()}};
            if (p.type == 1) {
                o.insert("host", p.host);
                o.insert("port", p.port);
                o.insert("user", p.user);
                o.insert("identity", p.identity);
            } else if (p.type == 2) {
                o.insert("serialPort", p.serialPort);
                o.insert("baud", p.baud);
            } else {
                o.insert("program", p.program);
                o.insert("workingDir", p.workingDir);
            }
            arr.append(o);
        }
        text = QString::fromUtf8(QJsonDocument(arr).toJson(QJsonDocument::Compact));
        return {};
    }
    if (name == "connect_profile") {
        const QString pname = args.value("name").toString();
        if (ConnectionProfileRegistry::instance()->profile(pname).isEmpty()) {
            isError = true;
            text = QStringLiteral("Unbekanntes Profil: %1").arg(pname);
            return {};
        }
        // Über den QML-Weg verbinden (window.connectProfile): löst das Vault-Passwort
        // intern auf und lädt die Session ins aktive Pane — exakt wie der GUI-Klick.
        bridgedCall([this, &pname] { emit connectProfileRequested(pname); }, isError, text);
        return {};
    }

    isError = true;
    text = QStringLiteral("Unbekanntes Tool: %1").arg(name);
    return {};
}

} // namespace qtmux
