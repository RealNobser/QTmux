#include <QtTest>
#include <QTemporaryDir>

#include "LinkDetector.h"

using namespace LinkDetector;

class TestLinkDetector : public QObject {
    Q_OBJECT

private slots:
    void urlHttp();
    void urlTrailingPunctuation();
    void urlKeepsBalancedParen();
    void disallowedSchemeIgnored();
    void mailto();
    void filePathAbsoluteExists();
    void filePathRelativeToCwd();
    void nonexistentPathIgnored();
    void networkPathsAreNeverLinks();
    void barewordFileInCwd();
    void multipleSpansOrdered();
};

void TestLinkDetector::urlHttp() {
    const auto s = detect(QStringLiteral("siehe https://example.com/x hier"));
    QCOMPARE(s.size(), 1);
    QCOMPARE(s.first().kind, Span::Url);
    QCOMPARE(s.first().target, QStringLiteral("https://example.com/x"));
}

void TestLinkDetector::urlTrailingPunctuation() {
    const auto s = detect(QStringLiteral("Link: https://example.com/pfad."));
    QCOMPARE(s.size(), 1);
    // Der abschließende Punkt gehört nicht zur URL.
    QCOMPARE(s.first().target, QStringLiteral("https://example.com/pfad"));
}

void TestLinkDetector::urlKeepsBalancedParen() {
    const auto s = detect(QStringLiteral("(https://en.wikipedia.org/wiki/C_(Sprache))"));
    QCOMPARE(s.size(), 1);
    // Die zur URL gehörende Klammer bleibt, die umschließende nicht.
    QCOMPARE(s.first().target, QStringLiteral("https://en.wikipedia.org/wiki/C_(Sprache)"));
}

void TestLinkDetector::disallowedSchemeIgnored() {
    QVERIFY(detect(QStringLiteral("javascript:alert(1)")).isEmpty());
    QVERIFY(detect(QStringLiteral("data:text/html,<b>x")).isEmpty());
    QVERIFY(!isAllowedScheme(QStringLiteral("javascript")));
    QVERIFY(isAllowedScheme(QStringLiteral("HTTPS")));  // case-insensitiv
}

void TestLinkDetector::mailto() {
    const auto s = detect(QStringLiteral("Mail an mailto:a@b.de bitte"));
    QCOMPARE(s.size(), 1);
    QCOMPARE(s.first().target, QStringLiteral("mailto:a@b.de"));
}

void TestLinkDetector::filePathAbsoluteExists() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString f = dir.filePath(QStringLiteral("log.txt"));
    QFile fh(f); QVERIFY(fh.open(QIODevice::WriteOnly)); fh.close();

    const auto s = detect(QStringLiteral("Fehler in %1 gefunden").arg(f));
    QCOMPARE(s.size(), 1);
    QCOMPARE(s.first().kind, Span::FilePath);
    QCOMPARE(s.first().target, QFileInfo(f).absoluteFilePath());
}

void TestLinkDetector::filePathRelativeToCwd() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QFile fh(dir.filePath(QStringLiteral("data.csv")));
    QVERIFY(fh.open(QIODevice::WriteOnly)); fh.close();

    const auto s = detect(QStringLiteral("siehe ./data.csv"), dir.path());
    QCOMPARE(s.size(), 1);
    QCOMPARE(s.first().target, QFileInfo(dir.filePath(QStringLiteral("data.csv"))).absoluteFilePath());
}

void TestLinkDetector::nonexistentPathIgnored() {
    // Ohne existierende Datei kein Treffer — die Existenzprüfung ist der Filter.
    QVERIFY(detect(QStringLiteral("nope /gibt/es/nicht/xyz.bin"), QString()).isEmpty());
}

void TestLinkDetector::networkPathsAreNeverLinks() {
    // Unter POSIX ist `//tmp/x` dieselbe Datei wie `/tmp/x`: Sie existiert, und trotzdem darf kein
    // Link entstehen. Das belegt, dass die Regel greift, nicht bloß eine fehlende Datei.
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QFile fh(dir.filePath(QStringLiteral("x.txt")));
    QVERIFY(fh.open(QIODevice::WriteOnly)); fh.close();
    const QString abs = QFileInfo(fh.fileName()).absoluteFilePath();
#if !defined(Q_OS_WIN)
    QVERIFY(QFileInfo::exists(QLatin1Char('/') + abs));
    QVERIFY(detect(QLatin1Char('/') + abs).isEmpty());
    QVERIFY(QFileInfo::exists(QStringLiteral("/./") + abs));
    QVERIFY(detect(QStringLiteral("/./") + abs).isEmpty());
    // Auch ein Arbeitsverzeichnis in dieser Form (es kann per OSC 7 aus der Ausgabe stammen).
    QVERIFY(detect(QStringLiteral("./x.txt"), QLatin1Char('/') + QFileInfo(abs).absolutePath()).isEmpty());
    QCOMPARE(detect(QStringLiteral("./x.txt"), QFileInfo(abs).absolutePath()).size(), qsizetype(1));
#endif
    QVERIFY(detect(QStringLiteral("\\\\server\\share\\x.txt")).isEmpty());
    QVERIFY(detect(QStringLiteral("//server/share/x.txt")).isEmpty());
    QVERIFY(detect(QStringLiteral("file://server/share/x.txt")).isEmpty());
    QVERIFY(detect(QStringLiteral("file:////server/share/x.txt")).isEmpty());
    QCOMPARE(detect(QStringLiteral("file:///tmp/x.txt")).size(), qsizetype(1));
    QVERIFY(detect(QStringLiteral("file://localhost/tmp/x.txt")).isEmpty());
    // Kodiert: Auch so ergibt das beim Öffnen einen Netzpfad.
    QVERIFY(detect(QStringLiteral("file:///%2Fserver/share/x.txt")).isEmpty());
    QVERIFY(detect(QStringLiteral("file:///%5Cserver/share/x.txt")).isEmpty());
    QVERIFY(detect(QStringLiteral("file:///%5C%5Cserver/share/x.txt")).isEmpty());
    QVERIFY(detect(QStringLiteral("file://localhost/%2Fserver/share/x.txt")).isEmpty());
    // Ein kodiertes Zeichen mitten im Pfad bleibt ein lokaler Pfad.
    QCOMPARE(detect(QStringLiteral("file:///tmp/a%20b.txt")).size(), qsizetype(1));
    QVERIFY(isRemoteFileUrl(QUrl(QStringLiteral("file:///%2Fserver/x"))));
    QVERIFY(isRemoteFileUrl(QUrl(QStringLiteral("file://server/x"))));
    QVERIFY(isRemoteFileUrl(QUrl(QStringLiteral("file://localhost/tmp/x"))));
    // Ungültig: `fromUserInput` machte daraus beim Klick eine http-URL.
    QVERIFY(detect(QStringLiteral("file://%2Fserver/x")).isEmpty());
    QVERIFY(!isRemoteFileUrl(QUrl::fromLocalFile(QFileInfo(abs).absolutePath())));
    QVERIFY(isNetworkPath(QStringLiteral("/\\server\\share")));
    QVERIFY(!isNetworkPath(QStringLiteral("/tmp//x")));
    QVERIFY(isNetworkPath(QStringLiteral("\\??\\UNC\\server\\share")));
    QVERIFY(isNetworkPath(QStringLiteral("/??/UNC/server/share")));
    QVERIFY(detect(QStringLiteral("file:///%3F%3F/UNC/server/share/x.txt")).isEmpty());
    // Punkt-Segmente: `QDir::cleanPath` macht daraus `//server/…`.
    QVERIFY(isNetworkPath(QStringLiteral("/.//server/x")));
    QVERIFY(isNetworkPath(QStringLiteral("/..//server/x")));
    QVERIFY(isNetworkPath(QStringLiteral("/x/..//server/y")));
    QVERIFY(isNetworkPath(QStringLiteral("/a/b/../..//server")));
    QVERIFY(isNetworkPath(QStringLiteral("/.\\\\server\\x")));
    QVERIFY(isNetworkPath(QStringLiteral("/./??/UNC/server")));
    QVERIFY(detect(QStringLiteral("file:///.//server/share/x.txt")).isEmpty());
    QVERIFY(detect(QStringLiteral("file:///x/..//server/share/x.txt")).isEmpty());
    QVERIFY(!isNetworkPath(QStringLiteral("/x/.//y")));
    QVERIFY(!isNetworkPath(QStringLiteral("/a/b/..//c")));
    QVERIFY(!isNetworkPath(QStringLiteral("/tmp/../x")));
    QVERIFY(!isNetworkPath(QStringLiteral("/")));
    QVERIFY(!isNetworkPath(QStringLiteral("/x/..")));
    QVERIFY(!isNetworkPath(QStringLiteral("/??")));
    QVERIFY(!isNetworkPath(QStringLiteral("/?x/y")));
    QVERIFY(!isNetworkPath(QStringLiteral("/??x")));
    QVERIFY(!isNetworkPath(QStringLiteral("C:\\x")));
}

void TestLinkDetector::barewordFileInCwd() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QFile fh(dir.filePath(QStringLiteral("README.md")));
    QVERIFY(fh.open(QIODevice::WriteOnly)); fh.close();
    // Ein Wort ohne Slash ist nur „pathish", wenn es existiert — hier tut es das.
    // (looksPathish verlangt einen Trenner; ein bareword erreicht die Existenzprüfung
    //  daher NICHT. Dieser Test dokumentiert die bewusste Grenze: bareword ⇒ kein Link.)
    QVERIFY(detect(QStringLiteral("siehe README.md"), dir.path()).isEmpty());
}

void TestLinkDetector::multipleSpansOrdered() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QFile fh(dir.filePath(QStringLiteral("a.txt")));
    QVERIFY(fh.open(QIODevice::WriteOnly)); fh.close();

    const auto s = detect(QStringLiteral("./a.txt und https://x.io/y"), dir.path());
    QCOMPARE(s.size(), 2);
    QVERIFY(s[0].start < s[1].start);
    QCOMPARE(s[0].kind, Span::FilePath);
    QCOMPARE(s[1].kind, Span::Url);
}

QTEST_MAIN(TestLinkDetector)
#include "tst_linkdetector.moc"
