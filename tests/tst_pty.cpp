#include <QtTest>
#include <QSignalSpy>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include "PtyBackend.h"
#include "TestPrograms.h"

using namespace qtmux;

class TestPty : public QObject {
    Q_OBJECT
private slots:
    void echoRoundtrip();
    void shellExitClosesBackend();
    void largeWriteIsNotTruncated();
};

// Grosse Eingaben duerfen nicht abgeschnitten werden (QTMUX-28). Der PTY-Master ist
// O_NONBLOCK und nimmt pro ::write() nur den freien Kernel-Puffer (~1 KB) auf; ohne
// Ausgangspuffer ging alles darueber hinaus STILL verloren (gemessen: 200 000 Bytes
// gesendet -> 1023 angekommen). Der Test schiebt ~195 KB durch eine echte Shell in
// eine Datei und vergleicht die Groesse.
void TestPty::largeWriteIsNotTruncated() {
    const QString outPath = QDir::temp().filePath(QStringLiteral("qtmux_bigwrite_test.txt"));
    QFile::remove(outPath);

    PtyBackend backend;
    const auto sh = qtmux_test::interactiveShell();
    backend.setProgram(sh.program);
    backend.setArguments(sh.args);
    QVERIFY(backend.start(80, 24));
    QTest::qWait(400);

    // Echo aus (sonst spiegelt die tty die 195 KB zurueck), dann in die Datei umleiten.
    backend.write("stty -echo; cat > " + outPath.toUtf8() + qtmux_test::enterKey());
    QTest::qWait(600);

    // Zeilen a 39 Byte -> Zeilendisziplin-Limits (MAX_CANON) spielen keine Rolle.
    QByteArray payload;
    for (int i = 0; i < 5000; ++i) {
        payload += QByteArray::number(i).rightJustified(5, '0');
        payload += QByteArray(33, 'x');
        payload += '\r';                      // tty uebersetzt CR->LF (1:1, Laenge bleibt)
    }
    backend.write(payload);
    backend.write(QByteArray(1, '\x04'));     // Strg+D nach den Daten -> cat schliesst

    // Die Zustellung laeuft gepuffert ueber den Write-Notifier -> Ereignisschleife.
    QTRY_COMPARE_WITH_TIMEOUT(QFileInfo(outPath).size(), qint64(payload.size()), 30000);

    backend.terminate();
    QFile::remove(outPath);
}

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
