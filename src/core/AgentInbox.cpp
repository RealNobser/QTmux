#include "AgentInbox.h"

#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QStringList>

namespace qtmux::agentinbox {

bool inboxKindKnown(const QString &kind) {
    return kind == QLatin1String(kKindClaudeCode);
}

QString endpointProblem(const QString &kind, const QString &address) {
    if (!inboxKindKnown(kind))
        return QCoreApplication::translate(
                   "AgentInbox", "Unbekannte Art „%1“ (bekannt: claude-code).").arg(kind);
    if (address.isEmpty())
        return QCoreApplication::translate("AgentInbox", "Die Adresse fehlt.");
    for (const QChar c : address)
        if (c.unicode() < 0x20 || c.unicode() == 0x7f)
            return QCoreApplication::translate("AgentInbox",
                                               "Die Adresse enthält Steuerzeichen.");
    if (!address.startsWith(QLatin1Char('/')))
        return QCoreApplication::translate(
            "AgentInbox", "Die Adresse muss ein absoluter Pfad auf einen Unix-Socket sein.");
    if (!address.endsWith(QLatin1String(".sock")))
        return QCoreApplication::translate("AgentInbox",
                                           "Die Adresse muss auf „.sock“ enden.");
    if (address.split(QLatin1Char('/')).contains(QLatin1String("..")))
        return QCoreApplication::translate("AgentInbox",
                                           "Die Adresse darf kein „..“ enthalten.");
    if (address.toUtf8().size() > 103)
        return QCoreApplication::translate(
            "AgentInbox", "Die Adresse ist zu lang für einen Unix-Socket (höchstens 103 Bytes).");
    return {};
}

QString frameSenderName(const QString &raw) {
    QString out;
    bool lastSpace = true;   // führende Leerzeichen gar nicht erst aufnehmen
    for (const QChar c : raw) {
        const bool keep = c.isLetterOrNumber()
                          || QStringLiteral("-_.,:()#/+").contains(c);
        if (keep) { out += c; lastSpace = false; }
        else if (!lastSpace) { out += QLatin1Char(' '); lastSpace = true; }
    }
    out = out.trimmed();
    if (out.size() > 60) out = out.left(60).trimmed();
    return out;
}

QByteArray claudeCodeFrame(const QString &text, const QString &senderName, const QString &msgId) {
    static const QString tag = QStringLiteral("cross-session-message");
    // Ende-Tag im Text entschärfen: Der Empfänger liest den Rahmen mit einem Muster, das
    // am LETZTEN Ende-Tag endet, und prüft ihn gegen seine eigene Maskierung — ein
    // Ende-Tag im Text ließe ihn die Absenderangabe verwerfen. Aus `</` wird `‹/`.
    static const QRegularExpression closing(
        QStringLiteral("<(\\s*/\\s*cross-session-message)"),
        QRegularExpression::CaseInsensitiveOption);
    QString body = text;
    body.replace(closing, QStringLiteral("‹\\1"));

    const QString name = frameSenderName(senderName);
    QString content = QLatin1Char('<') + tag;
    if (!name.isEmpty()) content += QStringLiteral(" from-name=\"%1\"").arg(name);
    content += QStringLiteral(">\n") + body + QStringLiteral("\n</") + tag + QLatin1Char('>');

    const QJsonObject frame{
        {QStringLiteral("msgV"), 1},
        {QStringLiteral("msg_id"), msgId},
        {QStringLiteral("type"), QStringLiteral("user")},
        {QStringLiteral("message"),
         QJsonObject{{QStringLiteral("role"), QStringLiteral("user")},
                     {QStringLiteral("content"), content}}},
        {QStringLiteral("priority"), QStringLiteral("next")},
    };
    // Compact = genau eine Zeile; Zeilenumbrüche im Text stehen als \n-Escape darin.
    return QJsonDocument(frame).toJson(QJsonDocument::Compact) + '\n';
}

int compareVersions(const QString &a, const QString &b) {
    static const QRegularExpression shape(QStringLiteral("^\\d+(\\.\\d+){0,3}$"));
    if (!shape.match(a).hasMatch() || !shape.match(b).hasMatch()) return 0;
    const QStringList pa = a.split(QLatin1Char('.')), pb = b.split(QLatin1Char('.'));
    for (int i = 0; i < qMax(pa.size(), pb.size()); ++i) {
        const int x = i < pa.size() ? pa.at(i).toInt() : 0;
        const int y = i < pb.size() ? pb.at(i).toInt() : 0;
        if (x != y) return x < y ? -1 : 1;
    }
    return 0;
}

} // namespace qtmux::agentinbox
