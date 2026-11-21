// Die Beenden-Rückfrage entfällt NUR auf dem Update-Weg (src/app/UpdateQuit.cpp).
//
// `quitForUpdate()` setzt `quitConfirmed`, damit `onClosing` nicht fragt, und schließt
// das Hauptfenster. Lehnt das Fenster ab, muss `quitConfirmed` wieder auf seinen
// alten Wert, sonst beendete das NÄCHSTE normale Schließen QTmux ohne Rückfrage.
// Der echte Beenden-Weg (tst_updatequit) kommt an diesen Zweig nicht heran: Main.qml
// lehnt auf dem Update-Weg nie ab. Darum hier ein kleines Fenster mit demselben
// Vertrag (`confirmQuit`, `quitConfirmed`, `onClosing`), das auf Wunsch ablehnt.

#include "UpdateQuit.h"

#include <QQmlApplicationEngine>
#include <QQuickWindow>
#include <QTest>

class TestUpdateQuitReset : public QObject {
    Q_OBJECT

private slots:
    void aRefusedUpdateQuitLeavesTheQuestionInPlace()
    {
        QQmlApplicationEngine engine;
        engine.loadData(R"qml(
            import QtQuick
            Window {
                width: 200; height: 100; visible: true
                property bool confirmQuit: true
                property bool quitConfirmed: false
                property bool refuseAll: true   // z. B. eine andere offene Arbeit
                property int asked: 0
                onClosing: (close) => {
                    if (refuseAll) { close.accepted = false; return }
                    if (confirmQuit && !quitConfirmed) { asked++; close.accepted = false }
                }
            })qml");
        QCOMPARE(engine.rootObjects().size(), 1);
        auto *win = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
        QVERIFY(win);
        QVERIFY(QTest::qWaitForWindowExposed(win));

        // Update-Weg, das Fenster lehnt ab: false, und die Rückfrage ist wieder scharf.
        QVERIFY(!qtmux::quitForUpdate(engine));
        QVERIFY(win->isVisible());
        QCOMPARE(win->property("quitConfirmed").toBool(), false);

        // Danach ein normales Schließen: Es fragt.
        win->setProperty("refuseAll", false);
        QVERIFY(!win->close());
        QCOMPARE(win->property("asked").toInt(), 1);
        QVERIFY(win->isVisible());
    }
};

QTEST_MAIN(TestUpdateQuitReset)
#include "tst_updatequitreset.moc"
