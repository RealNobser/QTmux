#include <QtTest>
#include <QSignalSpy>
#include "VtScreen.h"

using namespace qtmux;

// Farbkomponenten aus 0xRRGGBB (ohne QtGui).
static inline int chRed(quint32 c) { return (c >> 16) & 0xff; }
static inline int chBlue(quint32 c) { return c & 0xff; }

class TestVtScreen : public QObject {
    Q_OBJECT
private slots:
    void plainText();
    void ansiColor();
    void cursorAndDamage();
    void oscNotification();
    void oscPromptMarkers();
    void oscProgress();
    void oscAgentEvent();
    void trueColorRgb();
    void faintAttribute();
    void lineWrapContinuation();
};

// Echtes 24-Bit-RGB (ESC[38;2;r;g;b) muss exakt durchgereicht werden.
void TestVtScreen::trueColorRgb() {
    VtScreen vt(24, 80);
    vt.inputWrite("\x1b[38;2;136;136;136mX");
    const Cell c = vt.cell(0, 0);
    QVERIFY(!c.fgDefault);
    QCOMPARE(c.fg, quint32(0x888888));   // unverfälscht, kein Quantisieren auf die Palette
}

// Faint/Dim (SGR 2) wird erkannt und über SGR 22 wieder aufgehoben (libvterm-Patch).
// Ohne den Patch ging das Attribut verloren -> gedimmter Text rendert weiß.
void TestVtScreen::faintAttribute() {
    {   // SGR 2 auf Default-Vordergrund: faint gesetzt, fg bleibt Default.
        VtScreen vt(24, 80);
        vt.inputWrite("\x1b[2mX");
        const Cell c = vt.cell(0, 0);
        QVERIFY(c.faint);
        QVERIFY(c.fgDefault);            // Abdunklung passiert erst beim Rendern (Theme-fg)
        QVERIFY(!c.bold);
    }
    {   // SGR 22 (normale Intensität) hebt Faint UND Bold auf.
        VtScreen vt(24, 80);
        vt.inputWrite("\x1b[1;2mX\x1b[22mY");
        QVERIFY(vt.cell(0, 0).faint);    // X: faint
        QVERIFY(!vt.cell(0, 1).faint);   // Y: nach 22 nicht mehr
        QVERIFY(!vt.cell(0, 1).bold);
    }
    {   // Normaler Text ist nicht faint.
        VtScreen vt(24, 80);
        vt.inputWrite("X");
        QVERIFY(!vt.cell(0, 0).faint);
    }
}

// Eine zu lange Eingabe bricht (Autowrap) auf die nächste Zeile um; diese Zeile ist
// dann eine Flow-Fortsetzung. Genutzt von Copy, um einen umbrochenen Befehl als EINE
// logische Zeile (ohne \n) zu kopieren statt fälschlich als mehrzeilig.
void TestVtScreen::lineWrapContinuation() {
    VtScreen vt(24, 10);                 // 10 Spalten breit
    vt.inputWrite("ABCDEFGHIJKLMNO");     // 15 Zeichen -> wickelt nach Spalte 10 um
    QVERIFY(!vt.lineContinuation(0));     // erste Zeile: keine Fortsetzung
    QVERIFY(vt.lineContinuation(1));      // zweite Zeile: weicher Umbruch der ersten
    // Ein echter Zeilenumbruch (CRLF) erzeugt KEINE Fortsetzung.
    VtScreen vt2(24, 10);
    vt2.inputWrite("AB\r\nCD");
    QVERIFY(!vt2.lineContinuation(1));
}

// Einfacher Text landet 1:1 in den Zellen.
void TestVtScreen::plainText() {
    VtScreen vt(24, 80);
    vt.inputWrite("Hi");
    QCOMPARE(vt.cell(0, 0).text, QStringLiteral("H"));
    QCOMPARE(vt.cell(0, 1).text, QStringLiteral("i"));
    QVERIFY(vt.cell(0, 0).fgDefault); // ohne SGR: Default-Farbe
}

// SGR-Sequenz "roter Vordergrund" (ESC[31m) muss eine konkrete Farbe setzen.
void TestVtScreen::ansiColor() {
    VtScreen vt(24, 80);
    vt.inputWrite("\x1b[31mR");
    const Cell c = vt.cell(0, 0);
    QCOMPARE(c.text, QStringLiteral("R"));
    QVERIFY(!c.fgDefault);          // explizite Farbe gesetzt
    QVERIFY(chRed(c.fg) > chBlue(c.fg)); // rotdominant
}

// Cursorbewegung und Damage-Signal.
void TestVtScreen::cursorAndDamage() {
    VtScreen vt(24, 80);
    QSignalSpy damageSpy(&vt, &VtScreen::damaged);
    vt.inputWrite("abc");
    QVERIFY(damageSpy.count() >= 1);
    QCOMPARE(vt.cursor(), QPoint(3, 0)); // Cursor hinter "abc"
}

// OSC 9 (Notification) wird geparst und als notify() ausgegeben.
void TestVtScreen::oscNotification() {
    VtScreen vt(24, 80);
    QString got;
    QObject::connect(&vt, &VtScreen::notify, [&](const QString &t) { got = t; });
    vt.inputWrite("\x1b]9;Build fertig\x07");
    QCOMPARE(got, QStringLiteral("Build fertig"));
}

// OSC 133 Prompt-Marker (C = Befehl läuft, D;exit = beendet).
void TestVtScreen::oscPromptMarkers() {
    VtScreen vt(24, 80);
    char kind = 0;
    int exitCode = -99;
    QObject::connect(&vt, &VtScreen::promptMarker, [&](char k, int e) { kind = k; exitCode = e; });

    vt.inputWrite("\x1b]133;C\x07");
    QCOMPARE(kind, 'C');

    vt.inputWrite("\x1b]133;D;1\x07");
    QCOMPARE(kind, 'D');
    QCOMPARE(exitCode, 1);
}

// OSC 9;4 (ConEmu/Windows-Terminal-Fortschritt) wird als progress() ausgegeben,
// während OSC 9;<text> weiterhin eine Notification bleibt.
void TestVtScreen::oscProgress() {
    VtScreen vt(24, 80);
    int state = -1, value = -1;
    QString note;
    QObject::connect(&vt, &VtScreen::progress, [&](int s, int v) { state = s; value = v; });
    QObject::connect(&vt, &VtScreen::notify, [&](const QString &t) { note = t; });

    vt.inputWrite("\x1b]9;4;1;42\x07");      // normal, 42 %
    QCOMPARE(state, 1);
    QCOMPARE(value, 42);

    vt.inputWrite("\x1b]9;4;0\x07");         // Fortschritt aus
    QCOMPARE(state, 0);

    vt.inputWrite("\x1b]9;Hallo Welt\x07");  // weiterhin Notification, kein Fortschritt
    QCOMPARE(note, QStringLiteral("Hallo Welt"));
    QCOMPARE(state, 0);                       // unverändert
}

// OSC 777;qtmux-event;<kind>;<text> wird als agentEvent() ausgegeben; das
// klassische OSC 777;notify bleibt eine Notification.
void TestVtScreen::oscAgentEvent() {
    VtScreen vt(24, 80);
    QString kind, text, note;
    QObject::connect(&vt, &VtScreen::agentEvent, [&](const QString &k, const QString &t) { kind = k; text = t; });
    QObject::connect(&vt, &VtScreen::notify, [&](const QString &t) { note = t; });

    vt.inputWrite("\x1b]777;qtmux-event;done;Build fertig\x07");
    QCOMPARE(kind, QStringLiteral("done"));
    QCOMPARE(text, QStringLiteral("Build fertig"));

    // ';' im Text bleibt erhalten (Tokens ab Index 2 wieder gejoint).
    vt.inputWrite("\x1b]777;qtmux-event;question;Soll ich A;B oder C?\x07");
    QCOMPARE(kind, QStringLiteral("question"));
    QCOMPARE(text, QStringLiteral("Soll ich A;B oder C?"));

    // Leerer Text ist zulässig.
    vt.inputWrite("\x1b]777;qtmux-event;info\x07");
    QCOMPARE(kind, QStringLiteral("info"));
    QCOMPARE(text, QString());

    // Klassisches notify bleibt Notification (kein agentEvent).
    kind.clear();
    vt.inputWrite("\x1b]777;notify;Titel;Text\x07");
    QCOMPARE(note, QStringLiteral("Titel: Text"));
    QCOMPARE(kind, QString());   // agentEvent NICHT gefeuert
}

QTEST_MAIN(TestVtScreen)
#include "tst_vtscreen.moc"
