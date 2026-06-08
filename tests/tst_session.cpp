#include <QtTest>
#include <QSignalSpy>
#include "PtyBackend.h"
#include "VtScreen.h"
#include "Session.h"
#include "TestPrograms.h"

using namespace qtmux;

class TestSession : public QObject {
    Q_OBJECT
private slots:
    void echoReachesScreen();
    void shellInputEcho();
    void bellRaisesAttentionWhenInactive();
    void activeSessionIgnoresBell();
    void oscNotificationReachesSession();
    void osc133NonZeroExitSetsError();
};

static QString rowText(const VtScreen &vt, int row) {
    QString s;
    for (int c = 0; c < vt.cols(); ++c) s += vt.cell(row, c).text;
    return s.trimmed();
}

// Vollständige Kette: PTY-Prozess -> Bytes -> libvterm -> sichtbare Zellen.
void TestSession::echoReachesScreen() {
    PtyBackend backend;
    const auto cmd = qtmux_test::printLine(QStringLiteral("HELLO_QTMUX"));
    backend.setProgram(cmd.program);
    backend.setArguments(cmd.args);

    VtScreen screen(24, 80);
    QObject::connect(&backend, &ITerminalBackend::dataReceived,
                     &screen, &VtScreen::inputWrite);

    QVERIFY(backend.start(80, 24));
    QTRY_VERIFY_WITH_TIMEOUT(rowText(screen, 0).contains("HELLO_QTMUX"), 5000);
}

// Eingabe-Echo: in eine interaktive Shell getippter Text erscheint am Schirm.
void TestSession::shellInputEcho() {
    PtyBackend backend;
    const auto sh = qtmux_test::interactiveShell();
    backend.setProgram(sh.program);
    backend.setArguments(sh.args);
    VtScreen screen(24, 80);
    QObject::connect(&backend, &ITerminalBackend::dataReceived,
                     &screen, &VtScreen::inputWrite);
    QObject::connect(&screen, &VtScreen::outputToPty,
                     &backend, &ITerminalBackend::write);

    QVERIFY(backend.start(80, 24));
    // Auf Prompt warten, dann tippen (Enter = CR, wie im echten Terminal).
    QTest::qWait(500);
    backend.write("echo MARKER_123" + qtmux_test::enterKey());

    bool found = false;
    for (int attempt = 0; attempt < 100 && !found; ++attempt) {
        QTest::qWait(100);
        for (int r = 0; r < screen.rows(); ++r) {
            if (rowText(screen, r).contains("MARKER_123")) { found = true; break; }
        }
    }
    QVERIFY(found);
    backend.terminate();
}

// Eine nicht-fokussierte Session, die ein BEL empfängt, meldet "braucht Aufmerksamkeit".
void TestSession::bellRaisesAttentionWhenInactive() {
    Session sess;
    auto *pty = new PtyBackend;
    const auto cmd = qtmux_test::emitRaw(QByteArrayLiteral("\a"));   // BEL
    pty->setProgram(cmd.program);
    pty->setArguments(cmd.args);
    sess.attachBackend(pty, Session::Type::Shell, 80, 24);
    sess.setActive(false);

    QSignalSpy spy(&sess, &Session::attentionChanged);
    sess.start(80, 24);

    QTRY_VERIFY_WITH_TIMEOUT(sess.needsAttention(), 5000);
    QVERIFY(spy.count() >= 1);
}

// Eine aktive (fokussierte) Session löst bei BEL keine Aufmerksamkeit aus.
void TestSession::activeSessionIgnoresBell() {
    Session sess;
    auto *pty = new PtyBackend;
    const auto cmd = qtmux_test::emitRaw(QByteArrayLiteral("\a"));
    pty->setProgram(cmd.program);
    pty->setArguments(cmd.args);
    sess.attachBackend(pty, Session::Type::Shell, 80, 24);
    sess.setActive(true);

    sess.start(80, 24);
    QTest::qWait(500);
    QVERIFY(!sess.needsAttention());
}

// OSC 9 durch die ganze Kette: PTY -> VtScreen -> Session.lastNotification + Attention.
void TestSession::oscNotificationReachesSession() {
    Session sess;
    auto *pty = new PtyBackend;
    // OSC 9 ; BuildOK BEL  ->  ESC ] 9 ; BuildOK BEL
    const auto cmd = qtmux_test::emitRaw(QByteArrayLiteral("\033]9;BuildOK\007"));
    pty->setProgram(cmd.program);
    pty->setArguments(cmd.args);
    sess.attachBackend(pty, Session::Type::Shell, 80, 24);
    sess.setActive(false);
    sess.start(80, 24);

    QTRY_VERIFY_WITH_TIMEOUT(sess.lastNotification() == QStringLiteral("BuildOK"), 5000);
    QVERIFY(sess.needsAttention());
}

// OSC 133;D mit Exit != 0 setzt die Aktivität auf Error.
// Prozess bleibt am Leben (sleep), damit der Error-Zustand nicht von Closed überschrieben wird.
void TestSession::osc133NonZeroExitSetsError() {
    Session sess;
    auto *pty = new PtyBackend;
    // OSC 133 ; D ; 2 BEL (Befehlsende mit Exit-Code 2), danach am Leben bleiben.
    const auto cmd = qtmux_test::emitRawThenWait(QByteArrayLiteral("\033]133;D;2\007"), 5);
    pty->setProgram(cmd.program);
    pty->setArguments(cmd.args);
    sess.attachBackend(pty, Session::Type::Shell, 80, 24);
    sess.setActive(true);
    sess.start(80, 24);

    QTRY_COMPARE_WITH_TIMEOUT(sess.activityInt(),
                              static_cast<int>(Session::Activity::Error), 5000);
    sess.write("\x03");  // ^C, aufräumen
}

QTEST_MAIN(TestSession)
#include "tst_session.moc"
