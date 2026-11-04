#include <QtTest>
#include <QClipboard>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QWheelEvent>
#include <QGuiApplication>
#include <QHoverEvent>
#include <QMouseEvent>
#include <QSignalSpy>
#include <QTemporaryDir>

#include "ITerminalBackend.h"
#include "Session.h"
#include "TerminalItem.h"
#include "VtScreen.h"

using namespace qtmux;

namespace {
// Kein Prozess: Die Bytes kommen direkt in den VtScreen der Session.
class FakeBackend final : public ITerminalBackend {
public:
    bool start(int, int) override { return true; }
    void write(const QByteArray &) override {}
    void resize(int, int) override {}
    void terminate() override {}
};
} // namespace

/// Das echte TerminalItem mit echten Maus-Ereignissen, ohne Fenster: Trägt der Weg von der
/// Geste bis zum Öffnen, und erreicht die Auswahl-Regel den Bildschirm? Die Regeln selbst prüfen
/// tst_terminalselection und tst_screenlinks; hier geht es um die Verdrahtung. Läuft auf allen
/// drei Plattformen; `QDesktopServices::setUrlHandler` fängt das Öffnen ab, das Ausführen durch
/// das Betriebssystem bleibt der Gegenprobe an der lebenden Instanz vorbehalten.
class TestTerminalItem : public QObject {
    Q_OBJECT
public slots:
    void onOpen(const QUrl &url) { m_opened << url; }

private:
    QList<QUrl> m_opened;

    struct Fixture {
        Session session;
        TerminalItem item;
        QStringList revealed;
        Fixture() {
            session.attachBackend(new FakeBackend, Session::Type::Shell, 80, 24);
            item.setSession(&session);
            item.setSize(QSizeF(item.cellWidth() * 60 + 1, item.cellHeight() * 10 + 1));
            item.setRevealFunction([this](const QString &t) { revealed << t; return true; });
        }
        bool ready() const {
            VtScreen *sc = session.screen();
            return sc && sc->cols() == 60 && sc->rows() == 10;
        }
        void feed(const QByteArray &bytes) { session.screen()->inputWrite(bytes); }
        QPointF at(int col, int row) const {
            return QPointF((col + 0.5) * item.cellWidth(), (row + 0.5) * item.cellHeight());
        }
        void hover(const QPointF &pos) {
            QHoverEvent ev(QEvent::HoverMove, pos, pos, pos, Qt::NoModifier);
            QCoreApplication::sendEvent(&item, &ev);
        }
        void mouse(QEvent::Type type, const QPointF &pos, Qt::KeyboardModifiers mods = Qt::NoModifier) {
            const Qt::MouseButton button = type == QEvent::MouseMove ? Qt::NoButton : Qt::LeftButton;
            const Qt::MouseButtons buttons = type == QEvent::MouseButtonRelease ? Qt::NoButton : Qt::LeftButton;
            QMouseEvent ev(type, pos, pos, pos, button, buttons, mods);
            QCoreApplication::sendEvent(&item, &ev);
        }
        void click(const QPointF &pos, Qt::KeyboardModifiers mods = Qt::NoModifier) {
            mouse(QEvent::MouseButtonPress, pos, mods);
            mouse(QEvent::MouseButtonRelease, pos, mods);
        }
        void drag(const QPointF &from, const QPointF &to) {
            mouse(QEvent::MouseButtonPress, from);
            mouse(QEvent::MouseMove, to);
            mouse(QEvent::MouseButtonRelease, to);
        }
        QString copied() {
            QGuiApplication::clipboard()->clear();
            item.copy();
            return QGuiApplication::clipboard()->text();
        }
    };

    static Qt::KeyboardModifiers revealMods() {
#if defined(Q_OS_MACOS)
        return Qt::ControlModifier | Qt::AltModifier;
#else
        return Qt::ControlModifier | Qt::ShiftModifier;
#endif
    }

private slots:
    void initTestCase() {
        QDesktopServices::setUrlHandler(QStringLiteral("https"), this, "onOpen");
        QDesktopServices::setUrlHandler(QStringLiteral("file"), this, "onOpen");
    }
    void cleanupTestCase() {
        QDesktopServices::unsetUrlHandler(QStringLiteral("https"));
        QDesktopServices::unsetUrlHandler(QStringLiteral("file"));
    }
    void init() { m_opened.clear(); }

    // --- Links: die Gesten am Item ---------------------------------------------------------
    void ctrlClickOpensAUrl() {
        Fixture f;
        QTRY_VERIFY(f.ready());
        f.feed("siehe https://example.com/x hier\r\n");
        f.hover(f.at(10, 0));
        QCOMPARE(f.item.hoverLinkTarget(), QStringLiteral("https://example.com/x"));
        f.click(f.at(10, 0), Qt::ControlModifier);
        QCOMPARE(m_opened, QList<QUrl>{QUrl(QStringLiteral("https://example.com/x"))});
        QVERIFY(f.item.linkNotice().contains(QStringLiteral("https://example.com/x")));
        QVERIFY(!f.item.hasSelection());
    }

    void plainClickDoesNotOpen() {
        Fixture f;
        QTRY_VERIFY(f.ready());
        f.feed("siehe https://example.com/x hier\r\n");
        f.hover(f.at(10, 0));
        f.click(f.at(10, 0));
        QVERIFY(m_opened.isEmpty());
        QVERIFY(f.item.linkNotice().isEmpty());
    }

    void gestureWorksWithoutAPriorHover() {
        // Der Klick darf nicht vom Hover abhängen: Ein Ereignis kann verloren gehen, etwa wenn
        // das Fenster gerade erst aktiv wird.
        Fixture f;
        QTRY_VERIFY(f.ready());
        f.feed("https://example.com/x\r\n");
        f.click(f.at(3, 0), Qt::ControlModifier);
        QCOMPARE(m_opened.size(), 1);
    }

    void gestureBeatsAppMouse() {
        // Eine Anwendung mit Maus (Vollbild-Agent): Die Geste bleibt bei QTmux und erreicht die
        // Anwendung nicht, ein normaler Klick dagegen schon.
        Fixture f;
        QTRY_VERIFY(f.ready());
        QByteArray fromApp;
        QObject::connect(f.session.screen(), &VtScreen::outputToPty, &f.item,
                         [&fromApp](const QByteArray &b) { fromApp += b; });
        f.feed("\033[?1049h\033[?1000h\033[?1006h\033[1;1Hhttps://example.com/x");
        f.hover(f.at(3, 0));
        QCOMPARE(f.item.hoverLinkTarget(), QStringLiteral("https://example.com/x"));
        f.click(f.at(3, 0), Qt::ControlModifier);
        QCOMPARE(m_opened.size(), 1);
        QVERIFY2(fromApp.isEmpty(), fromApp.constData());
        f.click(f.at(3, 0));
        QCOMPARE(m_opened.size(), 1);
        QVERIFY(fromApp.contains("\033[<"));
    }

    void revealOnAUrlSaysWhyNothingHappens() {
        // Einer der Fälle hinter „geht nicht immer“: Auf einer URL kann die Geste nichts zeigen.
        Fixture f;
        QTRY_VERIFY(f.ready());
        f.feed("https://example.com/x\r\n");
        f.click(f.at(3, 0), revealMods());
        QVERIFY(m_opened.isEmpty());
        QVERIFY(f.revealed.isEmpty());
        QVERIFY(!f.item.linkNotice().isEmpty());
    }

    void revealAndOpenOnAFile() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = QDir(dir.path()).absoluteFilePath(QStringLiteral("datei.txt"));
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.close();

        Fixture f;
        QTRY_VERIFY(f.ready());
        // Der Pfad darf die Zeile sprengen; Hauptsache, der weiche Umbruch trägt.
        f.feed(path.toUtf8() + "\r\n");
        const QFileInfo fi(path);
        f.click(f.at(3, 0), revealMods());
        QCOMPARE(f.revealed.size(), 1);
        QCOMPARE(QFileInfo(f.revealed.first()).canonicalFilePath(), fi.canonicalFilePath());
        f.click(f.at(3, 0), Qt::ControlModifier);
        QCOMPARE(m_opened.size(), 1);
        QVERIFY(m_opened.first().isLocalFile());
    }

    void missingTheLinkIsReported() {
        Fixture f;
        QTRY_VERIFY(f.ready());
        f.feed("siehe https://example.com/x hier\r\n");
        f.click(f.at(1, 0), Qt::ControlModifier);
        QVERIFY(m_opened.isEmpty());
        QVERIFY(!f.item.linkNotice().isEmpty());
        // Daneben geklickt heißt: der Klick läuft normal weiter, Ziehen markiert also.
        f.mouse(QEvent::MouseButtonPress, f.at(0, 0), Qt::ControlModifier);
        f.mouse(QEvent::MouseMove, f.at(4, 0), Qt::ControlModifier);
        f.mouse(QEvent::MouseButtonRelease, f.at(4, 0), Qt::ControlModifier);
        QVERIFY(f.item.hasSelection());
    }

    void refusalByTheSystemIsReported() {
        if (QGuiApplication::platformName() != QLatin1String("offscreen"))
            QSKIP("Nur offscreen lehnt openUrl verlässlich ab");
        Fixture f;
        QTRY_VERIFY(f.ready());
        f.feed("ftp://example.com/x\r\n");
        f.click(f.at(3, 0), Qt::ControlModifier);
        QVERIFY(f.item.linkNotice().contains(QStringLiteral("ftp://example.com/x")));
        QVERIFY(!f.item.linkNotice().startsWith(QStringLiteral("Wird")));
    }

    // --- Links: zeitweises Versagen ----------------------------------------------------------
    void clickOpensWhatThePillNames() {
        // Der Pfad wird gegen das Arbeitsverzeichnis aufgelöst. Ändert es sich oder ist es gerade
        // nicht lesbar, darf der Klick nicht anders entscheiden als die Pille.
        QTemporaryDir a, b;
        QVERIFY(a.isValid() && b.isValid());
        QVERIFY(QDir(a.path()).mkpath(QStringLiteral("unter")));
        QFile file(QDir(a.path()).filePath(QStringLiteral("unter/datei.txt")));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.close();

        Fixture f;
        QTRY_VERIFY(f.ready());
        f.feed("\033]7;" + QUrl::fromLocalFile(QDir(a.path()).absolutePath()).toEncoded() + "\033\\");
        f.feed("unter/datei.txt\r\n");
        f.hover(f.at(3, 0));
        const QString pill = f.item.hoverLinkTarget();
        QVERIFY(!pill.isEmpty());
        f.feed("\033]7;" + QUrl::fromLocalFile(QDir(b.path()).absolutePath()).toEncoded() + "\033\\");
        f.click(f.at(3, 0), Qt::ControlModifier);
        QCOMPARE(m_opened.size(), 1);
        QCOMPARE(m_opened.first().toLocalFile(), pill);
    }

    void clickOpensTheLocalPathNotTheForeignUrlText() {
        // Anfrage und Fragment gingen sonst als Text an die Shell, an der Pfad-Prüfung vorbei.
        Fixture f;
        QTRY_VERIFY(f.ready());
        f.feed("file:///tmp/x.txt?a=1#b\r\n");
        f.click(f.at(3, 0), Qt::ControlModifier);
        QCOMPARE(m_opened.size(), 1);
        QVERIFY(!m_opened.first().hasQuery());
        QVERIFY(!m_opened.first().hasFragment());
        QCOMPARE(m_opened.first().toLocalFile(), QStringLiteral("/tmp/x.txt"));
    }

    void emojiBeforeALinkAtTheItem() {
        Fixture f;
        QTRY_VERIFY(f.ready());
        f.feed("\xF0\x9F\x98\x80 https://example.com/x\r\n");
        f.click(f.at(2, 0), Qt::ControlModifier);   // das Leerzeichen davor
        QVERIFY(m_opened.isEmpty());
        f.click(f.at(3, 0), Qt::ControlModifier);   // das h
        QCOMPARE(m_opened.size(), 1);
    }

    void softWrappedLinkFromItsSecondRow() {
        Fixture f;
        QTRY_VERIFY(f.ready());
        const QByteArray url = "https://example.com/" + QByteArray(70, 'a');
        f.feed(url + "\r\n");
        f.click(f.at(2, 1), Qt::ControlModifier);
        QCOMPARE(m_opened.size(), 1);
        QCOMPARE(m_opened.first().toString(), QString::fromLatin1(url));
    }

    // --- Auswahl: die Regel erreicht das Item --------------------------------------------------
    void dragSelectsAndCopies() {
        Fixture f;
        QTRY_VERIFY(f.ready());
        f.feed("zeile A\r\nzeile B\r\n");
        f.drag(f.at(0, 1), f.at(6, 1));
        QVERIFY(f.item.hasSelection());
        QCOMPARE(f.copied(), QStringLiteral("zeile B"));
    }

    void selectionFollowsScrollingText() {
        Fixture f;
        QTRY_VERIFY(f.ready());
        f.feed("zeile A\r\nzeile B\r\n");
        f.drag(f.at(0, 1), f.at(6, 1));
        for (int i = 0; i < 30; ++i) f.feed("mehr\r\n");
        QVERIFY(f.item.hasSelection());
        QCOMPARE(f.copied(), QStringLiteral("zeile B"));
    }

    void selectionKeepsItsTextAtTheScrollbackLimit() {
        // Oberhalb von 10000 Zeilen fiel je neuer Zeile die älteste heraus, die absolute Zählung
        // rutschte um eins, und die Auswahl lag auf der Nachbarzeile.
        Fixture f;
        QTRY_VERIFY(f.ready());
        for (int i = 0; i < 10050; ++i)
            f.feed(QByteArray("zeile ") + QByteArray::number(i) + "\r\n");
        f.drag(f.at(0, 2), f.at(20, 2));
        const QString before = f.copied();
        QVERIFY(before.startsWith(QStringLiteral("zeile")));
        for (int i = 0; i < 50; ++i) f.feed("neu\r\n");
        QVERIFY(f.item.hasSelection());
        QCOMPARE(f.copied(), before);
    }

    void reflowThatMovesTextDropsTheSelection() {
        Fixture f;
        QTRY_VERIFY(f.ready());
        f.session.screen()->setReflow(true);
        f.feed(QByteArray(100, 'x') + "\r\nzeile B\r\nzeile C\r\n");
        f.drag(f.at(0, 2), f.at(6, 2));
        QCOMPARE(f.copied(), QStringLiteral("zeile B"));
        f.item.setWidth(f.item.cellWidth() * 30 + 1);
        QTRY_COMPARE(f.session.screen()->cols(), 30);
        const QString now = f.copied();
        QVERIFY2(now.isEmpty() || now == QStringLiteral("zeile B"), qPrintable(now));
        QVERIFY(!f.item.hasSelection());
    }

    void altScreenDropsTheSelection() {
        Fixture f;
        QTRY_VERIFY(f.ready());
        f.feed("zeile A\r\n");
        f.drag(f.at(0, 0), f.at(6, 0));
        f.feed("\033[?1049h\033[1;1Hvollbild-text");
        QVERIFY(!f.item.hasSelection());
        QCOMPARE(f.copied(), QString());
    }

    void repaintRemovesTheSelection() {
        Fixture f;
        QTRY_VERIFY(f.ready());
        f.feed("zeile A\r\nzeile B\r\n");
        f.drag(f.at(0, 1), f.at(6, 1));
        QSignalSpy changed(&f.item, &TerminalItem::selectionChanged);
        f.feed("\033[2;1H\033[2Kganz anderer text");
        QVERIFY(!f.item.hasSelection());
        QCOMPARE(changed.count(), 1);
        QCOMPARE(f.copied(), QString());
    }

    void repaintDuringADragEndsIt() {
        Fixture f;
        QTRY_VERIFY(f.ready());
        f.feed("zeile A\r\nzeile B\r\n");
        f.mouse(QEvent::MouseButtonPress, f.at(0, 1));
        f.mouse(QEvent::MouseMove, f.at(6, 1));
        QVERIFY(f.item.hasSelection());
        f.feed("\033[2;1H\033[2Kganz anderer text");
        QVERIFY(!f.item.hasSelection());
        f.mouse(QEvent::MouseMove, f.at(9, 1));      // Ziehen läuft ins Leere
        QVERIFY(!f.item.hasSelection());
        f.mouse(QEvent::MouseButtonRelease, f.at(9, 1));
        QVERIFY(!f.item.hasSelection());
    }

    void changingTheSessionDropsTheSelection() {
        Fixture f;
        QTRY_VERIFY(f.ready());
        f.feed("zeile A\r\n");
        f.drag(f.at(0, 0), f.at(6, 0));
        QVERIFY(f.item.hasSelection());
        Session other;
        other.attachBackend(new FakeBackend, Session::Type::Shell, 80, 24);
        other.screen()->inputWrite("fremder text\r\n");
        f.item.setSession(&other);
        QVERIFY(!f.item.hasSelection());
        QCOMPARE(f.copied(), QString());
        f.item.setSession(nullptr);
    }

    void selectAllSurvivesOutput() {
        Fixture f;
        QTRY_VERIFY(f.ready());
        f.feed("zeile A\r\n");
        f.item.selectAll();
        f.feed("\033[1;1H\033[2Kanders");
        QVERIFY(f.item.hasSelection());
        QVERIFY(f.copied().contains(QStringLiteral("anders")));
    }

    void scrolledViewStaysPutAtTheScrollbackLimit() {
        // An der Grenze steht scrollbackCount() still. Die Ansicht rückte dann nicht mehr nach und
        // lief unter dem Anwender weg, samt allem, was er gerade ansieht.
        Fixture f;
        QTRY_VERIFY(f.ready());
        for (int i = 0; i < 10050; ++i)
            f.feed(QByteArray("zeile ") + QByteArray::number(i) + "\r\n");
        QVERIFY(f.session.screen()->scrollbackDropped() > 0);
        QWheelEvent wheel(f.at(5, 5), f.at(5, 5), QPoint(), QPoint(0, 120), Qt::NoButton,
                          Qt::NoModifier, Qt::NoScrollPhase, false);
        for (int i = 0; i < 20; ++i) QCoreApplication::sendEvent(&f.item, &wheel);
        f.drag(f.at(0, 0), f.at(10, 0));
        const QString top = f.copied();
        QVERIFY(top.startsWith(QStringLiteral("zeile")));
        for (int i = 0; i < 5; ++i) f.feed("neu\r\n");
        f.drag(f.at(0, 0), f.at(10, 0));
        QCOMPARE(f.copied(), top);
    }
};

int main(int argc, char **argv) {
    if (!qEnvironmentVariableIsSet("QT_QPA_PLATFORM"))
        qputenv("QT_QPA_PLATFORM", "offscreen");
    QGuiApplication app(argc, argv);
    TestTerminalItem t;
    return QTest::qExec(&t, argc, argv);
}

#include "tst_terminalitem.moc"
