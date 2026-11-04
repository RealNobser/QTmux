// ConPTY-Verhalten mit ECHTEN Bytes prüfen — auf jeder Plattform (Clink-Auftrag 2026-11-03).
//
// Owner-Befund: Mit Clink geht unter Windows Scrollback „teilweise“ kaputt, ebenso die
// Formatierung der History. Der Mitschnitt an einer Windows-11-Maschine (cmd.exe in
// CreatePseudoConsole, wie src/core/WindowsPty.cpp) zeigte zwei Mechanismen, die NICHT an
// Clink hängen, sondern an ConPTY — Clink macht sie nur häufiger (längere Zeilen durch
// Farb-Prompt, Autosuggest-Hinweis, Zeitstempel im `history`):
//
//   1. Bei jeder Größenänderung bricht ConPTY seinen eigenen Puffer um und malt den
//      Sichtbereich ab ESC[H neu, OHNE zu scrollen. Was dabei bei ihm über den oberen Rand
//      wanderte, hat das Terminal nie als Scroll gesehen — ohne eigenen Reflow wird die
//      oberste Zeile einfach überschrieben und fehlt für immer im Verlauf.
//   2. Jede ConPTY-Session beginnt mit ESC[2J ESC[H. Ein wiederhergestellter Verlauf
//      (QTMUX-130), der noch auf dem sichtbaren Bildschirm steht, wird davon gelöscht.
//
// Die Bytes stehen in conpty_captures.h (generiert, Herkunft und sha256 dort). Sie liegen
// als Escape-Literale vor statt als Binärdatei, damit kein CRLF-Umbau beim Checkout auf
// einem Windows-Runner sie verändern kann.
//
// 🔑 Die `…WithoutFix…`-Fälle sind die Gegenprobe zum Messmittel: Sie zeigen, dass die
// Mitschnitte den Fehler ohne die Abhilfe wirklich auslösen. Fiele einer davon grün aus,
// prüften die Haupttests nichts.

#include <QtTest>
#include <QRegularExpression>

#include "ITerminalBackend.h"
#include "PtyBackend.h"
#include "Session.h"
#include "VtScreen.h"
#include "conpty_captures.h"

using namespace qtmux;

namespace {

class FakeBackend final : public ITerminalBackend {
public:
    bool start(int, int) override { setState(BackendState::Running); return true; }
    void write(const QByteArray &) override {}
    void resize(int, int) override {}
    void terminate() override { setState(BackendState::Closed); }
    void feed(const QByteArray &d) { emit dataReceived(d); }
};

// Alle Zeilen der Form „<prefix> <n>“ aus Scrollback + Bildschirm, in Anzeigereihenfolge.
QList<int> numberedLines(const VtScreen &s, const QString &prefix) {
    const QString all = s.scrollbackText() + QLatin1Char('\n') + s.screenText();
    const QRegularExpression re(QStringLiteral("^%1 (\\d+)$").arg(prefix),
                                QRegularExpression::MultilineOption);
    QList<int> out;
    for (auto it = re.globalMatch(all); it.hasNext();) out << it.next().captured(1).toInt();
    return out;
}

QList<int> oneTo(int n) {
    QList<int> l;
    for (int i = 1; i <= n; ++i) l << i;
    return l;
}

using Capture = QList<conptycap::Segment>;

template <std::size_t N>
Capture capture(const conptycap::Segment (&segs)[N]) {
    return Capture(std::begin(segs), std::end(segs));
}

// Spielt den Mitschnitt in der Reihenfolge ab, in der Session::resize es tut: erst die
// neue Größe in den Screen, danach die Bytes, die ConPTY darauf hin schickt. Liefert je
// Segment die gefundenen „Zeile n“.
QList<QList<int>> play(VtScreen &s, const Capture &cap) {
    QList<QList<int>> perStep;
    for (int i = 0; i < cap.size(); ++i) {
        if (i > 0) s.setSize(cap[i].rows, cap[i].cols);
        s.inputWrite(QByteArray(cap[i].bytes, qsizetype(cap[i].size)));
        perStep << numberedLines(s, QStringLiteral("Zeile"));
    }
    return perStep;
}

Capture byName(const QString &which) {
    return which == QLatin1String("cmd-mit-clink-enhanced")
        ? capture(conptycap::kResizeClinkEnhanced)
        : capture(conptycap::kResizePlainCmd);
}

QByteArray restoredDump() {
    QByteArray d;
    for (int i = 1; i <= 40; ++i) d += "Verlauf " + QByteArray::number(i) + "\r\n";
    return d;
}

} // namespace

class TestConPtyReplay : public QObject {
    Q_OBJECT
private slots:
    void resizeKeepsEveryLine_data();
    void resizeKeepsEveryLine();
    void resizeWithoutFixLosesLines_data();
    void resizeWithoutFixLosesLines();
    void restoredHistorySurvivesConPtyStart();
    void restoreWithoutFixLosesHistory();
    void restoredHistoryWithOpenLastLine();
    void restoreOutsideConPtyStaysOnScreen();
    void conPtyHostFollowsBackend();
    void reflowHostileResizesDoNotCrash_data();
    void reflowHostileResizesDoNotCrash();
    void reflowRandomResizesStayConsistent();
    void reflowKeepsCursorOnLastWrappedRow();
    void classicRendererRepaintKeepsScrollback_data();
    void classicRendererRepaintKeepsScrollback();
    void fullscreenRendererLeavesNoScrollback();
    void eraseScrollbackSequenceIsIgnored();
};

void TestConPtyReplay::resizeKeepsEveryLine_data() {
    QTest::addColumn<QString>("which");
    QTest::newRow("cmd-mit-clink-enhanced") << QStringLiteral("cmd-mit-clink-enhanced");
    QTest::newRow("cmd-ohne-clink") << QStringLiteral("cmd-ohne-clink");
}

// Kern (Symptom „Scrollback teilweise kaputt“): Nach JEDER Größenänderung steht jede der
// 60 Zeilen genau einmal und in Reihenfolge im Verlauf.
void TestConPtyReplay::resizeKeepsEveryLine() {
    QFETCH(QString, which);
    const Capture cap = byName(which);
    VtScreen s(cap.first().rows, cap.first().cols);
    s.setReflow(true);
    const QList<QList<int>> steps = play(s, cap);
    for (int i = 0; i < steps.size(); ++i) {
        const QString where = QStringLiteral("nach Segment %1 (%2x%3)")
                                  .arg(i).arg(cap[i].cols).arg(cap[i].rows);
        QVERIFY2(steps[i] == oneTo(60), qPrintable(where));
    }
}

void TestConPtyReplay::resizeWithoutFixLosesLines_data() { resizeKeepsEveryLine_data(); }

// Gegenprobe: Derselbe Mitschnitt OHNE Reflow verliert Zeilen — so stand es vor dem Fix
// (gemessen: Clink-Enhanced „Zeile 49“ und „Zeile 57“, cmd ohne Clink „Zeile 46“).
void TestConPtyReplay::resizeWithoutFixLosesLines() {
    QFETCH(QString, which);
    const Capture cap = byName(which);
    VtScreen s(cap.first().rows, cap.first().cols);
    QVERIFY(!s.reflow());   // Vorgabe: aus (forkpty-Sessions bleiben unverändert)
    const QList<QList<int>> steps = play(s, cap);
    QCOMPARE(steps.first(), oneTo(60));   // vor dem ersten Resize ist alles da
    QVERIFY(steps.last().size() < 60);
}

// Kern (Symptom „History-Formatierung kaputt“ in der Lesart „wiederhergestellter
// Verlauf“): Verlauf + Marker überleben den ConPTY-Start, und das Prompt steht darunter.
void TestConPtyReplay::restoredHistorySurvivesConPtyStart() {
    Session sess;
    auto *fake = new FakeBackend;
    sess.attachBackend(fake, Session::Type::Shell, 80, 24);
    sess.setConPtyHost(true);
    QVERIFY(sess.screen()->reflow());
    sess.setPendingHistory(restoredDump());
    sess.markRestored(QDateTime());
    fake->feed(QByteArray(conptycap::kStartClink, qsizetype(conptycap::kStartClinkSize)));
    sess.resize(100, 30);   // erste echte Größe gibt den Verlauf frei

    const VtScreen &s = *sess.screen();
    QCOMPARE(numberedLines(s, QStringLiteral("Verlauf")), oneTo(40));
    QVERIFY(s.scrollbackText().contains(QStringLiteral("QTmux:")));
    QVERIFY(s.screenText().contains(QStringLiteral("WINHOST1 C:\\Users\\tester\\qtmux-clink>")));
    // Der Marker steht direkt über dem Bildschirm, nach dem letzten Verlaufseintrag.
    const QString sb = s.scrollbackText();
    QVERIFY(sb.indexOf(QStringLiteral("QTmux:")) > sb.indexOf(QStringLiteral("Verlauf 40")));
}

// Gegenprobe: Ohne ConPTY-Behandlung frisst das ESC[2J den sichtbaren Teil des Verlaufs.
void TestConPtyReplay::restoreWithoutFixLosesHistory() {
    Session sess;
    auto *fake = new FakeBackend;
    sess.attachBackend(fake, Session::Type::Shell, 80, 24);
    QVERIFY(!sess.conPtyHost());
    sess.setPendingHistory(restoredDump());
    sess.markRestored(QDateTime());
    fake->feed(QByteArray(conptycap::kStartClink, qsizetype(conptycap::kStartClinkSize)));
    sess.resize(100, 30);

    QVERIFY(numberedLines(*sess.screen(), QStringLiteral("Verlauf")).size() < 40);
    QVERIFY(!sess.screen()->screenText().contains(QStringLiteral("QTmux:")));
}

// Endet der Dump mitten in einer Zeile (ohne Marker, letzter Eintrag ohne CRLF), muss
// auch diese Zeile in den Scrollback — sie stünde sonst oben im Bildschirm und fiele dem
// ESC[2J zum Opfer.
void TestConPtyReplay::restoredHistoryWithOpenLastLine() {
    Session sess;
    auto *fake = new FakeBackend;
    sess.attachBackend(fake, Session::Type::Shell, 80, 24);
    sess.setConPtyHost(true);
    QByteArray dump = restoredDump();
    dump.chop(2);   // „Verlauf 40“ ohne abschließendes CRLF
    sess.setPendingHistory(dump);
    fake->feed(QByteArray(conptycap::kStartClink, qsizetype(conptycap::kStartClinkSize)));
    sess.resize(100, 30);
    QCOMPARE(numberedLines(*sess.screen(), QStringLiteral("Verlauf")), oneTo(40));
}

// forkpty-Sessions behalten das bisherige Verhalten: Der Verlauf bleibt sichtbar über dem
// frischen Prompt (eine Unix-Shell löscht beim Start nicht).
void TestConPtyReplay::restoreOutsideConPtyStaysOnScreen() {
    Session sess;
    auto *fake = new FakeBackend;
    sess.attachBackend(fake, Session::Type::Shell, 80, 24);
    sess.setPendingHistory(restoredDump());
    fake->feed(QByteArrayLiteral("$ "));
    sess.resize(100, 30);
    QVERIFY(sess.screen()->screenText().contains(QStringLiteral("Verlauf 40")));
    QCOMPARE(numberedLines(*sess.screen(), QStringLiteral("Verlauf")), oneTo(40));
}

// Die Weiche selbst: ConPTY-Host genau dann, wenn Windows UND das Backend ein PtyBackend
// ist (lokale Shell; SshBackend erbt davon). Attrappen und Plugin-Backends nie.
void TestConPtyReplay::conPtyHostFollowsBackend() {
#if defined(Q_OS_WIN)
    const bool expectPty = true;
#else
    const bool expectPty = false;
#endif
    {
        Session sess;
        sess.attachBackend(new PtyBackend, Session::Type::Shell, 80, 24);
        QCOMPARE(sess.conPtyHost(), expectPty);
        QCOMPARE(sess.screen()->reflow(), expectPty);
    }
    {
        Session sess;
        sess.attachBackend(new FakeBackend, Session::Type::Shell, 80, 24);
        QVERIFY(!sess.conPtyHost());
        QVERIFY(!sess.screen()->reflow());
    }
}

// libvterm-Reflow ist in 0.3.3 nicht absturzfest: Ein Zufallstester unter ASan scheiterte
// mit dem unveränderten Code in JEDEM von 1500 Läufen — abort() („screen_resize failed to
// update cursor position“) oder Lesen vor dem Pufferanfang in resize_buffer. Weil die
// Ausgabe vom Programm im Terminal kommt (per ssh auch von einem fremden Rechner), wäre das
// ein Absturz des ganzen QTmux samt aller Sessions. Behoben durch Patch 4 in
// third_party/libvterm/src/screen.c (QTMUX:-Marker, aus Neovims libvterm-Fassung).
void TestConPtyReplay::reflowHostileResizesDoNotCrash_data() {
    QTest::addColumn<QByteArray>("input");
    QTest::addColumn<int>("rows");
    QTest::addColumn<int>("cols");
    // Eine umgebrochene Zeile füllt den Schirm, Cursor in ihrer LETZTEN Zeile, schrumpfen.
    QTest::newRow("cursor-letzte-zeile") << QByteArray(29, 'a') << 3 << 5;
    // Mehr als ein Schirm am Stück: Zeile 0 ist Fortsetzung, Cursor darin.
    QTest::newRow("zeile0-fortsetzung-cursor") << QByteArray(35, 'b') << 3 << 7;
    // Zeile 0 ist Fortsetzung, Cursor in eigener Zeile darunter.
    QTest::newRow("zeile0-fortsetzung") << QByteArray(35, 'c') + "\r\nx" << 3 << 7;
}

void TestConPtyReplay::reflowHostileResizesDoNotCrash() {
    QFETCH(QByteArray, input);
    QFETCH(int, rows);
    QFETCH(int, cols);
    VtScreen s(3, 10);
    s.setReflow(true);
    s.inputWrite(input);
    s.setSize(rows, cols);   // vor Patch 4: abort() bzw. Lesen vor dem Puffer
    QVERIFY(s.cursor().y() >= 0 && s.cursor().y() < rows);
    QVERIFY(s.cursor().x() >= 0 && s.cursor().x() < cols);
}

// Cursor in der LETZTEN Zeile einer umgebrochenen Zeile, die nach dem Umbruch nicht mehr in
// den Schirm passt (29 Zeichen bei Breite 5 = 6 Zeilen > 3): libvterm schiebt die logische
// Zeile dann aus dem Schirm und setzt den Cursor in Zeile 0, Spalte = alte Spalte,
// begrenzt (9 -> 4). Vor Patch 4 blieb der Cursor dabei unbelegt (abort()); ohne die
// `<=`-Korrektur fiele er heute auf die Notbegrenzung (0,0) zurück.
void TestConPtyReplay::reflowKeepsCursorOnLastWrappedRow() {
    VtScreen s(3, 10);
    s.setReflow(true);
    s.inputWrite(QByteArray(29, 'a'));
    QCOMPARE(s.cursor(), QPoint(9, 2));
    s.setSize(3, 5);
    QCOMPARE(s.cursor(), QPoint(4, 0));
    QCOMPARE(s.screenText(), QString());
    // ⚠️ Bekannte Grenze (libvterm wie Neovim, hier bewusst NICHT als „richtig“ festgehalten):
    // Ist eine einzelne logische Zeile nach dem Umbruch höher als der ganze Schirm, landet
    // nur ihre erste alte Zeile im Scrollback (hier 10 von 29 Zeichen).
}

// Deterministischer Zufallslauf (gleiche Mechanik wie der ASan-Tester): schreiben,
// umbrechen, Cursor setzen, löschen, Alt-Screen, Größe ändern. Nach jedem Schritt darf auf
// dem Schirm nur stehen, was geschrieben wurde (a–z) — Fremdbytes aus einem Lesen vor dem
// Puffer fielen hier auf; ein abort() beendete den Testprozess.
void TestConPtyReplay::reflowRandomResizesStayConsistent() {
    for (unsigned seed = 0; seed < 300; ++seed) {
        unsigned st = seed * 2654435761u + 1;
        auto rnd = [&st](int n) { st = st * 1103515245u + 12345u; return int((st >> 16) % unsigned(n)); };
        int rows = 1 + rnd(8), cols = 1 + rnd(20);
        VtScreen s(rows, cols);
        s.setReflow(true);
        for (int step = 0; step < 200; ++step) {
            const int a = rnd(10);
            if (a < 5) {
                QByteArray b;
                const int n = rnd(40);
                for (int i = 0; i < n; ++i) b += char('a' + rnd(26));
                s.inputWrite(b);
            } else if (a == 5) {
                s.inputWrite("\r\n");
            } else if (a == 6) {
                s.inputWrite("\x1b[" + QByteArray::number(1 + rnd(rows + 2)) + ';'
                             + QByteArray::number(1 + rnd(cols + 2)) + 'H');
            } else if (a == 7) {
                static const char *const esc[] = {"\x1b[2J", "\x1b[K", "\x1b[S", "\x1bM",
                                                  "\x1b[?1049h", "\x1b[?1049l", "\x1b[3T"};
                s.inputWrite(esc[rnd(7)]);
            } else {
                rows = 1 + rnd(8);
                cols = 1 + rnd(20);
                s.setSize(rows, cols);
            }
            // Bewusst KEINE Cursor-Grenzprüfung hier: `VtScreen::cursor()` ist ein Cache aus
            // cbMoveCursor und steht nach Alt-Screen-/Scroll-Folgen auch OHNE Reflow
            // gelegentlich außerhalb (gemessen 2026-11-03, vorbestehend, eigener Faden).
            const QString where = QStringLiteral("seed %1 step %2").arg(seed).arg(step);
            for (int r = 0; r < rows; ++r)
                for (int c = 0; c < cols; ++c) {
                    const QString t = s.cell(r, c).text;
                    if (t.isEmpty() || t == QLatin1String(" ")) continue;
                    const QChar ch = t.at(0);
                    QVERIFY2(t.size() == 1 && ch >= QLatin1Char('a') && ch <= QLatin1Char('z'),
                             qPrintable(where + QStringLiteral(" Zelle %1,%2 = U+%3")
                                                    .arg(r).arg(c).arg(uint(ch.unicode()), 4, 16)));
                }
        }
    }
}

// --- Claude Codes klassischer Renderer bei einer Größenänderung (Resize-Auftrag 2026-11-04) ---
//
// Owner-Befund: Nach einer Größenänderung steht der Gesprächsinhalt doppelt im Verlauf, erst
// schmal, dann breit, und an der Naht folgt mitten im Satz ein Rest einer älteren Ausgabe.
// Gemessen an Claude Code 2.1.316 (pty-Mitschnitt, dazu gelesen im Paket): Der klassische
// Renderer sendet KEIN ESC[3J und KEIN ESC[2J. Er löscht nur den Sichtbereich zeilenweise an
// Ort und Stelle und malt die letzten Zeilen seines Frames in der neuen Breite. Was im
// Scrollback steht, hält er für unveränderlich. Unter ConPTY kommt davon nur die Neuzeichnung
// ab ESC[H an.
//
// 🔑 Diese Fälle halten die Terminal-Semantik fest, sie loben das Bild nicht: QTmux erfindet
// und verliert nichts, die Dopplung steht in den Bytes der Anwendung. Wer hier eine Abhilfe
// einbaut (etwa einen Textvergleich gegen das Scrollback-Ende), MUSS diese Erwartung bewusst
// umschreiben. Warum das bisher unterblieb, steht in der Feature-Referenz.
void TestConPtyReplay::classicRendererRepaintKeepsScrollback_data() {
    QTest::addColumn<bool>("reflow");
    QTest::addColumn<bool>("conptyForm");
    QTest::newRow("forkpty") << false << false;
    QTest::newRow("conpty")  << true  << true;
}

void TestConPtyReplay::classicRendererRepaintKeepsScrollback() {
    QFETCH(bool, reflow);
    QFETCH(bool, conptyForm);
    VtScreen s(10, 40);
    s.setReflow(reflow);
    for (int i = 1; i <= 30; ++i)
        s.inputWrite(QByteArray("Absatz ") + QByteArray::number(i) + " schmal\r\n");
    const int before = s.scrollbackCount();
    QVERIFY(before > 0);
    QVERIFY(s.scrollbackText().contains(QStringLiteral("Absatz 15 schmal")));

    s.setSize(20, 90);   // breiter und höher: kein Reflow-Schub, kein Scroll
    QCOMPARE(s.scrollbackCount(), before);

    QByteArray repaint;
    if (conptyForm) {
        repaint = "\x1b[?25l\x1b[H";   // ConPTY: nur die Neuzeichnung, Zeile für Zeile mit ESC[K
    } else {
        repaint = "\x1b[?25l\x1b[2D\x1b[6B\x1b[H";   // gemessene Folge von Claude Code
        for (int r = 0; r < 20; ++r) repaint += "\x1b[2K\x1b[1B";
        repaint += "\x1b[H";
    }
    for (int i = 15; i <= 30; ++i)
        repaint += QByteArray("Absatz ") + QByteArray::number(i) + " breit\x1b[K\r\n";
    repaint += "\x1b[?25h";
    s.inputWrite(repaint);

    QCOMPARE(s.scrollbackCount(), before);   // das Löschen an Ort und Stelle schiebt nichts
    const QStringList screen = s.screenText().split(QLatin1Char('\n'));
    QCOMPARE(screen.first(), QStringLiteral("Absatz 15 breit"));
    // Die Naht: Das Scrollback-Ende ist eine schmale Zeile, die Zeile danach eine breite ältere.
    const QStringList sb = s.scrollbackText().split(QLatin1Char('\n'));
    QVERIFY(sb.contains(QStringLiteral("Absatz 15 schmal")));
    QVERIFY(sb.last().endsWith(QStringLiteral(" schmal")));
}

// Der Vollbild-Renderer zeichnet im Alt-Screen: Das Scrollback bleibt leer, auch mit ESC[3J
// und mit Reflow. Deshalb gibt es dort keine Dopplung.
void TestConPtyReplay::fullscreenRendererLeavesNoScrollback() {
    VtScreen s(10, 40);
    s.setReflow(true);
    s.inputWrite("\x1b[?1049h\x1b[2J\x1b[H");
    QVERIFY(s.altScreen());
    for (int i = 1; i <= 9; ++i)
        s.inputWrite(QByteArray("Zeile ") + QByteArray::number(i) + "\r\n");
    s.setSize(20, 90);
    s.inputWrite("\x1b[2J\x1b[3J\x1b[HZeile neu\r\n");
    QVERIFY(s.altScreen());
    QCOMPARE(s.scrollbackCount(), 0);
    QCOMPARE(s.screenText(), QStringLiteral("Zeile neu"));
}

// ConPTY reicht ESC[3J durch (gemessen, auch ohne Größenänderung), QTmux wertet es nicht aus:
// `sb_clear` ist in VtScreen nicht gesetzt. Andere Terminals löschen dabei den Scrollback.
// Das ist NICHT die Ursache des Befunds oben; eine Änderung gälte auf allen Plattformen und
// bleibt eine eigene Entscheidung. Der Fall macht sie sichtbar, statt sie nebenbei zu treffen.
void TestConPtyReplay::eraseScrollbackSequenceIsIgnored() {
    VtScreen s(5, 40);
    for (int i = 1; i <= 20; ++i)
        s.inputWrite(QByteArray("Zeile ") + QByteArray::number(i) + "\r\n");
    const int before = s.scrollbackCount();
    QVERIFY(before > 0);
    s.inputWrite("\x1b[3J");
    QCOMPARE(s.scrollbackCount(), before);
    s.inputWrite("\x1b[2J\x1b[3J\x1b[H");
    QCOMPARE(s.scrollbackCount(), before);
}

QTEST_GUILESS_MAIN(TestConPtyReplay)
#include "tst_conptyreplay.moc"
