#pragma once

#include <QByteArray>
#include <QJsonDocument>
#include <QJsonValue>
#include <QList>
#include <QPair>
#include <QString>

#include <optional>

namespace qtmux::mcphttp {

/// Gui-freie HTTP-Schicht des MCP-Servers: Kopfteil zerlegen und begrenzen, Origin und
/// Pfad prüfen, und je Anfrage entscheiden, welche Protokollfassung sie spricht.
///
/// Zwei Fassungen nebeneinander (Spezifikation 2026-07-28, „dual-era server“):
///  - **alt**: Client verbindet mit `initialize` (QTmux bot bisher fest 2024-11-05).
///    Verhalten unverändert, auch Stapel und HTTP 200 bei unbekannter Methode.
///  - **neu** (2026-07-28, zustandslos): jede Anfrage trägt die Fassung in
///    `params._meta["io.modelcontextprotocol/protocolVersion"]` und in der Kopfzeile
///    `MCP-Protocol-Version`. Fehler mit HTTP-Status und Code nach Spezifikation.

inline constexpr int kMaxHeaderBytes = 16 * 1024;
inline constexpr int kMaxHeaderLines = 64;
inline constexpr int kMaxBodyBytes = 4 * 1024 * 1024;
inline constexpr char kModernVersion[] = "2026-07-28";
inline constexpr char kTasksExtension[] = "io.modelcontextprotocol/tasks";
inline constexpr char kMetaProtocolVersion[] = "io.modelcontextprotocol/protocolVersion";
inline constexpr char kMetaClientCapabilities[] = "io.modelcontextprotocol/clientCapabilities";
inline constexpr char kMetaServerInfo[] = "io.modelcontextprotocol/serverInfo";

// JSON-RPC-Codes der Revision 2026-07-28 (Schema, „Error-code policy“).
inline constexpr int kParseError = -32700;
inline constexpr int kInvalidRequest = -32600;
inline constexpr int kMethodNotFound = -32601;
inline constexpr int kInvalidParams = -32602;
inline constexpr int kHeaderMismatch = -32020;
inline constexpr int kMissingClientCapability = -32021;
inline constexpr int kUnsupportedProtocolVersion = -32022;

struct Head {
    QByteArray method;      ///< z. B. "POST"
    QByteArray path;        ///< ohne Abfrageteil
    QByteArray rawHeader;   ///< Anfragezeile + Kopfzeilen, ohne Leerzeile
    QList<QPair<QByteArray, QByteArray>> headers;   ///< Name klein geschrieben
    int contentLength = 0;
    int bodyStart = 0;      ///< Offset des Rumpfs im Puffer

    /// Wert der Kopfzeile (leer, wenn sie fehlt). Bei mehrfachem Vorkommen der erste.
    QByteArray value(const char *lowerName) const;
    int count(const char *lowerName) const;
};

enum class HeadState { Incomplete, Ready, Error };

struct HeadResult {
    HeadState state = HeadState::Incomplete;
    Head head;
    int status = 0;          ///< HTTP-Status bei Error
    QByteArray message;      ///< Klartext bei Error
};

/// Zerlegt den Kopfteil aus `buf`. Incomplete, solange das Kopfende fehlt und die
/// Grenzen nicht überschritten sind. Prüft: Anfragezeile, höchstens kMaxHeaderBytes
/// und kMaxHeaderLines, keine Steuerzeichen in Kopfzeilen, `Content-Length` höchstens
/// einmal und nur Ziffern, kein `Transfer-Encoding` (wird nicht unterstützt).
HeadResult parseHead(const QByteArray &buf);

/// Origin-Regel (DNS-Rebinding): fehlt der Kopf, ist es kein Browser → erlaubt.
/// Vorhanden → nur http(s) auf localhost, 127.0.0.1 oder [::1], beliebiger Port.
bool originAllowed(const QByteArray &origin);

/// Der MCP-Endpunkt (`/mcp`, `/mcp/`).
bool isMcpPath(const QByteArray &path);
/// Pfade, auf denen POST angenommen wird: der MCP-Endpunkt und `/`.
bool isPostPath(const QByteArray &path);
/// Verlangt dieser `Accept`-Kopf einen Ereignisstrom (GET-Stream eines MCP-Clients)?
bool acceptsEventStream(const QByteArray &accept);

/// Wert einer MCP-Kopfzeile als Text: entweder druckbares ASCII oder
/// `=?base64?<daten>?=` (UTF-8). nullopt bei ungültiger Form.
std::optional<QString> decodeHeaderText(const QByteArray &value);

/// Hat die Fassung die Form JJJJ-MM-TT?
bool isDateVersion(const QByteArray &version);
/// Ältere Fassungen, die über `initialize` laufen.
bool isLegacyVersion(const QByteArray &version);

struct Era {
    enum Kind { Legacy, Modern, Reject };
    Kind kind = Legacy;
    int httpStatus = 200;    ///< bei Reject
    int code = 0;            ///< JSON-RPC-Code bei Reject
    QString message;
    QJsonValue data;         ///< optional (z. B. supported/requested)
    QJsonValue id;           ///< id der Anfrage, soweit lesbar
};

/// Entscheidet die Fassung einer POST-Anfrage und prüft die Pflichten der neuen
/// Fassung (Kopfzeilen gegen den Rumpf, Fassung, Stapel). `doc` ist der geparste Rumpf
/// (bei Parse-Fehler ein leeres Dokument, `parsed=false`).
Era classify(const Head &head, const QJsonDocument &doc, bool parsed);

/// JSON-RPC-Fehlerantwort als kompaktes JSON.
QByteArray errorBody(const QJsonValue &id, int code, const QString &message,
                     const QJsonValue &data = QJsonValue::Undefined);

/// HTTP-Statustext.
QByteArray reasonPhrase(int status);

} // namespace qtmux::mcphttp
