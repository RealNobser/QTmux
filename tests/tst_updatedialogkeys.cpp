// Tastatur im Update-Dialog (qml/dialogs/UpdateDialog.qml) am geladenen QML.
//
// Regel (wie im Widgets-Dialog des Hubs): Steht der Tastaturfokus auf einem Knopf mit
// einer Aktion des Ablaufs, löst Enter genau DIESEN Knopf aus. Steht er auf einem
// Knopf ohne Aktion des Ablaufs („Details“, „Kopieren“), tut Enter nichts. Nie löst
// Enter mit Fokus auf einem anderen Knopf die Hauptaktion des Zustands aus (Befund:
// Tab auf „Später“ im Zustand „Bereit“, Enter beendete QTmux und installierte).
//
// Aufbau: Der Dialog wird aus dem Modul „QTmux“ geladen. `Updates` ist ein kleiner
// Stellvertreter: `flow` ist ein ECHTER Ablauf in der jeweiligen Szene des Hubs
// (appupdate::UpdateShowcase), `perform()` merkt sich nur die Aktion. So sieht der
// Dialog dieselbe `view` wie in der App, und keine Aktion verändert etwas.


#include "update/UpdateFlow.hpp"
#include "update/UpdateShowcase.hpp"
#include "update/UpdateTexts.hpp"

#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickItem>
#include <QQuickWindow>
#include <QSignalSpy>
#include <QTest>
#include <QtQml/QQmlExtensionPlugin>

#include <memory>

Q_IMPORT_QML_PLUGIN(QTmuxPlugin)

namespace {

class FakeUpdates : public QObject {
    Q_OBJECT
    Q_PROPERTY(QObject *flow READ flowObject CONSTANT)
    Q_PROPERTY(int textRevision READ textRevision CONSTANT)
    Q_PROPERTY(QString notesHighlight READ notesHighlight CONSTANT)

public:
    explicit FakeUpdates(appupdate::UpdateFlow *flow) : m_flow(flow) {}
    QObject *flowObject() const { return m_flow; }
    int textRevision() const { return 0; }
    QString notesHighlight() const { return {}; }

    Q_INVOKABLE QString text(const QString &key, const QVariantMap &args = {}) const
    {
        QHash<QString, QString> a;
        for (auto it = args.constBegin(); it != args.constEnd(); ++it)
            a.insert(it.key(), it.value().toString());
        const QByteArray k = key.toLatin1();
        return m_flow->texts().get(k.constData(), a);
    }
    Q_INVOKABLE QString iconSource(const QString &name) const
    {
        return name.isEmpty() ? QString() : QStringLiteral("qrc:/icons/%1.svg").arg(name);
    }
    Q_INVOKABLE bool perform(int action)
    {
        performed << action;
        return true;
    }
    Q_INVOKABLE void setDialogVisible(bool) {}

    QList<int> performed;

private:
    appupdate::UpdateFlow *m_flow;
};

void collectButtons(QQuickItem *item, QList<QQuickItem *> &out)
{
    if (!item) return;
    if (item->property("flowAction").isValid() && item->property("role").isValid()
        && item->isVisible())
        out << item;
    for (QQuickItem *c : item->childItems()) collectButtons(c, out);
}

} // namespace

class TestUpdateDialogKeys : public QObject {
    Q_OBJECT

private slots:
    void enterPressesTheFocusedButtonOnly_data()
    {
        QTest::addColumn<QString>("scene");
        for (const QString &s : appupdate::UpdateShowcase::scenes())
            if (appupdate::UpdateShowcase::surfaceFor(s) == QLatin1String("dialog"))
                QTest::newRow(qPrintable(s)) << s;
    }

    void enterPressesTheFocusedButtonOnly()
    {
        QFETCH(QString, scene);
        appupdate::UpdateShowcase::Options o;
        o.productSlug = QStringLiteral("qtmux");
        o.productName = QStringLiteral("QTmux");
        o.installed = QStringLiteral("1.9.15");
        o.language = QStringLiteral("en");
        o.impact.runningSessions = 1;
        appupdate::UpdateShowcase showcase(o);
        bool reached = false, ok = false;
        QVERIFY(showcase.enter(scene, [&](bool r) { reached = true; ok = r; }));
        QTRY_VERIFY_WITH_TIMEOUT(reached, 30000);
        QVERIFY2(ok, "Szene nicht erreicht");

        FakeUpdates fake(showcase.flow());
        QQmlApplicationEngine engine;
        engine.rootContext()->setContextProperty(QStringLiteral("Updates"), &fake);
        engine.loadData(R"qml(
            import QtQuick
            import QtQuick.Controls.Basic
            import QTmux
            ApplicationWindow {
                width: 1100; height: 720; visible: true
                property alias dialog: d
                UpdateDialog { id: d }
            })qml");
        QCOMPARE(engine.rootObjects().size(), 1);
        auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
        QVERIFY(window);
        window->requestActivate();
        QVERIFY(QTest::qWaitForWindowActive(window));
        QObject *dialog = window->property("dialog").value<QObject *>();
        QVERIFY(dialog);
        QMetaObject::invokeMethod(dialog, "open");
        QTRY_VERIFY(dialog->property("opened").toBool());

        const QVariantMap view = showcase.flow()->viewMap();
        const int enterAction = view.value(QStringLiteral("enterAction")).toInt();
        QList<QQuickItem *> buttons;
        collectButtons(qobject_cast<QQuickItem *>(dialog->property("contentItem").value<QObject *>()),
                       buttons);
        QVERIFY2(!buttons.isEmpty(), "keine Knöpfe im Dialog gefunden");

        int checked = 0;
        for (QQuickItem *b : buttons) {
            const int own = b->property("flowAction").toInt();
            const QString label = b->property("text").toString();
            b->forceActiveFocus(Qt::TabFocusReason);
            QTRY_VERIFY2(b->hasActiveFocus(), qPrintable(label));
            fake.performed.clear();
            QTest::keyClick(window, Qt::Key_Return);
            QTest::qWait(20);
            const QList<int> expected = own > 0 ? QList<int>{own} : QList<int>{};
            QVERIFY2(fake.performed == expected,
                     qPrintable(QStringLiteral("Szene %1, Fokus auf „%2“ (Aktion %3): Enter löste %4 "
                                               "aus, erwartet %5 (Hauptaktion des Zustands: %6)")
                                    .arg(scene, label)
                                    .arg(own)
                                    .arg(fake.performed.isEmpty()
                                             ? QStringLiteral("nichts")
                                             : QString::number(fake.performed.first()))
                                    .arg(own > 0 ? QString::number(own) : QStringLiteral("nichts"))
                                    .arg(enterAction)));
            ++checked;
        }
        QVERIFY(checked > 0);

        // Kein Fall „ohne Fokus im Dialog“: Solange der modale Dialog offen ist, bleibt
        // der Fokus in ihm (gemessen: nach forceActiveFocus() auf das Fenster traf Enter
        // weiter den zuletzt fokussierten Knopf). Die Enter-Aktion des Zustands wirkt
        // darum über den Hauptknopf, der beim Öffnen den Fokus bekommt (focusPrimary).
    }
};

QTEST_MAIN(TestUpdateDialogKeys)
#include "tst_updatedialogkeys.moc"
