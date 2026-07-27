#include <QtTest>
#include "TerminalSearch.h"

using namespace TerminalSearch;

class TestTerminalSearch : public QObject {
    Q_OBJECT
private slots:
    void emptyNeedleNoMatch();
    void findsAcrossLinesCaseInsensitive();
    void nonOverlapping();
    void caseSensitiveOption();
};

void TestTerminalSearch::emptyNeedleNoMatch() {
    QVERIFY(find({QStringLiteral("abc"), QStringLiteral("def")}, QString()).isEmpty());
    QVERIFY(find({QStringLiteral("abc")}, QStringLiteral("")).isEmpty());
}

// Default = Gross-/Kleinschreibung ignorieren; Treffer zeilenweise mit korrekter Spalte/Laenge.
void TestTerminalSearch::findsAcrossLinesCaseInsensitive() {
    const QStringList lines{QStringLiteral("Hello World"),
                            QStringLiteral("goodbye"),
                            QStringLiteral("say hello again")};
    const auto m = find(lines, QStringLiteral("hello"));
    QCOMPARE(m.size(), 2);
    QCOMPARE(m[0].line, 0); QCOMPARE(m[0].col, 0);  QCOMPARE(m[0].length, 5);
    QCOMPARE(m[1].line, 2); QCOMPARE(m[1].col, 4);  QCOMPARE(m[1].length, 5);
}

// Mehrere, nicht-ueberlappende Treffer in derselben Zeile.
void TestTerminalSearch::nonOverlapping() {
    const auto m = find({QStringLiteral("aaaa")}, QStringLiteral("aa"));
    QCOMPARE(m.size(), 2);
    QCOMPARE(m[0].col, 0);
    QCOMPARE(m[1].col, 2);
}

void TestTerminalSearch::caseSensitiveOption() {
    const auto m = find({QStringLiteral("Hello hello")}, QStringLiteral("hello"), /*caseSensitive=*/true);
    QCOMPARE(m.size(), 1);
    QCOMPARE(m[0].col, 6);
}

QTEST_MAIN(TestTerminalSearch)
#include "tst_terminalsearch.moc"
