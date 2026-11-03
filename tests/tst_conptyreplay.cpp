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

QTEST_GUILESS_MAIN(TestConPtyReplay)
#include "tst_conptyreplay.moc"
