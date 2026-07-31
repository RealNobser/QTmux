#include <QtTest>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>

#include "ShellIntegration.h"

using namespace qtmux;

// QTMUX-38: Die Shell-Helfer lagen nur im Repo — wer QTmux installiert, hatte sie nicht.
// Jetzt stecken sie als Ressource im Binary und werden auf Wunsch an einen stabilen Ort
// geschrieben.
class TestShellIntegration : public QObject {
    Q_OBJECT
private slots:
    void allBundledFilesExistInResources();
    void installWritesEveryFile();
    void shellScriptsBecomeExecutable();
    void secondRunReportsUnchanged();
    void changedFileIsRestored();
    void defaultTargetIsProfileIndependent();
    void hookExampleNamesAnExistingFile();
    void failsLoudlyOnUnwritableTarget();
};

// 🔑 Der eigentliche Wächter für die Ressource: qtmux_core ist eine STATISCHE Bibliothek,
// und ein Linker darf Objektdateien verwerfen, auf die niemand verweist. Fällt dieser Test,
// ist die Ressource zwar gebaut, aber nicht im Programm — und `install()` liefe in den
// "Mitgelieferte Datei fehlt"-Zweig.
void TestShellIntegration::allBundledFilesExistInResources() {
    const QStringList files = ShellIntegration::bundledFiles();
    QVERIFY(!files.isEmpty());
    for (const QString &f : files) {
        const QString res = QStringLiteral(":/shell-integration/") + f;
        QVERIFY2(QFile::exists(res), qPrintable(res));
        QFile fh(res);
        QVERIFY2(fh.open(QIODevice::ReadOnly), qPrintable(res));
        QVERIFY2(fh.size() > 0, qPrintable(res));
    }
    // Beide Haelften des Ereignisweges muessen dabei sein (Senden UND Warten) — genau
    // darauf verweist die Doku, und genau die fehlten dem Installationsnutzer.
    QVERIFY(files.contains(QStringLiteral("qtmux-emit.sh")));
    QVERIFY(files.contains(QStringLiteral("qtmux-wait.sh")));
    QVERIFY(files.contains(QStringLiteral("qtmux-emit.cmd")));
    QVERIFY(files.contains(QStringLiteral("qtmux-wait.cmd")));
}

void TestShellIntegration::installWritesEveryFile() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString target = tmp.filePath(QStringLiteral("shell-integration"));

    const ShellIntegrationResult r = ShellIntegration::install(target);
    QVERIFY2(r.ok, qPrintable(r.error));
    QCOMPARE(r.targetDir, target);
    QCOMPARE(r.written.size(), ShellIntegration::bundledFiles().size());
    QVERIFY(r.unchanged.isEmpty());

    for (const QString &f : ShellIntegration::bundledFiles()) {
        const QString path = target + QLatin1Char('/') + f;
        QVERIFY2(QFile::exists(path), qPrintable(path));
        // Inhaltsgleich mit der Ressource — sonst hilft die Datei nicht weiter.
        QFile a(QStringLiteral(":/shell-integration/") + f), b(path);
        QVERIFY(a.open(QIODevice::ReadOnly) && b.open(QIODevice::ReadOnly));
        QCOMPARE(b.readAll(), a.readAll());
    }
}

// Ohne Ausfuehrbar-Bit scheitert der Hook mit "Permission denied" — und zwar erst dann,
// wenn der Agent fertig ist, also genau im unguenstigsten Moment.
void TestShellIntegration::shellScriptsBecomeExecutable() {
#if defined(Q_OS_WIN)
    QSKIP("Ausführbar-Bit ist unter Windows bedeutungslos");
#else
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    QVERIFY(ShellIntegration::install(tmp.path()).ok);
    for (const QString &f : ShellIntegration::bundledFiles()) {
        const QFileInfo fi(tmp.path() + QLatin1Char('/') + f);
        if (f.endsWith(QStringLiteral(".sh")))
            QVERIFY2(fi.isExecutable(), qPrintable(f));
        else
            QVERIFY2(!fi.isExecutable(), qPrintable(f));   // nur Skripte, nicht README.md
    }
#endif
}

// Zweiter Lauf darf nicht sinnlos schreiben: sonst wandern bei jedem Aufruf die
// Zeitstempel, und der Anwender kann nicht erkennen, ob sich wirklich etwas geaendert hat.
void TestShellIntegration::secondRunReportsUnchanged() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    QVERIFY(ShellIntegration::install(tmp.path()).ok);

    const ShellIntegrationResult again = ShellIntegration::install(tmp.path());
    QVERIFY2(again.ok, qPrintable(again.error));
    QVERIFY(again.written.isEmpty());
    QCOMPARE(again.unchanged.size(), ShellIntegration::bundledFiles().size());
}

// Eine veraltete oder beschaedigte Datei muss wieder zur laufenden Fassung werden —
// das ist der Punkt "keine Versionsdrift" aus dem Ticket.
void TestShellIntegration::changedFileIsRestored() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    QVERIFY(ShellIntegration::install(tmp.path()).ok);

    const QString victim = tmp.path() + QStringLiteral("/qtmux-emit.sh");
    { QFile f(victim); QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
      f.write("# kaputt\n"); }

    const ShellIntegrationResult r = ShellIntegration::install(tmp.path());
    QVERIFY2(r.ok, qPrintable(r.error));
    QCOMPARE(r.written, QStringList{QStringLiteral("qtmux-emit.sh")});

    QFile a(QStringLiteral(":/shell-integration/qtmux-emit.sh")), b(victim);
    QVERIFY(a.open(QIODevice::ReadOnly) && b.open(QIODevice::ReadOnly));
    QCOMPARE(b.readAll(), a.readAll());
}

// 🔑 Das Ziel darf NICHT vom Profil abhaengen: ein Hook-Eintrag gilt fuer alle Profile.
// QStandardPaths::AppDataLocation traegt den applicationName, und der bekommt bei
// --profile/QTMUX_PROFILE ein Suffix — der Pfad waere dann je Instanz ein anderer.
void TestShellIntegration::defaultTargetIsProfileIndependent() {
    const QString before = ShellIntegration::defaultTargetDir();
    QVERIFY(!before.isEmpty());
    QVERIFY(before.endsWith(QStringLiteral("/QTmux/shell-integration")));

    const QString oldName = QCoreApplication::applicationName();
    QCoreApplication::setApplicationName(QStringLiteral("QTmux-testprofil"));
    QCOMPARE(ShellIntegration::defaultTargetDir(), before);
    QCoreApplication::setApplicationName(oldName);
}

void TestShellIntegration::hookExampleNamesAnExistingFile() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    QVERIFY(ShellIntegration::install(tmp.path()).ok);

    const QString line = ShellIntegration::hookCommandExample(tmp.path());
    QVERIFY(!line.isEmpty());
    // Der erste Token ist der Pfad; er MUSS auf eine tatsaechlich geschriebene Datei
    // zeigen — eine Beispielzeile, die ins Leere greift, ist schlimmer als keine.
    const QString path = line.left(line.indexOf(QStringLiteral(" done")));
    QVERIFY2(QFile::exists(path), qPrintable(path));
}

// Ein stiller Teilerfolg waere hier besonders teuer: Der Anwender traegt den Hook ein
// und merkt erst Wochen spaeter, dass nie ein Ereignis ankam.
void TestShellIntegration::failsLoudlyOnUnwritableTarget() {
#if defined(Q_OS_WIN)
    QSKIP("Rechte-Modell unterscheidet sich; der Pfad wird unter Unix geprüft");
#else
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString ro = tmp.filePath(QStringLiteral("nurlesen"));
    QVERIFY(QDir().mkpath(ro));
    QVERIFY(QFile::setPermissions(ro, QFileDevice::ReadOwner | QFileDevice::ExeOwner));

    const ShellIntegrationResult r = ShellIntegration::install(ro + QStringLiteral("/ziel"));
    QVERIFY(!r.ok);
    QVERIFY(!r.error.isEmpty());

    QFile::setPermissions(ro, QFileDevice::ReadOwner | QFileDevice::WriteOwner
                                  | QFileDevice::ExeOwner);   // sonst scheitert das Aufräumen
#endif
}

QTEST_MAIN(TestShellIntegration)
#include "tst_shellintegration.moc"
