#include "AgentRegistry.h"

namespace qtmux {

const QList<AgentInfo> &AgentRegistry::all() {
    static const QList<AgentInfo> kAgents = {
        {QStringLiteral("claude"),      QStringLiteral("claude"), QStringLiteral("Claude Code")},
        {QStringLiteral("codex"),       QStringLiteral("codex"),  QStringLiteral("Codex")},
        {QStringLiteral("gemini"),      QStringLiteral("gemini"), QStringLiteral("Gemini")},
        {QStringLiteral("antigravity"), QStringLiteral("agy"),    QStringLiteral("AntiGravity")},
        {QStringLiteral("aider"),       QStringLiteral("aider"),  QStringLiteral("Aider")},
        {QStringLiteral("cursor"),      QStringLiteral("cursor"), QStringLiteral("Cursor")},
        {QStringLiteral("qwen"),        QStringLiteral("qwen"),   QStringLiteral("Qwen Coder")},
        {QStringLiteral("opencode"),    QStringLiteral("opencode"), QStringLiteral("OpenCode")},
    };
    return kAgents;
}

const AgentInfo *AgentRegistry::detect(const QString &commandLine) {
    const QString trimmed = commandLine.trimmed();
    if (trimmed.isEmpty()) return nullptr;

    const QStringList tokens = trimmed.split(QChar(' '), Qt::SkipEmptyParts);
    for (const QString &token : tokens) {
        // Umgebungs-Präfixe und Variablenzuweisungen überspringen (env, VAR=val).
        if (token == QLatin1String("env") || token.contains('=')) continue;
        if (token == QLatin1String("sudo") || token == QLatin1String("command")) continue;

        // Nur den Basisnamen ohne Pfad vergleichen (Unix '/' wie Windows '\').
        QString base = token;
        const int slash = qMax(base.lastIndexOf(QLatin1Char('/')),
                               base.lastIndexOf(QLatin1Char('\\')));
        if (slash >= 0) base = base.mid(slash + 1);
        // Auf Windows endet das Programm oft auf .exe/.cmd/.bat — abschneiden.
        for (const QString &suffix : {QStringLiteral(".exe"), QStringLiteral(".cmd"),
                                      QStringLiteral(".bat")}) {
            if (base.endsWith(suffix, Qt::CaseInsensitive)) {
                base.chop(suffix.size());
                break;
            }
        }

        for (const AgentInfo &a : all()) {
            if (base.compare(a.command, Qt::CaseInsensitive) == 0) return &a;
        }
        // Erster echter Kommando-Token, der kein Präfix ist → kein Agent.
        return nullptr;
    }
    return nullptr;
}

} // namespace qtmux
