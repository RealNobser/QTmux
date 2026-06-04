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

QTEST_MAIN(TestVtScreen)
#include "tst_vtscreen.moc"
