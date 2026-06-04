#include <QtTest>
#include <QSignalSpy>
#include "PtyBackend.h"

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
    QByteArray received;
    connect(&backend, &ITerminalBackend::dataReceived,
            [&](const QByteArray &d) { received += d; });

    QVERIFY(backend.start(80, 24));
    QCOMPARE(backend.state(), BackendState::Running);

    backend.write("echo QTMUX_OK_42\n");

    QTRY_VERIFY_WITH_TIMEOUT(received.contains("QTMUX_OK_42"), 5000);
    backend.terminate();
}

// Beendet sich die Shell, muss das Backend in den Closed-Zustand wechseln.
void TestPty::shellExitClosesBackend() {
    PtyBackend backend;
    QSignalSpy spy(&backend, &ITerminalBackend::stateChanged);

    QVERIFY(backend.start(80, 24));
    backend.write("exit\n");

    QTRY_COMPARE_WITH_TIMEOUT(backend.state(), BackendState::Closed, 5000);
}

QTEST_MAIN(TestPty)
#include "tst_pty.moc"
