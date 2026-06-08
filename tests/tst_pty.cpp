#include <QtTest>
#include <QSignalSpy>
#include "PtyBackend.h"
#include "TestPrograms.h"

using namespace qtmux;

class TestPty : public QObject {
    Q_OBJECT
private slots:
    void echoRoundtrip();
    void shellExitClosesBackend();
};

// Schreibt ein Kommando in eine echte Shell und erwartet dessen Ausgabe zurück.
void TestPty::echoRoundtrip() {
    PtyBackend backend;
    const auto sh = qtmux_test::interactiveShell();
    backend.setProgram(sh.program);
    backend.setArguments(sh.args);
    QByteArray received;
    connect(&backend, &ITerminalBackend::dataReceived,
            [&](const QByteArray &d) { received += d; });

    QVERIFY(backend.start(80, 24));
    QCOMPARE(backend.state(), BackendState::Running);

    // Auf den Prompt warten, dann tippen (Enter = CR, wie im echten Terminal).
    QTest::qWait(300);
    backend.write("echo QTMUX_OK_42" + qtmux_test::enterKey());

    QTRY_VERIFY_WITH_TIMEOUT(received.contains("QTMUX_OK_42"), 10000);
    backend.terminate();
}

// Beendet sich die Shell, muss das Backend in den Closed-Zustand wechseln.
void TestPty::shellExitClosesBackend() {
    PtyBackend backend;
    const auto sh = qtmux_test::interactiveShell();
    backend.setProgram(sh.program);
    backend.setArguments(sh.args);
    QSignalSpy spy(&backend, &ITerminalBackend::stateChanged);

    QVERIFY(backend.start(80, 24));
    QTest::qWait(300);
    backend.write("exit" + qtmux_test::enterKey());

    QTRY_COMPARE_WITH_TIMEOUT(backend.state(), BackendState::Closed, 10000);
}

QTEST_MAIN(TestPty)
#include "tst_pty.moc"
