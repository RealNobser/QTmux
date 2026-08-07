// QTMUX-130: Der wiederhergestellte Scrollback darf erst eingespielt werden, wenn das
// Pane vermessen ist.
//
// 🔑 Worum es geht: `VtScreen::serializeAnsi` schreibt weiche Umbrueche bewusst OHNE
// CRLF, damit das Terminal beim Einspielen selbst auf die aktuelle Breite umbricht.
// Beim Restore steht die Session aber noch auf den Startwerten 80x24 — die echte Breite
// kommt erst ueber `TerminalItem::applyPendingResize` (Layout + 60 ms Entprellung,
// QTMUX-86). Frueher wurde der Dump sofort eingespielt: umbrochen bei 80, und
// `vterm_set_size` reflowt den Scrollback nicht mehr — der falsche Umbruch war
// eingefroren und wurde beim naechsten Beenden erneut gespeichert.
//
// 🔑 `oldOrderWouldWrapAt80` ist der Gegentest zum Messmittel: Er zeigt, dass derselbe
// Dump in einem 80-Spalten-Screen nachweislich zerhackt wird. Ohne ihn koennte der
// Haupttest gruen sein, weil die Pruefung gar nichts unterscheidet.

#include <QtTest>

#include "HistoryDump.h"
#include "ITerminalBackend.h"
#include "Session.h"
#include "VtScreen.h"

using namespace qtmux;

// Backend-Attrappe: liefert Bytes genau dann, wenn der Test es will. Ein echtes PTY
// taugt hier nicht — geprueft wird die REIHENFOLGE zwischen Verlauf und Ausgabe, und
// die haengt an einem Prozessstart sonst vom Zufall ab.
class FakeBackend : public ITerminalBackend {
    Q_OBJECT
public:
    bool start(int cols, int rows) override {
        startCols = cols; startRows = rows;
        setState(BackendState::Running);
        return true;
    }
    void write(const QByteArray &) override {}
    void resize(int cols, int rows) override { resizeCols = cols; resizeRows = rows; }
    void terminate() override { setState(BackendState::Closed); }

    void feed(const QByteArray &data) { emit dataReceived(data); }

    int startCols = -1, startRows = -1;
    int resizeCols = -1, resizeRows = -1;
};

class TestRestoreHistory : public QObject {
    Q_OBJECT
private slots:
    void oldOrderWouldWrapAt80();
    void historyWrapsAtMeasuredWidth();
    void backendOutputStaysBelowRestoredHistory();
    void identicalSizeAlsoReleasesHistory();
    void historyArrivesEvenWithoutResize();
    void safetyNetUsesLastKnownWidth();
    void pendingHistoryIsReportedUntilFlushed();
    void headerRoundTrip();
    void headerlessDumpStaysValid();

private:
    static QString rowText(const VtScreen &vt, int row);
    static QByteArray dumpWithLongLine(const QString &line);
};

QString TestRestoreHistory::rowText(const VtScreen &vt, int row) {
    QString s;
    for (int c = 0; c < vt.cols(); ++c) {
        const QString t = vt.cell(row, c).text;
        s += t.isEmpty() ? QStringLiteral(" ") : t;
    }
    while (s.endsWith(QLatin1Char(' '))) s.chop(1);
    return s;
}

// Erzeugt einen Dump so, wie ihn ein breites Fenster beim Beenden schreiben wuerde:
// eine Zeile, die breiter als 80 Spalten ist, aber in ihren Screen passte.
QByteArray TestRestoreHistory::dumpWithLongLine(const QString &line) {
    VtScreen wide(24, 120);
    wide.inputWrite(line.toUtf8() + "\r\n");
    return wide.serializeAnsi();
}

// Gegentest zum Messmittel: derselbe Dump in einem 80-Spalten-Screen wird zerhackt.
// Faellt dieser Test nicht, prueft der Haupttest unten nichts.
void TestRestoreHistory::oldOrderWouldWrapAt80() {
    const QString line(100, QLatin1Char('X'));
    const QByteArray dump = dumpWithLongLine(line);

    VtScreen narrow(24, 80);
    narrow.inputWrite(dump);

    QCOMPARE(rowText(narrow, 0).size(), 80);          // hart am alten Rand umbrochen
    QCOMPARE(rowText(narrow, 1).size(), 20);          // Rest in der Folgezeile
    QVERIFY(rowText(narrow, 0) != line);
}

// Kern: Der Verlauf wird auf die GEMESSENE Breite umbrochen, nicht auf die Startbreite.
void TestRestoreHistory::historyWrapsAtMeasuredWidth() {
    const QString line(100, QLatin1Char('X'));
    const QByteArray dump = dumpWithLongLine(line);

    Session sess;
    auto *fake = new FakeBackend;
    sess.attachBackend(fake, Session::Type::Shell, 80, 24);
    sess.setPendingHistory(dump);

    // Vor der Vermessung darf nichts im Screen stehen — sonst waere der Umbruch schon
    // gefallen und kein spaeteres resize koennte ihn noch heilen.
    QVERIFY(rowText(*sess.screen(), 0).isEmpty());

    sess.resize(120, 30);

    QCOMPARE(rowText(*sess.screen(), 0), line);       // vollstaendig in EINER Zeile
    QVERIFY(rowText(*sess.screen(), 1).isEmpty());
}

// Die zurueckgehaltene Backend-Ausgabe landet UNTER dem Verlauf, nicht darueber.
void TestRestoreHistory::backendOutputStaysBelowRestoredHistory() {
    const QString line(100, QLatin1Char('X'));
    const QByteArray dump = dumpWithLongLine(line);

    Session sess;
    auto *fake = new FakeBackend;
    sess.attachBackend(fake, Session::Type::Shell, 80, 24);
    sess.setPendingHistory(dump);

    // Die frische Shell meldet sich, bevor das Layout steht — der Normalfall.
    fake->feed(QByteArrayLiteral("PROMPT_MARKER\r\n"));
    QVERIFY(rowText(*sess.screen(), 0).isEmpty());    // noch zurueckgehalten

    sess.resize(120, 30);

    QCOMPARE(rowText(*sess.screen(), 0), line);
    QCOMPARE(rowText(*sess.screen(), 1), QStringLiteral("PROMPT_MARKER"));
}

// Trifft das Pane zufaellig genau die Startwerte 80x24, ist das trotzdem die erste
// gemessene Groesse — der Verlauf darf dort nicht haengenbleiben.
void TestRestoreHistory::identicalSizeAlsoReleasesHistory() {
    const QString line = QStringLiteral("KURZ_GENUG");
    const QByteArray dump = dumpWithLongLine(line);

    Session sess;
    auto *fake = new FakeBackend;
    sess.attachBackend(fake, Session::Type::Shell, 80, 24);
    sess.setPendingHistory(dump);
    QVERIFY(sess.hasPendingHistory());

    sess.resize(80, 24);                              // gleiche Groesse, echte Messung

    QVERIFY(!sess.hasPendingHistory());
    QCOMPARE(rowText(*sess.screen(), 0), line);
}

// Sicherheitsnetz: Kommt nie ein resize (headless, nie sichtbares Fenster), erscheint
// der Verlauf trotzdem — lieber ein 80-Spalten-Umbruch als eine leere Session.
void TestRestoreHistory::historyArrivesEvenWithoutResize() {
    const QString line = QStringLiteral("OHNE_RESIZE");
    const QByteArray dump = dumpWithLongLine(line);

    Session sess;
    auto *fake = new FakeBackend;
    sess.attachBackend(fake, Session::Type::Shell, 80, 24);
    sess.setPendingHistory(dump);

    QTRY_VERIFY_WITH_TIMEOUT(!sess.hasPendingHistory(), 5000);
    QCOMPARE(rowText(*sess.screen(), 0), line);
}

// Greift das Sicherheitsnetz (Fenster seit dem Neustart nie sichtbar, also nie
// vermessen), zaehlt die zuletzt bekannte Breite — NICHT der geratene Startwert 80.
// Ohne diese Regel traefe der Fehler weiterhin jedes nicht-aktive Fenster.
void TestRestoreHistory::safetyNetUsesLastKnownWidth() {
    const QString line(100, QLatin1Char('X'));
    const QByteArray dump = dumpWithLongLine(line);

    Session sess;
    auto *fake = new FakeBackend;
    sess.attachBackend(fake, Session::Type::Shell, 80, 24);
    sess.setPendingHistory(dump, 120);                // Breite aus der Dump-Kopfzeile

    QTRY_VERIFY_WITH_TIMEOUT(!sess.hasPendingHistory(), 5000);

    QCOMPARE(rowText(*sess.screen(), 0), line);       // ungebrochen, obwohl nie vermessen
    QCOMPARE(sess.cols(), 120);                       // Session meldet die uebernommene Breite
    QCOMPARE(fake->resizeCols, 120);                  // und das Backend weiss davon
}

// ⚠️ Waehrend der Verlauf aussteht, ist der Bildschirm leer. Wer in diesem Zustand
// speichert, ersetzt den Dump auf der Platte durch nichts — `SessionModel` fragt darum
// `hasPendingHistory()` ab, und diese Auskunft muss stimmen.
void TestRestoreHistory::pendingHistoryIsReportedUntilFlushed() {
    Session sess;
    auto *fake = new FakeBackend;
    sess.attachBackend(fake, Session::Type::Shell, 80, 24);
    QVERIFY(!sess.hasPendingHistory());               // ohne Restore nichts ausstehend

    sess.setPendingHistory(dumpWithLongLine(QStringLiteral("EGAL")));
    QVERIFY(sess.hasPendingHistory());
    QVERIFY(sess.screen()->serializeAnsi().isEmpty());  // genau der gefaehrliche Zustand

    sess.resize(120, 30);
    QVERIFY(!sess.hasPendingHistory());
    QVERIFY(!sess.screen()->serializeAnsi().isEmpty());

    // Ein leerer Dump aendert nichts (kein Zustand, der spaeter freigegeben werden muss).
    sess.setPendingHistory(QByteArray());
    QVERIFY(!sess.hasPendingHistory());
}

// Kopfzeile der Dump-Datei: schreiben und wieder abtrennen, ohne den Strom zu beruehren.
void TestRestoreHistory::headerRoundTrip() {
    const QByteArray dump = dumpWithLongLine(QStringLiteral("INHALT"));
    QByteArray datei = historydump::header(110) + dump;

    QCOMPARE(historydump::takeHeader(datei), 110);
    QCOMPARE(datei, dump);                            // exakt der urspruengliche Strom

    // Unbekannte Breite wird nicht als Zahl behauptet.
    QVERIFY(historydump::header(0).isEmpty());
    QVERIFY(historydump::header(-1).isEmpty());
}

// ⚠️ Dumps aus einer aelteren Fassung haben keine Kopfzeile. Sie muessen unveraendert
// gueltig bleiben — ein Pflicht-Header haette jeden vorhandenen Verlauf entwertet.
void TestRestoreHistory::headerlessDumpStaysValid() {
    QByteArray alt = dumpWithLongLine(QStringLiteral("ALTBESTAND"));
    const QByteArray unveraendert = alt;

    QCOMPARE(historydump::takeHeader(alt), 0);        // keine Breite bekannt
    QCOMPARE(alt, unveraendert);                      // und nichts abgeschnitten

    // Halb geschriebene Datei (Kopfzeile ohne Zeilenumbruch): nichts wegwerfen.
    QByteArray abgeschnitten(historydump::kHeaderPrefix);
    abgeschnitten += "110";
    const QByteArray vorher = abgeschnitten;
    QCOMPARE(historydump::takeHeader(abgeschnitten), 0);
    QCOMPARE(abgeschnitten, vorher);
}

QTEST_MAIN(TestRestoreHistory)
#include "tst_restorehistory.moc"
