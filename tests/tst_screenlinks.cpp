#include <QtTest>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>

#include "ScreenLinks.h"
#include "VtScreen.h"

using namespace qtmux;
using ScreenLinks::Gesture;
using FileManagerReveal::Platform;

/// Bildschirmzellen ↔ Links, an echten Bytes im VtScreen.
class TestScreenLinks : public QObject {
    Q_OBJECT

    // Emoji und Hilfen: 😀 belegt zwei Spalten und zwei QChar.
    static QByteArray emoji() { return QByteArray("\xF0\x9F\x98\x80"); }

private slots:
    // --- Gesten: alle drei Plattformen auf jedem Host -------------------------------------
    void gestures_data() {
        QTest::addColumn<int>("platform");
        QTest::addColumn<int>("mods");
        QTest::addColumn<int>("expected");
        const int ctrl = Qt::ControlModifier, shift = Qt::ShiftModifier, alt = Qt::AltModifier,
                  meta = Qt::MetaModifier, none = Qt::NoModifier;
        const int win = int(Platform::Windows), lin = int(Platform::Linux), mac = int(Platform::MacOS);
        const int N = int(Gesture::None), O = int(Gesture::Open), R = int(Gesture::Reveal);

        QTest::newRow("win plain") << win << none << N;
        QTest::newRow("win strg") << win << ctrl << O;
        QTest::newRow("win strg+umschalt") << win << (ctrl | shift) << R;
        QTest::newRow("win altgr") << win << (ctrl | alt) << N;
        QTest::newRow("win altgr+umschalt") << win << (ctrl | alt | shift) << N;
        QTest::newRow("win umschalt") << win << shift << N;

        QTest::newRow("linux strg") << lin << ctrl << O;
        QTest::newRow("linux strg+umschalt") << lin << (ctrl | shift) << R;
        QTest::newRow("linux strg+alt") << lin << (ctrl | alt) << N;

        QTest::newRow("mac plain") << mac << none << N;
        QTest::newRow("mac cmd") << mac << ctrl << O;
        QTest::newRow("mac cmd+alt") << mac << (ctrl | alt) << R;
        QTest::newRow("mac ctrl physisch") << mac << meta << N;
        QTest::newRow("mac cmd+ctrl physisch") << mac << (ctrl | meta) << N;
        QTest::newRow("mac cmd+umschalt") << mac << (ctrl | shift) << O;
    }

    void gestures() {
        QFETCH(int, platform);
        QFETCH(int, mods);
        QFETCH(int, expected);
        QCOMPARE(int(ScreenLinks::gestureFor(Qt::KeyboardModifiers(mods), Platform(platform))), expected);
    }

    // --- Spalten und Zeichen ---------------------------------------------------------------
    void plainLineMapsOneToOne() {
        VtScreen sc(5, 60);
        sc.inputWrite("siehe https://example.com/x hier\r\n");
        const auto hit = ScreenLinks::linkAt(sc, 0, 6);
        QCOMPARE(hit.target, QStringLiteral("https://example.com/x"));
        QCOMPARE(hit.segments.size(), 1);
        QCOMPARE(hit.segments[0].row, 0);
        QCOMPARE(hit.segments[0].c0, 6);
        QCOMPARE(hit.segments[0].c1, 26);
        QVERIFY(!ScreenLinks::linkAt(sc, 0, 5).valid());
        QVERIFY(ScreenLinks::linkAt(sc, 0, 26).valid());
        QVERIFY(!ScreenLinks::linkAt(sc, 0, 27).valid());
    }

    void emojiBeforeALinkKeepsTheColumns() {
        // Vorher: ein Zeichen je Spalte plus Surrogate, die Treffer rutschten je Emoji um eins nach
        // rechts. Unterstrichen war also eine Zelle hinter dem Link, und sein Anfang war tot.
        VtScreen sc(5, 60);
        sc.inputWrite(emoji() + " https://example.com/x\r\n");
        // Spalten: 0–1 Emoji, 2 Leerzeichen, ab 3 der Link (21 Zeichen, bis Spalte 23).
        const auto hit = ScreenLinks::linkAt(sc, 0, 3);
        QCOMPARE(hit.target, QStringLiteral("https://example.com/x"));
        QCOMPARE(hit.segments[0].c0, 3);
        QCOMPARE(hit.segments[0].c1, 23);
        QVERIFY(!ScreenLinks::linkAt(sc, 0, 2).valid());
        QVERIFY(!ScreenLinks::linkAt(sc, 0, 24).valid());
    }

    void severalEmojiStayAligned() {
        VtScreen sc(5, 60);
        sc.inputWrite(emoji() + emoji() + emoji() + " https://example.com/x\r\n");
        const auto hit = ScreenLinks::linkAt(sc, 0, 7);
        QVERIFY(hit.valid());
        QCOMPARE(hit.segments[0].c0, 7);
        QVERIFY(!ScreenLinks::linkAt(sc, 0, 6).valid());
    }

    void wideCharSpacerAddsNoSpaceToTheText() {
        VtScreen sc(5, 60);
        sc.inputWrite("https://example.com/\xE4\xB8\xAD\xE6\x96\x87/x\r\n");   // zwei CJK-Zeichen
        const auto hit = ScreenLinks::linkAt(sc, 0, 0);
        QCOMPARE(hit.target, QStringLiteral("https://example.com/") + QString::fromUtf8("\xE4\xB8\xAD\xE6\x96\x87") + QStringLiteral("/x"));
    }

    // --- Weiche Umbrüche --------------------------------------------------------------------
    void softWrappedUrlIsOneLink() {
        VtScreen sc(5, 20);
        sc.inputWrite("https://example.com/ganz/lang/und/noch/weiter\r\n");
        const QString full = QStringLiteral("https://example.com/ganz/lang/und/noch/weiter");
        // Jede Zeile des umbrochenen Links trifft dasselbe, vollständige Ziel.
        for (int row = 0; row < 3; ++row) {
            const auto hit = ScreenLinks::linkAt(sc, row, 0);
            QCOMPARE(hit.target, full);
            QCOMPARE(hit.segments.size(), 3);
            QCOMPARE(hit.segments[0].row, 0);
            QCOMPARE(hit.segments[0].c1, 19);
            QCOMPARE(hit.segments[2].row, 2);
            QCOMPARE(hit.segments[2].c1, full.size() - 41);
        }
    }

    void softWrappedUrlAcrossTheScrollbackBorder() {
        VtScreen sc(2, 20);
        sc.inputWrite("https://example.com/ganz/lang/und/noch/weiter\r\nx\r\n");
        QVERIFY(sc.scrollbackCount() > 0);
        const auto hit = ScreenLinks::linkAt(sc, 0, 3);
        QCOMPARE(hit.target, QStringLiteral("https://example.com/ganz/lang/und/noch/weiter"));
    }

    void hardBreakEndsTheLink() {
        VtScreen sc(5, 40);
        sc.inputWrite("https://example.com/a\r\nweiter/text\r\n");
        QCOMPARE(ScreenLinks::linkAt(sc, 0, 3).target, QStringLiteral("https://example.com/a"));
        QVERIFY(!ScreenLinks::linkAt(sc, 1, 3).valid());
    }

    void overlongChainIsNoLinkInsteadOfACutOne() {
        VtScreen sc(60, 10);
        QByteArray url = "https://example.com/";
        url += QByteArray(10 * 50, 'a');          // 52 Zeilen, weit über die Kettengrenze
        sc.inputWrite(url + "\r\n");
        QVERIFY(!ScreenLinks::linkAt(sc, 0, 2).valid());
        QVERIFY(!ScreenLinks::linkAt(sc, 30, 2).valid());
        // Eine Kette knapp innerhalb der Grenze bleibt vollständig.
        VtScreen ok(30, 10);
        QByteArray shortUrl = "https://example.com/";
        shortUrl += QByteArray(10 * 12, 'a');
        ok.inputWrite(shortUrl + "\r\n");
        QCOMPARE(ScreenLinks::linkAt(ok, 3, 2).target, QString::fromLatin1(shortUrl));
    }

    void cutAtTrailingPunctuationIsNoLink() {
        // Die Kette endet genau hinter einem Punkt. Der Detektor kürzt ihn weg, der Treffer endet
        // also vor dem Schnitt und sähe vollständig aus. In Wahrheit läuft der Link weiter.
        VtScreen sc(60, 10);
        const int cut = 10 * (ScreenLinks::kMaxChainRows + 1);       // Zeilen 0 bis 16 aus Sicht von Zeile 0
        QByteArray url = "https://example.com/";
        url += QByteArray(cut - 1 - url.size(), 'a');
        url += '.';
        url += QByteArray(100, 'b');
        sc.inputWrite(url + "\r\n");
        QVERIFY(ScreenLinks::logicalLine(sc, 0).cutBottom);
        QVERIFY(!ScreenLinks::linkAt(sc, 0, 2).valid());
    }

    void cutHeadDoesNotExposeAnInnerUrl() {
        // Ganz gelesen ist das EIN Link. Fehlt der Anfang, fände der Detektor nur noch das innere
        // Ziel, und ein Klick öffnete eine andere Adresse als die, die dasteht.
        VtScreen sc(60, 10);
        QByteArray url = "https://example.com/?u=";
        url += QByteArray(10 * 30 - url.size() - 5, 'a');
        url += "=https://c.example/x";
        sc.inputWrite(url + "\r\n");
        const int last = (url.size() - 1) / 10;
        QVERIFY(ScreenLinks::logicalLine(sc, last).cutTop);
        QVERIFY(!ScreenLinks::linkAt(sc, last, 2).valid());
    }

    void wordsAwayFromTheCutStayLinks() {
        // Nur das Wort am Schnitt fällt weg, nicht alles auf der abgeschnittenen Zeile.
        VtScreen sc(60, 10);
        QByteArray text = QByteArray(10 * 30, 'a');
        text += " https://b.example/x ";
        text += QByteArray(10 * 30, 'c');
        sc.inputWrite(text + "\r\n");
        const auto hit = ScreenLinks::linkAt(sc, 30, 2);
        QVERIFY(ScreenLinks::logicalLine(sc, 30).cutTop);
        QVERIFY(ScreenLinks::logicalLine(sc, 30).cutBottom);
        QCOMPARE(hit.target, QStringLiteral("https://b.example/x"));
    }

    void headFallenOutOfTheScrollbackIsSafe() {
        // Eine einzige lange Zeile über die Scrollback-Grenze hinaus: Die älteste verbliebene
        // Zeile ist dann eine Fortsetzung, ihr Anfang ist weg. Die Kette darf nicht auf Zeile -1
        // laufen (Lesen vor dem Puffer), und ein Link, dem der Anfang fehlt, ist keiner.
        VtScreen sc(3, 10);
        sc.inputWrite("https://example.com/" + QByteArray(10 * 10020, 'a'));
        QVERIFY(sc.scrollbackDropped() > 0);
        QVERIFY(sc.scrollbackContinuation(0));
        const auto line = ScreenLinks::logicalLine(sc, 0);
        QCOMPARE(line.firstRow, 0);
        QVERIFY(line.cutTop);
        QVERIFY(!ScreenLinks::linkAt(sc, 0, 2).valid());
    }

    // --- Ränder des Erkennens -----------------------------------------------------------------
    void boxCharactersEndALink() {
        VtScreen sc(5, 60);
        sc.inputWrite("\xE2\x94\x82https://example.com/x\xE2\x94\x82\r\n");   // │…│
        QCOMPARE(ScreenLinks::linkAt(sc, 0, 5).target, QStringLiteral("https://example.com/x"));
    }

    void relativePathNeedsAKnownWorkingDirectory() {
        // Ohne Arbeitsverzeichnis darf kein relativer Pfad gegen das Verzeichnis von QTmux selbst
        // aufgelöst werden: Das träfe eine fremde Datei.
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QVERIFY(QDir(dir.path()).mkpath(QStringLiteral("unter")));
        QFile f(dir.filePath(QStringLiteral("unter/datei.txt")));
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.close();
        const QString saved = QDir::currentPath();
        QVERIFY(QDir::setCurrent(dir.path()));

        VtScreen sc(5, 60);
        sc.inputWrite("unter/datei.txt\r\n");
        QVERIFY(!ScreenLinks::linkAt(sc, 0, 2).valid());
        const auto hit = ScreenLinks::linkAt(sc, 0, 2, dir.path());
        QVERIFY(QDir::setCurrent(saved));
        QVERIFY(hit.valid());
        QCOMPARE(hit.kind, LinkDetector::Span::FilePath);
        QVERIFY(hit.target.endsWith(QStringLiteral("unter/datei.txt")));
    }

    void outOfRangeRowsAreSafe() {
        VtScreen sc(5, 20);
        QCOMPARE(ScreenLinks::logicalLine(sc, -1).firstRow, -1);
        QCOMPARE(ScreenLinks::logicalLine(sc, 100).firstRow, -1);
        QVERIFY(!ScreenLinks::linkAt(sc, 100, 0).valid());
    }
};

QTEST_GUILESS_MAIN(TestScreenLinks)
#include "tst_screenlinks.moc"
