#include <QtTest>

#include "KeyEncoding.h"

using namespace qtmux;

/// Tests für die Gui-freie Tasten-Übersetzung (QTMUX-43). Schwerpunkt: die
/// Enter-Varianten — Shift/Alt+Enter fügen in Agenten-TUIs (Claude Code u. a.)
/// einen Zeilenumbruch ein (ESC CR), unmodifiziertes Enter sendet weiterhin CR ab.
class TestKeyEncoding : public QObject {
    Q_OBJECT
private slots:
    void plainEnterSendsCr() {
        QCOMPARE(encodeKeyBytes(Qt::Key_Return, Qt::NoModifier, QStringLiteral("\r")),
                 QByteArray("\r"));
        QCOMPARE(encodeKeyBytes(Qt::Key_Enter, Qt::KeypadModifier, QStringLiteral("\r")),
                 QByteArray("\r")); // Keypad-Enter ohne echte Modifier = normales Enter
    }

    void shiftEnterInsertsNewline() {
        // QTMUX-43: ESC CR — dieselbe Sequenz, die /terminal-setup anderswo auf
        // Shift+Enter legt. Vorher ununterscheidbar von Enter (sendete ab).
        QCOMPARE(encodeKeyBytes(Qt::Key_Return, Qt::ShiftModifier, QStringLiteral("\r")),
                 QByteArray("\x1b\r"));
        QCOMPARE(encodeKeyBytes(Qt::Key_Enter, Qt::ShiftModifier | Qt::KeypadModifier,
                                QStringLiteral("\r")),
                 QByteArray("\x1b\r"));
    }

    void altEnterInsertsNewline() {
        QCOMPARE(encodeKeyBytes(Qt::Key_Return, Qt::AltModifier, QStringLiteral("\r")),
                 QByteArray("\x1b\r"));
    }

    void ctrlEnterStaysCr() {
        // Bewusst unverändert: Ctrl+Enter ist kein Umbruch-Kürzel.
        QCOMPARE(encodeKeyBytes(Qt::Key_Return, Qt::ControlModifier, QStringLiteral("\r")),
                 QByteArray("\r"));
    }

    void tabAndShiftTab() {
        QCOMPARE(encodeKeyBytes(Qt::Key_Tab, Qt::NoModifier, QStringLiteral("\t")),
                 QByteArray("\t"));
        QCOMPARE(encodeKeyBytes(Qt::Key_Tab, Qt::ShiftModifier, QString()),
                 QByteArray("\x1b[Z"));
        QCOMPARE(encodeKeyBytes(Qt::Key_Backtab, Qt::ShiftModifier, QString()),
                 QByteArray("\x1b[Z"));
    }

    void navigationAndFunctionKeys() {
        QCOMPARE(encodeKeyBytes(Qt::Key_Up, Qt::NoModifier, QString()), QByteArray("\x1b[A"));
        QCOMPARE(encodeKeyBytes(Qt::Key_Delete, Qt::NoModifier, QString()), QByteArray("\x1b[3~"));
        QCOMPARE(encodeKeyBytes(Qt::Key_F1, Qt::NoModifier, QString()), QByteArray("\x1bOP"));
        QCOMPARE(encodeKeyBytes(Qt::Key_F12, Qt::NoModifier, QString()), QByteArray("\x1b[24~"));
    }

    void printableFallsBackToText() {
        // Druckbare Zeichen (und Ctrl-Steuercodes) kommen aus QKeyEvent::text().
        QCOMPARE(encodeKeyBytes(Qt::Key_A, Qt::NoModifier, QStringLiteral("a")),
                 QByteArray("a"));
        QCOMPARE(encodeKeyBytes(Qt::Key_J, Qt::ControlModifier, QStringLiteral("\n")),
                 QByteArray("\n")); // Ctrl+J = LF (Umbruch-Alternative in Agenten-TUIs)
        QCOMPARE(encodeKeyBytes(Qt::Key_Udiaeresis, Qt::NoModifier, QStringLiteral("ü")),
                 QStringLiteral("ü").toUtf8());
    }
};

QTEST_APPLESS_MAIN(TestKeyEncoding)
#include "tst_keyencoding.moc"
