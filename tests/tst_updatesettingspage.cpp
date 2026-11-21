// Seite „Updates“ (qml/prefs/CatUpdates.qml) am geladenen QML mit der echten Bridge.
//
// Familienregel: Nichts geht still verloren. Ein Proxy-Feld, in dem der Anwender gerade
// tippt (Fokus, noch nicht bestätigt), wird nicht überschrieben, wenn sich ANDERSWO eine
// Einstellung ändert (Schalter, Proxy-Art). Nur Import und „Zurücksetzen“ ersetzen die
// Felder bewusst. Schließen des Einstellungsfensters übernimmt das Feld mit Fokus.

#include "UpdateBridge.h"

#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickItem>
#include <QQuickWindow>
#include <QSettings>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>
#include <QtQml/QQmlExtensionPlugin>

#include <memory>

Q_IMPORT_QML_PLUGIN(QTmuxPlugin)

class TestUpdateSettingsPage : public QObject {
    Q_OBJECT

    QTemporaryDir m_settingsDir;

    static qtmux::UpdateBridge::Wiring wiring()
    {
        qtmux::UpdateBridge::Wiring w;
        w.language = QStringLiteral("en");
        w.requestQuit = [] { return false; };
        return w;
    }

    struct Page {
        std::unique_ptr<qtmux::UpdateBridge> bridge;
        std::unique_ptr<QQmlApplicationEngine> engine;
        QQuickWindow *window = nullptr;
        QQuickItem *host = nullptr;
        QQuickItem *user = nullptr;
    };

    static void load(Page &p, const QByteArray &qml)
    {
        p.bridge = std::make_unique<qtmux::UpdateBridge>(wiring());
        p.bridge->setProxyMode(2);   // manuell: Server-, Port- und Benutzerfeld sichtbar
        p.engine = std::make_unique<QQmlApplicationEngine>();
        p.engine->rootContext()->setContextProperty(QStringLiteral("Updates"), p.bridge.get());
        p.engine->loadData(qml);
        QVERIFY(!p.engine->rootObjects().isEmpty());
        p.window = qobject_cast<QQuickWindow *>(p.engine->rootObjects().first());
        QVERIFY(p.window);
        p.window->requestActivate();
        QVERIFY(QTest::qWaitForWindowActive(p.window));
        p.host = p.window->findChild<QQuickItem *>(QStringLiteral("updatesProxyHost"));
        p.user = p.window->findChild<QQuickItem *>(QStringLiteral("updatesProxyUser"));
        QVERIFY(p.host && p.user);
    }

    static const QByteArray &pageQml()
    {
        static const QByteArray q = R"qml(
            import QtQuick
            import QtQuick.Controls.Basic
            import QTmux
            ApplicationWindow {
                width: 900; height: 800; visible: true
                CatUpdates { anchors.fill: parent }
            })qml";
        return q;
    }

private slots:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        QVERIFY(m_settingsDir.isValid());
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, m_settingsDir.path());
        QCoreApplication::setOrganizationName(QStringLiteral("QTmuxTest"));
        QCoreApplication::setApplicationName(QStringLiteral("QTmux-updatesettingspage-test"));
    }
    void init() { QSettings().clear(); }

    void aFieldBeingTypedSurvivesAnotherChange()
    {
        Page p;
        load(p, pageQml());
        if (QTest::currentTestFailed()) return;
        p.host->forceActiveFocus();
        QTRY_VERIFY(p.host->hasActiveFocus());
        p.host->setProperty("text", QStringLiteral("proxy.typed.example"));
        // Eine andere Einstellung ändert sich, ohne dass der Fokus das Feld verlässt.
        p.bridge->setAutoCheck(!p.bridge->autoCheck());
        QTest::qWait(20);
        QCOMPARE(p.host->property("text").toString(), QStringLiteral("proxy.typed.example"));
        // Ein Feld ohne Fokus folgt dem gespeicherten Wert weiter.
        p.bridge->setProxyUser(QStringLiteral("stored-user"));
        QTRY_COMPARE(p.user->property("text").toString(), QStringLiteral("stored-user"));
    }

    void importOrResetReplacesEvenTheFocusedField()
    {
        Page p;
        load(p, pageQml());
        if (QTest::currentTestFailed()) return;
        p.bridge->setProxyHost(QStringLiteral("stored.example"));
        p.host->forceActiveFocus();
        QTRY_VERIFY(p.host->hasActiveFocus());
        p.host->setProperty("text", QStringLiteral("proxy.typed.example"));
        p.bridge->restoreDefaults();
        QTRY_COMPARE(p.host->property("text").toString(), QString());
    }

    void closingTheWindowTakesOverTheFocusedField()
    {
        Page p;
        load(p, R"qml(
            import QtQuick
            import QtQuick.Controls.Basic
            import QTmux
            ApplicationWindow {
                id: w
                width: 900; height: 800; visible: true
                CatUpdates { anchors.fill: parent }
                CommitOnClose { window: w }
            })qml");
        if (QTest::currentTestFailed()) return;
        p.host->forceActiveFocus();
        QTRY_VERIFY(p.host->hasActiveFocus());
        p.host->setProperty("text", QStringLiteral("proxy.closed.example"));
        p.window->close();
        QTRY_COMPARE(p.bridge->proxyHost(), QStringLiteral("proxy.closed.example"));
    }
};

QTEST_MAIN(TestUpdateSettingsPage)
#include "tst_updatesettingspage.moc"
