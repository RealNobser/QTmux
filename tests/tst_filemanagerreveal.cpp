// QTMUX-138: „Im Finder/Explorer/Dateimanager zeigen" — Befehlsermittlung je Plattform.
// Alle drei Plattformen werden auf JEDEM Host geprüft (die Ermittlung ist reine
// Zeichenkettenlogik); `localPathFor` prüft gegen echte Dateien in einem Temp-Ordner.
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>
#include <QUrl>

#include "FileManagerReveal.h"

using namespace FileManagerReveal;

class TestFileManagerReveal : public QObject {
    Q_OBJECT
private slots:
    void platformMacos_data();
    void platformMacos();
    void platformWindows_data();
    void platformWindows();
    void platformLinux_data();
    void platformLinux();
    void relativeOrEmptyGivesNothing();
    void localPathForFilesAndFolders();
    void localPathForRejectsNonLocal();
    void leadingDashStaysAPath();
};

// --- macOS: open -R <abs> ---------------------------------------------------------------
void TestFileManagerReveal::platformMacos_data() {
    QTest::addColumn<QString>("path");
    QTest::newRow("datei")      << QStringLiteral("/Users/a/notes.txt");
    QTest::newRow("leerzeichen") << QStringLiteral("/Users/a/Mein Ordner/b c.txt");
    QTest::newRow("umlaute")    << QStringLiteral("/Users/a/Übersicht/Größe.md");
    QTest::newRow("bindestrich") << QStringLiteral("/Users/a/-R");
    QTest::newRow("ordner")     << QStringLiteral("/Users/a/Projekte");
    QTest::newRow("komma")      << QStringLiteral("/tmp/a,b.txt");
}
void TestFileManagerReveal::platformMacos() {
    QFETCH(QString, path);
    const auto c = commands(path, Platform::MacOS);
    QCOMPARE(c.size(), 1);
    QCOMPARE(c[0].program, QStringLiteral("open"));
    // Genau zwei Argumente: Option und der unveränderte Pfad als EIN Element.
    QCOMPARE(c[0].arguments, (QStringList{QStringLiteral("-R"), path}));
}

// --- Windows: explorer.exe /select, <nativer Pfad> -------------------------------------
void TestFileManagerReveal::platformWindows_data() {
    QTest::addColumn<QString>("path");
    QTest::addColumn<QString>("native");
    QTest::newRow("datei")       << QStringLiteral("C:/Users/a/notes.txt")
                                 << QStringLiteral("C:\\Users\\a\\notes.txt");
    QTest::newRow("leerzeichen") << QStringLiteral("C:/Program Files/x y.txt")
                                 << QStringLiteral("C:\\Program Files\\x y.txt");
    QTest::newRow("umlaute")     << QStringLiteral("D:/Übersicht/Größe.md")
                                 << QStringLiteral("D:\\Übersicht\\Größe.md");
    QTest::newRow("bindestrich") << QStringLiteral("C:/tmp/-rf")
                                 << QStringLiteral("C:\\tmp\\-rf");
    QTest::newRow("ordner")      << QStringLiteral("C:/Users/a/Projekte")
                                 << QStringLiteral("C:\\Users\\a\\Projekte");
    QTest::newRow("unc")         << QStringLiteral("//server/share/a.txt")
                                 << QStringLiteral("\\\\server\\share\\a.txt");
}
void TestFileManagerReveal::platformWindows() {
    QFETCH(QString, path);
    QFETCH(QString, native);
    const auto c = commands(path, Platform::Windows);
    QCOMPARE(c.size(), 1);
    QCOMPARE(c[0].program, QStringLiteral("explorer.exe"));
    // „/select," und der Pfad als GETRENNTE Argumente — QProcess quotet den Pfad dann
    // allein, wie Explorer es erwartet.
    QCOMPARE(c[0].arguments, (QStringList{QStringLiteral("/select,"), native}));
}

// --- Linux: D-Bus ShowItems, Rückfall xdg-open <Elternordner> --------------------------
void TestFileManagerReveal::platformLinux_data() {
    QTest::addColumn<QString>("path");
    QTest::addColumn<QString>("uri");
    QTest::addColumn<QString>("parent");
    QTest::newRow("datei")       << QStringLiteral("/home/a/notes.txt")
                                 << QStringLiteral("file:///home/a/notes.txt")
                                 << QStringLiteral("/home/a");
    QTest::newRow("leerzeichen") << QStringLiteral("/home/a/Mein Ordner/b c.txt")
                                 << QStringLiteral("file:///home/a/Mein%20Ordner/b%20c.txt")
                                 << QStringLiteral("/home/a/Mein Ordner");
    QTest::newRow("umlaute")     << QStringLiteral("/home/a/Größe.md")
                                 << QStringLiteral("file:///home/a/Gr%C3%B6%C3%9Fe.md")
                                 << QStringLiteral("/home/a");
    // Komma MUSS kodiert sein: dbus-send trennt array:string:-Elemente daran.
    QTest::newRow("komma")       << QStringLiteral("/tmp/a,b.txt")
                                 << QStringLiteral("file:///tmp/a%2Cb.txt")
                                 << QStringLiteral("/tmp");
    QTest::newRow("bindestrich") << QStringLiteral("/tmp/-rf")
                                 << QStringLiteral("file:///tmp/-rf")
                                 << QStringLiteral("/tmp");
    QTest::newRow("ordner")      << QStringLiteral("/home/a/Projekte")
                                 << QStringLiteral("file:///home/a/Projekte")
                                 << QStringLiteral("/home/a");
    QTest::newRow("wurzelnah")   << QStringLiteral("/etc")
                                 << QStringLiteral("file:///etc")
                                 << QStringLiteral("/");
}
void TestFileManagerReveal::platformLinux() {
    QFETCH(QString, path);
    QFETCH(QString, uri);
    QFETCH(QString, parent);
    const auto c = commands(path, Platform::Linux);
    QCOMPARE(c.size(), 2);
    QCOMPARE(c[0].program, QStringLiteral("dbus-send"));
    QCOMPARE(c[0].arguments, (QStringList{
        QStringLiteral("--session"), QStringLiteral("--print-reply"),
        QStringLiteral("--dest=org.freedesktop.FileManager1"),
        QStringLiteral("--type=method_call"),
        QStringLiteral("/org/freedesktop/FileManager1"),
        QStringLiteral("org.freedesktop.FileManager1.ShowItems"),
        QStringLiteral("array:string:") + uri,
        QStringLiteral("string:")}));
    QCOMPARE(c[1].program, QStringLiteral("xdg-open"));
    QCOMPARE(c[1].arguments, QStringList{parent});
}

void TestFileManagerReveal::relativeOrEmptyGivesNothing() {
    for (Platform p : {Platform::MacOS, Platform::Windows, Platform::Linux}) {
        QVERIFY(commands(QString(), p).isEmpty());
        QVERIFY(commands(QStringLiteral("-R"), p).isEmpty());
        QVERIFY(commands(QStringLiteral("relativ/datei.txt"), p).isEmpty());
    }
    // Plattform-fremde Absolutpfade werden abgelehnt statt verbogen.
    QVERIFY(commands(QStringLiteral("C:/x.txt"), Platform::MacOS).isEmpty());
    QVERIFY(commands(QStringLiteral("/home/x.txt"), Platform::Windows).isEmpty());
}

void TestFileManagerReveal::localPathForFilesAndFolders() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString dir = QDir(tmp.path()).filePath(QStringLiteral("Mein Ördner"));
    QVERIFY(QDir().mkpath(dir));
    const QString file = QDir(dir).filePath(QStringLiteral("a b,ä.txt"));
    { QFile f(file); QVERIFY(f.open(QIODevice::WriteOnly)); f.write("x"); }
    const QString canonFile = QDir::cleanPath(QFileInfo(file).absoluteFilePath());
    const QString canonDir = QDir::cleanPath(QFileInfo(dir).absoluteFilePath());

    QCOMPARE(localPathFor(file), canonFile);
    QCOMPARE(localPathFor(dir), canonDir);
    QCOMPARE(localPathFor(dir + QStringLiteral("/")), canonDir);
    QCOMPARE(localPathFor(QUrl::fromLocalFile(file).toString()), canonFile);
    QCOMPARE(localPathFor(QString::fromLatin1(QUrl::fromLocalFile(file).toEncoded())), canonFile);
    // Und das Ergebnis ergibt auf dieser Plattform wirklich einen Befehl.
    QVERIFY(!commands(localPathFor(file), currentPlatform()).isEmpty());
}

void TestFileManagerReveal::localPathForRejectsNonLocal() {
    QVERIFY(localPathFor(QStringLiteral("https://example.com/a.txt")).isEmpty());
    QVERIFY(localPathFor(QStringLiteral("http://example.com")).isEmpty());
    QVERIFY(localPathFor(QStringLiteral("mailto:a@b.de")).isEmpty());
    QVERIFY(localPathFor(QStringLiteral("ftp://example.com/x")).isEmpty());
    QVERIFY(localPathFor(QString()).isEmpty());
    // Remote-Fall (SSH-Ausgabe): ein Pfad, den es lokal nicht gibt, zeigt nichts.
    QVERIFY(localPathFor(QStringLiteral("/nirgends/qtmux-138/gibt-es-nicht.txt")).isEmpty());
    QVERIFY(localPathFor(QStringLiteral("file:///nirgends/qtmux-138/x.txt")).isEmpty());
    // file:// mit fremdem Host ist nicht lokal.
    QVERIFY(localPathFor(QStringLiteral("file://server/share/x.txt")).isEmpty()
            || QFileInfo::exists(QStringLiteral("//server/share/x.txt")));
    // Relative Pfade hingen am CWD von QTmux, nicht der Session — abgelehnt.
    QVERIFY(localPathFor(QStringLiteral("tests")).isEmpty());
}

void TestFileManagerReveal::leadingDashStaysAPath() {
    // Eine echte Datei namens „-R": localPathFor liefert sie absolut, also nie als Option.
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString file = QDir(tmp.path()).filePath(QStringLiteral("-R"));
    { QFile f(file); QVERIFY(f.open(QIODevice::WriteOnly)); }
    const QString p = localPathFor(file);
    QVERIFY(!p.isEmpty());
    QVERIFY(!p.startsWith(QLatin1Char('-')));
    const auto c = commands(p, currentPlatform());
    QVERIFY(!c.isEmpty());
    QVERIFY(c[0].arguments.last().endsWith(QLatin1String("-R")));
    QVERIFY(!c[0].arguments.last().startsWith(QLatin1Char('-')));
}

QTEST_GUILESS_MAIN(TestFileManagerReveal)
#include "tst_filemanagerreveal.moc"
