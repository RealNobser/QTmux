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

    // --- QTMUX-84: Meta-Kodierung Alt+<Zeichen> → ESC + Zeichen ---------------

    void altLetterBecomesMetaSequence() {
        // Der Windows-Fall: text() ist bei Alt+Buchstabe LEER — vorher gingen darum
        // 0 Bytes raus (Alt+V für Claude Codes Bild-Einfügen kam nie an).
        QCOMPARE(encodeMetaSequence(Qt::Key_V, Qt::AltModifier, QString()),
                 QByteArray("\x1b" "v"));
        // readline-Kürzel, dieselbe Klasse.
        QCOMPARE(encodeMetaSequence(Qt::Key_B, Qt::AltModifier, QString()),
                 QByteArray("\x1b" "b"));
        QCOMPARE(encodeMetaSequence(Qt::Key_D, Qt::AltModifier, QString()),
                 QByteArray("\x1b" "d"));
        // Der Linux-Fall: text() ist gefüllt und wird layout-treu übernommen.
        QCOMPARE(encodeMetaSequence(Qt::Key_V, Qt::AltModifier, QStringLiteral("v")),
                 QByteArray("\x1b" "v"));
    }

    void altShiftKeepsUpperCase() {
        QCOMPARE(encodeMetaSequence(Qt::Key_V, Qt::AltModifier | Qt::ShiftModifier, QString()),
                 QByteArray("\x1b" "V"));
    }

    void altGrIsNotMetaEncoded() {
        // ⚠️ Gegenprobe zur teuersten Falle: Windows meldet AltGr als Ctrl+Alt.
        // AltGr+q muss „@" bleiben, nicht ESC q werden.
        QCOMPARE(encodeMetaSequence(Qt::Key_Q,
                                    Qt::AltModifier | Qt::ControlModifier,
                                    QStringLiteral("@")),
                 QByteArray());
        QCOMPARE(encodeKeyBytes(Qt::Key_Q, Qt::AltModifier | Qt::ControlModifier,
                                QStringLiteral("@")),
                 QByteArray("@"));
    }

    void metaSequenceRejectsNonCandidates() {
        // Ohne Alt gar nichts.
        QCOMPARE(encodeMetaSequence(Qt::Key_V, Qt::NoModifier, QStringLiteral("v")),
                 QByteArray());
        // Steuercodes bleiben unangetastet (kein ESC vor \x02).
        QCOMPARE(encodeMetaSequence(Qt::Key_B, Qt::AltModifier, QStringLiteral("\x02")),
                 QByteArray());
        // Doppelte Kodierung vermeiden, wenn die Plattform das ESC selbst liefert.
        QCOMPARE(encodeMetaSequence(Qt::Key_V, Qt::AltModifier, QStringLiteral("\x1b" "v")),
                 QByteArray());
        // Nicht-ASCII-Keycode ohne text() → nichts erfinden.
        QCOMPARE(encodeMetaSequence(Qt::Key_Adiaeresis, Qt::AltModifier, QString()),
                 QByteArray());
    }

    void altEnterKeepsQtmux43Behaviour() {
        // Die Meta-Kodierung darf QTMUX-43 nicht kapern: Enter wird im switch
        // behandelt, erreicht den Meta-Zweig also nie.
        QCOMPARE(encodeKeyBytes(Qt::Key_Return, Qt::AltModifier, QStringLiteral("\r")),
                 QByteArray("\x1b\r"));
        // Und Alt+Backspace bleibt DEL (Meta-Kodierung greift nur im default-Zweig).
        QCOMPARE(encodeKeyBytes(Qt::Key_Backspace, Qt::AltModifier, QString()),
                 QByteArray("\x7f"));
    }

    void encodeKeyUsesMetaOnlyOffMac() {
        // Plattform-Gate am Windows-Realfall (Alt+V ohne text()): Windows/Linux
        // kodieren Meta, macOS nicht — dort erzeugt Option Sonderzeichen (Option+v =
        // „√") und physisches Ctrl ist schon Meta.
        const QByteArray got = encodeKeyBytes(Qt::Key_V, Qt::AltModifier, QString());
        if (metaPrefixEnabled())
            QCOMPARE(got, QByteArray("\x1b" "v"));
        else
            QCOMPARE(got, QByteArray()); // wie vor QTMUX-84: nichts zu senden
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

    // --- MCP send_keys: benannte Tasten im tmux-Stil (encodeNamedKey) ---------

    void namedKeyCtrlChords() {
        // Der Anlass des Bugreports: Ctrl-U (Zeile leeren) war über MCP unsendbar.
        QCOMPARE(encodeNamedKey(QStringLiteral("C-u")), QByteArray("\x15"));
        QCOMPARE(encodeNamedKey(QStringLiteral("C-a")), QByteArray("\x01"));
        QCOMPARE(encodeNamedKey(QStringLiteral("C-c")), QByteArray("\x03"));
        QCOMPARE(encodeNamedKey(QStringLiteral("C-r")), QByteArray("\x12"));
        // Groß geschrieben dasselbe Steuerbyte (Caret-Konvention kennt kein Shift).
        QCOMPARE(encodeNamedKey(QStringLiteral("C-U")), QByteArray("\x15"));
        // Sonderfälle der Caret-Konvention.
        QCOMPARE(encodeNamedKey(QStringLiteral("C-Space")), QByteArray(1, '\0'));
        QCOMPARE(encodeNamedKey(QStringLiteral("C-?")), QByteArray("\x7f"));
        QCOMPARE(encodeNamedKey(QStringLiteral("C-[")), QByteArray("\x1b"));
    }

    void namedKeyMetaAndCombined() {
        QCOMPARE(encodeNamedKey(QStringLiteral("M-x")), QByteArray("\x1b" "x"));
        QCOMPARE(encodeNamedKey(QStringLiteral("A-x")), QByteArray("\x1b" "x")); // Alias
        QCOMPARE(encodeNamedKey(QStringLiteral("C-M-p")), QByteArray("\x1b\x10"));
        // Alt+Backspace = Wort löschen in readline.
        QCOMPARE(encodeNamedKey(QStringLiteral("M-Backspace")), QByteArray("\x1b\x7f"));
    }

    void namedKeySpecials() {
        QCOMPARE(encodeNamedKey(QStringLiteral("Enter")), QByteArray("\r"));
        QCOMPARE(encodeNamedKey(QStringLiteral("Escape")), QByteArray("\x1b"));
        QCOMPARE(encodeNamedKey(QStringLiteral("esc")), QByteArray("\x1b")); // Case-egal
        QCOMPARE(encodeNamedKey(QStringLiteral("Backspace")), QByteArray("\x7f"));
        QCOMPARE(encodeNamedKey(QStringLiteral("Tab")), QByteArray("\t"));
        QCOMPARE(encodeNamedKey(QStringLiteral("BTab")), QByteArray("\x1b[Z"));
        QCOMPARE(encodeNamedKey(QStringLiteral("S-Tab")), QByteArray("\x1b[Z"));
        QCOMPARE(encodeNamedKey(QStringLiteral("Space")), QByteArray(" "));
        QCOMPARE(encodeNamedKey(QStringLiteral("Up")), QByteArray("\x1b[A"));
        QCOMPARE(encodeNamedKey(QStringLiteral("Home")), QByteArray("\x1b[H"));
        QCOMPARE(encodeNamedKey(QStringLiteral("PageDown")), QByteArray("\x1b[6~"));
        QCOMPARE(encodeNamedKey(QStringLiteral("PgDn")), QByteArray("\x1b[6~"));
        QCOMPARE(encodeNamedKey(QStringLiteral("Delete")), QByteArray("\x1b[3~"));
        QCOMPARE(encodeNamedKey(QStringLiteral("F1")), QByteArray("\x1bOP"));
        QCOMPARE(encodeNamedKey(QStringLiteral("F5")), QByteArray("\x1b[15~"));
        QCOMPARE(encodeNamedKey(QStringLiteral("F12")), QByteArray("\x1b[24~"));
    }

    void namedKeyEnterFollowsQtmux43() {
        // S-/M-Enter = Umbruch einfügen (ESC CR) — dieselbe Regel wie am Keyboard.
        QCOMPARE(encodeNamedKey(QStringLiteral("S-Enter")), QByteArray("\x1b\r"));
        QCOMPARE(encodeNamedKey(QStringLiteral("M-Enter")), QByteArray("\x1b\r"));
        QCOMPARE(encodeNamedKey(QStringLiteral("C-Enter")), QByteArray("\r"));
    }

    void namedKeyModifiedCursorKeys() {
        // xterm-CSI-Modifier: 1 + Shift(1) + Alt(2) + Ctrl(4).
        QCOMPARE(encodeNamedKey(QStringLiteral("C-Right")), QByteArray("\x1b[1;5C"));
        QCOMPARE(encodeNamedKey(QStringLiteral("M-Up")), QByteArray("\x1b[1;3A"));
        QCOMPARE(encodeNamedKey(QStringLiteral("S-End")), QByteArray("\x1b[1;2F"));
        QCOMPARE(encodeNamedKey(QStringLiteral("C-S-Left")), QByteArray("\x1b[1;6D"));
        QCOMPARE(encodeNamedKey(QStringLiteral("C-PageUp")), QByteArray("\x1b[5;5~"));
        QCOMPARE(encodeNamedKey(QStringLiteral("C-F5")), QByteArray("\x1b[15;5~"));
        QCOMPARE(encodeNamedKey(QStringLiteral("C-F1")), QByteArray("\x1b[1;5P"));
    }

    void namedKeyUnknownIsEmpty() {
        // Leere Sequenz = „kein benannter Ausdruck" — der Aufrufer entscheidet über
        // Literal-Fallback (Wörter) bzw. Fehler (Chord-Tippfehler).
        QCOMPARE(encodeNamedKey(QStringLiteral("Entr")), QByteArray());
        QCOMPARE(encodeNamedKey(QStringLiteral("C-uu")), QByteArray());
        QCOMPARE(encodeNamedKey(QStringLiteral("C-ü")), QByteArray());
        QCOMPARE(encodeNamedKey(QStringLiteral("C-Tab")), QByteArray()); // kein Standard
        QCOMPARE(encodeNamedKey(QStringLiteral("F13")), QByteArray());
        QCOMPARE(encodeNamedKey(QString()), QByteArray());
        // Einzelzeichen ohne Modifier bleibt es selbst (Literal ohnehin identisch).
        QCOMPARE(encodeNamedKey(QStringLiteral("a")), QByteArray("a"));
        // Ein bloßes "C-" ist kein Chord.
        QCOMPARE(encodeNamedKey(QStringLiteral("C-")), QByteArray());
    }
};

QTEST_APPLESS_MAIN(TestKeyEncoding)
#include "tst_keyencoding.moc"
