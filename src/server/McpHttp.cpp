#include "McpHttp.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QUrl>

namespace qtmux::mcphttp {

QByteArray Head::value(const char *lowerName) const {
    for (const auto &h : headers)
        if (h.first == lowerName) return h.second;
    return {};
}

int Head::count(const char *lowerName) const {
    int n = 0;
    for (const auto &h : headers)
        if (h.first == lowerName) ++n;
    return n;
}

namespace {

HeadResult fail(int status, const QByteArray &msg) {
    HeadResult r;
    r.state = HeadState::Error;
    r.status = status;
    r.message = msg;
    return r;
}

bool isTokenChar(char c) {
    // RFC 9110 „tchar“
    if ((c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')) return true;
    switch (c) {
    case '!': case '#': case '$': case '%': case '&': case '\'': case '*': case '+':
    case '-': case '.': case '^': case '_': case '`': case '|': case '~':
        return true;
    default:
        return false;
    }
}

// Kopfzeilenwert: keine Steuerzeichen außer Tab, kein DEL. Bytes ab 0x80 lässt die
// Spezifikation für fremde Kopfzeilen zu (obs-text); MCP-eigene prüft decodeHeaderText
// strenger.
bool isFieldValue(const QByteArray &v) {
    for (char ch : v) {
        const auto c = static_cast<unsigned char>(ch);
        if ((c < 0x20 && c != '\t') || c == 0x7F) return false;
    }
    return true;
}

bool isPrintableAscii(const QByteArray &v) {
    for (char ch : v) {
        const auto c = static_cast<unsigned char>(ch);
        if (c < 0x20 || c > 0x7E) return false;
    }
    return true;
}

} // namespace

HeadResult parseHead(const QByteArray &buf) {
    const int end = buf.indexOf("\r\n\r\n");
    if (end < 0) {
        if (buf.size() > kMaxHeaderBytes)
            return fail(431, "Kopfteil zu groß");
        return {};
    }
    if (end > kMaxHeaderBytes) return fail(431, "Kopfteil zu groß");

    HeadResult r;
    Head &h = r.head;
    h.rawHeader = buf.left(end);
    h.bodyStart = end + 4;
    const QList<QByteArray> lines = h.rawHeader.split('\n');
    if (lines.size() > kMaxHeaderLines + 1) return fail(431, "Zu viele Kopfzeilen");

    // Anfragezeile: METHODE SP ZIEL SP HTTP/1.x
    QByteArray first = lines.first();
    if (first.endsWith('\r')) first.chop(1);
    const QList<QByteArray> parts = first.split(' ');
    if (parts.size() != 3 || !parts.at(2).startsWith("HTTP/1.")) return fail(400, "Ungültige Anfragezeile");
    for (char c : parts.at(0))
        if (!isTokenChar(c)) return fail(400, "Ungültige Methode");
    QByteArray target = parts.at(1);
    if (!isPrintableAscii(target)) return fail(400, "Ungültiges Ziel");
    // Absolute Form (RFC 9112 §3.2.2, „POST http://host/mcp“): nur der Pfad zählt.
    if (target.startsWith("http://") || target.startsWith("https://")) {
        const QUrl u(QString::fromLatin1(target), QUrl::StrictMode);
        if (!u.isValid()) return fail(400, "Ungültiges Ziel");
        target = u.path(QUrl::FullyEncoded).toLatin1();
        if (target.isEmpty()) target = "/";
    }
    if (!target.startsWith('/')) return fail(400, "Ungültiges Ziel");
    h.method = parts.at(0);
    const int q = target.indexOf('?');
    h.path = q < 0 ? target : target.left(q);

    for (int i = 1; i < lines.size(); ++i) {
        QByteArray l = lines.at(i);
        // Zeilenende CRLF; die letzte Zeile steht vor "\r\n\r\n" und trägt darum keins
        // mehr. Ein CR mitten in der Zeile fällt unten durch die Zeichenprüfung.
        if (l.endsWith('\r')) l.chop(1);
        const int colon = l.indexOf(':');
        if (colon <= 0) return fail(400, "Ungültige Kopfzeile");
        const QByteArray name = l.left(colon);
        for (char c : name)
            if (!isTokenChar(c)) return fail(400, "Ungültiger Kopfzeilenname");
        const QByteArray value = l.mid(colon + 1).trimmed();
        if (!isFieldValue(value)) return fail(400, "Steuerzeichen in einer Kopfzeile");
        h.headers.append({name.toLower(), value});
    }

    if (h.count("transfer-encoding") > 0) return fail(501, "Transfer-Encoding wird nicht unterstützt");
    const int clCount = h.count("content-length");
    if (clCount > 1) return fail(400, "Content-Length mehrfach");
    if (clCount == 1) {
        const QByteArray v = h.value("content-length");
        if (v.isEmpty() || v.size() > 10) return fail(400, "Ungültige Content-Length");
        for (char c : v)
            if (c < '0' || c > '9') return fail(400, "Ungültige Content-Length");
        const qint64 n = v.toLongLong();
        if (n > kMaxBodyBytes) return fail(413, "Request too large");
        h.contentLength = static_cast<int>(n);
    }
    r.state = HeadState::Ready;
    return r;
}

bool originAllowed(const QByteArray &origin) {
    if (origin.isEmpty()) return true;
    if (!isPrintableAscii(origin)) return false;
    const QUrl u(QString::fromLatin1(origin), QUrl::StrictMode);
    if (!u.isValid()) return false;
    const QString scheme = u.scheme().toLower();
    if (scheme != QLatin1String("http") && scheme != QLatin1String("https")) return false;
    if (!u.path().isEmpty() || u.hasQuery() || u.hasFragment() || !u.userInfo().isEmpty())
        return false;
    const QString host = u.host().toLower();
    return host == QLatin1String("localhost") || host == QLatin1String("127.0.0.1")
           || host == QLatin1String("::1");
}

bool isMcpPath(const QByteArray &path) {
    return path == "/mcp" || path == "/mcp/";
}

bool isPostPath(const QByteArray &path) {
    return isMcpPath(path) || path == "/";
}

bool acceptsEventStream(const QByteArray &accept) {
    return accept.toLower().contains("text/event-stream");
}

std::optional<QString> decodeHeaderText(const QByteArray &value) {
    if (!isPrintableAscii(value)) return std::nullopt;
    if (value.startsWith("=?base64?") && value.endsWith("?=") && value.size() >= 11) {
        const QByteArray b64 = value.mid(9, value.size() - 11);
        const auto dec = QByteArray::fromBase64Encoding(b64, QByteArray::AbortOnBase64DecodingErrors);
        if (!dec) return std::nullopt;
        const QString s = QString::fromUtf8(*dec);
        if (s.toUtf8() != *dec) return std::nullopt;   // kein gültiges UTF-8
        return s;
    }
    return QString::fromLatin1(value);
}

bool isDateVersion(const QByteArray &v) {
    // Fassungen sind Daten der Form JJJJ-MM-TT; nur dann trägt der Textvergleich.
    if (v.size() != 10 || v.at(4) != '-' || v.at(7) != '-') return false;
    for (int i : {0, 1, 2, 3, 5, 6, 8, 9})
        if (v.at(i) < '0' || v.at(i) > '9') return false;
    return true;
}

bool isLegacyVersion(const QByteArray &v) {
    return v == "2024-11-05" || v == "2025-03-26" || v == "2025-06-18" || v == "2025-11-25";
}

Era classify(const Head &head, const QJsonDocument &doc, bool parsed) {
    Era e;
    const QByteArray hv = head.value("mcp-protocol-version");
    const QJsonObject obj = doc.isObject() ? doc.object() : QJsonObject{};
    const QString method = obj.value(QStringLiteral("method")).toString();
    const QJsonObject params = obj.value(QStringLiteral("params")).toObject();
    const QJsonValue metaVersion =
        params.value(QStringLiteral("_meta")).toObject().value(QLatin1String(kMetaProtocolVersion));
    e.id = obj.value(QStringLiteral("id"));
    if (e.id.isUndefined()) e.id = QJsonValue::Null;

    auto reject = [&e](int status, int code, const QString &msg, const QJsonValue &data = {}) {
        e.kind = Era::Reject;
        e.httpStatus = status;
        e.code = code;
        e.message = msg;
        e.data = data;
        return e;
    };

    // `initialize` wählt immer die alte Fassung (Spezifikation: „An initialize request
    // selects legacy semantics“).
    if (method == QLatin1String("initialize")) return e;

    // Neue Fassung: _meta trägt eine Fassung, oder die Kopfzeile nennt ein DATUM ab
    // 2026-07-28 (nur bei JJJJ-MM-TT trägt der Textvergleich). Ältere Daten und alles, was
    // kein Datum ist, bleiben auf dem alten Weg, wie bisher.
    const bool modern = !metaVersion.isUndefined()
                        || (isDateVersion(hv) && !isLegacyVersion(hv) && hv >= QByteArray(kModernVersion));
    if (!modern) return e;   // alter Weg, unverändert

    if (!parsed) return reject(400, kParseError, QStringLiteral("Parse error"));
    if (doc.isArray())
        return reject(400, kInvalidRequest,
                      QStringLiteral("Stapelaufrufe gibt es in Fassung 2026-07-28 nicht."));
    if (!doc.isObject()) return reject(400, kInvalidRequest, QStringLiteral("Invalid Request"));
    // Benachrichtigungen: Für sie definiert die Spezifikation keine Pflicht-Kopfzeilen.
    if (!obj.contains(QStringLiteral("id"))) {
        e.kind = Era::Modern;
        return e;
    }

    if (hv.isEmpty() || head.count("mcp-protocol-version") != 1)
        return reject(400, kHeaderMismatch,
                      QStringLiteral("Header mismatch: MCP-Protocol-Version fehlt oder steht mehrfach."));
    if (!metaVersion.isString())
        return reject(400, kHeaderMismatch,
                      QStringLiteral("Header mismatch: _meta[\"%1\"] fehlt.").arg(QLatin1String(kMetaProtocolVersion)));
    const QString mv = metaVersion.toString();
    if (QString::fromLatin1(hv) != mv)
        return reject(400, kHeaderMismatch,
                      QStringLiteral("Header mismatch: MCP-Protocol-Version weicht von _meta ab."));
    if (mv != QLatin1String(kModernVersion))
        return reject(400, kUnsupportedProtocolVersion, QStringLiteral("Unsupported protocol version"),
                      QJsonObject{{"supported", QJsonArray{QLatin1String(kModernVersion)}},
                                  {"requested", mv.left(64)}});

    if (head.count("mcp-method") != 1 || QString::fromLatin1(head.value("mcp-method")) != method)
        return reject(400, kHeaderMismatch,
                      QStringLiteral("Header mismatch: Mcp-Method fehlt oder weicht von method ab."));

    QString nameInBody;
    bool needsName = false;
    if (method == QLatin1String("tools/call")) {
        needsName = true;
        nameInBody = params.value(QStringLiteral("name")).toString();
    } else if (method == QLatin1String("tasks/get") || method == QLatin1String("tasks/cancel")
               || method == QLatin1String("tasks/update")) {
        needsName = true;
        nameInBody = params.value(QStringLiteral("taskId")).toString();
    }
    if (needsName) {
        const auto decoded = head.count("mcp-name") == 1 ? decodeHeaderText(head.value("mcp-name"))
                                                         : std::nullopt;
        if (!decoded || *decoded != nameInBody)
            return reject(400, kHeaderMismatch,
                          QStringLiteral("Header mismatch: Mcp-Name fehlt oder weicht vom Rumpf ab."));
    }
    e.kind = Era::Modern;
    return e;
}

QByteArray errorBody(const QJsonValue &id, int code, const QString &message, const QJsonValue &data) {
    QJsonObject err{{"code", code}, {"message", message}};
    if (!data.isUndefined() && !data.isNull()) err.insert(QStringLiteral("data"), data);
    const QJsonObject o{{"jsonrpc", "2.0"}, {"id", id.isUndefined() ? QJsonValue::Null : id},
                        {"error", err}};
    return QJsonDocument(o).toJson(QJsonDocument::Compact);
}

QByteArray reasonPhrase(int status) {
    switch (status) {
    case 200: return "OK";
    case 202: return "Accepted";
    case 400: return "Bad Request";
    case 401: return "Unauthorized";
    case 403: return "Forbidden";
    case 404: return "Not Found";
    case 405: return "Method Not Allowed";
    case 413: return "Payload Too Large";
    case 431: return "Request Header Fields Too Large";
    case 501: return "Not Implemented";
    default: return "Bad Request";
    }
}

} // namespace qtmux::mcphttp
