#include "McpServer.h"
#include "SessionModel.h"
#include "Session.h"
#include "ProcessInfo.h"
#include "PluginHost.h"

#include <QTcpServer>
#include <QTcpSocket>
#include <QHostAddress>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonValue>
#include <QChar>

namespace qtmux {

static const char *kProtocolVersion = "2024-11-05";

McpServer::McpServer(QObject *parent) : QObject(parent) {}
McpServer::~McpServer() { stop(); }

bool McpServer::listening() const { return m_server && m_server->isListening(); }

void McpServer::setSessions(SessionModel *m) {
    if (m == m_sessions) return;
    m_sessions = m;
    emit sessionsChanged();
}

void McpServer::setPort(int p) {
    if (p == m_port) return;
    m_port = p;
    emit portChanged();
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
        if (m == QLatin1String("initialize") || m == QLatin1String("tools/call"))
            detectController(static_cast<quint16>(sock->peerPort()));
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

void McpServer::detectController(quint16 clientPort) {
    if (!m_sessions) return;
    // Welcher lokale Prozess hält die Verbindung zu unserem Port?
    const qint64 pid = procinfo::pidOfTcpClient(clientPort, static_cast<quint16>(m_port));
    if (pid <= 0) return;
    // Den Client seiner Session zuordnen: seine Vorfahrenkette enthält die
    // Shell-PID genau einer Session (Agent läuft als Kind dieser Shell).
    const QList<qint64> chain = procinfo::ancestorPids(pid);
    for (Session *s : m_sessions->sessions()) {
        const qint64 spid = s->processId();
        if (spid > 0 && chain.contains(spid)) {
            s->setMcpController(true);
            break;
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
            {"serverInfo", QJsonObject{{"name", "QTmux"}, {"version", "1.0.0"}}},
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
                      "agentId, needsAttention, lastNotification, mcpController, workingDir, "
                      "progress).",
                      {}, {}));
    tools.append(tool("create_session",
                      "Erstellt eine Session. type: 'shell' (Standard), 'ssh', 'serial' oder "
                      "'plugin'. Je nach Typ die passenden Felder füllen (siehe unten). Gibt die "
                      "neue Session-ID zurück.",
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
    tools.append(tool("focus_session", "Fokussiert eine Session und lädt sie ins aktive Pane.",
                      QJsonObject{{"id", intProp("Session-ID")}}, QJsonArray{"id"}));
    tools.append(tool("attach_controller",
                      "Markiert die Session mit dieser ID als steuernde MCP-Controller-Session "
                      "(roter Tab in der Sidebar). Üblich: der Agent liest $QTMUX_SESSION_ID "
                      "und ruft dieses Tool mit diesem Wert auf.",
                      QJsonObject{{"id", intProp("Session-ID des steuernden Agenten")}},
                      QJsonArray{"id"}));
    tools.append(tool("send_text",
                      "Sendet Text an eine Session (optional mit Enter). Mit broadcast=true "
                      "geht der Text an ALLE Sessions (id wird dann ignoriert).",
                      QJsonObject{{"id", intProp("Session-ID (entfällt bei broadcast)")},
                                  {"text", strProp("zu sendender Text")},
                                  {"enter", boolProp("Enter anhängen (Standard true)")},
                                  {"broadcast", boolProp("an alle Sessions senden (Standard false)")}},
                      QJsonArray{"text"}));
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
        return QJsonObject{
            {"id", s->id()},
            {"title", s->title()},
            {"type", static_cast<int>(s->type())},
            {"activity", s->activityInt()},
            {"agentId", s->agentId()},
            {"needsAttention", s->needsAttention()},
            {"lastNotification", s->lastNotification()},
            {"mcpController", s->mcpController()},
            {"workingDir", s->workingDirectory()},
            {"progressActive", s->progressActive()},
            {"progressState", s->progressState()},
            {"progressValue", s->progressValue()},
        };
    };

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
    if (name == "close_session") {
        const int row = m_sessions->rowForId(id);
        if (row < 0) { isError = true; text = QStringLiteral("Unbekannte ID."); return {}; }
        m_sessions->closeSession(row);
        text = QStringLiteral("ok");
        return {};
    }
    if (name == "focus_session") {
        const int row = m_sessions->rowForId(id);
        if (row < 0) { isError = true; text = QStringLiteral("Unbekannte ID."); return {}; }
        emit focusRequested(row);
        text = QStringLiteral("ok");
        return {};
    }
    if (name == "attach_controller") {
        if (!s) { isError = true; text = QStringLiteral("Unbekannte ID."); return {}; }
        s->setMcpController(true);   // roter Tab; gilt bis zum Schließen der Session
        text = QStringLiteral("ok");
        return {};
    }
    if (name == "send_text") {
        if (!s) { isError = true; text = QStringLiteral("Unbekannte ID."); return {}; }
        QByteArray data = args.value("text").toString().toUtf8();
        if (args.value("enter").toBool(true)) data += '\r';
        s->write(data);
        text = QStringLiteral("ok");
        return {};
    }
    if (name == "read_screen") {
        if (!s) { isError = true; text = QStringLiteral("Unbekannte ID."); return {}; }
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

    isError = true;
    text = QStringLiteral("Unbekanntes Tool: %1").arg(name);
    return {};
}

} // namespace qtmux
