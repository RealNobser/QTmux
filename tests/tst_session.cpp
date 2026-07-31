#include <QtTest>
#include <QSignalSpy>
#include "PtyBackend.h"
#include "VtScreen.h"
#include "Session.h"
#include "AgentEventHub.h"
#include "SleepInhibitor.h"
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
    void agentEventPulseAndExplicitAttention();
    void agentActivityStates();
    void sleepInhibitRule();
    void activityIsOnlyTrustedOnceReported();
    void loginScriptRunsOnConnect();
    void sshPasswordAutoFillOnPrompt();
    void enterIsSentSeparatelyAfterText();
    void writeWithEnterKeepsOrderOnRapidCalls();
    void agentCommandLineIsRemembered();
    void restoredAgentSetsIdentityAndRunsCommand();
    void gridSizeIsPublishedAndSignalled();
    void gitBranchFollowsWorkingDirectory();
    void gitBranchStaysEmptyForRemoteDirectory();
    void queuedTextWaitsWhileBackendNeverSpoke();
    void queuedTextIsDispatchedOnceQuiet();
    void osc7ReportedDirectoryWinsOverPolling();
    void osc7RemoteDirectoryIsNotUsedAsLocalCwd();
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
// MCP-Signalpfad (post_event/needs_attention/clear_attention laufen ueber diese Methoden):
// question/error wecken die inaktive Kachel, done/info nicht; flagAttention setzt explizit,
// clearAttention loescht, und eine fokussierte Session pulst nie.
void TestSession::agentEventPulseAndExplicitAttention() {
    { Session s; s.setActive(false);
      s.reportAgentEvent(QStringLiteral("done"), QStringLiteral("fertig"));
      QVERIFY(!s.needsAttention());                       // FYI -> kein Puls
      s.reportAgentEvent(QStringLiteral("info"), QStringLiteral("x"));
      QVERIFY(!s.needsAttention());
      s.reportAgentEvent(QStringLiteral("question"), QStringLiteral("Darf ich?"));
      QVERIFY(s.needsAttention()); }                       // Frage -> Puls
    { Session s; s.setActive(false);
      s.reportAgentEvent(QStringLiteral("error"), QStringLiteral("kaputt"));
      QVERIFY(s.needsAttention()); }                       // Fehler -> Puls
    { Session s; s.setActive(false);
      s.flagAttention(QStringLiteral("blockiert"));
      QVERIFY(s.needsAttention());
      QCOMPARE(s.lastNotification(), QStringLiteral("blockiert"));
      s.clearAttention();
      QVERIFY(!s.needsAttention()); }                      // explizit setzen + loeschen
    { Session s; s.setActive(true);
      s.reportAgentEvent(QStringLiteral("question"), QStringLiteral("x"));
      QVERIFY(!s.needsAttention()); }                      // fokussiert -> kein Puls
}

// Agent-gepushter Dauerzustand (MCP set_activity -> requestActivity) faerbt den Ring:
// idle=0, busy/running=1, waiting=2, error=3; Unbekanntes laesst den Zustand unveraendert.
void TestSession::agentActivityStates() {
    Session s;
    s.requestActivity(QStringLiteral("idle"));    QCOMPARE(s.activityInt(), 0);
    s.requestActivity(QStringLiteral("busy"));    QCOMPARE(s.activityInt(), 1);
    s.requestActivity(QStringLiteral("running")); QCOMPARE(s.activityInt(), 1);   // Alias
    s.requestActivity(QStringLiteral("waiting")); QCOMPARE(s.activityInt(), 2);
    s.requestActivity(QStringLiteral("error"));   QCOMPARE(s.activityInt(), 3);
    s.requestActivity(QStringLiteral("quatsch")); QCOMPARE(s.activityInt(), 3);   // unbekannt -> unveraendert
}

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

// QTMUX-85: Die getippte Agenten-Zeile muss MIT ihren Argumenten erhalten bleiben —
// sie ist die einzige Quelle, aus der sich der Agent später wiederherstellen lässt
// (er läuft nicht als `program`, sondern wird in die Shell getippt). write() ruft
// observeInput vor dem Backend, der Test braucht darum kein PTY — und startet so
// auch garantiert keinen echten Agenten.
void TestSession::agentCommandLineIsRemembered() {
    Session sess;
    QVERIFY(sess.agentCommand().isEmpty());

    sess.write("qwen --model x\r");
    QCOMPARE(sess.agentId(), QStringLiteral("qwen"));
    QCOMPARE(sess.agentCommand(), QStringLiteral("qwen --model x"));

    // Ein normaler Befehl danach überschreibt die gemerkte Agenten-Zeile NICHT.
    sess.write("ls -la\r");
    QCOMPARE(sess.agentCommand(), QStringLiteral("qwen --model x"));
}

// QTMUX-85: setRestoredAgent muss Kennung und Titel SELBST setzen — gestartet wird der
// Agent über das Login-Script, und das schreibt direkt ans Backend, läuft also an
// observeInput vorbei. Zweiter Teil: die vor dem Start gesetzte Startzeile wird
// tatsächlich abgesetzt. Niemand tippt — taucht der Marker auf, kam er von dort.
void TestSession::restoredAgentSetsIdentityAndRunsCommand() {
    Session sess;
    auto *pty = new PtyBackend;
    const auto sh = qtmux_test::interactiveShell();
    pty->setProgram(sh.program);
    pty->setArguments(sh.args);

    // Kennung/gemerkter Befehl einerseits, tatsächlich laufende Startzeile andererseits.
    sess.setRestoredAgent(QStringLiteral("qwen"), QStringLiteral("qwen --model x"));
    sess.setLoginScript(QStringLiteral("echo QTMUX_AGENT_RESTORE_MARKER"));
    QCOMPARE(sess.agentId(), QStringLiteral("qwen"));
    QCOMPARE(sess.agentCommand(), QStringLiteral("qwen --model x"));
    QCOMPARE(sess.title(), QStringLiteral("Qwen Coder"));

    sess.attachBackend(pty, Session::Type::Shell, 80, 24);
    sess.setActive(true);
    sess.start(80, 24);

    bool found = false;
    for (int attempt = 0; attempt < 100 && !found; ++attempt) {
        QTest::qWait(100);
        if (sess.screenText().contains("QTMUX_AGENT_RESTORE_MARKER")) found = true;
    }
    QVERIFY2(found, "Die Agenten-Startzeile wurde nie abgesetzt");
    sess.write("\x03");
    sess.shutdown();
}

// Ruhezustands-Sperre (QTMUX-89): Die Entscheidungsregel ist Gui-frei, damit genau
// diese Tabelle ohne echte Sessions und ohne Systemaufruf prüfbar ist.
void TestSession::sleepInhibitRule() {
    using A = Session::Activity;
    const QList<int> keine;
    const QList<int> nurIdle   { int(A::Idle) };
    const QList<int> einBusy   { int(A::Idle), int(A::Running) };
    const QList<int> nurWarten { int(A::Idle), int(A::Waiting) };
    const QList<int> fehler    { int(A::Error) };

    // Schalter AUS: nie sperren, egal was läuft. Das ist die Vorgabe.
    QVERIFY(!shouldPreventSleep(false, einBusy));
    QVERIFY(!shouldPreventSleep(false, keine));

    // Schalter AN: nur sperren, wenn wirklich jemand arbeitet.
    QVERIFY(!shouldPreventSleep(true, keine));
    QVERIFY(!shouldPreventSleep(true, nurIdle));
    QVERIFY(shouldPreventSleep(true, einBusy));

    // ⚠️ „Wartet auf Eingabe" ist KEIN Arbeiten — sonst bliebe der Rechner wegen einer
    // offenen Rückfrage die ganze Nacht wach. Dasselbe für einen Fehlerzustand.
    QVERIFY(!shouldPreventSleep(true, nurWarten));
    QVERIFY(!shouldPreventSleep(true, fehler));

    // Freigeben muss auf jeder Plattform gelingen, auch auf denen ohne Unterstützung —
    // sonst bliebe eine einmal gesetzte Sperre für immer stehen.
    SleepInhibitor inh;
    QVERIFY(!inh.isActive());
    QVERIFY(inh.setActive(false));
    if (SleepInhibitor::isSupported()) {
        QVERIFY(inh.setActive(true));
        QVERIFY(inh.isActive());
        QVERIFY(inh.setActive(true));    // idempotent: keine zweite Sperre
        QVERIFY(inh.setActive(false));
        QVERIFY(!inh.isActive());
    }
}

// Der Startwert von Session::activity ist `Running` (Sidebar-Ring sofort grün). Wer
// daraus Konsequenzen zieht, muss erst fragen, ob die Session ihren Zustand ueberhaupt
// meldet — sonst hielte eine Shell ohne Integration den Rechner dauerhaft wach.
void TestSession::activityIsOnlyTrustedOnceReported() {
    Session s;
    QCOMPARE(s.activityInt(), int(Session::Activity::Running));   // Anzeige-Vorgabe
    QVERIFY2(!s.activityReported(), "frische Session darf NICHT als gemeldet gelten");

    s.requestActivity(QStringLiteral("quatsch"));                 // unbekannt -> ignorieren
    QVERIFY(!s.activityReported());

    s.requestActivity(QStringLiteral("busy"));
    QVERIFY(s.activityReported());
    QCOMPARE(s.activityInt(), int(Session::Activity::Running));
}

// QTMUX-120: Die Statusleiste zeigt „80×24". Der Wert MUSS von der Session kommen und
// nicht vom TerminalItem — dorthin gelangt er erst nach der 60-ms-Entprellung aus
// QTMUX-86, und nur so bleiben die transienten Layout-Zwischengrößen (gemessen 80×2
// beim Window-Wechsel) aus der Anzeige heraus.
void TestSession::gridSizeIsPublishedAndSignalled() {
    Session s;
    // Startwerte sind eine Annahme, kein gemessener Wert — aber sie müssen plausibel
    // sein, damit die Leiste bis zum ersten Resize nichts Unsinniges zeigt.
    QCOMPARE(s.cols(), 80);
    QCOMPARE(s.rows(), 24);

    QSignalSpy spy(&s, &Session::sizeChanged);
    s.resize(120, 40);
    QCOMPARE(s.cols(), 120);
    QCOMPARE(s.rows(), 40);
    QCOMPARE(spy.count(), 1);

    // Gleiche Größe erneut: kein Signal. Sonst rechnet die Leiste bei jedem
    // Layout-Zucken neu, obwohl sich nichts geändert hat.
    s.resize(120, 40);
    QCOMPARE(spy.count(), 1);

    // Nur eine Kante ändert sich -> zählt als Änderung.
    s.resize(120, 41);
    QCOMPARE(spy.count(), 2);
    QCOMPARE(s.rows(), 41);
}

// OSC 7 (QTMUX-108): Die Shell meldet ihr Arbeitsverzeichnis selbst — und diese Meldung
// schlägt den gepollten Wert aus dem Backend. Geprüft wird die volle Kette
// PTY -> libvterm -> VtScreen -> Session, nicht nur die Regel für sich.
void TestSession::osc7ReportedDirectoryWinsOverPolling() {
    Session sess;
    auto *pty = new PtyBackend;
    // Bewusst ein Verzeichnis, in dem der Prozess NICHT steht (und das es nirgends
    // gibt): Nur so kann der Wert überhaupt aus der Meldung stammen. Genau das ist der
    // PowerShell-Fall — dort bleibt das Prozess-Arbeitsverzeichnis am Startort stehen.
    const auto cmd = qtmux_test::emitRawThenWait(
        QByteArrayLiteral("\033]7;file:///qtmux-osc7/gemeldet\007"), 5);
    pty->setProgram(cmd.program);
    pty->setArguments(cmd.args);
    sess.attachBackend(pty, Session::Type::Shell, 80, 24);
    sess.start(80, 24);

    QTRY_VERIFY_WITH_TIMEOUT(
        sess.workingDirectory() == QStringLiteral("/qtmux-osc7/gemeldet"), 5000);
    QVERIFY(!sess.workingDirectoryIsRemote());
    QVERIFY(sess.workingDirectoryHost().isEmpty());
    // Auch der Weg, an dem Persistenz und die Vererbung an neue Shells hängen.
    QCOMPARE(sess.currentWorkingDirectory(), QStringLiteral("/qtmux-osc7/gemeldet"));

    // Das Polling darf die Meldung NICHT wieder überschreiben — sonst flackerte die
    // Anzeige im 1500-ms-Takt des SessionModel zwischen beiden Werten hin und her.
    sess.refreshWorkingDirectory();
    QCOMPARE(sess.workingDirectory(), QStringLiteral("/qtmux-osc7/gemeldet"));

    // Kontrolle: Ohne Meldung liefert derselbe Weg etwas anderes. (Weich geprüft — ob
    // das Backend ein CWD ermitteln kann, hängt an der Plattform; auf Windows liest es
    // den PEB eines fremden Prozesses. Leer ist kein Befund, ein GLEICHER Wert schon.)
    Session plain;
    auto *pty2 = new PtyBackend;
    const auto idle = qtmux_test::emitRawThenWait(QByteArrayLiteral("x"), 5);
    pty2->setProgram(idle.program);
    pty2->setArguments(idle.args);
    plain.attachBackend(pty2, Session::Type::Shell, 80, 24);
    plain.start(80, 24);
    QTest::qWait(500);
    QVERIFY(plain.currentWorkingDirectory() != QStringLiteral("/qtmux-osc7/gemeldet"));
}

// Ein Verzeichnis auf einem FREMDEN Rechner (SSH, Container) wird angezeigt, darf aber
// nicht als lokales Arbeitsverzeichnis weiterlaufen: Dieser Wert wird persistiert und
// an neue Shells vererbt — der Pfad existiert hier womöglich gar nicht.
void TestSession::osc7RemoteDirectoryIsNotUsedAsLocalCwd() {
    Session sess;
    auto *pty = new PtyBackend;
    const auto cmd = qtmux_test::emitRawThenWait(
        QByteArrayLiteral("\033]7;file://qtmux-fremder-rechner/opt/build\007"), 5);
    pty->setProgram(cmd.program);
    pty->setArguments(cmd.args);
    sess.attachBackend(pty, Session::Type::Shell, 80, 24);
    sess.start(80, 24);

    QTRY_VERIFY_WITH_TIMEOUT(
        sess.workingDirectory() == QStringLiteral("/opt/build"), 5000);
    QVERIFY(sess.workingDirectoryIsRemote());
    QCOMPARE(sess.workingDirectoryHost(), QStringLiteral("qtmux-fremder-rechner"));
    QVERIFY2(sess.currentWorkingDirectory() != QStringLiteral("/opt/build"),
             "Pfad einer anderen Maschine darf nicht als lokales cwd durchgereicht werden");

    // Und das Polling darf die Anzeige nicht durch das lokale Verzeichnis des
    // ssh-Clients ersetzen — sonst stünde unter der Kachel der Ort des CLIENTS, während
    // der Anwender auf der Gegenstelle arbeitet. Genau daran hängt das Gate in
    // refreshWorkingDirectory(): Bei einer entfernten Meldung fällt currentWorkingDirectory()
    // ja bewusst auf den gepollten Wert zurück, der hier NICHT gespeichert werden darf.
    sess.refreshWorkingDirectory();
    QCOMPARE(sess.workingDirectory(), QStringLiteral("/opt/build"));
}

// QTMUX-58: Der Branch der Kachel haengt am Arbeitsverzeichnis der Session. Geprueft wird
// an einem ECHTEN Repository-Verzeichnis (kein Mock) — GitInfo liest ja Dateien.
void TestSession::gitBranchFollowsWorkingDirectory() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString repo = tmp.filePath(QStringLiteral("projekt"));
    QVERIFY(QDir().mkpath(repo + QStringLiteral("/.git")));
    QVERIFY(QDir().mkpath(repo + QStringLiteral("/unterordner")));
    auto writeHead = [&](const QByteArray &content) {
        QFile f(repo + QStringLiteral("/.git/HEAD"));
        QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
        f.write(content);
    };
    writeHead("ref: refs/heads/feature/xy\n");

    Session s;
    QVERIFY(s.gitBranch().isEmpty());               // ohne Verzeichnis: nichts

    QSignalSpy spy(&s, &Session::gitBranchChanged);
    // Verzeichnis ueber den echten Eingang setzen (OSC-7-Meldung, lokal).
    s.onWorkingDirectoryReported(repo + QStringLiteral("/unterordner"), QString(), true);
    s.refreshGitBranch();
    QCOMPARE(s.gitBranch(), QStringLiteral("feature/xy"));
    QVERIFY(!s.gitDetached());
    QCOMPARE(spy.count(), 1);

    // Zweiter Lauf ohne Aenderung: KEIN Signal — sonst repaintet die Sidebar im
    // 1500-ms-Poll-Takt dauernd ohne Grund.
    s.refreshGitBranch();
    QCOMPARE(spy.count(), 1);

    // 🔑 Der eigentliche Grund fuer den separaten Refresh: `git checkout` wechselt den
    // Branch, OHNE dass sich das Arbeitsverzeichnis aendert.
    writeHead("ref: refs/heads/main\n");
    s.refreshGitBranch();
    QCOMPARE(s.gitBranch(), QStringLiteral("main"));
    QCOMPARE(spy.count(), 2);

    // Detached HEAD -> kurzer SHA, Flag gesetzt.
    writeHead("1234567890abcdef1234567890abcdef12345678\n");
    s.refreshGitBranch();
    QVERIFY(s.gitDetached());
    QCOMPARE(s.gitBranch(), QStringLiteral("1234567"));
}

// 🔑 Ein per OSC 7 gemeldetes Verzeichnis auf einem FREMDEN Rechner darf keinen Branch
// liefern: Existiert derselbe Pfad hier zufaellig auch, waere es ein ANDERES Repository —
// die Kachel zeigte dann einen Branch, der mit der Sitzung nichts zu tun hat.
void TestSession::gitBranchStaysEmptyForRemoteDirectory() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString repo = tmp.filePath(QStringLiteral("fremd"));
    QVERIFY(QDir().mkpath(repo + QStringLiteral("/.git")));
    { QFile f(repo + QStringLiteral("/.git/HEAD"));
      QVERIFY(f.open(QIODevice::WriteOnly)); f.write("ref: refs/heads/main\n"); }

    // Gegenprobe zuerst: lokal gemeldet -> Branch kommt an.
    Session local;
    local.onWorkingDirectoryReported(repo, QString(), true);
    local.refreshGitBranch();
    QCOMPARE(local.gitBranch(), QStringLiteral("main"));

    // Derselbe Pfad, aber von einem fremden Host gemeldet -> leer.
    Session remote;
    remote.onWorkingDirectoryReported(repo, QStringLiteral("buildhost"), false);
    QVERIFY(remote.workingDirectoryIsRemote());
    remote.refreshGitBranch();
    QVERIFY2(remote.gitBranch().isEmpty(),
             "Branch eines fremden Rechners darf nicht auf der Kachel landen");
}

// QTMUX-90: Eine Session, die noch NIE etwas ausgegeben hat, ist nicht „ruhig", sondern
// noch nicht bereit — der Eintrag muss warten. Ohne diese Sperre schriebe die
// Warteschlange in eine Shell, deren Prompt erst noch kommt (dieselbe Falle wie beim
// Login-Script, QTMUX-98).
void TestSession::queuedTextWaitsWhileBackendNeverSpoke() {
    Session s;                                   // kein Backend -> nie Ausgabe
    QVERIFY(s.msSinceLastOutput() < 0);
    QCOMPARE(s.queuedCount(), 0);

    QSignalSpy spy(&s, &Session::queueChanged);
    QVERIFY(s.queueText(QStringLiteral("erster")));
    QVERIFY(s.queueText(QStringLiteral("zweiter")));
    QCOMPARE(s.queuedCount(), 2);                // beide bleiben stehen
    QVERIFY(spy.count() >= 2);

    // Leerer/whitespace-Text wird abgewiesen: als blankes Enter waere er in einem
    // Agenten-TUI eine HANDLUNG (bestaetigt die vorausgewaehlte Option).
    QVERIFY(!s.queueText(QStringLiteral("   ")));
    QCOMPARE(s.queuedCount(), 2);

    // Reihenfolge bleibt FIFO.
    QCOMPARE(s.promptQueue()->entries(),
             QStringList({QStringLiteral("erster"), QStringLiteral("zweiter")}));
}

// Der Gegenbeweis mit echtem PTY: Sobald die Shell gesprochen hat und wieder still ist,
// geht der eingereihte Text automatisch raus — ohne dass der Test ihn selbst schreibt.
void TestSession::queuedTextIsDispatchedOnceQuiet() {
    Session sess;
    auto *pty = new PtyBackend;
    const auto sh = qtmux_test::interactiveShell();
    pty->setProgram(sh.program);
    pty->setArguments(sh.args);
    sess.attachBackend(pty, Session::Type::Shell, 80, 24);
    sess.start(80, 24);

    QTest::qWait(800);                       // Prompt abwarten -> Uhr laeuft, dann Ruhe
    QVERIFY2(sess.msSinceLastOutput() >= 0, "Shell hat nichts ausgegeben");

    QVERIFY(sess.queueText(QStringLiteral("echo QUEUED_OK")));
    // Kein write() durch den Test — die Abgabe muss von allein kommen.
    QTRY_VERIFY_WITH_TIMEOUT(sess.screenText().contains(QStringLiteral("QUEUED_OK")), 8000);
    QTRY_COMPARE_WITH_TIMEOUT(sess.queuedCount(), 0, 8000);
    sess.write("\x03");
}

QTEST_MAIN(TestSession)
#include "tst_session.moc"
