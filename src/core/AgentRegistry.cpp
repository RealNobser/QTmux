#include "AgentRegistry.h"

namespace qtmux {

const QList<AgentInfo> &AgentRegistry::all() {
    // Vorlagen nur eintragen, wenn sie am echten CLI gegengeprüft wurden — ein
    // geratenes Flag lässt den Start scheitern und sieht wie ein QTmux-Fehler aus.
    // Verifiziert (2026-07-28/29, `<agent> --help`): claude, agy, opencode, hermes.
    // Einen dokumentierten interaktiven Picker hat bislang nur Claude Code.
    static const QList<AgentInfo> kAgents = {
        {QStringLiteral("claude"),      QStringLiteral("claude"), QStringLiteral("Claude Code"),
         QStringLiteral("--continue"), QStringLiteral("--resume"), QStringLiteral("--resume {id}")},
        {QStringLiteral("codex"),       QStringLiteral("codex"),  QStringLiteral("Codex"),
         {}, {}, {}},
        {QStringLiteral("gemini"),      QStringLiteral("gemini"), QStringLiteral("Gemini"),
         {}, {}, {}},
        {QStringLiteral("antigravity"), QStringLiteral("agy"),    QStringLiteral("AntiGravity"),
         QStringLiteral("--continue"), {}, QStringLiteral("--conversation {id}")},
        {QStringLiteral("aider"),       QStringLiteral("aider"),  QStringLiteral("Aider"),
         {}, {}, {}},
        {QStringLiteral("cursor"),      QStringLiteral("cursor"), QStringLiteral("Cursor"),
         {}, {}, {}},
        {QStringLiteral("qwen"),        QStringLiteral("qwen"),   QStringLiteral("Qwen Coder"),
         {}, {}, {}},
        {QStringLiteral("opencode"),    QStringLiteral("opencode"), QStringLiteral("OpenCode"),
         QStringLiteral("--continue"), {}, QStringLiteral("--session {id}")},
        {QStringLiteral("hermes"),      QStringLiteral("hermes"), QStringLiteral("Hermes"),
         QStringLiteral("--continue"), {}, QStringLiteral("--resume {id}")},
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

/// Vorlage des Modus, oder leer.
static QString templateFor(const AgentInfo &a, ResumeMode mode) {
    switch (mode) {
    case ResumeMode::Last:     return a.resumeLastArgs;
    case ResumeMode::Pick:     return a.resumePickArgs;
    case ResumeMode::Reported: return a.resumeIdArgs;
    case ResumeMode::None:     break;
    }
    return {};
}

bool AgentRegistry::supportsResumeMode(const AgentInfo &a, ResumeMode mode) {
    return !templateFor(a, mode).trimmed().isEmpty();
}

QString AgentRegistry::resumeCommand(const QString &commandLine, ResumeMode mode,
                                     const QString &sessionRef) {
    if (mode == ResumeMode::None) return commandLine;
    int idx = -1;
    const AgentInfo *a = detect(commandLine, &idx);
    if (!a || idx < 0) return commandLine;

    QString tmpl = templateFor(*a, mode).trimmed();
    if (tmpl.isEmpty()) return commandLine;   // Agent kann diesen Weg nicht

    if (tmpl.contains(QLatin1String("{id}"))) {
        // Ohne Referenz NICHT auf einen anderen Weg ausweichen — lieber frisch
        // starten als stillschweigend eine fremde Unterhaltung aufmachen.
        const QString ref = sessionRef.trimmed();
        if (ref.isEmpty()) return commandLine;
        tmpl.replace(QLatin1String("{id}"), ref);
    }

    QStringList tokens = commandLine.trimmed().split(QChar(' '), Qt::SkipEmptyParts);
    const QStringList add = tmpl.split(QChar(' '), Qt::SkipEmptyParts);
    if (add.isEmpty()) return commandLine;
    // Idempotenz: steht das Argument schon da, nicht noch einmal einfügen — sonst
    // sammelt sich über mehrere Neustarts `--continue --continue …` an.
    if (tokens.contains(add.first())) return commandLine;

    for (int k = 0; k < add.size(); ++k) tokens.insert(idx + 1 + k, add.at(k));
    return tokens.join(QChar(' '));
}

} // namespace qtmux
