#include <QtTest>
#include <QSettings>
#include <QStandardPaths>

#include "HotkeyRegistry.h"

using namespace qtmux;

/// Tests für die konfigurierbare Hotkey-Registry (QTMUX-15). QSettings-Testmodus,
/// berührt keine echten Nutzereinstellungen.
class TestHotkeys : public QObject {
    Q_OBJECT
private slots:
    void initTestCase() {
        QStandardPaths::setTestModeEnabled(true);
        QCoreApplication::setOrganizationName(QStringLiteral("QTmux"));
        QCoreApplication::setApplicationName(QStringLiteral("QTmuxHotkeysTest"));
    }

    void defaultsAndOverride() {
        auto *r = HotkeyRegistry::instance();
        r->resetAll();

        // Defaults vorhanden, nichts benutzerdefiniert.
        QVERIFY(r->actionIds().contains(QStringLiteral("actNewSession")));
        QCOMPARE(r->sequence(QStringLiteral("actNewSession")), QStringLiteral("Ctrl+T"));
        QVERIFY(!r->isCustom(QStringLiteral("actNewSession")));

        // bindings-Map liefert die effektiven Sequenzen.
        QCOMPARE(r->bindings().value(QStringLiteral("actSplitH")).toString(),
                 QStringLiteral("Ctrl+Shift+E"));

        // Override setzen.
        r->setBinding(QStringLiteral("actNewSession"), QStringLiteral("Ctrl+Shift+N"));
        QCOMPARE(r->sequence(QStringLiteral("actNewSession")), QStringLiteral("Ctrl+Shift+N"));
        QVERIFY(r->isCustom(QStringLiteral("actNewSession")));
        QCOMPARE(r->bindings().value(QStringLiteral("actNewSession")).toString(),
                 QStringLiteral("Ctrl+Shift+N"));

        // Persistenz: frische QSettings sieht den Override.
        {
            QSettings s;
            s.beginGroup(QStringLiteral("hotkeys"));
            QCOMPARE(s.value(QStringLiteral("actNewSession")).toString(),
                     QStringLiteral("Ctrl+Shift+N"));
            s.endGroup();
        }

        // Setzen auf den Default entfernt den Override wieder.
        r->setBinding(QStringLiteral("actNewSession"), QStringLiteral("Ctrl+T"));
        QVERIFY(!r->isCustom(QStringLiteral("actNewSession")));

        // reset() einzeln.
        r->setBinding(QStringLiteral("actQuit"), QStringLiteral("Ctrl+Shift+Q"));
        QVERIFY(r->isCustom(QStringLiteral("actQuit")));
        r->reset(QStringLiteral("actQuit"));
        QCOMPARE(r->sequence(QStringLiteral("actQuit")), QStringLiteral("Ctrl+Q"));
    }

    void conflictDetection() {
        auto *r = HotkeyRegistry::instance();
        r->resetAll();
        // "Ctrl+W" gehört per Default actCloseSession → Konflikt für eine andere Aktion.
        QCOMPARE(r->conflict(QStringLiteral("Ctrl+W"), QStringLiteral("actSplitH")),
                 QStringLiteral("actCloseSession"));
        // Für die Aktion selbst ist es kein Konflikt (exceptId).
        QVERIFY(r->conflict(QStringLiteral("Ctrl+W"), QStringLiteral("actCloseSession")).isEmpty());
        // Unbelegte Sequenz → kein Konflikt.
        QVERIFY(r->conflict(QStringLiteral("Ctrl+Alt+F12"), QString()).isEmpty());
        // Case-insensitiv.
        QCOMPARE(r->conflict(QStringLiteral("ctrl+t"), QString()),
                 QStringLiteral("actNewSession"));
    }

    void multiChordSequence() {
        auto *r = HotkeyRegistry::instance();
        r->resetAll();
        // Multi-Chord wird als String akzeptiert und unverändert geführt.
        r->setBinding(QStringLiteral("actCommandPalette"), QStringLiteral("Ctrl+K, Ctrl+P"));
        QCOMPARE(r->sequence(QStringLiteral("actCommandPalette")),
                 QStringLiteral("Ctrl+K, Ctrl+P"));
        r->resetAll();
        QVERIFY(!r->isCustom(QStringLiteral("actCommandPalette")));
    }
};

QTEST_GUILESS_MAIN(TestHotkeys)
#include "tst_hotkeys.moc"
