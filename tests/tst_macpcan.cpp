// Unit-Test für die Textumsetzung des MacPCAN-Plugins (QTMUX-9): Frame→Zeile
// (formatFrame) und getippte Zeile→Frame (parseSendCommand). Bewusst ohne PCBUSB
// und ohne Hardware — testet nur den vendorierten CanFrame + CanText.h.

#include <QtTest>

#include "CanText.h"

using namespace mac_pcan;

class TestMacPcan : public QObject {
    Q_OBJECT
private slots:
    void formatStandard();
    void formatExtended();
    void parseStandardFrame();
    void parseExtendedById();
    void parseExtendedByPrefix();
    void parseRejectsBadInput();
};

// Standard-Frame: 3-stellige ID, Länge in [], Datenbytes groß-hex.
void TestMacPcan::formatStandard() {
    core::CanFrame f;
    f.id = 0x123;
    f.dlc = 4;
    f.timestamp_us = 12'345'678; // 12.345678 s
    f.data[0] = 0xDE; f.data[1] = 0xAD; f.data[2] = 0xBE; f.data[3] = 0xEF;
    const QString line = QString::fromUtf8(text::formatFrame(f));

    QVERIFY(line.contains(QStringLiteral("12.345678")));
    QVERIFY(line.contains(QStringLiteral("123")));
    QVERIFY(line.contains(QStringLiteral("[4]")));
    QVERIFY(line.contains(QStringLiteral("DE AD BE EF")));
    QVERIFY(!line.contains(QLatin1Char('E')) || line.indexOf(QStringLiteral("DEAD")) < 0); // kein Extended-Flag
}

// Extended-Frame: 8-stellige ID + Flag 'E'.
void TestMacPcan::formatExtended() {
    core::CanFrame f;
    f.id = 0x18DAF110;
    f.dlc = 0;
    f.flags = core::CanFrame::Extended;
    const QString line = QString::fromUtf8(text::formatFrame(f));
    QVERIFY(line.contains(QStringLiteral("18DAF110")));
    QVERIFY(line.contains(QStringLiteral("[0]")));
    QVERIFY(line.trimmed().endsWith(QLatin1Char('E'))); // Extended-Flag angehängt
}

void TestMacPcan::parseStandardFrame() {
    QString err;
    auto f = text::parseSendCommand(QStringLiteral("123 DE AD BE EF"), &err);
    QVERIFY2(f.has_value(), qPrintable(err));
    QCOMPARE(f->id, 0x123u);
    QCOMPARE(int(f->dlc), 4);
    QVERIFY(!f->isExtended());
    QCOMPARE(int(f->data[0]), 0xDE);
    QCOMPARE(int(f->data[3]), 0xEF);
}

// ID > 0x7FF erzwingt automatisch Extended.
void TestMacPcan::parseExtendedById() {
    auto f = text::parseSendCommand(QStringLiteral("800"));
    QVERIFY(f.has_value());
    QCOMPARE(f->id, 0x800u);
    QVERIFY(f->isExtended());
    QCOMPARE(int(f->dlc), 0);
}

// Präfix 'x' erzwingt Extended auch bei kleiner ID.
void TestMacPcan::parseExtendedByPrefix() {
    auto f = text::parseSendCommand(QStringLiteral("x1F 00"));
    QVERIFY(f.has_value());
    QCOMPARE(f->id, 0x1Fu);
    QVERIFY(f->isExtended());
    QCOMPARE(int(f->dlc), 1);
}

void TestMacPcan::parseRejectsBadInput() {
    QString err;
    QVERIFY(!text::parseSendCommand(QStringLiteral(""), &err).has_value());       // leer
    QVERIFY(!text::parseSendCommand(QStringLiteral("ZZ"), &err).has_value());     // ID kein hex
    QVERIFY(!text::parseSendCommand(QStringLiteral("100 GG"), &err).has_value()); // Byte kein hex
    QVERIFY(!text::parseSendCommand(QStringLiteral("100 1FF"), &err).has_value());// Byte > 0xFF
    QVERIFY(!text::parseSendCommand(QStringLiteral("100 0 1 2 3 4 5 6 7 8"), &err)
                 .has_value());                                                    // > 8 Bytes
}

QTEST_MAIN(TestMacPcan)
#include "tst_macpcan.moc"
