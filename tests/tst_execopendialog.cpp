// QTMUX-151: Die Rückfrage vor dem Öffnen einer ausführbaren Datei — der Dialog selbst.
//
// Geprüft wird, was der Owner festgelegt hat und was sich leicht still verdreht:
//   1. Die Vorgabe ist ABBRECHEN. Enter bestätigt nicht (AppDialog kennt „Öffnen“ nicht als
//      Bestätigen-Knopf), der Shortcut des Dialogs lehnt ab.
//   2. Es gibt genau „Öffnen“ und „Abbrechen“, kein „nicht mehr fragen“.
//   3. Der vollständige Pfad steht da, umbrochen statt gekürzt.
//
// Der Test lädt die ECHTEN qml/Ui/AppDialog.qml und qml/dialogs/ExecOpenDialog.qml (byte-gleich
// kopiert) in ein Attrappen-Modul „QTmux“ mit einem Theme-Stub, wie test_combowidth. Tasten
// schickt er nicht: Ob ein Shortcut ohne aktives Fenster feuert, hängt an der Plattform. Er löst
// stattdessen das Signal des Shortcuts aus und prüft, was daran hängt.
#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

class tst_execopendialog : public QObject {
    Q_OBJECT

private:
    QTemporaryDir m_dir;

    bool writeFile(const QString &rel, const QByteArray &content) {
        QFile f(m_dir.filePath(rel));
        if (!f.open(QIODevice::WriteOnly))
            return false;
        return f.write(content) == content.size();
    }
    bool copyReal(const QString &source, const QString &rel) {
        QFile real(QStringLiteral(QTMUX_SOURCE_DIR "/") + source);
        return real.open(QIODevice::ReadOnly) && writeFile(rel, real.readAll());
    }

    // Standardknöpfe wie in QQuickDialogButtonBox / QPlatformDialogHelper.
    enum : int { Ok = 0x400, Open = 0x2000, Save = 0x800, Yes = 0x4000, Cancel = 0x400000 };

private slots:
    void initTestCase() {
        QVERIFY(m_dir.isValid());
        QVERIFY(QDir(m_dir.path()).mkpath(QStringLiteral("QTmux")));
        QVERIFY(copyReal(QStringLiteral("qml/Ui/AppDialog.qml"), QStringLiteral("QTmux/AppDialog.qml")));
        QVERIFY(copyReal(QStringLiteral("qml/dialogs/ExecOpenDialog.qml"),
                         QStringLiteral("QTmux/ExecOpenDialog.qml")));
        QVERIFY(writeFile(QStringLiteral("QTmux/qmldir"),
                          "module QTmux\n"
                          "singleton Theme 1.0 Theme.qml\n"
                          "AppDialog 1.0 AppDialog.qml\n"
                          "ExecOpenDialog 1.0 ExecOpenDialog.qml\n"));
        QVERIFY(writeFile(QStringLiteral("QTmux/Theme.qml"),
                          "pragma Singleton\nimport QtQuick\nQtObject {\n"
                          "  property color bgElevated: \"#ffffff\"\n"
                          "  property color border: \"#d0d0d0\"\n"
                          "  property color textBright: \"#111111\"\n"
                          "}\n"));
    }

    void cancelIsTheDefault() {
        QQmlEngine engine;
        engine.addImportPath(m_dir.path());
        QQmlComponent comp(&engine);
        comp.setData("import QtQuick\n"
                     "import QTmux\n"
                     "Window {\n"
                     "  width: 800; height: 600; visible: true\n"
                     "  property alias dialog: dlg\n"
                     "  ExecOpenDialog { id: dlg; objectName: \"dlg\" }\n"
                     "}\n",
                     QUrl::fromLocalFile(m_dir.filePath(QStringLiteral("probe.qml"))));
        QScopedPointer<QObject> root(comp.create());
        QVERIFY2(root, qPrintable(comp.errorString()));
        QObject *dlg = root->property("dialog").value<QObject *>();
        QVERIFY(dlg);

        // Genau zwei Knöpfe, und keiner davon gilt AppDialog als Bestätigen-Knopf: Dessen
        // eigener Enter-Shortcut bleibt damit aus.
        const int buttons = dlg->property("standardButtons").toInt();
        QCOMPARE(buttons, int(Open) | int(Cancel));
        QCOMPARE(dlg->property("hasAccept").toBool(), false);
        QVERIFY(dlg->property("modal").toBool());

        const QString path = QStringLiteral("C:/Users/jemand/Ein sehr langer Ordnername mit Leerzeichen/")
            + QString(120, QLatin1Char('x')) + QStringLiteral("/werkzeug.bat");
        QVERIFY(dlg->setProperty("path", path));

        QSignalSpy accepted(dlg, SIGNAL(accepted()));
        QSignalSpy rejected(dlg, SIGNAL(rejected()));
        QVERIFY(QMetaObject::invokeMethod(dlg, "open"));
        QTRY_VERIFY(dlg->property("opened").toBool());

        // Der vollständige Pfad, umbrochen, nicht gekürzt.
        QObject *label = dlg->findChild<QObject *>(QStringLiteral("pathLabel"));
        QVERIFY(label);
        QCOMPARE(label->property("text").toString(), path);
        QCOMPARE(label->property("elide").toInt(), int(Qt::ElideNone));
        QCOMPARE(label->property("truncated").toBool(), false);
        QVERIFY(label->property("lineCount").toInt() > 1);

        // Enter lehnt ab. Der Shortcut ist scharf, solange der Dialog sichtbar ist.
        QObject *enter = dlg->findChild<QObject *>(QStringLiteral("enterRejects"));
        QVERIFY(enter);
        QCOMPARE(enter->property("enabled").toBool(), true);
        const QVariantList keys = enter->property("sequences").toList();
        QCOMPARE(keys.size(), 2);
        QVERIFY(QMetaObject::invokeMethod(enter, "activated"));
        QCOMPARE(rejected.count(), 1);
        QCOMPARE(accepted.count(), 0);
        QTRY_VERIFY(!dlg->property("visible").toBool());
        QCOMPARE(enter->property("enabled").toBool(), false);

        // Gegenprobe: „Öffnen“ bestätigt, und zwar nur das.
        QVERIFY(QMetaObject::invokeMethod(dlg, "open"));
        QTRY_VERIFY(dlg->property("opened").toBool());
        QVERIFY(QMetaObject::invokeMethod(dlg, "accept"));
        QCOMPARE(accepted.count(), 1);
        QCOMPARE(rejected.count(), 1);
    }
};

int main(int argc, char **argv) {
    // Ohne Display lauffähig (CI, SSH): offscreen, sofern nichts anderes verlangt ist.
    if (!qEnvironmentVariableIsSet("QT_QPA_PLATFORM"))
        qputenv("QT_QPA_PLATFORM", "offscreen");
    QGuiApplication app(argc, argv);
    tst_execopendialog t;
    return QTest::qExec(&t, argc, argv);
}
#include "tst_execopendialog.moc"
