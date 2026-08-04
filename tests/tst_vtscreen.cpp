#include <QtTest>
#include <QSignalSpy>
#include <QSysInfo>
#include <QTemporaryDir>
#include "VtScreen.h"
#include "AltScroll.h"
#include "LinkDetector.h"
#include "Session.h"   // nur für die Gui-freie Vorrangregel effectiveWorkingDirectory()

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
    void emojiPresentationWidth();
    void lineWrapContinuation();
    void mouseReporting();
    void linkDetectionOnScreenLine();
    void serializeAnsiRoundTrip();
    void clearViewportKeepsScrollback();
    void altScreenTracked();
    void altScrollModeTracked();
    void altScrollRule();
    void resetInputModesClearsMouse();
    void osc7WorkingDirectory();
    void osc7HostHandling();
    void osc7TakesPrecedenceOverPolling();
};

// OSC 7 (QTMUX-108): Die Shell meldet ihr Arbeitsverzeichnis selbst. Geprüft wird der
// ECHTE Weg — Bytes durch den Parser —, nicht eine herausgelöste Hilfsfunktion.
void TestVtScreen::osc7WorkingDirectory() {
    VtScreen vt(24, 80);
    QSignalSpy spy(&vt, &VtScreen::workingDirectoryReported);
    QVERIFY(vt.reportedWorkingDirectory().isEmpty());   // ungemeldet = unbekannt

    // Normalfall, BEL-terminiert. Der Host fehlt („file:///…") → lokal.
    vt.inputWrite("\x1b]7;file:///Users/nobser/Projekte\x07");
    QCOMPARE(vt.reportedWorkingDirectory(), QStringLiteral("/Users/nobser/Projekte"));
    QVERIFY(vt.reportedWorkingDirectoryIsLocal());
    QVERIFY(vt.reportedWorkingDirectoryHost().isEmpty());
    QCOMPARE(spy.count(), 1);

    // Dieselbe Meldung erneut (OSC 7 kommt an JEDER Prompt) → kein weiteres Signal.
    vt.inputWrite("\x1b]7;file:///Users/nobser/Projekte\x07");
    QCOMPARE(spy.count(), 1);

    // ST-terminiert (ESC \) statt BEL — beide Formen sind zulässig.
    vt.inputWrite("\x1b]7;file:///tmp/zwei\x1b\\");
    QCOMPARE(vt.reportedWorkingDirectory(), QStringLiteral("/tmp/zwei"));
    QCOMPARE(spy.count(), 2);

    // Prozent-Kodierung: Leerzeichen und UTF-8-Umlaute müssen dekodiert werden.
    vt.inputWrite("\x1b]7;file:///tmp/mit%20Leerzeichen/Gr%C3%BC%C3%9Fe\x07");
    QCOMPARE(vt.reportedWorkingDirectory(),
             QStringLiteral("/tmp/mit Leerzeichen/Grüße"));

    // Windows: der führende Trenner vor dem Laufwerksbuchstaben gehört zur URL, nicht
    // zum Pfad — sonst entstünde „/C:/Users/…", das kein Werkzeug öffnen kann.
    vt.inputWrite("\x1b]7;file:///C:/Users/nobser\x07");
    QCOMPARE(vt.reportedWorkingDirectory(), QStringLiteral("C:/Users/nobser"));
    // PowerShell kodiert den Doppelpunkt (EscapeDataString) — gleiches Ergebnis.
    vt.inputWrite("\x1b]7;file:///D%3A/Projekte\x07");
    QCOMPARE(vt.reportedWorkingDirectory(), QStringLiteral("D:/Projekte"));

    // Unbrauchbares darf den bekannten Stand NICHT überschreiben: fremdes Schema,
    // leerer Rumpf, Host ohne Pfad.
    const int before = spy.count();
    vt.inputWrite("\x1b]7;http://example.com/x\x07");
    vt.inputWrite("\x1b]7;\x07");
    vt.inputWrite("\x1b]7;file://rechner\x07");
    QCOMPARE(vt.reportedWorkingDirectory(), QStringLiteral("D:/Projekte"));
    QCOMPARE(spy.count(), before);
}

// Der Host-Teil entscheidet, ob der Pfad zu DIESER Maschine gehört. Fremde Hosts
// (SSH, Container) werden übernommen, aber als nicht-lokal gekennzeichnet — sonst
// liefe ein Pfad von einem anderen Rechner als lokales Verzeichnis weiter.
void TestVtScreen::osc7HostHandling() {
    {   // "localhost" zählt als diese Maschine.
        VtScreen vt(24, 80);
        vt.inputWrite("\x1b]7;file://localhost/tmp/eins\x07");
        QCOMPARE(vt.reportedWorkingDirectory(), QStringLiteral("/tmp/eins"));
        QVERIFY(vt.reportedWorkingDirectoryIsLocal());
    }
    {   // Der eigene Rechnername ebenfalls (so meldet es bash mit gesetztem $HOSTNAME).
        const QString own = QSysInfo::machineHostName();
        if (!own.isEmpty()) {
            VtScreen vt(24, 80);
            vt.inputWrite(("\x1b]7;file://" + own + "/tmp/zwei\x07").toUtf8());
            QVERIFY2(vt.reportedWorkingDirectoryIsLocal(), qPrintable(own));
            // FQDN-Form derselben Maschine gilt genauso (Shells melden mal so, mal so).
            VtScreen vt2(24, 80);
            vt2.inputWrite(("\x1b]7;file://" + own.section('.', 0, 0)
                            + ".example.invalid/tmp/drei\x07").toUtf8());
            QVERIFY(vt2.reportedWorkingDirectoryIsLocal());
        }
    }
    {   // Fremder Rechner: Pfad und Host stehen bereit, aber als nicht-lokal.
        VtScreen vt(24, 80);
        QString host, path; bool local = true;
        QObject::connect(&vt, &VtScreen::workingDirectoryReported,
                         [&](const QString &p, const QString &h, bool l) {
                             path = p; host = h; local = l;
                         });
        vt.inputWrite("\x1b]7;file://qtmux-fremder-rechner/opt/build\x07");
        QCOMPARE(path, QStringLiteral("/opt/build"));
        QCOMPARE(host, QStringLiteral("qtmux-fremder-rechner"));
        QVERIFY(!local);
        QVERIFY(!vt.reportedWorkingDirectoryIsLocal());
        QCOMPARE(vt.reportedWorkingDirectoryHost(),
                 QStringLiteral("qtmux-fremder-rechner"));
    }
}

// Die Vorrangregel der Session (QTMUX-108): Eine lokale Meldung schlägt den gepollten
// Wert; ohne Meldung — oder bei einem fremden Rechner — bleibt der bisherige Weg gültig.
// (Liegt hier statt in tst_session, weil der Auftrag nur diese Testdatei umfasst.)
void TestVtScreen::osc7TakesPrecedenceOverPolling() {
    using qtmux::effectiveWorkingDirectory;
    // Gemeldet + lokal gewinnt — genau der PowerShell-Fall, wo der gepollte Wert
    // dauerhaft das Startverzeichnis zeigt.
    QCOMPARE(effectiveWorkingDirectory(QStringLiteral("D:/Projekte/QTmux"), true,
                                       QStringLiteral("C:/Windows/System32")),
             QStringLiteral("D:/Projekte/QTmux"));
    // Fremder Rechner (SSH/Container): der Pfad existiert hier nicht → gepollter Wert.
    QCOMPARE(effectiveWorkingDirectory(QStringLiteral("/opt/build"), false,
                                       QStringLiteral("/Users/nobser")),
             QStringLiteral("/Users/nobser"));
    // Keine Meldung → unverändertes Verhalten.
    QCOMPARE(effectiveWorkingDirectory(QString(), true, QStringLiteral("/Users/nobser")),
             QStringLiteral("/Users/nobser"));
}

// Alternate Screen (DECSET/DECRST 1049) wird verfolgt (QTMUX-104): daran hängt, ob
// TerminalItem Maus-Events an die App weiterleitet — im Primary Screen (Shell am Prompt)
// nicht, damit ein hängen gebliebenes Tracking-Flag den Prompt nicht mit Codes flutet.
void TestVtScreen::altScreenTracked() {
    VtScreen vt(24, 80);
    QVERIFY(!vt.altScreen());                 // Start: Primary Screen
    vt.inputWrite("\x1b[?1049h");             // TUI betritt Alternate Screen
    QVERIFY(vt.altScreen());
    vt.inputWrite("\x1b[?1049l");             // TUI verlässt ihn wieder
    QVERIFY(!vt.altScreen());
}

// Alternate Scroll (DECSET/DECRST 1007) wird verfolgt. Das ist der Modus, mit dem eine App
// das Rad als Cursor-Tasten anfordert, OHNE Maus-Tracking einzuschalten — der Codex-Agent
// macht genau das (1049h + 1007h, kein 1000/1002/1003/1006, am Binary gezählt). Ohne
// libvterm-Patch 3 lief 1007 in den `default:`-Zweig von set_dec_mode und war unsichtbar;
// der CSI-Fallback sieht ihn nie, weil „CSI ? Ps h" als Sequenz erkannt ist. Gegenprobe zum
// Bug „Scrollen unmöglich": ohne den Patch bleibt altScroll() dauerhaft false.
void TestVtScreen::altScrollModeTracked() {
    VtScreen vt(24, 80);
    QVERIFY(!vt.altScroll());                 // Start: die App hat nichts verlangt
    vt.inputWrite("\x1b[?1007h");
    QVERIFY(vt.altScroll());
    vt.inputWrite("\x1b[?1007l");
    QVERIFY(!vt.altScroll());
    // Mehrere Modi in EINER Sequenz (so schickt es crossterm) müssen beide ankommen.
    VtScreen vt2(24, 80);
    vt2.inputWrite("\x1b[?1049h\x1b[?1007h");
    QVERIFY(vt2.altScreen());
    QVERIFY(vt2.altScroll());
    // 1007 darf den Alt-Screen NICHT mitschalten (und umgekehrt) — es sind zwei Modi.
    VtScreen vt3(24, 80);
    vt3.inputWrite("\x1b[?1007h");
    QVERIFY(!vt3.altScreen());
}

// Die Gui-freie Entscheidungsregel (AltScroll.h). Sie liegt an EINER Stelle, damit
// TerminalItem nur noch fragt — dieselbe Machart wie `shouldPreventSleep` (QTMUX-89).
void TestVtScreen::altScrollRule() {
    using M = AltScrollMode;
    // Reihenfolge der Argumente: mode, altScreen, mouseTracking, appRequested(1007),
    //                            hasScrollback, hasAgentKeys
    // --- Alternate Screen (vim, less, htop) ---------------------------------
    // Vorgabe: nur auf ausdrückliche Anforderung der App (DECSET 1007).
    QVERIFY(wheelGoesToApp(M::OnlyWhenRequested, true, 0, true, false, false));
    QVERIFY(!wheelGoesToApp(M::OnlyWhenRequested, true, 0, false, false, false));
    // „Immer" deckt zusätzlich die Anzeigeprogramme ab, die 1007 nicht senden.
    QVERIFY(wheelGoesToApp(M::AlwaysInAltScreen, true, 0, false, false, false));
    // 🔑 Im Alt-Screen darf ein vorhandener lokaler Scrollback NICHT abhalten: der
    // stammt vom Primary Screen (Shell vor dem TUI) und wäre der falsche Inhalt.
    // Gegenprobe: mit `!hasScrollback` auch in diesem Zweig fällt die Zeile.
    QVERIFY(wheelGoesToApp(M::AlwaysInAltScreen, true, 0, false, /*hasScrollback*/true, false));

    // --- Primary Screen: der Codex-Fall ------------------------------------
    // Codex läuft NICHT im Alt-Screen und sendet kein 1007 (am laufenden Programm
    // gemessen) — es zählt allein, dass eine gemessene Taste vorliegt und QTmux selbst
    // nichts zu zeigen hat. Ohne diese Klausel bliebe das Ticket unbehoben; genau diese
    // Zeile ist der Kern des Fixes.
    QVERIFY(wheelGoesToApp(M::OnlyWhenRequested, /*alt*/false, 0, /*1007*/false,
                           /*hasScrollback*/false, /*hasAgentKeys*/true));
    // 🔑 Der eigene Verlauf hat Vorrang: sobald QTmux Scrollback hat, scrollt es selbst.
    // Das heilt auch „Agent beendet, Shell wieder da".
    QVERIFY(!wheelGoesToApp(M::OnlyWhenRequested, false, 0, false, /*hasScrollback*/true, true));
    // 🔑 Ohne gemessene Taste wird im Primary Screen NIE etwas geschickt — an einem
    // Shell-Prompt blättern Cursor-Tasten die Befehls-Historie durch. Gilt auch für
    // „Immer" und selbst bei hängen gebliebenem 1007-Flag nach einem TUI-Absturz.
    QVERIFY(!wheelGoesToApp(M::OnlyWhenRequested, false, 0, /*1007*/true, false, false));
    QVERIFY(!wheelGoesToApp(M::AlwaysInAltScreen, false, 0, /*1007*/true, false, false));

    // --- Maus-Tracking hat immer Vorrang -----------------------------------
    // Greift die App die Maus (z. B. Claude Code), meldet QTMUX-104 das Rad als Taste
    // 4/5 mitsamt Zelle — dieselbe Rastung darf nicht zusätzlich als Taste gehen.
    QVERIFY(!wheelGoesToApp(M::OnlyWhenRequested, true, /*mouse*/2, true, false, false));
    QVERIFY(!wheelGoesToApp(M::AlwaysInAltScreen, true, /*mouse*/1, false, false, false));
    QVERIFY(!wheelGoesToApp(M::OnlyWhenRequested, false, /*mouse*/3, false, false, true));

    // Persistierte Zahlen: unbekannt/negativ → die zurückhaltende Richtung, nie „immer".
    QCOMPARE(altScrollModeFromInt(0), M::OnlyWhenRequested);
    QCOMPARE(altScrollModeFromInt(1), M::AlwaysInAltScreen);
    QCOMPARE(altScrollModeFromInt(99), M::OnlyWhenRequested);
    QCOMPARE(altScrollModeFromInt(-1), M::OnlyWhenRequested);
}

// resetInputModes() löst hängendes Maus-Tracking (der manuelle Notausgang), ohne den
// Bildschirm zu leeren. Gegenprobe zum Bug: Tracking bleibt sonst != 0.
void TestVtScreen::resetInputModesClearsMouse() {
    VtScreen vt(24, 80);
    vt.inputWrite("eins\r\nzwei");            // etwas Inhalt
    vt.inputWrite("\x1b[?1000h\x1b[?1006h");  // ein TUI aktiviert Maus-Tracking …
    QVERIFY(vt.mouseTracking() != 0);
    // … und endet unsauber (kein DECRST). Der manuelle Reset räumt auf:
    vt.resetInputModes();
    QCOMPARE(vt.mouseTracking(), 0);
    // 🔑 Alternate Scroll (1007) wird dabei bewusst NICHT gelöscht (Begründung in
    // VtScreen::resetInputModes): ein hängendes 1007 schadet nicht, weil das Rad nur im
    // Alt-Screen zur Cursor-Taste wird — es zu löschen würde nur einem *laufenden* Codex
    // das Rad abschalten, der 1007h nie erneut sendet.
    vt.inputWrite("\x1b[?1007h");
    vt.resetInputModes();
    QVERIFY(vt.altScroll());
    // Inhalt bleibt erhalten (kein Clear).
    QVERIFY(vt.screenText().contains(QStringLiteral("eins")));
    QVERIFY(vt.screenText().contains(QStringLiteral("zwei")));
}

// „Bildschirm leeren" darf NICHTS verlieren (QTMUX-61): Alles oberhalb der Cursorzeile
// wandert in den Scrollback, die Cursorzeile rückt nach oben. Der Test misst genau das,
// was der Anwender fürchtet — dass der Verlauf weg ist.
void TestVtScreen::clearViewportKeepsScrollback() {
    VtScreen vt(10, 40);
    QCOMPARE(vt.scrollbackCount(), 0);
    // Fünf Zeilen Inhalt, danach steht der Cursor in Zeile 5 (0-basiert) — wie ein Prompt
    // unter bereits gelaufener Ausgabe.
    vt.inputWrite("eins\r\nzwei\r\ndrei\r\nvier\r\n$ ");
    QCOMPARE(vt.cursor().y(), 4);
    QVERIFY(vt.screenText().contains(QStringLiteral("eins")));

    QVERIFY(vt.clearViewportKeepScrollback());

    // Die vier Zeilen darüber liegen jetzt im Scrollback …
    QCOMPARE(vt.scrollbackCount(), 4);
    const QString sb = vt.scrollbackText();
    for (const char *w : {"eins", "zwei", "drei", "vier"})
        QVERIFY2(sb.contains(QLatin1String(w)), w);
    // … und sind vom sichtbaren Bildschirm verschwunden.
    QVERIFY(!vt.screenText().contains(QStringLiteral("eins")));
    // Die Prompt-Zeile steht oben, der Cursor sitzt darauf (Spalte erhalten).
    QCOMPARE(vt.cursor().y(), 0);
    QCOMPARE(vt.cursor().x(), 2);
    // Direkt an der Zelle prüfen: screenText() schneidet rechte Leerzeichen ab, ein
    // startsWith("$ ") schlüge also fehl, obwohl die Zeile korrekt steht.
    QCOMPARE(vt.cell(0, 0).text, QStringLiteral("$"));

    // Zweiter Aufruf ohne neue Ausgabe: der Prompt steht bereits oben, es darf NICHTS
    // passieren. Ohne die Nullprüfung läse ein Terminal CSI 0 S als „rolle um 1" und
    // schluckte die Prompt-Zeile.
    QVERIFY(!vt.clearViewportKeepScrollback());
    QCOMPARE(vt.scrollbackCount(), 4);
    // Direkt an der Zelle prüfen: screenText() schneidet rechte Leerzeichen ab, ein
    // startsWith("$ ") schlüge also fehl, obwohl die Zeile korrekt steht.
    QCOMPARE(vt.cell(0, 0).text, QStringLiteral("$"));
}

// Integration: ein per Terminal geschriebener Pfad/URL muss als exakt der String bei
// LinkDetector ankommen. Sichert die Kette VtScreen-Zellen → Zeilentext → Heuristik,
// die im GUI-Pfad (TerminalItem::absLineText) sonst nur manuell prüfbar wäre.
void TestVtScreen::linkDetectionOnScreenLine() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QFile fh(dir.filePath(QStringLiteral("report.txt")));
    QVERIFY(fh.open(QIODevice::WriteOnly)); fh.close();

    VtScreen vt(24, 80);
    vt.inputWrite("Fertig: ./report.txt und https://example.com/x");

    // Zeile 0 wie im GUI zu Text zusammensetzen (ein Zeichen je Spalte).
    QString line;
    for (int col = 0; col < 80; ++col) {
        const Cell c = vt.cell(0, col);
        line += c.text.isEmpty() ? QStringLiteral(" ") : c.text;
    }

    const auto spans = LinkDetector::detect(line, dir.path());
    QCOMPARE(spans.size(), 2);
    QCOMPARE(spans[0].kind, LinkDetector::Span::FilePath);
    QCOMPARE(spans[0].target,
             QFileInfo(dir.filePath(QStringLiteral("report.txt"))).absoluteFilePath());
    QCOMPARE(spans[1].kind, LinkDetector::Span::Url);
    QCOMPARE(spans[1].target, QStringLiteral("https://example.com/x"));
}

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
void TestVtScreen::emojiPresentationWidth() {
    // Unicode TR#51: Basiszeichen mit Text-Default + VS-16 (U+FE0F) ist die farbige
    // Emoji-Form und belegt ZWEI Zellen. Ohne diese Regel bleibt die Zelle einspaltig,
    // die Glyphe wird aber doppelt breit gezeichnet und beschädigt die Nachbarzelle
    // (Symptom: fremde Emoji-Bruchstücke auf ganz anderen Zeichen).
    {   // U+26A0 + VS-16: zwei Zellen, Folgezeichen erst in Spalte 2.
        VtScreen vt(24, 80);
        vt.inputWrite("\xe2\x9a\xa0\xef\xb8\x8fN");
        QCOMPARE(int(vt.cell(0, 0).width), 2);
        QCOMPARE(vt.cell(0, 2).text, QStringLiteral("N"));
    }
    {   // Dasselbe Basiszeichen OHNE VS-16 bleibt die einspaltige Textform.
        VtScreen vt(24, 80);
        vt.inputWrite("\xe2\x9a\xa0N");
        QCOMPARE(int(vt.cell(0, 0).width), 1);
        QCOMPARE(vt.cell(0, 1).text, QStringLiteral("N"));
    }
    {   // VS-15 (U+FE0E) erzwingt umgekehrt die Textform bei einem Emoji-Default.
        VtScreen vt(24, 80);
        vt.inputWrite("\xe2\x9c\x85\xef\xb8\x8eN");   // U+2705 + VS-15
        QCOMPARE(int(vt.cell(0, 0).width), 1);
    }
}

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

// Maus-Reporting: DECSET 1000/1006 aktiviert Tracking; mouseButton/mouseMove
// erzeugen die passenden Escape-Sequenzen über outputToPty. Ohne Tracking (oder
// nach 1000l) darf nichts gesendet werden.
void TestVtScreen::mouseReporting() {
    VtScreen vt(24, 80);
    QByteArray out;
    QObject::connect(&vt, &VtScreen::outputToPty,
                     [&](const QByteArray &d) { out += d; });

    // Anfangs kein Tracking → mouseButton ist ein No-op.
    QCOMPARE(vt.mouseTracking(), 0);
    vt.mouseButton(1, true, 5, 10, Qt::NoModifier);
    QVERIFY(out.isEmpty());

    // DECSET 1000 (Klick-Tracking) einschalten.
    vt.inputWrite("\x1b[?1000h");
    QVERIFY(vt.mouseTracking() != 0);

    // Linksklick in Zelle (row 5, col 10) → X10-Maus-Report ESC [ M ...
    out.clear();
    vt.mouseButton(1, true, 5, 10, Qt::NoModifier);
    QVERIFY(!out.isEmpty());
    QVERIFY(out.startsWith("\x1b[M"));
    vt.mouseButton(1, false, 5, 10, Qt::NoModifier);   // Button wieder freigeben

    // Zusätzlich SGR-Encoding (1006): Report als ESC [ < b ; col ; row M/m,
    // 1-basiert. Linksklick in (row 5, col 10) → "\x1b[<0;11;6M".
    // (libvterm entprellt: ein Press wirkt nur als Zustandswechsel, daher sauberes
    // press→release-Paar.)
    vt.inputWrite("\x1b[?1006h");
    out.clear();
    vt.mouseButton(1, true, 5, 10, Qt::NoModifier);
    QCOMPARE(out, QByteArray("\x1b[<0;11;6M"));
    out.clear();
    vt.mouseButton(1, false, 5, 10, Qt::NoModifier);
    QCOMPARE(out, QByteArray("\x1b[<0;11;6m"));

    // Scrollrad hoch (Button 4) → SGR-Code 64.
    out.clear();
    vt.mouseButton(4, true, 5, 10, Qt::NoModifier);
    QCOMPARE(out, QByteArray("\x1b[<64;11;6M"));

    // Tracking wieder aus → keine Ausgabe mehr.
    vt.inputWrite("\x1b[?1000l");
    QCOMPARE(vt.mouseTracking(), 0);
    out.clear();
    vt.mouseButton(2, true, 5, 10, Qt::NoModifier);
    QVERIFY(out.isEmpty());
}

// Session-Restore Stufe 2 (QTMUX-81): serializeAnsi() muss Inhalt UND Attribute
// (Farbe, Truecolor, Bold, Underline) so ausgeben, dass ein Wieder-Einspeisen den
// Zustand reproduziert; weiche Umbrüche bleiben EINE logische Zeile (kein \r\n).
void TestVtScreen::serializeAnsiRoundTrip() {
    {   // Farb-/Attribut-Round-Trip in einen zweiten Screen.
        VtScreen a(24, 80);
        a.inputWrite("\x1b[31mR\x1b[0m\x1b[38;2;255;128;0mO\x1b[0m\x1b[1mB\x1b[0m\x1b[4mU\x1b[0mx");
        const QByteArray dump = a.serializeAnsi();
        QVERIFY(dump.contains("38;2;255;128;0"));      // Truecolor bleibt im Strom erhalten

        VtScreen b(24, 80);
        b.inputWrite(dump);
        QVERIFY(!b.cell(0, 0).fgDefault);              // R: gefärbt (Palette 1)
        QVERIFY(!b.cell(0, 1).fgDefault);              // O: Truecolor
        QCOMPARE(b.cell(0, 1).fg, quint32(0xff8000));  //    exakt erhalten
        QVERIFY(b.cell(0, 2).bold);                    // B: fett
        QVERIFY(b.cell(0, 3).underline);               // U: unterstrichen
        QCOMPARE(b.cell(0, 4).text, QStringLiteral("x"));
        QVERIFY(b.cell(0, 4).fgDefault);               // x: wieder Default
    }
    {   // Weicher Umbruch bleibt EIN logisches Stück (kein \r\n mitten im Wort).
        VtScreen a(24, 10);
        a.inputWrite("ABCDEFGHIJKLMNO");               // 15 > 10 Spalten -> Soft-Wrap
        QVERIFY(a.serializeAnsi().contains("ABCDEFGHIJKLMNO"));
    }
}

QTEST_MAIN(TestVtScreen)
#include "tst_vtscreen.moc"
