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

    // Seitenleisten-Umschalter (Design 2a): Der Default muss plattformgerecht sein und
    // darf mit keiner anderen Aktion kollidieren. Auf Windows/Linux ist Ctrl+B bewusst
    // NICHT belegt (tmux-Präfix, readline backward-char) — die Gegenprobe hält das fest.
    void toggleSidebarDefault() {
        auto *r = HotkeyRegistry::instance();
        r->resetAll();
        const QString id = QStringLiteral("actToggleSidebar");
        QVERIFY(r->actionIds().contains(id));

        const QString seq = r->sequence(id);
#if defined(Q_OS_MACOS)
        QCOMPARE(seq, QStringLiteral("Ctrl+B"));     // Qt-"Ctrl" ist hier Cmd
#else
        QCOMPARE(seq, QStringLiteral("Ctrl+Shift+L"));
        QVERIFY2(seq != QStringLiteral("Ctrl+B"),
                 "Ctrl+B gehoert auf Windows/Linux der Shell (tmux-Praefix)");
#endif
        // Konfliktfrei: keine andere Aktion hat diese Sequenz.
        QVERIFY(r->conflict(seq, id).isEmpty());
    }

    // Keine Sequenz darf zweimal als Vorgabe vorkommen. Qt meldet doppelte Shortcuts als
    // „ambiguous" und feuert dann keinen von beiden — der Fehler ist also unsichtbar, bis
    // ein Anwender die Taste drückt. Genau so lagen actVault und actClearScreen beide auf
    // Ctrl+Shift+K (bis 2026-07-30). Gegenprobe: mit der alten Doppelbelegung fällt der
    // Test (nachgestellt über setBinding).
    void defaultsAreConflictFree() {
        auto *r = HotkeyRegistry::instance();
        r->resetAll();
        for (const QString &id : r->actionIds()) {
            const QString seq = r->sequence(id);
            if (seq.isEmpty()) continue;    // ohne Kürzel = kein Konflikt
            QVERIFY2(r->conflict(seq, id).isEmpty(),
                     qPrintable(QStringLiteral("Doppelte Vorgabe %1 (%2 vs. %3)")
                                    .arg(seq, id, r->conflict(seq, id))));
        }
        // Positivkontrolle: Der Detektor findet eine künstlich erzeugte Doppelbelegung.
        r->setBinding(QStringLiteral("actVault"), r->sequence(QStringLiteral("actClearScreen")));
        QCOMPARE(r->conflict(r->sequence(QStringLiteral("actVault")), QStringLiteral("actVault")),
                 QStringLiteral("actClearScreen"));
        r->resetAll();
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

    // QTMUX-47: Die Kürzel-Kategorie (qml/prefs/CatHotkeys.qml) gruppiert die Aktionen
    // nach Sitzungen · Panes · Verbindungen · Ansicht & App. Unbekannte IDs landen in
    // „Weitere" — würde eine Bucket-ID umbenannt, verschwände die Aktion still aus ihrer
    // Gruppe. Dieser Guard koppelt die QML-Gruppierung an die Registry: jede genannte ID
    // muss existieren.
    void groupingBucketsExist() {
        auto *r = HotkeyRegistry::instance();
        const QStringList buckets = {
            // Sitzungen
            QStringLiteral("actNewSession"), QStringLiteral("actCloseSession"),
            QStringLiteral("actNextSession"), QStringLiteral("actPrevSession"),
            // Panes
            QStringLiteral("actClosePane"), QStringLiteral("actSplitH"),
            QStringLiteral("actSplitV"), QStringLiteral("actBroadcast"),
            // Verbindungen
            QStringLiteral("actNewSsh"), QStringLiteral("actNewSerial"),
            QStringLiteral("actConnections"), QStringLiteral("actVault"),
            // Ansicht & App
            QStringLiteral("actCommandPalette"), QStringLiteral("actMcpToggle"),
            QStringLiteral("actZoomReset"), QStringLiteral("actToggleTheme"),
            QStringLiteral("actSettings"), QStringLiteral("actAbout"),
            QStringLiteral("actQuit")
        };
        const QStringList ids = r->actionIds();
        for (const QString &b : buckets)
            QVERIFY2(ids.contains(b), qPrintable(QStringLiteral("Bucket-ID fehlt in der Registry: %1").arg(b)));
    }
};

QTEST_GUILESS_MAIN(TestHotkeys)
#include "tst_hotkeys.moc"
