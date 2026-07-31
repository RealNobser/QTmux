#include "ProjectCommands.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <algorithm>

namespace qtmux {
namespace {

/// Liest hoechstens den Kopf einer Datei — mehr braucht der Frontmatter nie.
///
/// Liefert `false`, wenn die Datei zu gross, unlesbar oder **binaer** ist (ein NUL-Byte
/// im Kopf ist das verlaessliche Merkmal; Textformate haben keins) — solche Dateien
/// werden komplett uebersprungen. `true` mit leerem `*text` heisst dagegen: gelesen,
/// aber ohne Frontmatter — der Befehl existiert, nur seine Beschreibung fehlt.
bool readHead(const QString &path, QString *text)
{
    QFileInfo fi(path);
    if (!fi.isFile() || fi.isSymLink() || fi.size() > ProjectCommands::maxFileBytes)
        return false;

    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return false;
    const QByteArray head = f.read(ProjectCommands::headBytes);
    if (head.contains('\0'))
        return false;
    *text = QString::fromUtf8(head);
    return true;
}

/// Entfernt Anfuehrungszeichen um einen YAML-Skalar und loest die beiden Escapes auf,
/// die in doppelt gequoteten Werten vorkommen. Ungequotete Werte bleiben unangetastet.
QString unquoteYaml(QString v)
{
    if (v.size() >= 2 && v.startsWith(u'"') && v.endsWith(u'"')) {
        v = v.mid(1, v.size() - 2);
        v.replace(QLatin1String("\\\""), QLatin1String("\""));
        v.replace(QLatin1String("\\\\"), QLatin1String("\\"));
        return v;
    }
    if (v.size() >= 2 && v.startsWith(u'\'') && v.endsWith(u'\'')) {
        v = v.mid(1, v.size() - 2);
        v.replace(QLatin1String("''"), QLatin1String("'"));
        return v;
    }
    return v;
}

/// Holt einen Wert der obersten Ebene aus dem YAML-Frontmatter (`---` … `---`).
///
/// Bewusst KEIN vollstaendiger YAML-Parser: gesucht sind genau die flachen Schluessel
/// `name` und `description`. Eingerueckte Zeilen gehoeren zu einer verschachtelten
/// Struktur (`metadata:` → `hermes:` → …) und werden daher nicht als Treffer gewertet —
/// sonst faende ein `description:` innerhalb eines Unterbaums faelschlich statt.
/// Block-Skalare (`|`, `>`) werden unterstuetzt und zu EINER Zeile zusammengefasst,
/// weil die Palette einzeilig anzeigt.
QString frontMatterValue(const QString &text, const QString &key)
{
    const QStringList lines = text.split(u'\n');
    int i = 0;
    // Fuehrende Leerzeilen und ein etwaiges BOM ueberspringen.
    while (i < lines.size() && lines.at(i).trimmed().isEmpty())
        ++i;
    if (i >= lines.size() || lines.at(i).trimmed() != QLatin1String("---"))
        return {};
    ++i;

    const QString prefix = key + QLatin1Char(':');
    for (; i < lines.size(); ++i) {
        const QString raw = lines.at(i);
        const QString trimmed = raw.trimmed();
        if (trimmed == QLatin1String("---") || trimmed == QLatin1String("..."))
            break; // Ende des Frontmatters
        if (raw.startsWith(u' ') || raw.startsWith(u'\t'))
            continue; // eingerueckt = nicht die oberste Ebene
        if (!raw.startsWith(prefix))
            continue;

        QString value = raw.mid(prefix.size()).trimmed();
        if (value.startsWith(u'|') || value.startsWith(u'>')) {
            // Block-Skalar: alle folgenden eingerueckten Zeilen gehoeren dazu.
            QStringList block;
            for (int j = i + 1; j < lines.size(); ++j) {
                const QString &b = lines.at(j);
                if (b.trimmed().isEmpty()) { block << QString(); continue; }
                if (!b.startsWith(u' ') && !b.startsWith(u'\t'))
                    break;
                block << b.trimmed();
            }
            value = block.join(QLatin1Char(' ')).simplified();
            return value;
        }
        return unquoteYaml(value).trimmed();
    }
    return {};
}

/// Loest die Escapes eines TOML-Basic-Strings auf (nur die praktisch vorkommenden).
QString unescapeTomlBasic(QString v)
{
    v.replace(QLatin1String("\\\""), QLatin1String("\""));
    v.replace(QLatin1String("\\n"), QLatin1String("\n"));
    v.replace(QLatin1String("\\t"), QLatin1String("\t"));
    v.replace(QLatin1String("\\\\"), QLatin1String("\\"));
    return v;
}

/// Holt `description = …` aus einer Gemini-CLI-Befehlsdatei.
///
/// Gesucht wird nur auf der obersten Ebene: sobald eine Tabellen-Ueberschrift (`[…]`)
/// beginnt, wird abgebrochen — ein `description` in einer Untertabelle beschreibt
/// etwas anderes. Unterstuetzt einzeilige Basic-/Literal-Strings und die
/// `"""`/`'''`-Mehrzeiler, mit denen `prompt` ueblicherweise geschrieben wird.
QString tomlDescription(const QString &text)
{
    const QStringList lines = text.split(u'\n');
    for (int i = 0; i < lines.size(); ++i) {
        const QString line = lines.at(i).trimmed();
        if (line.startsWith(u'['))
            break;
        if (line.startsWith(u'#'))
            continue;
        if (!line.startsWith(QLatin1String("description")))
            continue;
        const int eq = line.indexOf(u'=');
        if (eq < 0)
            continue;
        if (line.left(eq).trimmed() != QLatin1String("description"))
            continue;

        QString value = line.mid(eq + 1).trimmed();
        const bool multiBasic = value.startsWith(QLatin1String("\"\"\""));
        const bool multiLiteral = value.startsWith(QLatin1String("'''"));
        if (multiBasic || multiLiteral) {
            const QString delim = multiBasic ? QStringLiteral("\"\"\"") : QStringLiteral("'''");
            QString rest = value.mid(3);
            QStringList block;
            int close = rest.indexOf(delim);
            while (close < 0 && i + 1 < lines.size()) {
                block << rest;
                rest = lines.at(++i);
                close = rest.indexOf(delim);
            }
            if (close >= 0)
                block << rest.left(close);
            const QString joined = block.join(QLatin1Char(' ')).simplified();
            return multiBasic ? unescapeTomlBasic(joined) : joined;
        }
        if (value.startsWith(u'"')) {
            const int close = value.indexOf(u'"', 1);
            if (close > 0)
                return unescapeTomlBasic(value.mid(1, close - 1));
        } else if (value.startsWith(u'\'')) {
            const int close = value.indexOf(u'\'', 1);
            if (close > 0)
                return value.mid(1, close - 1);
        }
        return {};
    }
    return {};
}

/// Verkettet die Pfadbestandteile zum Befehlsnamen: `db/reset` → `db:reset`.
QString commandName(const QString &relativePath)
{
    QString n = relativePath;
    n.replace(u'/', u':');
    return n;
}

/// Sammelt rekursiv alle regulaeren Dateien unter `dir` (Pfad relativ zu `dir`).
/// Symlinks — Dateien wie Verzeichnisse — werden **nicht** verfolgt: ein fremdes
/// Projekt darf den Scanner nicht aus seinem Verzeichnis herausfuehren.
void collectFiles(const QString &dir, const QString &relative, int depth, QStringList *out)
{
    if (depth > ProjectCommands::maxDepth || out->size() > ProjectCommands::maxEntries)
        return;

    QDir d(dir);
    const QFileInfoList entries = d.entryInfoList(
        QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot | QDir::NoSymLinks | QDir::Readable,
        QDir::Name);
    for (const QFileInfo &fi : entries) {
        if (fi.isSymLink())
            continue; // doppelt gesichert, unabhaengig vom Filter-Verhalten
        const QString rel = relative.isEmpty() ? fi.fileName()
                                               : relative + QLatin1Char('/') + fi.fileName();
        if (fi.isDir())
            collectFiles(fi.absoluteFilePath(), rel, depth + 1, out);
        else if (fi.isFile())
            *out << rel;
        if (out->size() > ProjectCommands::maxEntries)
            return;
    }
}

/// Ein Befehlsordner (`*/commands`): jede Datei mit passender Endung ist ein Befehl.
void scanCommandDir(const QString &root, const QString &source, const QString &suffix,
                    const QString &agentId, QList<ProjectCommand> *out)
{
    const QString dir = root + QLatin1Char('/') + source;
    if (!QFileInfo(dir).isDir())
        return;

    QStringList files;
    collectFiles(dir, QString(), 0, &files);
    for (const QString &rel : std::as_const(files)) {
        if (!rel.endsWith(suffix, Qt::CaseInsensitive))
            continue;
        const QString path = dir + QLatin1Char('/') + rel;
        QString text;
        if (!readHead(path, &text))
            continue; // zu gross, binaer oder unlesbar: bewusst uebersprungen

        ProjectCommand c;
        c.name = commandName(rel.left(rel.size() - suffix.size()));
        c.description = suffix == QLatin1String(".toml") ? tomlDescription(text)
                                                         : frontMatterValue(text, QStringLiteral("description"));
        c.source = source;
        c.filePath = path;
        c.agentId = agentId;
        if (!c.name.isEmpty())
            *out << c;
    }
}

/// Ein Skill-Ordner (`*/skills`): jedes Verzeichnis mit `SKILL.md` ist ein Skill.
/// 🔑 Der Name kommt aus dem **Verzeichnis**, nicht aus dem Frontmatter-`name` — so
/// beschreibt es die Claude-Code-Doku fuer Projekt-Skills (`name` ist dort nur das
/// Anzeige-Label). Zwischenverzeichnisse (Kategorien, wie sie Hermes benutzt) werden
/// mit `:` vorangestellt.
void scanSkillDir(const QString &root, const QString &source, const QString &agentId,
                  QList<ProjectCommand> *out)
{
    const QString dir = root + QLatin1Char('/') + source;
    if (!QFileInfo(dir).isDir())
        return;

    QStringList files;
    collectFiles(dir, QString(), 0, &files);
    for (const QString &rel : std::as_const(files)) {
        if (!rel.endsWith(QLatin1String("SKILL.md"), Qt::CaseSensitive))
            continue;
        const int slash = rel.lastIndexOf(u'/');
        if (slash < 0)
            continue; // SKILL.md direkt im skills-Ordner: kein Skill-Verzeichnis, kein Name
        const QString path = dir + QLatin1Char('/') + rel;
        QString text;
        if (!readHead(path, &text))
            continue; // zu gross, binaer oder unlesbar: bewusst uebersprungen

        ProjectCommand c;
        c.name = commandName(rel.left(slash));
        c.description = frontMatterValue(text, QStringLiteral("description"));
        c.source = source;
        c.filePath = path;
        c.agentId = agentId;
        if (!c.name.isEmpty())
            *out << c;
    }
}

} // namespace

QList<ProjectCommand> ProjectCommands::scan(const QString &workingDir)
{
    QList<ProjectCommand> out;
    if (workingDir.isEmpty())
        return out;

    QString root = QDir::fromNativeSeparators(workingDir);
    while (root.size() > 1 && root.endsWith(u'/'))
        root.chop(1);
    if (!QFileInfo(root).isDir())
        return out;

    scanCommandDir(root, QStringLiteral(".claude/commands"), QStringLiteral(".md"),
                   QStringLiteral("claude"), &out);
    scanSkillDir(root, QStringLiteral(".claude/skills"), QStringLiteral("claude"), &out);
    scanCommandDir(root, QStringLiteral(".gemini/commands"), QStringLiteral(".toml"),
                   QStringLiteral("gemini"), &out);
    scanCommandDir(root, QStringLiteral(".junie/commands"), QStringLiteral(".md"),
                   QStringLiteral("junie"), &out);
    // `.agents/skills` ist der agentenneutral geteilte Ort → bewusst KEINE agentId.
    scanSkillDir(root, QStringLiteral(".agents/skills"), QString(), &out);

    std::sort(out.begin(), out.end(), [](const ProjectCommand &a, const ProjectCommand &b) {
        const int byName = QString::compare(a.name, b.name, Qt::CaseInsensitive);
        if (byName != 0)
            return byName < 0;
        return a.filePath < b.filePath;
    });
    if (out.size() > maxEntries)
        out = out.mid(0, maxEntries);
    return out;
}

QList<ProjectCommand> ProjectCommands::filterForAgent(const QList<ProjectCommand> &all,
                                                      const QString &agentId)
{
    if (agentId.isEmpty())
        return all; // keine Kenntnis → nichts verbergen
    QList<ProjectCommand> out;
    for (const ProjectCommand &c : all)
        if (c.agentId.isEmpty() || c.agentId == agentId)
            out << c;
    return out;
}

} // namespace qtmux
