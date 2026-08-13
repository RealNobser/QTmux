#include <QtTest>

#include "ITerminalBackend.h"
#include "Session.h"

using namespace qtmux;

// Fängt alles ab, was die Session Richtung PTY schreibt — inklusive der
// Bracketed-Paste-Marker, die libvterm über outputToPty → backend->write sendet.
class CaptureBackend final : public ITerminalBackend {
public:
    bool start(int, int) override { setState(BackendState::Running); return true; }
    void write(const QByteArray &d) override { captured += d; }
    void resize(int, int) override {}
    void terminate() override { setState(BackendState::Closed); }

    // Testseite: die Zielanwendung "spricht" zur Session (z. B. DECSET 2004).
    void feed(const QByteArray &d) { emit dataReceived(d); }

    QByteArray captured;
};

/// Tests für die Paste-Rahmung von Text-Nutzlasten (Bugreport: lange
/// send_text-Nutzlasten wurden von der Ziel-TUI gestückelt gelesen und
/// teil-abgeschickt). Kern: writePasted rahmt in ESC[200~ … ESC[201~, aber NUR
/// wenn die Anwendung DECSET 2004 aktiviert hat — und entfernt ein in der
/// Nutzlast enthaltenes ESC[201~ (Rahmen-Ausbruch).
class TestPasteWrite : public QObject {
    Q_OBJECT

    static constexpr const char *kStart = "\x1b[200~";
    static constexpr const char *kEnd = "\x1b[201~";

    struct Rig {
        Session sess;
        CaptureBackend *backend;   // Session besitzt ihn
        Rig() {
            backend = new CaptureBackend;
            sess.attachBackend(backend, Session::Type::Shell, 80, 24);
            sess.start(80, 24);
        }
        void enableBracketedPaste() { backend->feed("\x1b[?2004h"); }
    };

private slots:
    void framedWhenAppEnabledMode() {
        Rig rig;
        rig.enableBracketedPaste();
        rig.backend->captured.clear();
        rig.sess.writePasted("hallo welt");
        QCOMPARE(rig.backend->captured,
                 QByteArray(kStart) + "hallo welt" + QByteArray(kEnd));
    }

    void notFramedWithoutMode() {
        Rig rig;   // Anwendung hat 2004 NIE aktiviert
        rig.backend->captured.clear();
        rig.sess.writePasted("hallo welt");
        QCOMPARE(rig.backend->captured, QByteArray("hallo welt"));
    }

    void multilineStaysOnePaste() {
        // Der Bugreport-Fall: mehrzeilige Nutzlast muss als EIN Rahmen ankommen —
        // die \n liegen INNERHALB der Marker und sind für die TUI Paste-Material,
        // keine Tastendrücke.
        Rig rig;
        rig.enableBracketedPaste();
        rig.backend->captured.clear();
        const QByteArray payload("Absatz eins\nAbsatz zwei\nAbsatz drei");
        rig.sess.writePasted(payload);
        const QByteArray &out = rig.backend->captured;
        QCOMPARE(out.count(kStart), 1);
        QCOMPARE(out.count(kEnd), 1);
        QVERIFY(out.startsWith(kStart));
        QVERIFY(out.endsWith(kEnd));
        QCOMPARE(out.mid(qsizetype(qstrlen(kStart)),
                         out.size() - qstrlen(kStart) - qstrlen(kEnd)),
                 payload);
    }

    void breakoutSequenceIsStripped() {
        // Sicherheitshälfte des Fixes: Ein ESC[201~ IN der Nutzlast schlösse den
        // Rahmen von innen — alles danach liefe wieder als Tastendrücke. Es muss
        // restlos entfernt werden; übrig bleibt genau EIN 201~: der Schlussmarker.
        Rig rig;
        rig.enableBracketedPaste();
        rig.backend->captured.clear();
        rig.sess.writePasted(QByteArray("harmlos\x1b[201~danach Tastendruecke?"));
        const QByteArray &out = rig.backend->captured;
        QVERIFY2(out.count(kEnd) == 1 && out.endsWith(kEnd),
                 "RAHMEN-AUSBRUCH: Ein in der Nutzlast eingebettetes ESC[201~ hat "
                 "den Paste-Rahmen vorzeitig geschlossen — der Rest der Nutzlast "
                 "wird von der Zielanwendung wieder als Tastendruecke gelesen.");
        // Beide Nutzlast-Hälften sind da, nur der Ausbruchs-Marker fehlt.
        QCOMPARE(out, QByteArray(kStart) + "harmlosdanach Tastendruecke?"
                          + QByteArray(kEnd));
    }

    void breakoutStrippedEvenWithoutMode() {
        // Auch ohne aktiven Modus wird entfernt: getippt ergäbe die Sequenz nur
        // eine bedeutungslose CSI — konsistentes Verhalten ist hier mehr wert.
        Rig rig;
        rig.backend->captured.clear();
        rig.sess.writePasted(QByteArray("a\x1b[201~b"));
        QCOMPARE(rig.backend->captured, QByteArray("ab"));
    }

    void pastedEnterIsDelayedAndOutsideFrame() {
        // QTMUX-31 bleibt erhalten: Das Enter kommt NACH dem Schlussmarker als
        // eigener, zeitlich abgesetzter Tastendruck — nie als Paste-Inhalt.
        Rig rig;
        rig.enableBracketedPaste();
        rig.backend->captured.clear();
        rig.sess.writePastedWithEnter("befehl", 80);
        QVERIFY2(!rig.backend->captured.contains('\r'),
                 "Enter kam im selben Schreibvorgang wie die Nutzlast");
        QTRY_VERIFY_WITH_TIMEOUT(rig.backend->captured.endsWith('\r'), 2000);
        QCOMPARE(rig.backend->captured,
                 QByteArray(kStart) + "befehl" + QByteArray(kEnd) + "\r");
    }

    void enterDelayScalesWithSize() {
        // Netz für Ziele ohne Bracketed Paste: 60 ms Basis + 1 ms je 8 Byte,
        // Deckel 2000 ms. Nur der DEFAULT skaliert — explizite Werte gewinnen
        // (das setzt die Aufruferseite um, s. McpServer/SessionModel).
        QCOMPARE(Session::pasteEnterDelayMs(0), 60);
        QCOMPARE(Session::pasteEnterDelayMs(1500), 60 + 1500 / 8);
        QCOMPARE(Session::pasteEnterDelayMs(10000), 60 + 10000 / 8);
        QCOMPARE(Session::pasteEnterDelayMs(1000000), 2000);   // Deckel
    }
};

QTEST_MAIN(TestPasteWrite)
#include "tst_pastewrite.moc"
