#include <QtTest>

#include "OwnerDraft.h"

using namespace qtmux;

using Kind = OwnerDraftState::Kind;
Q_DECLARE_METATYPE(OwnerDraftState)

namespace {
// Eine Folge einzelner Tasten, wie die Oberfläche sie schickt: je Taste ein Aufruf.
OwnerDraftState typed(std::initializer_list<QByteArray> keys, OwnerDraftState s = {}) {
    for (const QByteArray &k : keys) s = nextOwnerDraft(s, k, HumanInputKind::Key);
    return s;
}
const QByteArray kUp("\x1b[A"), kDown("\x1b[B"), kRight("\x1b[C"), kLeft("\x1b[D");
const QByteArray kBackspace("\x7f"), kEnter("\r"), kCtrlC("\x03"), kCtrlU("\x15");
const QByteArray kEsc("\x1b"), kShiftEnter("\x1b\r"), kShiftTab("\x1b[Z");
} // namespace

/// Die Regel „der Mensch hat getippt und noch nicht abgeschickt“ als reine Funktion.
///
/// Geprüft wird je Regel BEIDES: dass der Merker steht, wo ein Entwurf stehen kann, und
/// dass er fällt, wo die Zeile sicher leer ist. Die zweite Hälfte ist keine Kür — ein
/// Merker, der nie fällt, hält jede Nachricht für immer zurück.
class TestOwnerDraft : public QObject {
    Q_OBJECT
private slots:
    void startsClean() {
        QVERIFY(!OwnerDraftState{}.pending());
        QVERIFY(!nextOwnerDraft({}, QByteArray(), HumanInputKind::Key).pending());
    }

    // --- Der Merker steht --------------------------------------------------------
    void typingSetsTheFlagAndCounts() {
        const OwnerDraftState s = typed({"h", "a", "l"});
        QVERIFY(s.pending());
        QCOMPARE(s.kind, Kind::Counted);
        QCOMPARE(s.count, 3);
    }

    void umlautsCountAsOneEach() {
        // ä ö ü ß sind je ein Zeichen und je ein Backspace — sie bleiben zählbar.
        const OwnerDraftState s = typed({QByteArray("ä"), QByteArray("ß")});
        QCOMPARE(s.kind, Kind::Counted);
        QCOMPARE(s.count, 2);
    }

    void severalKeysInOneCallAreReadInOrder() {
        // IME bzw. Broadcast: mehrere Zeichen in einem Aufruf.
        OwnerDraftState s = nextOwnerDraft({}, "hallo", HumanInputKind::Key);
        QCOMPARE(s.count, 5);
        // … auch mit dem Abschicken am Ende.
        s = nextOwnerDraft({}, "hallo\r", HumanInputKind::Key);
        QVERIFY(!s.pending());
        // … und Tippen NACH dem Abschicken ist ein neuer Entwurf.
        s = nextOwnerDraft({}, "a\rb", HumanInputKind::Key);
        QCOMPARE(s.kind, Kind::Counted);
        QCOMPARE(s.count, 1);
    }

    void pasteSetsTheFlagEvenWithALineEnd() {
        // Im Paste-Rahmen ist ein Zeilenende Text, kein Abschicken.
        const OwnerDraftState s = nextOwnerDraft({}, "zeile eins\rzeile zwei\r", HumanInputKind::Paste);
        QCOMPARE(s.kind, Kind::Open);
    }

    void emojiAndCombiningMarksAreNotCountable() {
        // Bei ihnen entfernt ein Backspace je nach Programm einen halben oder mehrere
        // Codepunkte — mitzählen wäre geraten.
        QCOMPARE(typed({QByteArray("😀")}).kind, Kind::Open);
        QCOMPARE(typed({"e", QByteArray("\xcc\x81")}).kind, Kind::Open);   // e + Akut
        QCOMPARE(typed({QByteArray("\xff")}).kind, Kind::Open);            // kein UTF-8
    }

    void historyAndSuggestionKeysMayCreateADraft_data() {
        QTest::addColumn<QByteArray>("key");
        QTest::newRow("Hoch (Verlauf)") << kUp;
        QTest::newRow("Runter") << kDown;
        QTest::newRow("Rechts (Vorschlag übernehmen)") << kRight;
        QTest::newRow("Hoch, Anwendungsmodus") << QByteArray("\x1bOA");
        QTest::newRow("Tab") << QByteArray("\t");
        QTest::newRow("Strg-P") << QByteArray("\x10");
        QTest::newRow("Strg-R") << QByteArray("\x12");
        QTest::newRow("Strg-Y") << QByteArray("\x19");
        QTest::newRow("Alt+Punkt") << QByteArray("\x1b.");
        QTest::newRow("Umschalt+Enter (Umbruch im Entwurf)") << kShiftEnter;
    }
    void historyAndSuggestionKeysMayCreateADraft() {
        // Aus einer LEEREN Zeile kann so ein Entwurf werden, ohne dass ein Zeichen fiel.
        QFETCH(QByteArray, key);
        QCOMPARE(typed({key}).kind, Kind::Open);
    }

    void neutralKeysLeaveACleanLineClean_data() {
        QTest::addColumn<QByteArray>("key");
        QTest::newRow("Escape") << kEsc;
        QTest::newRow("Umschalt+Tab") << kShiftTab;
        QTest::newRow("Bild auf") << QByteArray("\x1b[5~");
        QTest::newRow("Bild ab") << QByteArray("\x1b[6~");
        QTest::newRow("Links") << kLeft;
        QTest::newRow("Pos1") << QByteArray("\x1b[H");
        QTest::newRow("Entf") << QByteArray("\x1b[3~");
        QTest::newRow("F5") << QByteArray("\x1b[15~");
        QTest::newRow("Strg-L") << QByteArray("\x0c");
        QTest::newRow("Strg-O") << QByteArray("\x0f");
        QTest::newRow("Backspace") << kBackspace;
        QTest::newRow("Strg-U") << kCtrlU;
        QTest::newRow("Strg-D") << QByteArray("\x04");
    }
    void neutralKeysLeaveACleanLineClean() {
        // Sonst stünde der Merker nach jedem Blättern oder Modus-Wechsel — und fiele nie.
        QFETCH(QByteArray, key);
        QVERIFY(!typed({key}).pending());
    }

    // --- Der Merker fällt --------------------------------------------------------
    void enterAndCtrlCReleaseFromEveryState_data() {
        QTest::addColumn<OwnerDraftState>("before");
        QTest::addColumn<QByteArray>("key");
        const OwnerDraftState counted{Kind::Counted, 4}, open{Kind::Open, 0};
        QTest::newRow("Enter, gezählt") << counted << kEnter;
        QTest::newRow("Enter, offen") << open << kEnter;
        QTest::newRow("Zeilenvorschub, offen") << open << QByteArray("\n");
        QTest::newRow("Strg-C, gezählt") << counted << kCtrlC;
        QTest::newRow("Strg-C, offen") << open << kCtrlC;
    }
    void enterAndCtrlCReleaseFromEveryState() {
        QFETCH(OwnerDraftState, before);
        QFETCH(QByteArray, key);
        QVERIFY(before.pending());
        QVERIFY(!nextOwnerDraft(before, key, HumanInputKind::Key).pending());
        QVERIFY(endsOwnerDraft(key, HumanInputKind::Key));
    }

    void backspacingEverythingReleases() {
        OwnerDraftState s = typed({"a", "b", "c"});
        s = typed({kBackspace, kBackspace}, s);
        QVERIFY(s.pending());            // eines steht noch
        QCOMPARE(s.count, 1);
        s = typed({kBackspace}, s);
        QVERIFY(!s.pending());           // alle gelöscht: nachweislich leer
        s = typed({kBackspace}, s);
        QVERIFY(!s.pending());           // Backspace in der leeren Zeile zählt nicht ins Minus
        QCOMPARE(typed({"x"}, s).count, 1);
    }

    void backspaceDoesNotReleaseAnOpenDraft() {
        // Nach einer Einfügung oder Cursor-Bewegung weiß niemand, wie viel noch dasteht.
        OwnerDraftState s = nextOwnerDraft({}, "eingefügt", HumanInputKind::Paste);
        for (int i = 0; i < 50; ++i) s = typed({kBackspace}, s);
        QVERIFY(s.pending());
        s = typed({"a", "b", kLeft, kBackspace, kBackspace}, {});
        QVERIFY(s.pending());            // Links: das zweite Backspace löscht evtl. nichts mehr
    }

    void ctrlUReleasesOnlyACountedLine() {
        QVERIFY(!typed({"a", "b", kCtrlU}).pending());
        // Mehrzeiliger Entwurf: Strg-U nimmt nur eine Zeile weg.
        QVERIFY(typed({"a", kShiftEnter, "b", kCtrlU}).pending());
    }

    void escapeNeverReleases() {
        // In Agenten-TUIs bricht Escape die laufende Arbeit ab, die Eingabezeile bleibt.
        QVERIFY(typed({"a", kEsc}).pending());
        QVERIFY(typed({"a", kEsc, kEsc}).pending());
        QCOMPARE(typed({"a", "b", kEsc}).count, 2);   // und sie bleibt zählbar
    }

    void movingTheCursorEndsCounting() {
        QCOMPARE(typed({"a", "b", kLeft}).kind, Kind::Open);
        QCOMPARE(typed({"a", "b", QByteArray("\x1b\x7f")}).kind, Kind::Open);   // Alt+Backspace
        QCOMPARE(typed({"a", QByteArray("\x17")}).kind, Kind::Open);            // Strg-W
    }

    void typingAfterAnOpenDraftStaysOpen() {
        QCOMPARE(typed({kUp, "a", "b"}).kind, Kind::Open);
    }

    // --- endsOwnerDraft -----------------------------------------------------------
    void endsOwnerDraftLooksAtTheLastKey() {
        QVERIFY(endsOwnerDraft("abc\r", HumanInputKind::Key));
        QVERIFY(!endsOwnerDraft("\rabc", HumanInputKind::Key));
        QVERIFY(!endsOwnerDraft(kShiftEnter, HumanInputKind::Key));
        QVERIFY(!endsOwnerDraft("zeile\r", HumanInputKind::Paste));
        QVERIFY(!endsOwnerDraft(QByteArray(), HumanInputKind::Key));
    }

    void truncatedSequencesStayOnTheSafeSide() {
        // Abgeschnittene Escape- oder UTF-8-Folge: lieber ein Merker zu viel.
        QVERIFY(typed({QByteArray("\x1b[")}).pending());
        QVERIFY(typed({QByteArray("\xc3")}).pending());
    }
};

QTEST_APPLESS_MAIN(TestOwnerDraft)
#include "tst_ownerdraft.moc"
