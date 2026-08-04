#include "AgentRegistry.h"

namespace qtmux {

bool AgentInfo::matches(const QString &base) const {
    if (base.compare(command, Qt::CaseInsensitive) == 0) return true;
    for (const QString &a : aliases)
        if (base.compare(a, Qt::CaseInsensitive) == 0) return true;
    return false;
}

const QList<AgentInfo> &AgentRegistry::all() {
    // Vorlagen nur eintragen, wenn sie am echten CLI gegengeprüft wurden — ein
    // geratenes Flag lässt den Start scheitern und sieht wie ein QTmux-Fehler aus.
    // Verifiziert an `<agent> --help`: claude, agy, opencode, hermes (2026-07-28/29),
    // codex (2026-07-31, alle drei Wege). Alle übrigen bleiben bewusst leer und
    // starten damit frisch. Einen Picker per Kommandozeile haben nur claude und codex.
    //
    // Designierte Initialisierer (C++20): Ein neues Feld verschiebt damit keine Werte
    // mehr still nach hinten — vorher hätte ein Feld in der Mitte alle 9 Einträge
    // lautlos verdreht.
    static const QList<AgentInfo> kAgents = {
        {.id = QStringLiteral("claude"), .command = QStringLiteral("claude"),
         .displayName = QStringLiteral("Claude Code"),
         .resumeLastArgs = QStringLiteral("--continue"),
         .resumePickArgs = QStringLiteral("--resume"),
         .resumeIdArgs   = QStringLiteral("--resume {id}")},

        // Codex fortsetzt über ein UNTERKOMMANDO, nicht über ein Flag: `codex resume`
        // zeigt den Picker, `--last` nimmt die jüngste, ein positionales SESSION_ID die
        // gemeldete. `codex resume` nimmt dieselben Optionen wie der Direktstart an
        // (-m/--model, -p/--profile, -s/--sandbox … am `--help` geprüft), eine bereits
        // getippte Optionsliste bleibt also gültig.
        // 🔑 Codex scrollt seinen Verlauf mit **Shift+Pfeil**, NICHT mit einfachen
        // Cursor-Tasten — obwohl es DECSET 1007 setzt und damit genau die anfordert.
        // Am laufenden Codex 0.146.0 gemessen (2026-08-03, isolierte Instanz): Shift+Up
        // scrollt zeilenweise hoch, 3× Shift+Down führt exakt zurück; ESC[A, ESC O A,
        // PageUp/PageDown und End bewirken **nichts**. Ohne diesen Eintrag wäre die
        // 1007-Auswertung für den Anwender wirkungslos geblieben.
        {.id = QStringLiteral("codex"), .command = QStringLiteral("codex"),
         .displayName = QStringLiteral("Codex"),
         .resumeLastArgs = QStringLiteral("resume --last"),
         .resumePickArgs = QStringLiteral("resume"),
         .resumeIdArgs   = QStringLiteral("resume {id}"),
         .scrollUpKeys   = QByteArrayLiteral("\x1b[1;2A"),
         .scrollDownKeys = QByteArrayLiteral("\x1b[1;2B")},

        {.id = QStringLiteral("gemini"), .command = QStringLiteral("gemini"),
         .displayName = QStringLiteral("Gemini")},

        {.id = QStringLiteral("antigravity"), .command = QStringLiteral("agy"),
         .displayName = QStringLiteral("AntiGravity"),
         .resumeLastArgs = QStringLiteral("--continue"),
         .resumeIdArgs   = QStringLiteral("--conversation {id}")},

        {.id = QStringLiteral("aider"), .command = QStringLiteral("aider"),
         .displayName = QStringLiteral("Aider")},

        // ⚠️ NICHT `cursor` (QTMUX-88): das ist der Editor-Starter, wie `code` bei VS Code.
        // Der Agent hieß bei Einführung `cursor-agent`; die Dokumentation installiert
        // inzwischen `agent` (cursor.com/docs/cli/installation, geprüft 2026-07-31) —
        // deshalb beide Namen. `agent` ist zugegeben generisch: nennt jemand ein eigenes
        // Skript so, bekommt dessen Session das Cursor-Etikett. Folgen sind begrenzt
        // (Titel/Icon; die Fortsetzungs-Vorlagen sind leer, es wird also kein Flag
        // untergeschoben) — und ein Streichen kostet ein Wort.
        {.id = QStringLiteral("cursor"), .command = QStringLiteral("cursor-agent"),
         .displayName = QStringLiteral("Cursor"),
         .aliases = {QStringLiteral("agent")}},

        {.id = QStringLiteral("qwen"), .command = QStringLiteral("qwen"),
         .displayName = QStringLiteral("Qwen Coder")},

        {.id = QStringLiteral("opencode"), .command = QStringLiteral("opencode"),
         .displayName = QStringLiteral("OpenCode"),
         .resumeLastArgs = QStringLiteral("--continue"),
         .resumeIdArgs   = QStringLiteral("--session {id}")},

        {.id = QStringLiteral("hermes"), .command = QStringLiteral("hermes"),
         .displayName = QStringLiteral("Hermes"),
         .resumeLastArgs = QStringLiteral("--continue"),
         .resumeIdArgs   = QStringLiteral("--resume {id}")},

        // --- 2026-07-31 ergänzt (QTMUX-88) -------------------------------------
        // Kommandonamen an der jeweiligen Primärquelle geprüft, Fortsetzungs-Vorlagen
        // NICHT (die CLIs sind hier nicht installiert) → bewusst leer.
        {.id = QStringLiteral("copilot"), .command = QStringLiteral("copilot"),
         .displayName = QStringLiteral("Copilot CLI")},

        {.id = QStringLiteral("crush"), .command = QStringLiteral("crush"),
         .displayName = QStringLiteral("Crush")},

        {.id = QStringLiteral("goose"), .command = QStringLiteral("goose"),
         .displayName = QStringLiteral("Goose")},

        // Amp fortsetzt laut Handbuch über `amp threads continue` — als Unterkommando
        // grundsätzlich eintragbar, aber nicht am echten CLI gegengeprüft. Bleibt leer,
        // bis jemand es laufen hat (dasselbe Muster wie codex).
        {.id = QStringLiteral("amp"), .command = QStringLiteral("amp"),
         .displayName = QStringLiteral("Amp")},

        // --- 2026-07-31, zweite Hälfte (QTMUX-88) ------------------------------
        // Jeder Kommandoname am **bin-Feld der npm-Registry** bzw. an der Hersteller-
        // dokumentation belegt — NICHT am Paket- oder Homebrew-Formelnamen. Genau da
        // lag der Fehler der Vorrecherche: `brew install gemini-cli` legt kein Kommando
        // `gemini-cli` an, die Formel macht `bin.install_symlink libexec.glob("bin/*")`
        // und verlinkt damit exakt das npm-`bin` (`gemini`). Fortsetzungs-Vorlagen
        // bleiben durchweg leer — keines dieser CLIs ist hier installiert, und ein
        // ungeprüftes Flag sieht für den Anwender wie ein QTmux-Fehler aus.
        {.id = QStringLiteral("junie"), .command = QStringLiteral("junie"),
         .displayName = QStringLiteral("Junie")},                 // @jetbrains/junie-cli

        {.id = QStringLiteral("grok"), .command = QStringLiteral("grok"),
         .displayName = QStringLiteral("Grok Build")},            // @xai-official/grok

        {.id = QStringLiteral("droid"), .command = QStringLiteral("droid"),
         .displayName = QStringLiteral("Droid")},                 // docs.factory.ai/cli

        // Kilo liefert als einziger tatsächlich ZWEI Kommandonamen aus
        // (npm @kilocode/cli, bin: {"kilo", "kilocode"}) — der einzige belegte Alias
        // dieser Runde, und einer, den die Vorrecherche nicht kannte.
        {.id = QStringLiteral("kilo"), .command = QStringLiteral("kilo"),
         .displayName = QStringLiteral("Kilo CLI"),
         .aliases = {QStringLiteral("kilocode")}},

        // ⚠️ `kiro-cli`, NICHT `kiro` (kiro.dev/docs/cli/installation) — derselbe
        // Fall wie cursor/cursor-agent: Kiro ist die IDE, kiro-cli der Agent.
        {.id = QStringLiteral("kiro"), .command = QStringLiteral("kiro-cli"),
         .displayName = QStringLiteral("Kiro")},

        // ⚠️ `auggie`, NICHT `augment` (npm @augmentcode/auggie, bin: {"auggie"}).
        {.id = QStringLiteral("augment"), .command = QStringLiteral("auggie"),
         .displayName = QStringLiteral("Auggie")},

        {.id = QStringLiteral("openclaw"), .command = QStringLiteral("openclaw"),
         .displayName = QStringLiteral("OpenClaw")},              // npm openclaw

        // ⚠️ `kimi`, NICHT `kimi-code-cli` (github.com/MoonshotAI/kimi-cli) — das
        // Produkt heißt „Kimi Code CLI", das Kommando ist kurz.
        {.id = QStringLiteral("kimi"), .command = QStringLiteral("kimi"),
         .displayName = QStringLiteral("Kimi Code")},

        {.id = QStringLiteral("iflow"), .command = QStringLiteral("iflow"),
         .displayName = QStringLiteral("iFlow CLI")},             // @iflow-ai/iflow-cli
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
            if (a.matches(base)) {
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

QByteArray AgentRegistry::scrollKeysFor(const QString &agentId, bool up) {
    if (agentId.isEmpty()) return {};
    for (const AgentInfo &a : all())
        if (a.id == agentId) return up ? a.scrollUpKeys : a.scrollDownKeys;
    return {};
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

    // Ist die Vorlage ein UNTERKOMMANDO (erstes Zeichen kein '-', z. B. codex' `resume
    // --last`), darf die Zeile nicht schon ein eigenes Unterkommando tragen: aus
    // `codex exec "…"` würde sonst `codex resume --last exec "…"` — eine kaputte Zeile.
    // Bei Flag-Vorlagen (`--continue`) ist das Einfügen dagegen immer gültig, deshalb
    // hängt die Prüfung an der Form der Vorlage, nicht am Agenten.
    if (!add.first().startsWith(QLatin1Char('-')) && idx + 1 < tokens.size()
        && !tokens.at(idx + 1).startsWith(QLatin1Char('-')))
        return commandLine;

    for (int k = 0; k < add.size(); ++k) tokens.insert(idx + 1 + k, add.at(k));
    return tokens.join(QChar(' '));
}

} // namespace qtmux
