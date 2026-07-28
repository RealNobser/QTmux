#include "AgentRegistry.h"

namespace qtmux {

const QList<AgentInfo> &AgentRegistry::all() {
    // `resumeArgs` nur eintragen, wenn das Flag am echten CLI gegengeprüft wurde —
    // ein geratenes Flag lässt den Start scheitern und sieht wie ein QTmux-Fehler aus.
    // Verifiziert (2026-07-28, `<agent> --help`): claude, agy, opencode, hermes.
    static const QList<AgentInfo> kAgents = {
        {QStringLiteral("claude"),      QStringLiteral("claude"), QStringLiteral("Claude Code"),
         QStringLiteral("--continue")},
        {QStringLiteral("codex"),       QStringLiteral("codex"),  QStringLiteral("Codex"), {}},
        {QStringLiteral("gemini"),      QStringLiteral("gemini"), QStringLiteral("Gemini"), {}},
        {QStringLiteral("antigravity"), QStringLiteral("agy"),    QStringLiteral("AntiGravity"),
         QStringLiteral("--continue")},
        {QStringLiteral("aider"),       QStringLiteral("aider"),  QStringLiteral("Aider"), {}},
        {QStringLiteral("cursor"),      QStringLiteral("cursor"), QStringLiteral("Cursor"), {}},
        {QStringLiteral("qwen"),        QStringLiteral("qwen"),   QStringLiteral("Qwen Coder"), {}},
        {QStringLiteral("opencode"),    QStringLiteral("opencode"), QStringLiteral("OpenCode"),
         QStringLiteral("--continue")},
        {QStringLiteral("hermes"),      QStringLiteral("hermes"), QStringLiteral("Hermes"),
         QStringLiteral("--continue")},
    };
    return kAgents;
}

const AgentInfo *AgentRegistry::detect(const QString &commandLine, int *tokenIndex) {
    const QString trimmed = commandLine.trimmed();
    if (trimmed.isEmpty()) return nullptr;

    const QStringList tokens = trimmed.split(QChar(' '), Qt::SkipEmptyParts);
    for (int i = 0; i < tokens.size(); ++i) {
        const QString &token = tokens.at(i);
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
            if (base.compare(a.command, Qt::CaseInsensitive) == 0) {
                if (tokenIndex) *tokenIndex = i;
                return &a;
            }
        }
        // Erster echter Kommando-Token, der kein Präfix ist → kein Agent.
        return nullptr;
    }
    return nullptr;
}

QString AgentRegistry::resumeCommand(const QString &commandLine) {
    int idx = -1;
    const AgentInfo *a = detect(commandLine, &idx);
    if (!a || a->resumeArgs.trimmed().isEmpty() || idx < 0) return commandLine;

    QStringList tokens = commandLine.trimmed().split(QChar(' '), Qt::SkipEmptyParts);
    const QStringList resume = a->resumeArgs.split(QChar(' '), Qt::SkipEmptyParts);
    if (resume.isEmpty()) return commandLine;
    // Idempotenz: steht das Fortsetzungs-Argument schon da, nicht noch einmal einfügen —
    // sonst sammelt sich über mehrere Neustarts `--continue --continue …` an.
    if (tokens.contains(resume.first())) return commandLine;

    for (int k = 0; k < resume.size(); ++k) tokens.insert(idx + 1 + k, resume.at(k));
    return tokens.join(QChar(' '));
}

} // namespace qtmux
