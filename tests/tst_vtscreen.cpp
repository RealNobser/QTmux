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
};

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

QTEST_MAIN(TestVtScreen)
#include "tst_vtscreen.moc"
