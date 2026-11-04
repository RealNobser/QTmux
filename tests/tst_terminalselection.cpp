#include <QtTest>
#include <QRandomGenerator>

#include "TerminalSelection.h"
#include "VtScreen.h"

using namespace qtmux;

/// Regel der Auswahl: Sie markiert genau den ausgewählten Text oder sie ist weg, nie fremden
/// Text. Geprüft an echten Bytes im VtScreen, je Bewegungsart mindestens ein Fall:
/// neue Ausgabe, Scrollback-Grenze, Größenänderung (mit und ohne Reflow), Neumalen an Ort und
/// Stelle, Scroll-Region, Alt-Screen, Bildschirm löschen. Dazu ein Zufallslauf, der die Regel
/// selbst prüft, statt einzelne Abläufe aufzuzählen.
class TestTerminalSelection : public QObject {
    Q_OBJECT

    // Absolute Zeile, die `needle` enthält; -1 = keine.
    static int findRow(const VtScreen &sc, const QString &needle) {
        const int total = sc.scrollbackCount() + sc.rows();
        for (int r = 0; r < total; ++r) {
            QString line;
            if (r < sc.scrollbackCount()) {
                for (const Cell &c : sc.scrollbackLine(r)) line += c.text.isEmpty() ? QStringLiteral(" ") : c.text;
            } else {
                for (int col = 0; col < sc.cols(); ++col) {
                    const QString t = sc.cell(r - sc.scrollbackCount(), col).text;
                    line += t.isEmpty() ? QStringLiteral(" ") : t;
                }
            }
            if (line.contains(needle)) return r;
        }
        return -1;
    }

    static void select(TerminalSelection &sel, const VtScreen &sc, const QString &needle) {
        const int row = findRow(sc, needle);
        QVERIFY2(row >= 0, qPrintable(needle));
        sel.begin(sc, 0, row);
        sel.extendTo(sc, needle.size() - 1, row);
        QCOMPARE(sel.text(sc), needle);
    }

    static void feed(VtScreen &sc, TerminalSelection &sel, const QByteArray &bytes) {
        sc.inputWrite(bytes);
        sel.revalidate(sc);
    }

private slots:
    // --- Neue Ausgabe -------------------------------------------------------------------
    void newOutputKeepsTheTextMarked() {
        VtScreen sc(5, 20);
        sc.inputWrite("zeile A\r\nzeile B\r\nzeile C\r\n");
        TerminalSelection sel;
        select(sel, sc, QStringLiteral("zeile B"));
        for (int i = 0; i < 20; ++i) feed(sc, sel, "mehr\r\n");
        QVERIFY(sc.scrollbackCount() > 0);
        QVERIFY(sel.hasSelection());
        QCOMPARE(sel.text(sc), QStringLiteral("zeile B"));
        // Der Renderer markiert dieselbe Zeile, in der der Text jetzt steht.
        int from = -1, to = -1;
        QVERIFY(sel.rowSpan(sc, findRow(sc, QStringLiteral("zeile B")), sc.cols(), from, to));
        QCOMPARE(from, 0);
        QCOMPARE(to, 6);
        QVERIFY(!sel.rowSpan(sc, findRow(sc, QStringLiteral("zeile A")), sc.cols(), from, to));
    }

    void outputThatDoesNotScrollLeavesItAlone() {
        VtScreen sc(10, 20);
        sc.inputWrite("zeile A\r\nzeile B\r\n");
        TerminalSelection sel;
        select(sel, sc, QStringLiteral("zeile A"));
        feed(sc, sel, "darunter\r\n");
        QCOMPARE(sel.text(sc), QStringLiteral("zeile A"));
    }

    void keepScrollbackClearMovesWithTheText() {
        VtScreen sc(5, 20);
        sc.inputWrite("zeile A\r\nzeile B\r\nprompt");
        TerminalSelection sel;
        select(sel, sc, QStringLiteral("zeile B"));
        QVERIFY(sc.clearViewportKeepScrollback());
        sel.revalidate(sc);
        QCOMPARE(sel.text(sc), QStringLiteral("zeile B"));
    }

    // --- Scrollback-Grenze ---------------------------------------------------------------
    void scrollbackLimitDoesNotShiftTheSelection() {
        VtScreen sc(5, 30);
        for (int i = 0; i < 10100; ++i)
            sc.inputWrite(QByteArray("alt ") + QByteArray::number(i) + "\r\n");
        QVERIFY(sc.scrollbackDropped() > 0);
        const QString needle = QStringLiteral("alt 5000");
        TerminalSelection sel;
        select(sel, sc, needle);
        const int rowBefore = findRow(sc, needle);
        const qint64 droppedBefore = sc.scrollbackDropped();
        for (int i = 0; i < 50; ++i) feed(sc, sel, "neu\r\n");
        QCOMPARE(sc.scrollbackDropped(), droppedBefore + 50);
        QVERIFY(sel.hasSelection());
        QCOMPARE(sel.text(sc), needle);
        // Der Text ist um 50 Zeilen nach oben gerutscht, und die Auswahl mit ihm. Mit der
        // alten absoluten Zählung stünde sie jetzt auf „alt 5050“.
        QCOMPARE(findRow(sc, needle), rowBefore - 50);
        int from = -1, to = -1;
        QVERIFY(sel.rowSpan(sc, rowBefore - 50, sc.cols(), from, to));
        QVERIFY(!sel.rowSpan(sc, rowBefore, sc.cols(), from, to));
    }

    void droppedTextTakesTheSelectionAlong() {
        VtScreen sc(5, 30);
        for (int i = 0; i < 10000; ++i) sc.inputWrite("alt\r\n");
        const QString needle = QStringLiteral("markiert");
        sc.inputWrite("markiert\r\n");
        TerminalSelection sel;
        select(sel, sc, needle);
        for (int i = 0; i < 10100; ++i) sc.inputWrite("neu\r\n");
        QVERIFY(sel.revalidate(sc));
        QVERIFY(!sel.hasSelection());
    }

    void droppedScrollbackSelectionDisappearsToo() {
        // Anders als oben lag der Text schon beim Markieren im Scrollback: Es gibt keinen
        // Vergleichstext, allein die stabile Nummer verrät, dass er weg ist.
        VtScreen sc(5, 30);
        sc.inputWrite("markiert\r\n");
        for (int i = 0; i < 20; ++i) sc.inputWrite("neu\r\n");
        QVERIFY(findRow(sc, QStringLiteral("markiert")) < sc.scrollbackCount());
        TerminalSelection sel;
        select(sel, sc, QStringLiteral("markiert"));
        for (int i = 0; i < 10100; ++i) sc.inputWrite("neu\r\n");
        QVERIFY(sc.scrollbackDropped() > 0);
        QVERIFY(sel.revalidate(sc));
        QVERIFY(!sel.hasSelection());
        QCOMPARE(sel.text(sc), QString());
    }

    // --- Neumalen an Ort und Stelle --------------------------------------------------------
    void repaintWithOtherTextClearsIt() {
        VtScreen sc(5, 20);
        sc.inputWrite("zeile A\r\nzeile B\r\n");
        TerminalSelection sel;
        select(sel, sc, QStringLiteral("zeile B"));
        sc.inputWrite("\033[2;1H\033[2Kanderer Text");
        QVERIFY(sel.revalidate(sc));
        QVERIFY(!sel.hasSelection());
        QCOMPARE(sel.text(sc), QString());
    }

    void repaintWithTheSameTextKeepsIt() {
        VtScreen sc(5, 20);
        sc.inputWrite("zeile A\r\nzeile B\r\n");
        TerminalSelection sel;
        select(sel, sc, QStringLiteral("zeile B"));
        sc.inputWrite("\033[2;1H\033[2Kzeile B");
        QVERIFY(!sel.revalidate(sc));
        QCOMPARE(sel.text(sc), QStringLiteral("zeile B"));
    }

    void changeOutsideTheSelectionKeepsIt() {
        VtScreen sc(5, 30);
        sc.inputWrite("zeile A  spinner 1\r\nzeile B\r\n");
        TerminalSelection sel;
        select(sel, sc, QStringLiteral("zeile A"));
        // Dieselbe Zeile, aber rechts der Auswahl (ein Spinner neben markiertem Text) ...
        sc.inputWrite("\033[1;18H2");
        QVERIFY(!sel.revalidate(sc));
        // ... und eine andere Zeile.
        sc.inputWrite("\033[2;1H\033[2Kganz anders");
        QVERIFY(!sel.revalidate(sc));
        QCOMPARE(sel.text(sc), QStringLiteral("zeile A"));
    }

    void attributesDoNotCount() {
        VtScreen sc(5, 20);
        sc.inputWrite("zeile A\r\n");
        TerminalSelection sel;
        select(sel, sc, QStringLiteral("zeile A"));
        sc.inputWrite("\033[1;1H\033[1;31mzeile A\033[0m");
        QVERIFY(!sel.revalidate(sc));
    }

    void changeInAMultiRowSelectionClearsIt() {
        VtScreen sc(5, 20);
        sc.inputWrite("eins\r\nzwei\r\ndrei\r\n");
        TerminalSelection sel;
        sel.begin(sc, 0, 0);
        sel.extendTo(sc, 3, 2);
        QCOMPARE(sel.text(sc), QStringLiteral("eins\nzwei\ndrei"));
        sc.inputWrite("\033[2;1Hzwo!");   // Mitte der Auswahl
        QVERIFY(sel.revalidate(sc));
    }

    void clearScreenClearsIt() {
        VtScreen sc(5, 20);
        sc.inputWrite("zeile A\r\n");
        TerminalSelection sel;
        select(sel, sc, QStringLiteral("zeile A"));
        sc.inputWrite("\033[2J");
        QVERIFY(sel.revalidate(sc));
    }

    // --- Scroll-Region (kein Scrollback-Zuwachs) ----------------------------------------------
    void scrollRegionClearsIt() {
        VtScreen sc(6, 20);
        sc.inputWrite("kopf\r\neins\r\nzwei\r\ndrei\r\nvier\r\nfuss");
        TerminalSelection sel;
        select(sel, sc, QStringLiteral("zwei"));
        const int sbBefore = sc.scrollbackCount();
        sc.inputWrite("\033[2;5r\033[5;1H\n");   // Region 2–5, unten eine Zeile rollen
        QCOMPARE(sc.scrollbackCount(), sbBefore);
        QVERIFY(sel.revalidate(sc));
    }

    // --- Alt-Screen ---------------------------------------------------------------------------
    void altScreenClearsALiveSelection() {
        VtScreen sc(5, 20);
        sc.inputWrite("zeile A\r\n");
        TerminalSelection sel;
        select(sel, sc, QStringLiteral("zeile A"));
        sc.inputWrite("\033[?1049h");
        QVERIFY(sel.revalidate(sc));
        // Zurück auf dem Primärschirm steht der Text wieder da, die Auswahl bleibt weg.
        sc.inputWrite("\033[?1049l");
        QVERIFY(!sel.revalidate(sc));
        QVERIFY(!sel.hasSelection());
    }

    void altScreenKeepsAScrollbackSelection() {
        VtScreen sc(3, 20);
        sc.inputWrite("zeile A\r\neins\r\nzwei\r\ndrei\r\nvier\r\n");
        QVERIFY(sc.scrollbackCount() > 0);
        TerminalSelection sel;
        select(sel, sc, QStringLiteral("zeile A"));
        sc.inputWrite("\033[?1049hvollbild\033[?1049l");
        QVERIFY(!sel.revalidate(sc));
        QCOMPARE(sel.text(sc), QStringLiteral("zeile A"));
    }

    void altScreenScrollingClearsIt() {
        // Das Muster einer Vollbild-Oberfläche: Inhalt rollt im Alt-Screen, kein Scrollback.
        VtScreen sc(5, 20);
        sc.inputWrite("\033[?1049h\033[1;1Heins\r\nzwei\r\ndrei\r\nvier\r\nfuenf");
        TerminalSelection sel;
        select(sel, sc, QStringLiteral("drei"));
        sc.inputWrite("\033[S");
        QCOMPARE(sc.scrollbackCount(), 0);
        QVERIFY(sel.revalidate(sc));
    }

    // --- Größenänderung -----------------------------------------------------------------------
    void reflowThatMovesTextClearsIt() {
        VtScreen sc(6, 20);
        sc.setReflow(true);
        sc.inputWrite("1234567890123456789012345\r\nzeile B\r\n");
        TerminalSelection sel;
        select(sel, sc, QStringLiteral("zeile B"));
        sc.setSize(6, 10);
        sel.revalidate(sc);
        // Der lange Text belegt jetzt drei statt zwei Zeilen, „zeile B“ steht eine tiefer.
        // Die alte Stelle zeigt fremden Text, also ist die Auswahl weg.
        QVERIFY(!sel.hasSelection());
    }

    void reflowLeavesAScrollbackSelection() {
        VtScreen sc(3, 20);
        sc.setReflow(true);
        sc.inputWrite("zeile A\r\neins\r\nzwei\r\ndrei\r\nvier\r\n");
        TerminalSelection sel;
        select(sel, sc, QStringLiteral("zeile A"));
        sc.setSize(3, 10);
        QVERIFY(!sel.revalidate(sc));
        QCOMPARE(sel.text(sc), QStringLiteral("zeile A"));
    }

    void wideningWithoutReflowKeepsIt() {
        VtScreen sc(5, 20);
        sc.inputWrite("zeile A\r\nzeile B\r\n");
        TerminalSelection sel;
        sel.begin(sc, 0, 0);
        sel.extendTo(sc, 6, 1);
        const QString before = sel.text(sc);
        sc.setSize(5, 40);
        QVERIFY(!sel.revalidate(sc));
        QCOMPARE(sel.text(sc), before);
    }

    void narrowingThatCutsTheTextClearsIt() {
        VtScreen sc(5, 30);
        sc.inputWrite("1234567890 markiert\r\n");
        TerminalSelection sel;
        select(sel, sc, QStringLiteral("1234567890 markiert"));
        sc.setSize(5, 10);
        QVERIFY(sel.revalidate(sc));
    }

    void narrowingDoesNotRunIntoTheNextRow() {
        // Die alte lineare Zellnummer (Zeile*Spalten+Spalte) lief nach dem Schmalziehen mit einer
        // Spalte >= neue Breite in die Folgezeile über. Eine Auswahl aus Leerzellen übersteht
        // das Schmalziehen, sie darf dann aber auf keiner Zeile etwas Falsches melden.
        VtScreen sc(5, 30);
        sc.inputWrite("x\r\nzeile B\r\n");
        TerminalSelection sel;
        sel.begin(sc, 25, 0);
        sel.extendTo(sc, 28, 0);
        sc.setSize(5, 10);
        sel.revalidate(sc);
        int from = -1, to = -1;
        QVERIFY(!sel.rowSpan(sc, 1, sc.cols(), from, to));
    }

    void heightChangeKeepsTheText() {
        VtScreen sc(8, 20);
        sc.inputWrite("zeile A\r\nzeile B\r\neins\r\nzwei\r\ndrei\r\nvier\r\nfuenf\r\n");
        TerminalSelection sel;
        select(sel, sc, QStringLiteral("zeile B"));
        sc.setSize(3, 20);
        sel.revalidate(sc);
        if (sel.hasSelection()) QCOMPARE(sel.text(sc), QStringLiteral("zeile B"));
        else QFAIL("Höhe ändern schob den Text nur in den Scrollback, die Auswahl muss bleiben");
    }

    // --- Alles auswählen ------------------------------------------------------------------------
    void selectAllStaysAndGrows() {
        VtScreen sc(3, 20);
        sc.inputWrite("eins\r\nzwei\r\n");
        TerminalSelection sel;
        sel.selectAll();
        QVERIFY(sel.hasSelection());
        sc.inputWrite("\033[1;1H\033[2Kanders");   // würde jede normale Auswahl löschen
        QVERIFY(!sel.revalidate(sc));
        QVERIFY(sel.text(sc).contains(QStringLiteral("anders")));
        for (int i = 0; i < 10; ++i) sc.inputWrite("mehr\r\n");
        QVERIFY(!sel.revalidate(sc));
        QVERIFY(sel.text(sc).contains(QStringLiteral("anders")));
        int from = -1, to = -1;
        QVERIFY(sel.rowSpan(sc, sc.scrollbackCount() + sc.rows() - 1, sc.cols(), from, to));
    }

    // --- Text und Ausdehnung --------------------------------------------------------------------
    void aBareAnchorIsNoSelection() {
        VtScreen sc(5, 20);
        sc.inputWrite("zeile A\r\n");
        TerminalSelection sel;
        sel.begin(sc, 2, 0);
        QVERIFY(!sel.hasSelection());
        QVERIFY(sel.anchorSet());
        QVERIFY(sel.extendTo(sc, 3, 0));
        QVERIFY(sel.hasSelection());
        QVERIFY(!sel.extendTo(sc, 3, 0));   // gleiche Zelle: nichts Neues
    }

    void draggingBackwardsSelectsTheSameText() {
        VtScreen sc(5, 20);
        sc.inputWrite("eins\r\nzwei\r\n");
        TerminalSelection sel;
        sel.begin(sc, 3, 1);
        sel.extendTo(sc, 0, 0);
        QCOMPARE(sel.text(sc), QStringLiteral("eins\nzwei"));
    }

    void softWrapHasNoNewline() {
        VtScreen sc(5, 10);
        sc.inputWrite("1234567890abcde\r\n");
        TerminalSelection sel;
        sel.begin(sc, 0, 0);
        sel.extendTo(sc, 4, 1);
        QCOMPARE(sel.text(sc), QStringLiteral("1234567890abcde"));
    }

    void wideCharHasNoStraySpace() {
        VtScreen sc(5, 20);
        sc.inputWrite("a\xF0\x9F\x98\x80" "b\r\n");   // a😀b, das Emoji belegt zwei Spalten
        TerminalSelection sel;
        sel.begin(sc, 0, 0);
        sel.extendTo(sc, 3, 0);
        QCOMPARE(sel.text(sc), QStringLiteral("a") + QString::fromUtf8("\xF0\x9F\x98\x80") + QStringLiteral("b"));
    }

    void wideCharInTheScrollbackHasNoStraySpace() {
        VtScreen sc(2, 20);
        sc.inputWrite("a\xF0\x9F\x98\x80" "b\r\neins\r\nzwei\r\n");
        QVERIFY(sc.scrollbackCount() > 0);
        TerminalSelection sel;
        sel.begin(sc, 0, 0);
        sel.extendTo(sc, 3, 0);
        QCOMPARE(sel.text(sc), QStringLiteral("a") + QString::fromUtf8("\xF0\x9F\x98\x80") + QStringLiteral("b"));
    }

    void trailingBlanksAreTrimmed() {
        VtScreen sc(5, 20);
        sc.inputWrite("eins\r\nzwei\r\n");
        TerminalSelection sel;
        sel.begin(sc, 0, 0);
        sel.extendTo(sc, 19, 0);
        QCOMPARE(sel.text(sc), QStringLiteral("eins"));
    }

    // --- Die Regel selbst ----------------------------------------------------------------------
    void randomChangesNeverMarkForeignText_data() {
        QTest::addColumn<bool>("reflow");
        QTest::newRow("ohne Reflow") << false;
        QTest::newRow("mit Reflow") << true;
    }

    void randomChangesNeverMarkForeignText() {
        // Jede Zeile trägt eine eindeutige Marke. Nach JEDER Änderung gilt: Auswahl weg, oder ihr
        // Text ist genau der ausgewählte. Leerzeichen und Umbrüche zählen dabei nicht, weil ein
        // Reflow den weichen Umbruch einer Zeile verändern darf.
        QFETCH(bool, reflow);
        const auto squeeze = [](QString s) {
            return s.remove(QLatin1Char(' ')).remove(QLatin1Char('\n'));
        };
        QRandomGenerator rng(20261104u);
        int keptAfterChange = 0, cleared = 0;
        for (int run = 0; run < 300; ++run) {
            VtScreen sc(8, 24);
            sc.setReflow(reflow);
            int mark = 0;
            const auto line = [&] { return QByteArray("T") + QByteArray::number(mark++) + "\r\n"; };
            for (int i = 0; i < 20; ++i) sc.inputWrite(line());

            const int total = sc.scrollbackCount() + sc.rows();
            const int a = rng.bounded(total), b = rng.bounded(total);
            TerminalSelection sel;
            sel.begin(sc, rng.bounded(sc.cols()), a);
            sel.extendTo(sc, rng.bounded(sc.cols()), b);
            if (!sel.hasSelection()) continue;
            const QString original = squeeze(sel.text(sc));

            for (int step = 0; step < 12 && sel.hasSelection(); ++step) {
                switch (rng.bounded(8)) {
                case 0: sc.inputWrite(line()); break;
                case 1: {                                        // Neumalen an fremder Stelle
                    const QByteArray pos = "\033[" + QByteArray::number(1 + rng.bounded(8)) + ";1H\033[2K";
                    sc.inputWrite(pos + "N" + QByteArray::number(mark++));
                    break;
                }
                case 2: sc.inputWrite("\033[2;6r\033[6;1H\n\033[r"); break;   // Scroll-Region
                case 3: sc.inputWrite("\033[S"); break;
                case 4: sc.inputWrite("\033[?1049h\033[2;1HALT\033[?1049l"); break;
                case 5: sc.setSize(8, 10 + rng.bounded(30)); break;
                case 6: sc.setSize(3 + rng.bounded(10), sc.cols()); break;
                case 7: sc.inputWrite("\033[2J\033[H"); break;
                }
                const bool wasOn = sel.hasSelection();
                sel.revalidate(sc);
                if (sel.hasSelection()) {
                    ++keptAfterChange;
                    QCOMPARE(squeeze(sel.text(sc)), original);
                } else if (wasOn) {
                    ++cleared;
                }
            }
        }
        // Der Lauf muss beide Ausgänge wirklich treffen, sonst prüft er nichts.
        QVERIFY2(keptAfterChange > 50, qPrintable(QString::number(keptAfterChange)));
        QVERIFY2(cleared > 50, qPrintable(QString::number(cleared)));
    }
};

QTEST_GUILESS_MAIN(TestTerminalSelection)
#include "tst_terminalselection.moc"
