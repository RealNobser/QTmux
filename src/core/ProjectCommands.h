#pragma once

#include <QList>
#include <QString>

namespace qtmux {

/// Ein vom Projekt bereitgestellter Agenten-Befehl bzw. Skill (QTMUX-96).
struct ProjectCommand {
    /// Der Name OHNE fuehrenden Schraegstrich — die Palette schickt spaeter `/<name>`
    /// an die Session. Unterverzeichnisse werden mit `:` verkettet (`db/reset.md`
    /// → `db:reset`), s. Namensregeln in [ProjectCommands::scan].
    QString name;
    /// Aus dem YAML-Frontmatter (`description:`) bzw. dem TOML-Feld `description`.
    /// Fehlt sie, bleibt sie **leer** — hier wird nichts erfunden.
    QString description;
    /// Fundort relativ zum Arbeitsverzeichnis, z. B. `.claude/commands`,
    /// `.claude/skills`, `.gemini/commands`, `.junie/commands`, `.agents/skills`.
    QString source;
    /// Absoluter Pfad der gefundenen Datei (`…/foo.md`, `…/foo.toml`, `…/SKILL.md`).
    QString filePath;
    /// ID aus der AgentRegistry (`claude`, `gemini`, `junie`) — **leer** bei
    /// `.agents/skills`, das ist der agentenneutral geteilte Ort.
    QString agentId;
};

/// Findet die Befehle und Skills, die ein Projekt im Arbeitsverzeichnis mitbringt
/// (QTMUX-96). Gui-frei, damit die Palette (QML) nur noch anzeigen muss.
///
/// **Es wird ausschliesslich gelesen.** Nichts wird ausgefuehrt, nichts geschrieben,
/// Symlinks werden nicht verfolgt (weder Dateien noch Verzeichnisse) — ein Projekt aus
/// fremder Quelle darf den Scanner nicht aus seinem Verzeichnis herausfuehren.
///
/// 🔑 **Was an echter Struktur vorgefunden wurde** (Erhebung 2026-07-31 auf der
/// Entwicklungsmaschine, statt die Formate zu raten):
/// - `.claude/commands/<name>.md` — flach, YAML-Frontmatter mit `description:`,
///   daneben `argument-hint:`/`allowed-tools:` (gesehen in `RAFTNG/.claude/commands`,
///   `~/.claude/commands` und den offiziellen Plugin-Marketplaces).
///   **Ein verschachteltes Beispiel gab es hier nirgends** — die aktuelle Doku
///   (code.claude.com/docs/en/slash-commands) nennt fuer `.claude/commands/` nur
///   „File name without extension" und schweigt zu Unterverzeichnissen. Die im Ticket
///   genannte Form `commands/db/reset.md` → `/db:reset` stammt aus der aelteren
///   Namespace-Regel und ist bei **Gemini CLI** die dokumentierte Form. Wir verketten
///   darum einheitlich mit `:` — der Nutzer sieht dann beide Bestandteile und kann den
///   Namen notfalls von Hand kuerzen, waehrend ein weggeworfenes Unterverzeichnis den
///   Befehl mehrdeutig machen wuerde.
/// - Skills liegen als `<skill-verzeichnis>/SKILL.md` mit Frontmatter `name:` und
///   `description:` (gesehen: `~/.hermes/skills/<kategorie>/<skill>/SKILL.md` — also
///   durchaus mit Zwischenverzeichnissen — sowie `~/.cursor/skills-cursor/<skill>/`
///   und die Plugin-Skills unter `~/.claude/plugins/**/skills/<skill>/`).
///   🔑 Der Befehlsname eines Projekt-Skills kommt laut Doku aus dem **Verzeichnisnamen**;
///   das Frontmatter-`name` ist dort nur das Anzeige-Label. Genau so wird es hier
///   gemacht — Zwischenverzeichnisse (Kategorien) landen wie oben als `:`-Praefix.
/// - `.gemini/commands/*.toml`, `.junie/commands/*.md` und `.agents/skills/` waren auf
///   dieser Maschine **nicht** vorhanden; sie folgen den dokumentierten Formaten.
class ProjectCommands {
public:
    /// Scannt `workingDir`. Ein Verzeichnis ohne die bekannten Ordner liefert eine
    /// **leere Liste**, keinen Fehler. Ergebnis ist stabil sortiert (Name, dann Pfad),
    /// damit die Palette nicht bei jedem Oeffnen die Reihenfolge wechselt.
    static QList<ProjectCommand> scan(const QString &workingDir);

    /// Grenzen — bewusst oeffentlich, damit der Test sie kennt statt sie zu raten.
    /// Sie schuetzen davor, dass ein fremdes Projekt den Scanner beschaeftigt haelt.
    static constexpr int maxDepth = 6;          ///< Verzeichnistiefe unter dem Fundort
    static constexpr int maxEntries = 500;      ///< Gesamtzahl gelieferter Eintraege
    static constexpr qint64 maxFileBytes = 512 * 1024; ///< groessere Dateien: uebersprungen
    static constexpr qint64 headBytes = 8 * 1024;      ///< nur so viel wird gelesen
};

} // namespace qtmux
