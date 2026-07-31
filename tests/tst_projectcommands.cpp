#include <QtTest>
#include <QTemporaryDir>
#include <QDir>
#include <QFile>

#include "ProjectCommands.h"

using namespace qtmux;

/// Tests fuer den Gui-freien Scanner der Projekt-Befehle und -Skills (QTMUX-96).
///
/// Gearbeitet wird durchweg mit **echten** temporaeren Verzeichnissen (`QTemporaryDir`) —
/// der Scanner liest das Dateisystem, ein Attrappen-Modell wuerde genau die Fragen
/// offenlassen, um die es hier geht (Rekursion, Symlinks, Endungen, Frontmatter).
class TestProjectCommands : public QObject {
    Q_OBJECT

    QTemporaryDir m_dir;

    QString root() const { return m_dir.path(); }

    /// Legt eine Datei samt Verzeichnissen an; `rel` ist relativ zur Wurzel.
    void write(const QString &rel, const QByteArray &content)
    {
        const QString path = root() + QLatin1Char('/') + rel;
        QVERIFY(QDir().mkpath(QFileInfo(path).absolutePath()));
        QFile f(path);
        QVERIFY2(f.open(QIODevice::WriteOnly), qPrintable(path));
        QCOMPARE(f.write(content), qint64(content.size()));
    }

    /// Sucht einen Eintrag nach Namen; liefert einen leeren Eintrag, wenn er fehlt.
    static ProjectCommand find(const QList<ProjectCommand> &list, const QString &name)
    {
        for (const ProjectCommand &c : list)
            if (c.name == name)
                return c;
        return {};
    }

    static QStringList names(const QList<ProjectCommand> &list)
    {
        QStringList n;
        for (const ProjectCommand &c : list)
            n << c.name;
        return n;
    }

private slots:
    void initTestCase() { QVERIFY(m_dir.isValid()); }

    /// Ein Verzeichnis ohne die bekannten Ordner liefert eine leere Liste, KEINEN Fehler —
    /// und ein gar nicht existierender Pfad ebenso. Das ist der Normalfall: die allermeisten
    /// Arbeitsverzeichnisse bringen nichts dergleichen mit.
    void emptyOrMissingDirIsNoError()
    {
        QTemporaryDir empty;
        QVERIFY(empty.isValid());
        QVERIFY(ProjectCommands::scan(empty.path()).isEmpty());
        QVERIFY(ProjectCommands::scan(empty.path() + QStringLiteral("/gibt-es-nicht")).isEmpty());
        QVERIFY(ProjectCommands::scan(QString()).isEmpty());
        // Ein Ordner, der zwar existiert, aber leer ist, zaehlt genauso.
        QVERIFY(QDir().mkpath(empty.path() + QStringLiteral("/.claude/commands")));
        QVERIFY(ProjectCommands::scan(empty.path()).isEmpty());
    }

    /// Claude Code: `.claude/commands/<name>.md`, Beschreibung aus dem YAML-Frontmatter.
    /// So sehen die echten Dateien aus (`RAFTNG/.claude/commands/gui-testlauf.md`,
    /// `~/.claude/commands/feierabend.md`, die offiziellen Plugin-Marketplaces).
    void claudeCommandIsFoundWithDescription()
    {
        write(QStringLiteral(".claude/commands/deploy.md"),
              "---\n"
              "description: Rollt die aktuelle Fassung aus\n"
              "argument-hint: [umgebung]\n"
              "allowed-tools: [\"Bash\"]\n"
              "---\n"
              "\n# Deploy\nText.\n");

        const auto all = ProjectCommands::scan(root());
        const ProjectCommand c = find(all, QStringLiteral("deploy"));
        QCOMPARE(c.name, QStringLiteral("deploy"));
        QCOMPARE(c.description, QStringLiteral("Rollt die aktuelle Fassung aus"));
        QCOMPARE(c.source, QStringLiteral(".claude/commands"));
        QCOMPARE(c.agentId, QStringLiteral("claude"));
        QVERIFY(c.filePath.endsWith(QStringLiteral(".claude/commands/deploy.md")));
        QVERIFY(QFileInfo(c.filePath).isFile());
    }

    /// Unterverzeichnisse werden mit `:` verkettet — bei Gemini CLI die dokumentierte
    /// Form (`.gemini/commands/git/commit.toml` → `/git:commit`), fuer `.claude/commands`
    /// die aeltere Namespace-Regel aus dem Ticket. Entscheidend fuer den Test ist, dass
    /// das Unterverzeichnis NICHT verlorengeht.
    void nestedNamesUseColon()
    {
        write(QStringLiteral(".claude/commands/db/reset.md"),
              "---\ndescription: Datenbank zuruecksetzen\n---\n");
        write(QStringLiteral(".claude/commands/db/migrate/up.md"), "kein Frontmatter\n");

        const auto all = ProjectCommands::scan(root());
        QCOMPARE(find(all, QStringLiteral("db:reset")).description,
                 QStringLiteral("Datenbank zuruecksetzen"));
        QVERIFY(names(all).contains(QStringLiteral("db:migrate:up")));
        // Gegenprobe: der nackte Dateiname allein darf nicht entstehen.
        QVERIFY(!names(all).contains(QStringLiteral("reset")));
    }

    /// Fehlt die Beschreibung, bleibt sie **leer** — es wird nichts erfunden (weder der
    /// erste Absatz noch der Dateiname). Drei Spielarten: gar kein Frontmatter, ein
    /// Frontmatter ohne `description`, und ein `description` in einem Unterbaum
    /// (`metadata:` → `hermes:` → …), das die oberste Ebene nicht beschreibt.
    void missingDescriptionStaysEmpty()
    {
        write(QStringLiteral(".claude/commands/nackt.md"), "# Nur Text\n");
        write(QStringLiteral(".claude/commands/ohne.md"), "---\nargument-hint: [x]\n---\nText\n");
        write(QStringLiteral(".claude/commands/tief.md"),
              "---\nmetadata:\n  hermes:\n    description: gehoert nicht hierher\n---\nText\n");

        const auto all = ProjectCommands::scan(root());
        QVERIFY(names(all).contains(QStringLiteral("nackt")));
        QVERIFY(find(all, QStringLiteral("nackt")).description.isEmpty());
        QVERIFY(find(all, QStringLiteral("ohne")).description.isEmpty());
        QVERIFY(names(all).contains(QStringLiteral("tief")));
        QVERIFY2(find(all, QStringLiteral("tief")).description.isEmpty(),
                 "eingerueckte Schluessel gehoeren zu einem Unterbaum, nicht zum Befehl");
    }

    /// Gequotete und als Block-Skalar geschriebene Beschreibungen — beide kommen in
    /// echten Dateien vor (`description: "…"` in den Hermes-Skills, `|`/`>` in laengeren).
    void quotedAndBlockDescriptions()
    {
        write(QStringLiteral(".claude/commands/quoted.md"),
              "---\ndescription: \"Sucht \\\"Treffer\\\" im Log\"\n---\n");
        write(QStringLiteral(".claude/commands/block.md"),
              "---\ndescription: >\n  Erste Zeile\n  zweite Zeile\nname: block\n---\n");

        const auto all = ProjectCommands::scan(root());
        QCOMPARE(find(all, QStringLiteral("quoted")).description,
                 QStringLiteral("Sucht \"Treffer\" im Log"));
        QCOMPARE(find(all, QStringLiteral("block")).description,
                 QStringLiteral("Erste Zeile zweite Zeile"));
    }

    /// Skills: ein Verzeichnis mit `SKILL.md`. 🔑 Der Befehlsname kommt aus dem
    /// **Verzeichnis**, nicht aus dem Frontmatter-`name` — so beschreibt es die
    /// Claude-Code-Doku fuer Projekt-Skills (dort ist `name` nur das Anzeige-Label).
    /// Kategorie-Zwischenverzeichnisse (so legt Hermes seine Skills ab) bleiben als
    /// `:`-Praefix erhalten.
    void skillsUseDirectoryNameNotFrontmatterName()
    {
        write(QStringLiteral(".claude/skills/deploy-staging/SKILL.md"),
              "---\nname: fancy\ndescription: Bringt die Testumgebung hoch\n---\nText\n");
        write(QStringLiteral(".claude/skills/research/arxiv/SKILL.md"),
              "---\nname: arxiv\ndescription: \"Search arXiv papers.\"\n---\n");
        // Beiwerk im Skill-Verzeichnis darf keinen zweiten Eintrag erzeugen.
        write(QStringLiteral(".claude/skills/deploy-staging/reference.md"), "Hilfsdatei\n");

        const auto all = ProjectCommands::scan(root());
        const ProjectCommand s = find(all, QStringLiteral("deploy-staging"));
        QCOMPARE(s.name, QStringLiteral("deploy-staging"));
        QCOMPARE(s.description, QStringLiteral("Bringt die Testumgebung hoch"));
        QCOMPARE(s.source, QStringLiteral(".claude/skills"));
        QCOMPARE(s.agentId, QStringLiteral("claude"));
        QVERIFY2(!names(all).contains(QStringLiteral("fancy")),
                 "das Frontmatter-name ist Anzeige-Label, nicht der Befehl");
        QVERIFY2(!names(all).contains(QStringLiteral("reference")),
                 "nur SKILL.md macht einen Skill aus");
        QCOMPARE(find(all, QStringLiteral("research:arxiv")).description,
                 QStringLiteral("Search arXiv papers."));
    }

    /// `.agents/skills` ist der agentenneutral geteilte Ort → **keine** agentId.
    void sharedSkillsHaveNoAgentId()
    {
        write(QStringLiteral(".agents/skills/review/SKILL.md"),
              "---\ndescription: Gemeinsamer Review-Skill\n---\n");

        const auto all = ProjectCommands::scan(root());
        const ProjectCommand s = find(all, QStringLiteral("review"));
        QCOMPARE(s.source, QStringLiteral(".agents/skills"));
        QVERIFY2(s.agentId.isEmpty(), "agentenneutral: kein Agent darf beansprucht werden");
        QCOMPARE(s.description, QStringLiteral("Gemeinsamer Review-Skill"));
    }

    /// Gemini CLI: TOML statt Frontmatter. Einzeiler, Mehrzeiler (`"""`) und
    /// Literal-Strings (`'`) — und ein `description` in einer Untertabelle zaehlt NICHT.
    void geminiTomlDescriptions()
    {
        write(QStringLiteral(".gemini/commands/git/commit.toml"),
              "description = \"Erzeugt einen Commit\"\n"
              "prompt = \"\"\"\nMach was.\n\"\"\"\n");
        write(QStringLiteral(".gemini/commands/lang.toml"),
              "# Kommentar\ndescription = 'Setzt die Sprache'\nprompt = \"x\"\n");
        write(QStringLiteral(".gemini/commands/multi.toml"),
              "description = \"\"\"Erste Zeile\nzweite Zeile\"\"\"\n");
        write(QStringLiteral(".gemini/commands/table.toml"),
              "prompt = \"x\"\n[meta]\ndescription = \"gehoert der Tabelle\"\n");

        const auto all = ProjectCommands::scan(root());
        const ProjectCommand c = find(all, QStringLiteral("git:commit"));
        QCOMPARE(c.description, QStringLiteral("Erzeugt einen Commit"));
        QCOMPARE(c.agentId, QStringLiteral("gemini"));
        QCOMPARE(c.source, QStringLiteral(".gemini/commands"));
        QCOMPARE(find(all, QStringLiteral("lang")).description, QStringLiteral("Setzt die Sprache"));
        QCOMPARE(find(all, QStringLiteral("multi")).description,
                 QStringLiteral("Erste Zeile zweite Zeile"));
        QVERIFY(names(all).contains(QStringLiteral("table")));
        QVERIFY2(find(all, QStringLiteral("table")).description.isEmpty(),
                 "ein description in einer Untertabelle beschreibt etwas anderes");
    }

    /// Junie: `.junie/commands/*.md` — wie Claude, aber mit eigener agentId.
    void junieCommands()
    {
        write(QStringLiteral(".junie/commands/plan.md"),
              "---\ndescription: Planungslauf\n---\n");

        const ProjectCommand c = find(ProjectCommands::scan(root()), QStringLiteral("plan"));
        QCOMPARE(c.agentId, QStringLiteral("junie"));
        QCOMPARE(c.source, QStringLiteral(".junie/commands"));
        QCOMPARE(c.description, QStringLiteral("Planungslauf"));
    }

    /// Nur die passende Endung zaehlt: keine `.txt`/`.png` in `commands`, kein `.md` in
    /// `.gemini/commands`, und eine `SKILL.md` ohne eigenes Verzeichnis ist kein Skill.
    void onlyMatchingFilesCount()
    {
        write(QStringLiteral(".claude/commands/liesmich.txt"), "kein Befehl\n");
        write(QStringLiteral(".gemini/commands/falsch.md"), "kein Gemini-Befehl\n");
        write(QStringLiteral(".claude/skills/SKILL.md"), "---\ndescription: heimatlos\n---\n");

        const auto all = ProjectCommands::scan(root());
        QVERIFY(!names(all).contains(QStringLiteral("liesmich")));
        QVERIFY(!names(all).contains(QStringLiteral("falsch")));
        QVERIFY(!names(all).contains(QStringLiteral("SKILL")));
        QVERIFY(!names(all).contains(QString()));
    }

    /// Binaere und uebergrosse Dateien werden uebersprungen — nicht mit leerer
    /// Beschreibung gelistet. Ein `.md` mit NUL-Byte ist keine Textdatei, und eine
    /// riesige Datei will niemand fuer einen Frontmatter oeffnen.
    void binaryAndOversizedFilesAreSkipped()
    {
        QByteArray binary = "---\ndescription: sieht harmlos aus\n---\n";
        binary.append('\0');
        binary.append("\x89PNG");
        write(QStringLiteral(".claude/commands/binaer.md"), binary);

        QByteArray big = "---\ndescription: riesig\n---\n";
        big.append(QByteArray(int(ProjectCommands::maxFileBytes) + 1024, 'x'));
        write(QStringLiteral(".claude/commands/riesig.md"), big);

        const auto all = ProjectCommands::scan(root());
        QVERIFY2(!names(all).contains(QStringLiteral("binaer")), "binaere Datei uebersprungen");
        QVERIFY2(!names(all).contains(QStringLiteral("riesig")), "uebergrosse Datei uebersprungen");
    }

    /// Symlinks werden NICHT verfolgt — weder Dateien noch Verzeichnisse. Ein fremdes
    /// Projekt darf den Scanner nicht aus seinem Verzeichnis herausfuehren.
    void symlinksAreNotFollowed()
    {
#ifdef Q_OS_WIN
        QSKIP("QFile::link erzeugt auf Windows eine .lnk-Verknuepfung, keinen Symlink");
#else
        QTemporaryDir outside;
        QVERIFY(outside.isValid());
        const QString secretDir = outside.path() + QStringLiteral("/geheim");
        QVERIFY(QDir().mkpath(secretDir));
        QFile secret(secretDir + QStringLiteral("/fremd.md"));
        QVERIFY(secret.open(QIODevice::WriteOnly));
        secret.write("---\ndescription: von draussen\n---\n");
        secret.close();

        QVERIFY(QDir().mkpath(root() + QStringLiteral("/.claude/commands")));
        // (a) Symlink auf eine Datei ausserhalb
        QVERIFY(QFile::link(secret.fileName(),
                            root() + QStringLiteral("/.claude/commands/verlinkt.md")));
        // (b) Symlink auf ein Verzeichnis ausserhalb
        QVERIFY(QFile::link(secretDir, root() + QStringLiteral("/.claude/commands/raus")));

        const auto all = ProjectCommands::scan(root());
        QVERIFY2(!names(all).contains(QStringLiteral("verlinkt")), "Datei-Symlink nicht folgen");
        QVERIFY2(!names(all).contains(QStringLiteral("raus:fremd")), "Verzeichnis-Symlink nicht folgen");
        QVERIFY(!names(all).contains(QStringLiteral("fremd")));
#endif
    }

    /// Die Reihenfolge ist stabil und alphabetisch (ohne Ruecksicht auf Gross-/Kleinschreibung) —
    /// die Palette soll nicht bei jedem Oeffnen anders aussehen. Zwei Laeufe liefern dasselbe.
    void resultIsSortedAndStable()
    {
        QTemporaryDir d;
        QVERIFY(d.isValid());
        for (const QString &n : { QStringLiteral("zebra"), QStringLiteral("Alpha"),
                                  QStringLiteral("mitte") }) {
            QVERIFY(QDir().mkpath(d.path() + QStringLiteral("/.claude/commands")));
            QFile f(d.path() + QStringLiteral("/.claude/commands/") + n + QStringLiteral(".md"));
            QVERIFY(f.open(QIODevice::WriteOnly));
            f.write("---\ndescription: x\n---\n");
        }
        const auto a = ProjectCommands::scan(d.path());
        const auto b = ProjectCommands::scan(d.path());
        QCOMPARE(names(a), QStringList({ QStringLiteral("Alpha"), QStringLiteral("mitte"),
                                         QStringLiteral("zebra") }));
        QCOMPARE(names(b), names(a));
    }

    /// Alle fuenf Orte zusammen in EINEM Verzeichnis: jeder liefert seinen Beitrag, und
    /// gleichnamige Eintraege aus verschiedenen Quellen bleiben beide erhalten (die
    /// Palette unterscheidet sie ueber `source`/`agentId`).
    void allSourcesTogether()
    {
        QTemporaryDir d;
        QVERIFY(d.isValid());
        const auto put = [&](const QString &rel, const QByteArray &c) {
            const QString p = d.path() + QLatin1Char('/') + rel;
            QVERIFY(QDir().mkpath(QFileInfo(p).absolutePath()));
            QFile f(p);
            QVERIFY(f.open(QIODevice::WriteOnly));
            f.write(c);
        };
        put(QStringLiteral(".claude/commands/doppelt.md"), "---\ndescription: aus commands\n---\n");
        put(QStringLiteral(".claude/skills/doppelt/SKILL.md"), "---\ndescription: aus skills\n---\n");
        put(QStringLiteral(".gemini/commands/g.toml"), "description = \"g\"\n");
        put(QStringLiteral(".junie/commands/j.md"), "---\ndescription: j\n---\n");
        put(QStringLiteral(".agents/skills/a/SKILL.md"), "---\ndescription: a\n---\n");

        const auto all = ProjectCommands::scan(d.path());
        QCOMPARE(all.size(), 5);
        QCOMPARE(names(all).count(QStringLiteral("doppelt")), 2);
        QStringList sources;
        for (const ProjectCommand &c : all)
            if (!sources.contains(c.source))
                sources << c.source;
        std::sort(sources.begin(), sources.end());
        QCOMPARE(sources, QStringList({ QStringLiteral(".agents/skills"),
                                        QStringLiteral(".claude/commands"),
                                        QStringLiteral(".claude/skills"),
                                        QStringLiteral(".gemini/commands"),
                                        QStringLiteral(".junie/commands") }));
    }
};

QTEST_MAIN(TestProjectCommands)
#include "tst_projectcommands.moc"
