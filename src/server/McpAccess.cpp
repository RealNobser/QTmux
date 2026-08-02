#include "McpAccess.h"

#include <QCoreApplication>
#include <QRandomGenerator>
#include <QSettings>

namespace qtmux::mcpaccess {

Bind resolveBind(const QString &raw) {
    Bind b;
    b.requested = raw;
    const QString t = raw.trimmed();
    if (t.isEmpty()) return b;                       // Vorgabe: Loopback

    const QString lower = t.toLower();
    if (lower == QLatin1String("localhost")) return b;
    if (lower == QLatin1String("any") || t == QLatin1String("*")) {
        b.address = QHostAddress(QHostAddress::Any);
        return b;
    }

    QHostAddress addr;
    if (!addr.setAddress(t)) {
        b.error = QCoreApplication::translate(
            "McpAccess",
            "Ungültige Bind-Adresse „%1“. Erlaubt sind IP-Literale (z. B. 127.0.0.1, "
            "0.0.0.0, 192.168.0.10) sowie localhost/any. Es bleibt bei 127.0.0.1.")
            .arg(t);
        return b;                                    // address bleibt Loopback
    }
    b.address = addr;
    return b;
}

QString configuredBindText(bool *fromEnv) {
    if (fromEnv) *fromEnv = false;
    if (qEnvironmentVariableIsSet("QTMUX_MCP_BIND")) {
        const QString v = qEnvironmentVariable("QTMUX_MCP_BIND").trimmed();
        if (!v.isEmpty()) {
            if (fromEnv) *fromEnv = true;
            return v;
        }
    }
    return QSettings().value(QStringLiteral("mcp/bindAddress"), QString()).toString().trimmed();
}

Bind defaultBind() {
    bool fromEnv = false;
    Bind b = resolveBind(configuredBindText(&fromEnv));
    b.fromEnvironment = fromEnv;
    return b;
}

QString resolveToken() {
    if (qEnvironmentVariableIsSet("QTMUX_MCP_TOKEN")) {
        const QString v = qEnvironmentVariable("QTMUX_MCP_TOKEN").trimmed();
        if (!v.isEmpty()) return v;
    }
    return QSettings().value(QStringLiteral("mcp/token"), QString()).toString().trimmed();
}

bool tokenFromEnvironment() {
    return qEnvironmentVariableIsSet("QTMUX_MCP_TOKEN")
           && !qEnvironmentVariable("QTMUX_MCP_TOKEN").trimmed().isEmpty();
}

QString generateToken() {
    // 32 Byte aus der Systemquelle (QRandomGenerator::system() = getrandom/BCrypt, NICHT
    // der schnelle Mersenne-Twister von QRandomGenerator::global()).
    quint32 words[8];
    QRandomGenerator::system()->fillRange(words);
    QByteArray raw(reinterpret_cast<const char *>(words), int(sizeof(words)));
    return QString::fromLatin1(
        raw.toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals));
}

bool tokenAccepted(const QByteArray &presented, const QByteArray &expected) {
    if (expected.isEmpty()) return false;
    // Über die volle Länge beider Puffer laufen und die Abweichung erst am Ende
    // auswerten. Die Längendifferenz fließt in `diff` ein, statt früh abzubrechen.
    quint32 diff = static_cast<quint32>(presented.size() ^ expected.size());
    const qsizetype n = qMax(presented.size(), expected.size());
    for (qsizetype i = 0; i < n; ++i) {
        const quint8 a = i < presented.size() ? static_cast<quint8>(presented.at(i)) : 0u;
        const quint8 b = i < expected.size() ? static_cast<quint8>(expected.at(i)) : 0u;
        diff |= static_cast<quint32>(a ^ b);
    }
    return diff == 0;
}

QByteArray bearerToken(const QByteArray &headerBlock) {
    for (const QByteArray &line : headerBlock.split('\n')) {
        const QByteArray l = line.trimmed();
        if (!l.toLower().startsWith("authorization:")) continue;
        const QByteArray value = l.mid(l.indexOf(':') + 1).trimmed();
        if (!value.toLower().startsWith("bearer")) return {};
        const QByteArray rest = value.mid(6).trimmed();   // "Bearer" + Trenner
        return rest;
    }
    return {};
}

StartCheck checkStart(const Bind &bind, const QString &token) {
    StartCheck c;
    c.authRequired = authRequiredFor(bind);
    if (c.authRequired && token.isEmpty()) {
        c.allowed = false;
        c.reason = QCoreApplication::translate(
            "McpAccess",
            "MCP-Server nicht gestartet: Bindung an %1 macht ihn über das Netz "
            "erreichbar, es ist aber kein Zugriffs-Token gesetzt. Über MCP lässt sich "
            "beliebiger Text in laufende Terminals schreiben — ein Token ist deshalb "
            "Pflicht. Setzen in den Einstellungen (Agenten → MCP-Server) oder per "
            "QTMUX_MCP_TOKEN.")
            .arg(bind.text());
    }
    return c;
}

} // namespace qtmux::mcpaccess
