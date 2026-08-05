// Sicherheitsbefund aus RAFTNG (2026-08-06): Ein QFile::readAll() auf einen
// fremdbestimmten Pfad liess den Prozess in zwei Sekunden von 17 MB auf 30,4 GB
// wachsen — /dev/zero liefert unendlich Nullbytes.
//
// 🔑 Der Test prueft vor allem EINES: dass die Groessenpruefung ALLEIN nicht
// reicht. Gerätedateien und FIFOs melden `size() == 0`, bestuenden sie also — der
// wirksame Schutz ist `isFile()`.

#include <QtTest>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#ifdef Q_OS_UNIX
#include <sys/stat.h>
#include <sys/types.h>
#endif

#include "SafeFileRead.h"

using namespace qtmux;

class TestSafeFileRead : public QObject {
    Q_OBJECT

    QTemporaryDir m_dir;

private slots:
    void initTestCase() { QVERIFY(m_dir.isValid()); }

    void readsAnOrdinaryFile();
    void rejectsAFileOverTheLimit();
    void rejectsADirectory();
    void rejectsAMissingFile();
    void sizeAloneWouldNotProtect();
#ifdef Q_OS_UNIX
    void rejectsACharacterDevice();
    void rejectsAFifoInsteadOfBlockingForever();
#endif
};

void TestSafeFileRead::readsAnOrdinaryFile()
{
    const QString p = m_dir.filePath(QStringLiteral("gut.txt"));
    QFile f(p);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write("hallo");
    f.close();

    const auto r = safefile::read(p, 1024);
    QVERIFY2(r.ok, qPrintable(r.error));
    QCOMPARE(r.data, QByteArray("hallo"));
    QVERIFY(r.error.isEmpty());
}

void TestSafeFileRead::rejectsAFileOverTheLimit()
{
    const QString p = m_dir.filePath(QStringLiteral("gross.bin"));
    QFile f(p);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write(QByteArray(4096, 'x'));
    f.close();

    const auto r = safefile::read(p, 1024);
    QVERIFY(!r.ok);
    QVERIFY(r.data.isEmpty());
    // Die Meldung muss den Anwender in die richtige Richtung schicken.
    QVERIFY(r.error.contains(QStringLiteral("groß")) || r.error.contains(QStringLiteral("gross")));
}

void TestSafeFileRead::rejectsADirectory()
{
    const auto r = safefile::read(m_dir.path(), 1024 * 1024);
    QVERIFY(!r.ok);
    QVERIFY(r.data.isEmpty());
}

void TestSafeFileRead::rejectsAMissingFile()
{
    const auto r = safefile::read(m_dir.filePath(QStringLiteral("gibtsnicht")), 1024);
    QVERIFY(!r.ok);
}

// Der Kern der Sache, als eigener Fall festgehalten: Waere die Groesse das
// Kriterium, kaeme /dev/zero DURCH — es meldet 0. Wer diese Zusicherung
// aufweicht, baut den RAFTNG-Fehler nach.
void TestSafeFileRead::sizeAloneWouldNotProtect()
{
#ifdef Q_OS_UNIX
    const QFileInfo zero(QStringLiteral("/dev/zero"));
    if (!zero.exists())
        QSKIP("/dev/zero gibt es hier nicht");
    QCOMPARE(zero.size(), qint64(0));     // <- genau deshalb reicht Groesse nicht
    QVERIFY(!zero.isFile());              // <- und genau das faengt es
#else
    QSKIP("Gerätedateien dieser Art gibt es nur unter Unix");
#endif
}

#ifdef Q_OS_UNIX
void TestSafeFileRead::rejectsACharacterDevice()
{
    if (!QFileInfo::exists(QStringLiteral("/dev/zero")))
        QSKIP("/dev/zero gibt es hier nicht");

    // OHNE die Haertung wuerde dieser Aufruf den Speicher fuellen, bis der
    // Prozess stirbt. Er muss sofort und mit klarer Meldung zurueckkommen.
    QElapsedTimer t; t.start();
    const auto r = safefile::read(QStringLiteral("/dev/zero"), 1024 * 1024);
    QVERIFY(!r.ok);
    QVERIFY(r.data.isEmpty());
    QVERIFY2(t.elapsed() < 2000, "Der Aufruf muss sofort zurueckkommen, nicht lesen");

    // 🔑 Der Test prueft die RICHTIGE Schicht. Es gibt zwei, und sie fangen
    // Verschiedenes:
    //   isFile()            -> haelt Geraete UND FIFOs auf, bevor gelesen wird
    //   gedeckeltes read()  -> faengt zusaetzlich die schlicht zu grosse Datei
    // Ohne die Zeile unten bestuende dieser Fall auch OHNE isFile(): Das
    // gedeckelte Lesen holt 1 MB + 1 Byte von /dev/zero, merkt "zu gross" und
    // meldet einen Fehler — der Test waere gruen und die Pruefung trotzdem weg.
    // Genau so gemessen (2026-08-06). Beim FIFO faellt das gedeckelte Lesen aus:
    // es blockiert, bis Daten kommen, also fuer immer. DESHALB haengt die
    // Sicherheit an isFile(), und deshalb wird hier die MELDUNG geprueft.
    QVERIFY2(r.error.contains(QStringLiteral("gewöhnliche Datei")),
             qPrintable(QStringLiteral("Erwartet wurde die isFile-Meldung, kam: ") + r.error));
}

void TestSafeFileRead::rejectsAFifoInsteadOfBlockingForever()
{
    const QString p = m_dir.filePath(QStringLiteral("warteschlange"));
    if (::mkfifo(p.toLocal8Bit().constData(), 0600) != 0)
        QSKIP("FIFO liess sich nicht anlegen");

    // ⚠️ Der heimtueckische Fall: Ein FIFO frisst KEINEN Speicher, es blockiert
    // fuer immer. Der Fehler saehe wie ein Haenger aus, nicht wie ein Leck —
    // ohne isFile() haenge dieser Test hier bis zum ctest-Timeout.
    QElapsedTimer t; t.start();
    const auto r = safefile::read(p, 1024);
    QVERIFY(!r.ok);
    QVERIFY2(t.elapsed() < 2000, "Der Aufruf darf nicht blockieren");
    QFile::remove(p);
}
#endif

QTEST_MAIN(TestSafeFileRead)
#include "tst_safefileread.moc"
