#include <QtTest>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QTemporaryDir>

#include "GitInfo.h"

using namespace qtmux;

/// Tests für den Gui-freien Git-Branch-Leser (QTMUX-58).
///
/// Alle Fixtures sind **echte** temporäre Verzeichnisse (QTemporaryDir) mit echten
/// `.git`-Strukturen — kein Mock. Nur so decken sie die Fälle ab, an denen eine naive
/// Umsetzung scheitert: `.git` als *Datei* (Worktree/Submodul), detached HEAD, und ein
/// Arbeitsverzeichnis tief unterhalb der Repo-Wurzel.
class TestGitInfo : public QObject {
    Q_OBJECT

private:
    /// Schreibt `content` nach `path` und legt fehlende Verzeichnisse an.
    static void writeFile(const QString &path, const QByteArray &content) {
        QDir().mkpath(QFileInfo(path).absolutePath());
        QFile f(path);
        QVERIFY2(f.open(QIODevice::WriteOnly), qPrintable(path));
        QCOMPARE(f.write(content), qint64(content.size()));
    }

    /// Legt ein Repo mit `.git`-VERZEICHNIS an (der Normalfall) und gibt die Wurzel zurück.
    static QString makePlainRepo(const QTemporaryDir &tmp, const QString &name,
                                 const QByteArray &head) {
        const QString root = tmp.filePath(name);
        writeFile(root + "/.git/HEAD", head);
        return root;
    }

private slots:
    // --- Normalfall ---------------------------------------------------------

    void plainRepoGivesBranch() {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const QString root = makePlainRepo(tmp, "repo", "ref: refs/heads/main\n");

        const GitInfo g = GitInfo::forDirectory(root);
        QVERIFY(g.valid);
        QVERIFY(!g.detached);
        QCOMPARE(g.branch, QStringLiteral("main"));
        QVERIFY(g.shortSha.isEmpty());          // bei einem Branch bewusst nicht aufgelöst
    }

    void branchNameMayContainSlashes() {
        // Genau der Fall dieses Tickets: `feat/qtmux-58-git-branch`. Wer nach dem letzten
        // `/` schneidet, liefert hier „qtmux-58-git-branch" — der Branch wäre falsch benannt.
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const QString root = makePlainRepo(tmp, "repo", "ref: refs/heads/feat/qtmux-58-git-branch\n");

        const GitInfo g = GitInfo::forDirectory(root);
        QVERIFY(g.valid);
        QCOMPARE(g.branch, QStringLiteral("feat/qtmux-58-git-branch"));
    }

    void headMayEndWithCrLfOrNothing() {
        // Eine unter Windows geschriebene HEAD endet auf CRLF; manche Werkzeuge schreiben
        // gar keinen Zeilenumbruch. Beides darf den Namen nicht verunreinigen.
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        QCOMPARE(GitInfo::forDirectory(makePlainRepo(tmp, "crlf", "ref: refs/heads/main\r\n")).branch,
                 QStringLiteral("main"));
        QCOMPARE(GitInfo::forDirectory(makePlainRepo(tmp, "bare", "ref: refs/heads/main")).branch,
                 QStringLiteral("main"));
    }

    // --- Aufwärtssuche ------------------------------------------------------

    void searchesUpwardsFromDeepDirectory() {
        // Das Arbeitsverzeichnis einer Session liegt fast nie in der Repo-Wurzel.
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const QString root = makePlainRepo(tmp, "repo", "ref: refs/heads/develop\n");
        const QString deep = root + "/src/core/nested";
        QVERIFY(QDir().mkpath(deep));

        const GitInfo g = GitInfo::forDirectory(deep);
        QVERIFY(g.valid);
        QCOMPARE(g.branch, QStringLiteral("develop"));
    }

    void stopsAtRootWhenThereIsNoRepo() {
        // QTemporaryDir liegt im Systemtemp — oberhalb davon liegt kein Repo. Der Lauf muss
        // an der Dateisystemwurzel enden (und darf nicht hängen).
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const QString deep = tmp.filePath("a/b/c");
        QVERIFY(QDir().mkpath(deep));

        const GitInfo g = GitInfo::forDirectory(deep);
        QVERIFY(!g.valid);
        QVERIFY(g.branch.isEmpty());
    }

    // --- .git als DATEI: Worktree und Submodul ------------------------------

    void gitFileWithAbsolutePathIsFollowed() {
        // `git worktree`: `.git` ist eine Datei mit absolutem Zeiger auf
        // <repo>/.git/worktrees/<name>. Dort — nicht in <repo>/.git — liegt das HEAD des
        // Worktrees. (Genau diese Lage hat der Arbeitsbaum, in dem dieser Test entstand.)
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const QString main = tmp.filePath("main");
        writeFile(main + "/.git/HEAD", "ref: refs/heads/main\n");
        const QString wtGitDir = main + "/.git/worktrees/w3";
        writeFile(wtGitDir + "/HEAD", "ref: refs/heads/feat/qtmux-58-git-branch\n");

        const QString wt = tmp.filePath("worktree");
        writeFile(wt + "/.git", QStringLiteral("gitdir: %1\n").arg(wtGitDir).toUtf8());

        const GitInfo g = GitInfo::forDirectory(wt);
        QVERIFY(g.valid);
        QCOMPARE(g.branch, QStringLiteral("feat/qtmux-58-git-branch"));

        // Gegenprobe, dass wirklich dem Zeiger gefolgt wurde und nicht zufällig das
        // Haupt-Repo gelesen wurde: dessen Branch heißt anders.
        QVERIFY(g.branch != QStringLiteral("main"));
    }

    void gitFileWithRelativePathIsFollowed() {
        // Submodul-Normalfall: der Zeiger ist relativ und wird gegen das Verzeichnis der
        // `.git`-Datei aufgelöst — nicht gegen das Arbeitsverzeichnis des Prozesses.
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const QString super = tmp.filePath("super");
        writeFile(super + "/.git/modules/sub/HEAD", "ref: refs/heads/sub-branch\n");
        writeFile(super + "/sub/.git", "gitdir: ../.git/modules/sub\n");

        const QString deep = super + "/sub/lib/detail";
        QVERIFY(QDir().mkpath(deep));

        const GitInfo g = GitInfo::forDirectory(deep);
        QVERIFY(g.valid);
        QCOMPARE(g.branch, QStringLiteral("sub-branch"));
    }

    void danglingGitFileIsInvalid() {
        // Zeiger ins Leere (gelöschter Worktree): ungültig — und NICHT weiter nach oben
        // suchen, sonst meldete ein aufgeräumter Worktree still den Branch des Elternrepos.
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const QString outer = tmp.filePath("outer");
        writeFile(outer + "/.git/HEAD", "ref: refs/heads/outer-branch\n");
        const QString inner = outer + "/inner";
        writeFile(inner + "/.git", "gitdir: /nicht/vorhanden/xyz\n");

        const GitInfo g = GitInfo::forDirectory(inner);
        QVERIFY(!g.valid);
        QVERIFY(g.branch.isEmpty());
    }

    // --- bare-Repository ----------------------------------------------------

    void bareRepoIsDetected() {
        // Ein bare-Repo (`<name>.git`) hat keinen Arbeitsbaum: HEAD, objects/ und refs/
        // liegen direkt in der Wurzel, ein `.git` gibt es nicht.
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const QString bare = tmp.filePath("projekt.git");
        writeFile(bare + "/HEAD", "ref: refs/heads/main\n");
        QVERIFY(QDir().mkpath(bare + "/objects"));
        QVERIFY(QDir().mkpath(bare + "/refs/heads"));

        const GitInfo g = GitInfo::forDirectory(bare);
        QVERIFY(g.valid);
        QVERIFY(!g.detached);
        QCOMPARE(g.branch, QStringLiteral("main"));
    }

    void bareRepoIsFoundFromSubdirectory() {
        // Auch aus einem Unterverzeichnis heraus (etwa `objects/`) — die Aufwärtssuche
        // muss die bare-Wurzel genauso finden wie eine `.git`-Wurzel.
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const QString bare = tmp.filePath("projekt.git");
        writeFile(bare + "/HEAD", "ref: refs/heads/release/2.0\n");
        QVERIFY(QDir().mkpath(bare + "/objects/pack"));
        QVERIFY(QDir().mkpath(bare + "/refs/heads"));

        const GitInfo g = GitInfo::forDirectory(bare + "/objects/pack");
        QVERIFY(g.valid);
        QCOMPARE(g.branch, QStringLiteral("release/2.0"));
    }

    void headFileAloneIsNoRepo() {
        // Der Fehlalarm, den die bare-Erkennung erzeugen könnte: ein gewöhnliches
        // Verzeichnis mit einer beliebigen Datei namens `HEAD` (Fixture, Datenformat,
        // Dokumentation) darf KEIN Repository vortäuschen — sonst hinge an der Kachel ein
        // erfundener Branch. Erst alle drei Marken zusammen zählen.
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const QString dir = tmp.filePath("kein-repo");
        writeFile(dir + "/HEAD", "ref: refs/heads/main\n");
        QVERIFY(!GitInfo::forDirectory(dir).valid);

        // auch mit einer der beiden anderen Marken noch nicht
        QVERIFY(QDir().mkpath(dir + "/refs"));
        QVERIFY(!GitInfo::forDirectory(dir).valid);
    }

    // --- Detached HEAD ------------------------------------------------------

    void detachedHeadGivesShortSha() {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const QByteArray sha = "a8e2595f0c1d2e3b4a596877889900aabbccddee";  // 40 Hex
        QCOMPARE(sha.size(), 40);
        const QString root = makePlainRepo(tmp, "repo", sha + "\n");

        const GitInfo g = GitInfo::forDirectory(root);
        QVERIFY(g.valid);
        QVERIFY(g.detached);
        QVERIFY(g.branch.isEmpty());            // es GIBT keinen Branch — nichts erfinden
        QCOMPARE(g.shortSha, QStringLiteral("a8e2595"));
    }

    void detachedHeadWithSha256() {
        // SHA-256-Repositories schreiben 64 Zeichen. Eine Längenprüfung auf exakt 40 würde
        // das als „unverständlich" abtun.
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const QByteArray sha = QByteArray("0123456789abcdef").repeated(4);
        QCOMPARE(sha.size(), 64);
        const QString root = makePlainRepo(tmp, "repo", sha + "\n");

        const GitInfo g = GitInfo::forDirectory(root);
        QVERIFY(g.valid);
        QVERIFY(g.detached);
        QCOMPARE(g.shortSha, QStringLiteral("0123456"));
    }

    // --- Fehlerfälle: nie ein Absturz, nie eine Warnung ---------------------

    void missingAndEmptyDirectoriesAreInvalid() {
        QVERIFY(!GitInfo::forDirectory(QString()).valid);
        QVERIFY(!GitInfo::forDirectory(QStringLiteral("")).valid);
        QVERIFY(!GitInfo::forDirectory(QStringLiteral("/gibt/es/nicht/12345")).valid);
    }

    void brokenHeadIsInvalid() {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        // leeres HEAD, Müllinhalt, abgeschnittener SHA, und `ref:` ohne Ziel
        QVERIFY(!GitInfo::forDirectory(makePlainRepo(tmp, "leer",  "")).valid);
        QVERIFY(!GitInfo::forDirectory(makePlainRepo(tmp, "muell", "kaputt\n")).valid);
        QVERIFY(!GitInfo::forDirectory(makePlainRepo(tmp, "kurz",  "a8e2595\n")).valid);
        QVERIFY(!GitInfo::forDirectory(makePlainRepo(tmp, "ref0",  "ref:\n")).valid);
    }

    void missingHeadFileIsInvalid() {
        // `.git` existiert als Verzeichnis, aber ohne HEAD (z. B. abgebrochener Klon).
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const QString root = tmp.filePath("repo");
        QVERIFY(QDir().mkpath(root + "/.git"));
        QVERIFY(!GitInfo::forDirectory(root).valid);
    }

    void logsNothingOnFailure() {
        // Der Aufruf erfolgt zyklisch; ein Verzeichnis ohne Repo ist der Normalfall und darf
        // die Konsole nicht fluten. QtTest schlägt bei unerwarteter Ausgabe nicht von selbst
        // an — deshalb hier ein eigener Message-Handler.
        static int messages = 0;
        messages = 0;
        QtMessageHandler prev = qInstallMessageHandler(
            [](QtMsgType, const QMessageLogContext &, const QString &) { ++messages; });

        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        GitInfo::forDirectory(tmp.path());                          // kein Repo
        GitInfo::forDirectory(QStringLiteral("/gibt/es/nicht/12345"));  // Verzeichnis fehlt
        GitInfo::forDirectory(makePlainRepo(tmp, "muell", "kaputt\n")); // HEAD unverständlich

        // Wichtig: auch die Pfade abdecken, in denen eine Datei NICHT geöffnet werden kann —
        // genau dort ist ein qWarning() am naheliegendsten.
        const QString kaputt = tmp.filePath("ohne-head");
        QVERIFY(QDir().mkpath(kaputt + "/.git"));                   // .git-Verzeichnis ohne HEAD
        GitInfo::forDirectory(kaputt);
        const QString dangling = tmp.filePath("dangling");
        writeFile(dangling + "/.git", "gitdir: /nicht/vorhanden/xyz\n");
        GitInfo::forDirectory(dangling);                            // Zeiger ins Leere

        qInstallMessageHandler(prev);
        QCOMPARE(messages, 0);
    }

    // --- Polling-Tauglichkeit ----------------------------------------------

    void isCheapEnoughForPolling() {
        // Kein git-Prozess: 500 Abfragen müssen weit unter einer Sekunde bleiben. Ein
        // Prozessstart je Aufruf läge bei mehreren Millisekunden und risse die Schranke
        // um ein Vielfaches. Die Schranke ist bewusst großzügig (Faktor ~100 über dem
        // gemessenen Wert), damit sie auf langsamen CI-Runnern nicht flattert.
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const QString root = makePlainRepo(tmp, "repo", "ref: refs/heads/main\n");
        const QString deep = root + "/a/b/c";
        QVERIFY(QDir().mkpath(deep));

        QElapsedTimer t;
        t.start();
        for (int i = 0; i < 500; ++i)
            QVERIFY(GitInfo::forDirectory(deep).valid);
        QVERIFY2(t.elapsed() < 1000, qPrintable(QStringLiteral("500 Abfragen dauerten %1 ms")
                                                    .arg(t.elapsed())));
    }
};

QTEST_APPLESS_MAIN(TestGitInfo)
#include "tst_gitinfo.moc"
