#include <QtTest>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>

#include "ScreenLinks.h"
#include "VtScreen.h"
#include "link_wrap_captures.h"

using namespace qtmux;
using ScreenLinks::Gesture;
using FileManagerReveal::Platform;

/// Bildschirmzellen ↔ Links, an echten Bytes im VtScreen.
class TestScreenLinks : public QObject {
    Q_OBJECT

    // Emoji und Hilfen: 😀 belegt zwei Spalten und zwei QChar.
    static QByteArray emoji() { return QByteArray("\xF0\x9F\x98\x80"); }

    using Blocked = ScreenLinks::Hit::Blocked;
    static QByteArray bytes(const linkwrapcap::Bytes &b) {
        return QByteArray(b.bytes, static_cast<qsizetype>(b.size));
    }
    // Die Probe-Adressen der Mitschnitte: https://example.com/<fall>/n001/…/n030?fall=<fall>&ende=1
    static QString probeUrl(const char *tag, int parts = 30) {
        QString u = QStringLiteral("https://example.com/%1/").arg(QLatin1String(tag));
        for (int i = 1; i <= parts; ++i)
            u += QStringLiteral("n%1%2").arg(i, 3, 10, QLatin1Char('0')).arg(i < parts ? "/" : "");
        return u + QStringLiteral("?fall=%1&ende=1").arg(QLatin1String(tag));
    }
    static int firstInk(const VtScreen &sc, int row) {
        for (int c = 0; c < sc.cols(); ++c)
            if (!sc.cell(row, c).text.trimmed().isEmpty()) return c;
        return -1;
    }
    static int lastInk(const VtScreen &sc, int row) {
        for (int c = sc.cols() - 1; c >= 0; --c)
            if (!sc.cell(row, c).text.trimmed().isEmpty()) return c;
        return -1;
    }
    // Trifft JEDE Zelle von c0 bis c1 der Zeile dasselbe, vollständige Ziel?
    static bool everyCellOpens(const VtScreen &sc, int row, int c0, int c1, const QString &target,
                               int segments) {
        if (c0 < 0 || c1 < c0) return false;
        for (int c = c0; c <= c1; ++c) {
            const auto hit = ScreenLinks::linkAt(sc, row, c);
            if (hit.target != target || hit.segments.size() != segments) {
                qWarning() << "Zeile" << row << "Spalte" << c << "liefert" << hit.target
                           << "mit" << hit.segments.size() << "Abschnitten";
                return false;
            }
        }
        return true;
    }
    // Liefert irgendeine Zelle der Zeile ein Ziel? Dann welches (leer = keine).
    static QStringList targetsInRow(const VtScreen &sc, int row) {
        QStringList out;
        for (int c = 0; c < sc.cols(); ++c) {
            const auto hit = ScreenLinks::linkAt(sc, row, c);
            if (hit.valid() && !out.contains(hit.target)) out << hit.target;
        }
        return out;
    }

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


    // --- Vom Programm umbrochene Links (Owner-Befund 2026-11-13) ---------------------------
    // Die Vorlagen sind wörtliche Stücke aus Mitschnitten von Claude Code, s. link_wrap_captures.h.
    // Claude Code bricht lange Adressen SELBST um und setzt die Folgezeilen per Cursor: Es gibt
    // keine Fortsetzungs-Marke. 1.9.11 öffnete auf der ersten Zeile das abgeschnittene Stück.

    void claudeFullscreenHardWrapNeverOpensACutTarget() {
        VtScreen sc(29, 84);
        sc.inputWrite("\033[?1049h\033[2J\033[1;3H" + bytes(linkwrapcap::kFullscreenCodeBlock));
        QVERIFY(sc.altScreen());
        QCOMPARE(lastInk(sc, 0), 83);                       // bis in die letzte Spalte geschrieben
        QVERIFY(!sc.lineContinuation(1));                    // und trotzdem keine Marke
        QCOMPARE(firstInk(sc, 1), 2);
        // Keine Zeile öffnet ein Ziel: nicht das abgeschnittene, nicht ein zusammengesetztes.
        for (int row = 0; row < 3; ++row) QCOMPARE(targetsInRow(sc, row), QStringList());
        const auto head = ScreenLinks::linkAt(sc, 0, 10);
        QCOMPARE(head.blocked, Blocked::MaybeCut);
        QVERIFY(head.segments.isEmpty());                    // nichts wird unterstrichen
        // Die Folgezeilen sagen bei einem Klick dasselbe statt „Kein Link an dieser Stelle“.
        QVERIFY(ScreenLinks::continuesCutLink(sc, 1, 5));
        QVERIFY(ScreenLinks::continuesCutLink(sc, 2, 5));
    }

    void sameScreenDifferentMeaning() {
        // 🔑 Der Grund, warum NICHT zusammengesetzt wird. Beide Bilder sind echt: links eine
        // Adresse, die Claude Code mitten im Wort umbricht, rechts eine, die genau am Rand
        // endet, mit einem neuen Wort darunter. Volle Zeile, keine Marke, Folgezeile ab Spalte
        // 2. Wer die erste zusammensetzt, öffnet bei der zweiten `…xxZdanach`.
        VtScreen wrapped(29, 84), complete(29, 84);
        wrapped.inputWrite("\033[1;3H" + bytes(linkwrapcap::kFullscreenCodeBlock));
        complete.inputWrite("\033[1;3H" + bytes(linkwrapcap::kClassicWordAfterEdge));
        for (const VtScreen *sc : {&wrapped, &complete}) {
            QCOMPARE(lastInk(*sc, 0), 83);
            QVERIFY(!sc->lineContinuation(1));
            QCOMPARE(firstInk(*sc, 1), 2);
        }
        QCOMPARE(complete.cell(1, 2).text, QStringLiteral("d"));     // „danach folgt …“
        QCOMPARE(targetsInRow(complete, 0), QStringList());
        QCOMPARE(targetsInRow(complete, 1), QStringList());
        QCOMPARE(ScreenLinks::linkAt(complete, 0, 20).blocked, Blocked::MaybeCut);
    }

    void twoUrlsBelowEachOtherAreNeverGlued() {
        VtScreen sc(29, 84);
        sc.inputWrite("\033[1;3H" + bytes(linkwrapcap::kClassicTwoUrlsBelowEachOther));
        QCOMPARE(lastInk(sc, 0), 83);
        QCOMPARE(targetsInRow(sc, 0), QStringList());        // erste Adresse: unklar, also keine
        QCOMPARE(ScreenLinks::linkAt(sc, 0, 20).blocked, Blocked::MaybeCut);
        // Die zweite steht für sich und bleibt ein Link, mit genau ihrem Ziel.
        QCOMPARE(targetsInRow(sc, 1), QStringList{QStringLiteral("https://example.com/r4b/s01/s02/s03/s04Z")});
        QCOMPARE(targetsInRow(sc, 2), QStringList());
    }

    void toolOutputWrappedBeforeTheEdge() {
        // Die Werkzeugausgabe bricht Claude Code sechs Spalten vor dem Rand um. Die mittlere
        // Zeile verrät es: ein einziges Wort, das in derselben Spalte endet.
        VtScreen sc(29, 84);
        sc.inputWrite("\033[1;6H" + bytes(linkwrapcap::kClassicToolOutput));
        QCOMPARE(lastInk(sc, 0), 78);
        QCOMPARE(lastInk(sc, 1), 78);
        for (int row = 0; row < 3; ++row) QCOMPARE(targetsInRow(sc, row), QStringList());
        QCOMPARE(ScreenLinks::linkAt(sc, 0, 20).blocked, Blocked::MaybeCut);
    }

    void urlBeforeTheEdgeStaysALink() {
        // Die Regel trifft nur den Rand. Eine Adresse, die davor endet, ist vollständig.
        VtScreen sc(5, 40);
        sc.inputWrite("https://example.com/abcdefghijklmnopq\r\nweiter im Text\r\n");   // 37 Zeichen
        QCOMPARE(lastInk(sc, 0), 36);
        QCOMPARE(ScreenLinks::linkAt(sc, 0, 5).target, QStringLiteral("https://example.com/abcdefghijklmnopq"));
    }

    void urlAtTheEdgeFollowedByNothingStaysALink() {
        VtScreen sc(5, 40);
        sc.inputWrite("https://example.com/abcdefghijklmnopqrst\r\n\r\nspaeter\r\n");    // genau 40
        QCOMPARE(lastInk(sc, 0), 39);
        QVERIFY(ScreenLinks::linkAt(sc, 0, 5).valid());
    }

    void urlAtTheEdgeFollowedByTextIsNoLink() {
        // Auch ohne Einrückung, auch mit einem Wort, das nie zu einer Adresse gehören würde:
        // Am Bildschirm ist das nicht von einer umbrochenen Adresse zu unterscheiden.
        VtScreen sc(5, 40);
        sc.inputWrite("https://example.com/abcdefghijklmnopqrst\r\nweiter und mehr\r\n");
        QVERIFY(!sc.lineContinuation(1));
        const auto hit = ScreenLinks::linkAt(sc, 0, 5);
        QVERIFY(!hit.valid());
        QCOMPARE(hit.blocked, Blocked::MaybeCut);
        // Die Rückmeldung „vielleicht die Fortsetzung“ gilt dem ersten Wort darunter, nicht
        // jedem Wort der Zeile und keiner leeren Stelle.
        QVERIFY(ScreenLinks::continuesCutLink(sc, 1, 2));
        QVERIFY(!ScreenLinks::continuesCutLink(sc, 1, 12));          // „mehr“
        QVERIFY(!ScreenLinks::continuesCutLink(sc, 1, 6));           // Leerzeichen
        QVERIFY(!ScreenLinks::continuesCutLink(sc, 2, 0));
    }

    void urlAtTheEdgeAboveSomethingThatCannotContinueIt() {
        // Beginnt die nächste Zeile mit einem Zeichen, das in keiner Adresse steht (hier ein
        // Anführungszeichen und ein Rahmen), kann sie keine Fortsetzung sein.
        for (const QByteArray &below : {QByteArray("\"so\" steht es da"), QByteArray("\xE2\x94\x94\xE2\x94\x80\xE2\x94\x80")}) {
            VtScreen sc(5, 40);
            sc.inputWrite("https://example.com/abcdefghijklmnopqrst\r\n" + below + "\r\n");
            QCOMPARE(lastInk(sc, 0), 39);
            QCOMPARE(ScreenLinks::linkAt(sc, 0, 5).target, QStringLiteral("https://example.com/abcdefghijklmnopqrst"));
        }
    }

    void indentedNextLineIsNotAttached() {
        // Eingerückte Folgezeile, die NICHT dazugehört: kein zusammengesetztes Ziel.
        VtScreen sc(5, 40);
        sc.inputWrite("https://example.com/abcdefghijklmnopqrst\r\n    eingerueckt/weiter\r\n");
        for (int row = 0; row < 2; ++row) QCOMPARE(targetsInRow(sc, row), QStringList());
    }

    void oneColumnBeforeTheEdgeCounts() {
        // Claude Code bricht die Eingabe des Anwenders eine Spalte vor dem Rand um.
        VtScreen sc(5, 40);
        sc.inputWrite("https://example.com/abcdefghijklmnopqrs\r\n  t/uvw\r\n");          // 39 Zeichen
        QCOMPARE(lastInk(sc, 0), 38);
        QCOMPARE(ScreenLinks::linkAt(sc, 0, 5).blocked, Blocked::MaybeCut);
    }

    void softWrapWithATrailingWordStaysALink() {
        // Gegenprobe: Hat das TERMINAL umbrochen, steht die Marke da, und sie zählt. Die
        // Adresse endet in der letzten Spalte, der Rest der Zeile folgt nach einem Leerzeichen.
        VtScreen sc(5, 40);
        sc.inputWrite("https://example.com/abcdefghijklmnopqrst und weiter\r\n");
        QVERIFY(sc.lineContinuation(1));
        QCOMPARE(ScreenLinks::linkAt(sc, 0, 5).target, QStringLiteral("https://example.com/abcdefghijklmnopqrst"));
    }

    void conPtyMarksDoNotVouch() {
        // Unter ConPTY entstehen Marken, die mit dem Inhalt nichts zu tun haben: Es löscht mit
        // Leerzeichen-Läufen über Zeilengrenzen, die Marke bleibt stehen, die Zeile wird danach
        // per Cursor neu beschrieben. Dasselbe Bild wie oben, aber kein Beleg.
        for (const bool conPty : {false, true}) {
            VtScreen sc(5, 40);
            sc.setReflow(conPty);
            sc.inputWrite("\033[1;1H" + QByteArray(41, ' '));           // Überlauf: Marke auf Zeile 2
            QVERIFY(sc.lineContinuation(1));
            sc.inputWrite("\033[1;1Hhttps://example.com/abcdefghijklmnopqrst\033[2;3Huvw/xyz");
            QVERIFY(sc.lineContinuation(1));                             // sie klebt
            const auto hit = ScreenLinks::linkAt(sc, 0, 5);
            if (conPty) {
                QVERIFY(!hit.valid());
                QCOMPARE(hit.blocked, Blocked::MaybeCut);
                QVERIFY(ScreenLinks::continuesCutLink(sc, 1, 3));
            } else {
                QVERIFY(hit.valid());
            }
        }
    }

    void conPtySoftWrapAfterASpaceStaysALink() {
        // Gewöhnlicher Fließtext bleibt auch unter ConPTY ein Link: Adresse, ein Leerzeichen,
        // dann bricht das Terminal um. Die vorletzte Spalte zählt dort nur OHNE Marke.
        VtScreen sc(5, 40);
        sc.setReflow(true);
        sc.inputWrite("https://example.com/abcdefghijklmnopqrs weiter im Text\r\n");      // 39 Zeichen
        QCOMPARE(lastInk(sc, 0), 38);
        QVERIFY(sc.lineContinuation(1));
        QCOMPARE(firstInk(sc, 1), 0);
        QCOMPARE(ScreenLinks::linkAt(sc, 0, 5).target, QStringLiteral("https://example.com/abcdefghijklmnopqrs"));
    }

    void tableCellFilledToItsBorder() {
        // Tabellen zeichnet Claude Code mit Rahmen und bricht in der Zelle um.
        const QByteArray bar = "\xE2\x94\x82";   // │
        const auto row = [&bar](const QByteArray &a, const QByteArray &b) {     // Zellen 2 und 32 breit
            return bar + ' ' + a.leftJustified(2) + ' ' + bar + ' ' + b.leftJustified(32) + ' ' + bar + "\r\n";
        };
        VtScreen sc(5, 60);
        sc.inputWrite(row("ab", "https://example.com/abcdefghijkl") + row("cd", "mnop/qrst"));
        QCOMPARE(lastInk(sc, 0), 40);
        QCOMPARE(targetsInRow(sc, 0), QStringList());
        QCOMPARE(ScreenLinks::linkAt(sc, 0, 10).blocked, Blocked::MaybeCut);
        QVERIFY(ScreenLinks::continuesCutLink(sc, 1, 8));
        QVERIFY(!ScreenLinks::continuesCutLink(sc, 1, 2));           // Nachbarzelle
        // Gegenprobe: Die Zelle ist nicht voll, die Adresse also zu Ende.
        VtScreen ok(5, 60);
        ok.inputWrite(row("ab", "https://example.com/abcdefg") + row("cd", "mnop/qrst"));
        QCOMPARE(targetsInRow(ok, 0), QStringList{QStringLiteral("https://example.com/abcdefg")});
        // Die Zelle darunter ist leer: Auch dann ist die Adresse zu Ende.
        VtScreen empty(5, 60);
        empty.inputWrite(row("ab", "https://example.com/abcdefghijkl") + row("cd", ""));
        QCOMPARE(targetsInRow(empty, 0), QStringList{QStringLiteral("https://example.com/abcdefghijkl")});
        // Und eine volle Zelle über dem Tabellenende bleibt ein Link.
        VtScreen last(5, 60);
        last.inputWrite(row("ab", "https://example.com/abcdefghijkl")
                        + "\xE2\x94\x94" + QByteArray("\xE2\x94\x80").repeated(39) + "\xE2\x94\x98\r\n");
        QCOMPARE(targetsInRow(last, 0), QStringList{QStringLiteral("https://example.com/abcdefghijkl")});
    }

    void equalLengthUrlsBelowEachOtherStayLinks() {
        // Alltag: Listen gleich langer Adressen. Jede steht für sich.
        VtScreen sc(5, 60);
        sc.inputWrite("https://example.com/eins\r\nhttps://example.com/zwei\r\nhttps://example.com/drei\r\n");
        QCOMPARE(targetsInRow(sc, 0), QStringList{QStringLiteral("https://example.com/eins")});
        QCOMPARE(targetsInRow(sc, 1), QStringList{QStringLiteral("https://example.com/zwei")});
    }

    void equalLengthPathsBelowEachOtherStayLinks() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QVERIFY(QDir(dir.path()).mkpath(QStringLiteral("src")));
        for (const char *name : {"src/a.txt", "src/b.txt"}) {
            QFile f(dir.filePath(QLatin1String(name)));
            QVERIFY(f.open(QIODevice::WriteOnly));
        }
        VtScreen sc(5, 60);
        sc.inputWrite("src/a.txt\r\nsrc/b.txt\r\n");
        QVERIFY(ScreenLinks::linkAt(sc, 0, 2, dir.path()).valid());
        QVERIFY(ScreenLinks::linkAt(sc, 1, 2, dir.path()).valid());
    }

    void wideCharAtTheSoftWrapDoesNotCutTheLink() {
        // Passt ein breites Zeichen nicht mehr in die letzte Spalte, bricht das Terminal davor um
        // und lässt die Spalte leer. Als Leerzeichen gelesen endete die Adresse dort.
        VtScreen sc(5, 20);
        const QByteArray wide = "\xE4\xB8\xAD";                          // 中, zwei Spalten
        sc.inputWrite("https://example.com/" + wide.repeated(1) + "abcdefghijklmnopq" + wide + "/ende\r\n");
        // 20 + 2 + 17 = 39 Spalten: Das zweite 中 müsste in Spalte 39 beginnen und passt nicht.
        QVERIFY(sc.cell(1, 19).text.isEmpty());
        QVERIFY(sc.lineContinuation(2));
        const QString full = QStringLiteral("https://example.com/") + QString::fromUtf8(wide)
                           + QStringLiteral("abcdefghijklmnopq") + QString::fromUtf8(wide) + QStringLiteral("/ende");
        for (int row = 0; row < 3; ++row) QCOMPARE(ScreenLinks::linkAt(sc, row, 1).target, full);
    }

    // --- Hyperlinks des Programms (OSC 8) -----------------------------------------------------
    // Mit FORCE_HYPERLINK=1 nennt Claude Code das Ziel selbst, auf jeder Zeile dasselbe.

    void claudeFullscreenOsc8OpensTheFullTargetOnEveryLine() {
        VtScreen sc(29, 84);
        sc.inputWrite("\033[?1049h\033[2J\033[1;21H" + bytes(linkwrapcap::kFullscreenOsc8));
        const QString full = probeUrl("A");
        QCOMPARE(lastInk(sc, 0), 83);
        QVERIFY(!sc.lineContinuation(1));
        QVERIFY(everyCellOpens(sc, 0, 20, 83, full, 3));
        QVERIFY(everyCellOpens(sc, 1, firstInk(sc, 1), lastInk(sc, 1), full, 3));
        // Dritte Zeile: Hinter der Adresse geht der Satz weiter, der gehört nicht mehr dazu.
        const int end = firstInk(sc, 2) + int(QStringLiteral("/n026/n027/n028/n029/n030?fall=A&ende=1").size()) - 1;
        QVERIFY(everyCellOpens(sc, 2, firstInk(sc, 2), end, full, 3));
        QVERIFY(!ScreenLinks::linkAt(sc, 2, end + 1).valid());
        const auto hit = ScreenLinks::linkAt(sc, 1, 10);
        QVERIFY(hit.declared);
        QCOMPARE(hit.segments[0], (ScreenLinks::Segment{0, 20, 83}));
        QCOMPARE(hit.segments[1].row, 1);
        QCOMPARE(hit.segments[2].row, 2);
    }

    void claudeClassicOsc8ToolOutput() {
        VtScreen sc(29, 84);
        sc.inputWrite("\033[1;6H" + bytes(linkwrapcap::kClassicOsc8ToolOutput));
        const QString full = probeUrl("D");
        for (int row = 0; row < 3; ++row)
            QVERIFY(everyCellOpens(sc, row, firstInk(sc, row), lastInk(sc, row), full, 3));
    }

    void osc8ArrivesThroughConPty() {
        // ConPTY reicht OSC 8 durch, mit eigener id (`u-…%…`) und ST statt BEL. Die Marken
        // dort sind kein Maßstab, das Ziel kommt vom Programm.
        VtScreen sc(29, 84);
        sc.setReflow(true);
        sc.inputWrite(bytes(linkwrapcap::kConPtyOsc8));
        const QString full = probeUrl("A");
        QVERIFY(everyCellOpens(sc, 19, 20, 83, full, 3));
        QVERIFY(everyCellOpens(sc, 20, firstInk(sc, 20), lastInk(sc, 20), full, 3));
        QVERIFY(everyCellOpens(sc, 21, firstInk(sc, 21), lastInk(sc, 21), full, 3));
    }

    void declaredTargetBeatsTheVisibleText() {
        // Was geöffnet wird, nennt die Pille: das Ziel des Programms, nicht der Text davor.
        VtScreen sc(5, 60);
        sc.inputWrite("\033]8;;https://example.org/ziel\033\\https://example.com/text\033]8;;\033\\ danach\r\n");
        const auto hit = ScreenLinks::linkAt(sc, 0, 5);
        QCOMPARE(hit.target, QStringLiteral("https://example.org/ziel"));
        QVERIFY(hit.declared);
        QCOMPARE(hit.segments, (QList<ScreenLinks::Segment>{{0, 0, 23}}));
        QVERIFY(!ScreenLinks::linkAt(sc, 0, 26).valid());
    }

    void declaredTargetWithoutAUrlAsText() {
        VtScreen sc(5, 60);
        sc.inputWrite("siehe \033]8;id=x;https://example.org/doku\007die Doku\033]8;;\007.\r\n");
        QCOMPARE(ScreenLinks::linkAt(sc, 0, 8).target, QStringLiteral("https://example.org/doku"));
        QVERIFY(!ScreenLinks::linkAt(sc, 0, 5).valid());
        QVERIFY(!ScreenLinks::linkAt(sc, 0, 14).valid());       // der Punkt dahinter
    }

    void refusedDeclaredTargets_data() {
        QTest::addColumn<QByteArray>("uri");
        QTest::addColumn<bool>("file");
        QTest::newRow("javascript") << QByteArray("javascript:alert(1)") << false;
        QTest::newRow("fremdes Schema") << QByteArray("vscode://datei/x") << false;
        QTest::newRow("data") << QByteArray("data:text/html,x") << false;
        QTest::newRow("ohne Schema") << QByteArray("example.com/x") << false;
        QTest::newRow("relativer Pfad") << QByteArray("src/main.cpp") << false;
        QTest::newRow("UNC") << QByteArray("\\\\server\\freigabe\\x") << false;
        QTest::newRow("Leerzeichen") << QByteArray("https://example.com/a b") << false;
        // Eine Datei nennt ein Hyperlink des Programms nie, auch keine lokale: Der sichtbare
        // Text sagt nicht, wohin er zeigt.
        QTest::newRow("file lokal") << QByteArray("file:///tmp/datei.txt") << true;
        QTest::newRow("file lokal, Programm") << QByteArray("file:///Applications/Rechner.app") << true;
        QTest::newRow("file Laufwerk") << QByteArray("file:///C:/Windows/System32/calc.exe") << true;
        QTest::newRow("file gross geschrieben") << QByteArray("FILE:///tmp/datei.txt") << true;
        QTest::newRow("file ohne Schraegstriche") << QByteArray("file:/tmp/datei.txt") << true;
        QTest::newRow("file mit Rechner") << QByteArray("file://server/freigabe/x") << true;
        QTest::newRow("file localhost") << QByteArray("file://localhost/etc/hosts") << true;
        QTest::newRow("file Netzpfad") << QByteArray("file:////server/freigabe/x") << true;
        QTest::newRow("file kodierter Netzpfad") << QByteArray("file:///%2Fserver/freigabe/x") << true;
        QTest::newRow("file Punkt-Segmente") << QByteArray("file:///x/..//server/y") << true;
        // Zeichen, die die Pille anders zeigt, als sie beim Öffnen ankommen.
        QTest::newRow("Richtungsumkehr U+202E") << QByteArray("https://example.com/\xE2\x80\xAEgpj.exe") << false;
        QTest::newRow("Richtungs-Isolat U+2066") << QByteArray("https://example.com/\xE2\x81\xA6x") << false;
        QTest::newRow("Leerzeichen ohne Breite U+200B") << QByteArray("https://example.com/a\xE2\x80\x8B" "b") << false;
        QTest::newRow("weiches Trennzeichen U+00AD") << QByteArray("https://exam\xC2\xADple.com/") << false;
        QTest::newRow("Byte-Reihenfolge-Marke U+FEFF") << QByteArray("https://example.com/\xEF\xBB\xBFx") << false;
        QTest::newRow("Tag-Zeichen U+E0041") << QByteArray("https://example.com/\xF3\xA0\x81\x81x") << false;
        QTest::newRow("Zeilentrenner U+2028") << QByteArray("https://example.com/a\xE2\x80\xA8" "b") << false;
    }

    void refusedDeclaredTargets() {
        // Wird das Ziel eines Programms abgelehnt, öffnet auch der Text an der Stelle nichts:
        // Er ist nicht das Ziel.
        QFETCH(QByteArray, uri);
        QFETCH(bool, file);
        VtScreen sc(5, 60);
        sc.inputWrite("\033]8;;" + uri + "\033\\https://example.com/sichtbar\033]8;;\033\\\r\n");
        QVERIFY(sc.cell(0, 3).link != 0);
        const auto hit = ScreenLinks::linkAt(sc, 0, 3);
        QVERIFY(!hit.valid());
        QCOMPARE(hit.blocked, file ? Blocked::RefusedFile : Blocked::Refused);
        QCOMPARE(hit.refused, QString::fromUtf8(uri));
        QVERIFY(hit.segments.isEmpty());
    }

    void declaredFileNeverOpensButTheSamePathAsTextDoes() {
        // Dieselbe vorhandene Datei, zweimal: als Ziel hinter fremdem Text (öffnet nie) und als
        // sichtbarer Pfad (öffnet wie bisher). Der Unterschied ist, was der Anwender liest.
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath(QStringLiteral("werkzeug.command"));
        QFile f(path);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.close();
        const QByteArray target = QUrl::fromLocalFile(path).toEncoded();
        QVERIFY(target.startsWith("file:///"));

        VtScreen sc(5, 200);
        sc.inputWrite("\033]8;;" + target + "\033\\https://example.com/harmlos\033]8;;\033\\\r\n");
        sc.inputWrite(path.toUtf8() + "\r\n");
        const auto hidden = ScreenLinks::linkAt(sc, 0, 5, dir.path());
        QVERIFY(!hidden.valid());
        QCOMPARE(hidden.blocked, Blocked::RefusedFile);
        QCOMPARE(hidden.refused, QString::fromUtf8(target));
        const auto visible = ScreenLinks::linkAt(sc, 1, 5, dir.path());
        QVERIFY(visible.valid());
        QVERIFY(!visible.declared);
        QCOMPARE(visible.kind, LinkDetector::Span::FilePath);
        QCOMPARE(QFileInfo(visible.target).canonicalFilePath(), QFileInfo(path).canonicalFilePath());
        // Und die Angabe als Text, `file:///…` ausgeschrieben, bleibt ein Link wie in 1.9.11.
        VtScreen text(5, 200);
        text.inputWrite(target + "\r\n");
        QVERIFY(ScreenLinks::linkAt(text, 0, 5, dir.path()).valid());
    }

    void declaredTargetRejectsWhatThePillCannotShow() {
        // Direkt an der Prüfung, ohne Terminal davor: Sie darf sich nicht darauf verlassen, was
        // ein Parser durchreicht.
        const QString base = QStringLiteral("https://example.com/a");
        QCOMPARE(LinkDetector::declaredTarget(base), base);
        for (const char32_t cp : {char32_t(0x1B), char32_t(0x7F), char32_t(0x85), char32_t(0x9C),
                                  char32_t(0xAD), char32_t(0x200D), char32_t(0x202A), char32_t(0x202E),
                                  char32_t(0x2069), char32_t(0x2029), char32_t(0xFEFF),
                                  char32_t(0xE0001), char32_t(0xE007F)}) {
            // Nicht über QString::fromUcs4: Das liest ein führendes U+FEFF als Marke und entfernt es.
            const QString uri = base + QStringView(QChar::fromUcs4(cp)).toString() + QStringLiteral("b");
            QCOMPARE(uri.size(), base.size() + (cp > 0xFFFF ? 2 : 1) + 1);
            QVERIFY2(LinkDetector::declaredTarget(uri).isEmpty(),
                     qPrintable(QStringLiteral("U+%1").arg(uint(cp), 4, 16, QLatin1Char('0'))));
        }
        // Eine halbe Ersatzzeichen-Folge ist kein Zeichen, gleich an welcher Stelle.
        QVERIFY(LinkDetector::declaredTarget(base + QChar(0xD83D)).isEmpty());
        QVERIFY(LinkDetector::declaredTarget(base + QChar(0xDE00) + QStringLiteral("b")).isEmpty());
        // Gegenprobe: Buchstaben und Bildzeichen bleiben erlaubt, auch jenseits von U+FFFF.
        for (const QString &ok : {QString::fromUtf8("https://example.com/gr\xC3\xBC\xC3\x9F" "e"),
                                  QString::fromUtf8("https://example.com/\xF0\x9F\x98\x80"),
                                  QString::fromUtf8("https://example.com/\xE6\x97\xA5\xE6\x9C\xAC")})
            QCOMPARE(LinkDetector::declaredTarget(ok), ok);
    }

    void allowedDeclaredTargets() {
        for (const QByteArray &uri : {QByteArray("https://example.com/x?a=1&b=2#teil"),
                                      QByteArray("http://example.com/"), QByteArray("mailto:jemand@example.com"),
                                      QByteArray("ftp://example.com/datei"),
                                      // Buchstaben und Bildzeichen jenseits von ASCII sind keine
                                      // Formatzeichen, auch nicht jenseits von U+FFFF.
                                      QByteArray("https://example.com/gr\xC3\xBC\xC3\x9F" "e"),
                                      QByteArray("https://example.com/\xF0\x9F\x98\x80")}) {
            VtScreen sc(5, 60);
            sc.inputWrite("\033]8;;" + uri + "\033\\Text\033]8;;\033\\\r\n");
            QCOMPARE(ScreenLinks::linkAt(sc, 0, 1).target, QString::fromUtf8(uri));
        }
    }

    void networkPathAcrossAWrapIsNoLink() {
        // Netzpfade bleiben gesperrt, gleich wie sie auf den Schirm kommen: weich umbrochen,
        // hart umbrochen oder als Hyperlink des Programms.
        VtScreen soft(5, 20);
        soft.inputWrite("\\\\server\\freigabe\\ordner\\datei.txt\r\n");
        QVERIFY(soft.lineContinuation(1));
        VtScreen url(5, 20);
        url.inputWrite("file://server/freigabe/ordner/datei.txt\r\n");
        VtScreen hard(5, 20);
        hard.inputWrite("file://server/freiga\r\nbe/ordner/datei.txt\r\n");
        for (const VtScreen *sc : {&soft, &url, &hard})
            for (int row = 0; row < 2; ++row) QCOMPARE(targetsInRow(*sc, row), QStringList());
    }

    void sameIdOnTwoLinesIsOneLink() {
        VtScreen sc(5, 30);
        sc.inputWrite("\033]8;id=a;https://example.com/eins\033\\erste Zeile\033]8;;\033\\\r\n"
                      "  \033]8;id=a;https://example.com/eins\033\\zweite\033]8;;\033\\ und \033]8;id=b;https://example.com/zwei\033\\andere\033]8;;\033\\\r\n");
        const auto hit = ScreenLinks::linkAt(sc, 1, 3);
        QCOMPARE(hit.target, QStringLiteral("https://example.com/eins"));
        QCOMPARE(hit.segments, (QList<ScreenLinks::Segment>{{0, 0, 10}, {1, 2, 7}}));
        const auto other = ScreenLinks::linkAt(sc, 1, 14);
        QCOMPARE(other.target, QStringLiteral("https://example.com/zwei"));
        QCOMPARE(other.segments.size(), 1);
    }

    void hyperlinkOnAWideCharacter() {
        VtScreen sc(5, 30);
        sc.inputWrite("\033]8;;https://example.com/w\033\\\xE4\xB8\xAD\xE6\x96\x87\033]8;;\033\\x\r\n");
        for (int col = 0; col < 4; ++col)                 // beide Hälften beider Zeichen
            QCOMPARE(ScreenLinks::linkAt(sc, 0, col).target, QStringLiteral("https://example.com/w"));
        QVERIFY(!ScreenLinks::linkAt(sc, 0, 4).valid());
    }

    void hyperlinkInTheScrollback() {
        VtScreen sc(3, 30);
        sc.inputWrite("\033]8;;https://example.com/alt\033\\oben\033]8;;\033\\\r\n1\r\n2\r\n3\r\n4\r\n");
        QVERIFY(sc.scrollbackCount() >= 2);
        QCOMPARE(ScreenLinks::linkAt(sc, 0, 1).target, QStringLiteral("https://example.com/alt"));
    }

    void droppedHyperlinkFallsBackToTheText() {
        // Ist die Tabelle voll, fällt der älteste Eintrag heraus. Seine Zellen sind dann Text
        // wie jeder andere: Eine Adresse darin wird erkannt, ein beliebiges Wort nicht.
        VtScreen sc(5, 60);
        sc.inputWrite("\033]8;;https://example.org/ziel\033\\Wort\033]8;;\033\\ "
                      "\033]8;;https://example.org/zwei\033\\https://example.com/text\033]8;;\033\\\r\n");
        QCOMPARE(ScreenLinks::linkAt(sc, 0, 1).target, QStringLiteral("https://example.org/ziel"));
        for (int i = 0; i < VtScreen::kMaxHyperlinks; ++i)
            sc.inputWrite("\033]8;;https://example.net/" + QByteArray::number(i) + "\033\\\033]8;;\033\\");
        QVERIFY(sc.hyperlink(sc.cell(0, 1).link).isEmpty());
        QVERIFY(!ScreenLinks::linkAt(sc, 0, 1).valid());
        QCOMPARE(ScreenLinks::linkAt(sc, 0, 8).target, QStringLiteral("https://example.com/text"));
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
