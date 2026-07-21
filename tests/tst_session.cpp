#include <QtTest>
#include <QSignalSpy>
#include "PtyBackend.h"
#include "VtScreen.h"
#include "Session.h"
#include "AgentEventHub.h"
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
    void agentEventReachesHub();
    void loginScriptRunsOnConnect();
    void sshPasswordAutoFillOnPrompt();
    void enterIsSentSeparatelyAfterText();
    void writeWithEnterKeepsOrderOnRapidCalls();
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

// Inter-Agenten-Benachrichtigung: ein OSC 777;qtmux-event aus dem PTY erreicht über
// VtScreen -> Session::onAgentEvent den AgentEventHub mit der eigenen Session-ID als Quelle.
void TestSession::agentEventReachesHub() {
    Session sess;
    auto *pty = new PtyBackend;
    // OSC 777 ; qtmux-event ; done ; Build fertig BEL
    const auto cmd = qtmux_test::emitRaw(QByteArrayLiteral("\033]777;qtmux-event;done;Build fertig\007"));
    pty->setProgram(cmd.program);
    pty->setArguments(cmd.args);
    sess.attachBackend(pty, Session::Type::Shell, 80, 24);
    sess.start(80, 24);

    auto *hub = AgentEventHub::instance();
    QTRY_VERIFY_WITH_TIMEOUT(hub->latestFrom(sess.id()).seq > 0, 5000);
    const auto ev = hub->latestFrom(sess.id());
    QCOMPARE(ev.kind, AgentEventHub::Kind::Done);
    QCOMPARE(ev.text, QStringLiteral("Build fertig"));
    QCOMPARE(ev.sourceSessionId, sess.id());
}

// Login-Script (QTMUX-23): ein per setLoginScript gesetzter Befehl wird nach dem
// Verbindungsaufbau AUTOMATISCH gesendet (kein write() durch den Test) und erscheint
// dadurch am Schirm. Beweist die Auto-Send-Kette über den Fallback-Timer.
void TestSession::loginScriptRunsOnConnect() {
    Session sess;
    auto *pty = new PtyBackend;
    const auto sh = qtmux_test::interactiveShell();
    pty->setProgram(sh.program);
    pty->setArguments(sh.args);
    sess.setLoginScript(QStringLiteral("echo QTMUX_LOGIN_MARKER"));
    sess.attachBackend(pty, Session::Type::Shell, 80, 24);
    sess.setActive(true);
    sess.start(80, 24);

    // Niemand tippt — taucht der Marker auf, hat das Login-Script ihn gesendet.
    bool found = false;
    for (int attempt = 0; attempt < 100 && !found; ++attempt) {
        QTest::qWait(100);
        if (sess.screenText().contains("QTMUX_LOGIN_MARKER")) found = true;
    }
    QVERIFY(found);
    sess.write("\x03");   // ^C, aufräumen
    sess.shutdown();
}

// SSH-Passwort-Auto-Fill (QTMUX-22-Integration): ein per setSshPassword gesetztes
// Geheimnis wird automatisch an die erste "Password:"-Abfrage gesendet. Der Prozess
// liest es und echot "PWGOT:<wert>" — niemand tippt, also beweist das Auftauchen die
// Auto-Send-Kette (Prompt-Erkennung -> Schreiben ans PTY).
void TestSession::sshPasswordAutoFillOnPrompt() {
    Session sess;
    auto *pty = new PtyBackend;
    const auto cmd = qtmux_test::passwordPrompt();
    pty->setProgram(cmd.program);
    pty->setArguments(cmd.args);
    sess.setSshPassword(QStringLiteral("hunter2"));
    sess.attachBackend(pty, Session::Type::Ssh, 80, 24);
    sess.setActive(true);
    sess.start(80, 24);

    bool found = false;
    for (int attempt = 0; attempt < 100 && !found; ++attempt) {
        QTest::qWait(100);
        if (sess.screenText().contains("PWGOT:hunter2")) found = true;
    }
    QVERIFY(found);
    sess.shutdown();
}


// QTMUX-31: Das abschließende Enter darf NICHT im selben Schreibvorgang stehen wie der
// Text — TUI-Anwendungen werten einen in einem Rutsch ankommenden Block als
// Einfügevorgang und machen aus dem \r einen Zeilenumbruch im Eingabefeld statt eines
// Absendens. Der Test belegt die Absetzung am Verhalten einer echten Shell: unmittelbar
// nach dem Aufruf ist der Befehl noch NICHT ausgeführt, nach der Verzögerung schon.
void TestSession::enterIsSentSeparatelyAfterText() {
    Session sess;
    auto *pty = new PtyBackend;
    const auto sh = qtmux_test::interactiveShell();
    pty->setProgram(sh.program);
    pty->setArguments(sh.args);
    sess.attachBackend(pty, Session::Type::Shell, 80, 24);
    sess.setActive(true);
    sess.start(80, 24);
    QTest::qWait(500);   // Prompt abwarten

    // Die Ausgabe (Zeile ist exakt der Marker) unterscheidet sich vom getippten
    // Befehl ("echo <marker>") — daran hängt die Unterscheidung "getippt" vs "gelaufen".
    auto hasOutputLine = [&sess]() {
        const QStringList rows = sess.screenText().split(QLatin1Char('\n'));
        for (const QString &r : rows)
            if (r.trimmed() == QLatin1String("ENTER_SEP_MARKER")) return true;
        return false;
    };

    sess.writeWithEnter("echo ENTER_SEP_MARKER", 400);
    QTest::qWait(150);
    QVERIFY2(!hasOutputLine(), "Enter kam zu früh — es wurde offenbar im selben "
                               "Schreibvorgang wie der Text gesendet");
    QTRY_VERIFY_WITH_TIMEOUT(hasOutputLine(), 5000);

    sess.write("\x03");
    sess.shutdown();
}

// Zwei schnell aufeinanderfolgende Aufrufe an DIESELBE Session dürfen sich nicht
// verschränken (Text2 vor Enter1) — das ausstehende Enter wird vorher nachgeholt.
void TestSession::writeWithEnterKeepsOrderOnRapidCalls() {
    Session sess;
    auto *pty = new PtyBackend;
    const auto sh = qtmux_test::interactiveShell();
    pty->setProgram(sh.program);
    pty->setArguments(sh.args);
    sess.attachBackend(pty, Session::Type::Shell, 80, 24);
    sess.setActive(true);
    sess.start(80, 24);
    QTest::qWait(500);

    // Zweiter Aufruf, bevor das erste Enter (5 s) fällig wäre.
    sess.writeWithEnter("echo RAPID_ONE", 5000);
    sess.writeWithEnter("echo RAPID_TWO", 50);

    // Beide Befehle müssen laufen, und zwar in dieser Reihenfolge.
    QTRY_VERIFY_WITH_TIMEOUT(sess.screenText().contains("RAPID_TWO"), 8000);
    const QString out = sess.screenText();
    QVERIFY2(out.contains("RAPID_ONE"), "erster Befehl wurde nie abgeschickt");
    QVERIFY2(out.indexOf("RAPID_ONE") < out.indexOf("RAPID_TWO"),
             "Reihenfolge vertauscht");

    sess.write("\x03");
    sess.shutdown();
}

QTEST_MAIN(TestSession)
#include "tst_session.moc"
