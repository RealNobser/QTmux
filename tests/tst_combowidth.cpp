// QTMUX-145: AppComboBox bringt ihre Breite selbst mit.
//
// Befund: Die Proxy-Art (HTTP/SOCKS5) stand in einer Zeile OHNE Layout-Breite und fiel
// zu einer 14-px-Pille zusammen — implicitWidth bestand nur aus dem Caret, der Text war
// unsichtbar, der Caret lag bei x = −10 links außerhalb. Ursache: Der Basic-Stil holt
// seine Mindestbreite aus dem Hintergrund (140 px), unser Hintergrund hat keine, und die
// Standard-Politik misst den Text über ein TextField, das implicitWidth 0 meldet.
//
// Der Test lädt die ECHTE qml/Ui/AppComboBox.qml (byte-gleich kopiert) in ein
// Attrappen-Modul „QTmux“ (Theme-Singleton + AppPopupBg als Stub, das Caret-SVG als
// Ressource) und prüft an einer Box ohne jede Layout-Breite:
//   1. der Caret ist geladen (Positivkontrolle — ohne Bild wäre jede Breite „genug“),
//   2. die Box ist so breit, dass der LÄNGSTE Eintrag ganz vor dem Caret endet, auch
//      wenn gerade der kürzere angezeigt wird,
//   3. der Caret liegt vollständig innerhalb der Box.
#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickItem>
#include <QTemporaryDir>
#include <QtTest>

class tst_combowidth : public QObject {
    Q_OBJECT

private:
    QTemporaryDir m_dir;

    bool writeFile(const QString &rel, const QByteArray &content) {
        QFile f(m_dir.filePath(rel));
        if (!f.open(QIODevice::WriteOnly))
            return false;
        return f.write(content) == content.size();
    }

private slots:
    void initTestCase() {
        QVERIFY(m_dir.isValid());
        QVERIFY(QDir(m_dir.path()).mkpath(QStringLiteral("QTmux")));
        QFile real(QStringLiteral(QTMUX_SOURCE_DIR "/qml/Ui/AppComboBox.qml"));
        QVERIFY2(real.open(QIODevice::ReadOnly), qPrintable(real.fileName()));
        QVERIFY(writeFile(QStringLiteral("QTmux/AppComboBox.qml"), real.readAll()));
        QVERIFY(writeFile(QStringLiteral("QTmux/qmldir"),
                          "module QTmux\n"
                          "singleton Theme 1.0 Theme.qml\n"
                          "AppPopupBg 1.0 AppPopupBg.qml\n"
                          "AppComboBox 1.0 AppComboBox.qml\n"));
        QVERIFY(writeFile(QStringLiteral("QTmux/Theme.qml"),
                          "pragma Singleton\nimport QtQuick\nQtObject {\n"
                          "  property color bgElevated: \"#ffffff\"\n"
                          "  property color accent: \"#2563eb\"\n"
                          "  property color border: \"#d0d0d0\"\n"
                          "  property color textBright: \"#111111\"\n"
                          "  property color textDim: \"#666666\"\n"
                          "  property color sidebarHover: \"#eeeeee\"\n"
                          "}\n"));
        QVERIFY(writeFile(QStringLiteral("QTmux/AppPopupBg.qml"),
                          "import QtQuick\nRectangle {}\n"));
    }

    void widestEntryFitsBeforeCaret() {
        QQmlEngine engine;
        engine.addImportPath(m_dir.path());
        QQmlComponent comp(&engine);
        // Wie die Proxy-Zeile: Box ohne Layout-Breite neben einem festen Feld; angezeigt
        // ist der KÜRZERE Eintrag, Platz braucht der längere.
        comp.setData("import QtQuick\n"
                     "import QtQuick.Layouts\n"
                     "import QTmux\n"
                     "Item {\n"
                     "  width: 800; height: 100\n"
                     "  RowLayout {\n"
                     "    AppComboBox { id: box; objectName: \"box\"\n"
                     "                  model: [\"HTTP\", \"SOCKS5\"]; currentIndex: 0 }\n"
                     "    Item { Layout.preferredWidth: 170; height: 32 }\n"
                     "  }\n"
                     "  Text { objectName: \"widest\"; text: \"SOCKS5\"; font: box.font }\n"
                     "}\n",
                     QUrl::fromLocalFile(m_dir.filePath(QStringLiteral("probe.qml"))));
        std::unique_ptr<QObject> root(comp.create());
        QVERIFY2(root, qPrintable(comp.errorString()));

        auto *box = root->findChild<QQuickItem *>(QStringLiteral("box"));
        auto *widest = root->findChild<QQuickItem *>(QStringLiteral("widest"));
        QVERIFY(box);
        QVERIFY(widest);
        auto *indicator = box->property("indicator").value<QQuickItem *>();
        auto *content = box->property("contentItem").value<QQuickItem *>();
        QVERIFY(indicator);
        QVERIFY(content);
        // Das SVG lädt asynchron erst, wenn ein Fenster rendert — hier genügt die
        // gesetzte sourceSize; ohne geladene Größe misst der Test nichts.
        QTRY_VERIFY2(indicator->width() > 0, "Caret ohne Breite — Messung wertlos");

        const qreal textW = widest->implicitWidth();
        QVERIFY2(textW > 20, "Breitenprobe des längsten Eintrags unplausibel");
        const qreal textEnd = box->property("leftPadding").toReal()
                              + content->property("leftPadding").toReal() + textW;
        const QString diag = QStringLiteral("box.width=%1 indicator.x=%2 indicator.width=%3 "
                                            "Textende=%4 (Text %5)")
                                 .arg(box->width()).arg(indicator->x()).arg(indicator->width())
                                 .arg(textEnd).arg(textW);
        qInfo().noquote() << diag;
        QVERIFY2(indicator->x() >= textEnd, qPrintable(QStringLiteral("Längster Eintrag läuft "
                                                                      "unter den Caret: ") + diag));
        QVERIFY2(indicator->x() >= 0 && indicator->x() + indicator->width() <= box->width(),
                 qPrintable(QStringLiteral("Caret liegt außerhalb der Box: ") + diag));
    }
};

int main(int argc, char **argv) {
    if (!qEnvironmentVariableIsSet("QT_QPA_PLATFORM"))
        qputenv("QT_QPA_PLATFORM", "offscreen");
    QGuiApplication app(argc, argv);
    tst_combowidth t;
    return QTest::qExec(&t, argc, argv);
}

#include "tst_combowidth.moc"
