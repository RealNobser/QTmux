#include <QtTest>
#include <QSignalSpy>
#include "PluginHost.h"

using namespace qtmux;

// Plugin-System (QTMUX-8): lädt das echte Echo-Demo-Plugin aus <build>/plugins
// (Pfad via QTMUX_PLUGIN_DIR, von CMake als Test-Env gesetzt) und beweist die
// Kette QPluginLoader → PluginInterface → Backend-I/O.
class TestPlugins : public QObject {
    Q_OBJECT
private slots:
    void initTestCase();
    void loadsEchoPlugin();
    void backendRoundtrip();
    void unknownIdsFail();
};

void TestPlugins::initTestCase() {
    QVERIFY2(qEnvironmentVariableIsSet("QTMUX_PLUGIN_DIR"),
             "QTMUX_PLUGIN_DIR nicht gesetzt — Test braucht das gebaute Demo-Plugin");
    const int n = PluginHost::instance().loadAll();
    QVERIFY2(n >= 1, qPrintable(QStringLiteral("Kein Plugin geladen: %1")
                                    .arg(PluginHost::instance().lastError())));
}

// Host findet das Echo-Plugin und meldet seinen Backend-Typ.
void TestPlugins::loadsEchoPlugin() {
    QVERIFY(PluginHost::instance().pluginCount() >= 1);
    const QVariantList types = PluginHost::instance().backendTypesVariant();
    bool found = false;
    for (const QVariant &v : types) {
        const QVariantMap m = v.toMap();
        if (m.value("pluginId") == QStringLiteral("echo-demo")
            && m.value("typeId") == QStringLiteral("echo")) {
            found = true;
            QCOMPARE(m.value("name").toString(), QStringLiteral("Echo (Demo)"));
        }
    }
    QVERIFY2(found, "Backend-Typ echo-demo/echo nicht gemeldet");
    QCOMPARE(PluginHost::instance().backendTypeName("echo-demo", "echo"),
             QStringLiteral("Echo (Demo)"));

    // loadAll ist idempotent: zweiter Lauf lädt nichts erneut.
    QCOMPARE(PluginHost::instance().loadAll(), 0);
}

// Backend erzeugen, starten, Eingabe → Echo, Strg+D → Closed.
void TestPlugins::backendRoundtrip() {
    ITerminalBackend *b = PluginHost::instance().createBackend("echo-demo", "echo");
    QVERIFY(b);
    std::unique_ptr<ITerminalBackend> guard(b);

    QSignalSpy dataSpy(b, &ITerminalBackend::dataReceived);
    QSignalSpy stateSpy(b, &ITerminalBackend::stateChanged);
    QVERIFY(b->start(80, 24));
    QCOMPARE(b->state(), BackendState::Running);
    QVERIFY(dataSpy.count() >= 1);   // Banner

    b->write(QByteArrayLiteral("hallo\r"));
    QByteArray all;
    for (const auto &args : dataSpy) all += args.at(0).toByteArray();
    QVERIFY2(all.contains("hallo"), "Eingabe wurde nicht zurückgespiegelt");

    b->write(QByteArrayLiteral("\x04"));   // Strg+D beendet
    QCOMPARE(b->state(), BackendState::Closed);
    QVERIFY(stateSpy.count() >= 2);        // Running + Closed
}

// Unbekannte Plugin-/Typ-IDs liefern nullptr (Restore mit fehlendem Plugin).
void TestPlugins::unknownIdsFail() {
    QVERIFY(!PluginHost::instance().createBackend("gibts-nicht", "echo"));
    QVERIFY(!PluginHost::instance().createBackend("echo-demo", "gibts-nicht"));
    QVERIFY(PluginHost::instance().backendTypeName("echo-demo", "gibts-nicht").isEmpty());
}

QTEST_MAIN(TestPlugins)
#include "tst_plugins.moc"
