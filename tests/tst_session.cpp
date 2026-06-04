#include <QtTest>
#include <QSignalSpy>
#include "PtyBackend.h"
#include "VtScreen.h"
#include "Session.h"

using namespace qtmux;

class TestSession : public QObject {
    Q_OBJECT
private slots:
    void echoReachesScreen();
    void shellInputEcho();
    void bellRaisesAttentionWhenInactive();
    void activeSessionIgnoresBell();
};

static QString rowText(const VtScreen &vt, int row) {
    QString s;
    for (int c = 0; c < vt.cols(); ++c) s += vt.cell(row, c).text;
    return s.trimmed();
}

// Vollständige Kette: PTY-Prozess -> Bytes -> libvterm -> sichtbare Zellen.
void TestSession::echoReachesScreen() {
    PtyBackend backend;
    backend.setProgram(QStringLiteral("/bin/echo"));
    backend.setArguments({QStringLiteral("HELLO_QTMUX")});

    VtScreen screen(24, 80);
    QObject::connect(&backend, &ITerminalBackend::dataReceived,
                     &screen, &VtScreen::inputWrite);

    QVERIFY(backend.start(80, 24));
    QTRY_VERIFY_WITH_TIMEOUT(rowText(screen, 0).contains("HELLO_QTMUX"), 5000);
}

// Eingabe-Echo: in eine interaktive Shell getippter Text erscheint am Schirm.
void TestSession::shellInputEcho() {
    PtyBackend backend;
    VtScreen screen(24, 80);
    QObject::connect(&backend, &ITerminalBackend::dataReceived,
                     &screen, &VtScreen::inputWrite);
    QObject::connect(&screen, &VtScreen::outputToPty,
                     &backend, &ITerminalBackend::write);

    QVERIFY(backend.start(80, 24));
    // Auf Prompt warten, dann tippen.
    QTest::qWait(300);
    backend.write("echo MARKER_123\n");

    bool found = false;
    for (int attempt = 0; attempt < 50 && !found; ++attempt) {
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
    pty->setProgram(QStringLiteral("/usr/bin/printf"));
    pty->setArguments({QStringLiteral("\\a")});   // printf interpretiert \a als BEL
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
    pty->setProgram(QStringLiteral("/usr/bin/printf"));
    pty->setArguments({QStringLiteral("\\a")});
    sess.attachBackend(pty, Session::Type::Shell, 80, 24);
    sess.setActive(true);

    sess.start(80, 24);
    QTest::qWait(500);
    QVERIFY(!sess.needsAttention());
}

QTEST_MAIN(TestSession)
#include "tst_session.moc"
