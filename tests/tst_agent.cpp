#include <QtTest>
#include "AgentRegistry.h"

using namespace qtmux;

class TestAgent : public QObject {
    Q_OBJECT
private slots:
    void detectsAgy();
    void detectsWithPathAndEnv();
    void ignoresUnknown();
};

// "agy" (Google AntiGravity) muss erkannt werden.
void TestAgent::detectsAgy() {
    const AgentInfo *a = AgentRegistry::detect("agy");
    QVERIFY(a != nullptr);
    QCOMPARE(a->id, QStringLiteral("antigravity"));
    QCOMPARE(a->displayName, QStringLiteral("AntiGravity"));
}

// Pfade und Umgebungs-Präfixe dürfen die Erkennung nicht verhindern.
void TestAgent::detectsWithPathAndEnv() {
    QVERIFY(AgentRegistry::detect("/usr/local/bin/agy --resume") != nullptr);
    QVERIFY(AgentRegistry::detect("env FOO=1 agy") != nullptr);
    QCOMPARE(AgentRegistry::detect("claude")->id, QStringLiteral("claude"));
}

// Normale Kommandos sind keine Agenten.
void TestAgent::ignoresUnknown() {
    QCOMPARE(AgentRegistry::detect("ls -la"), nullptr);
    QCOMPARE(AgentRegistry::detect("vim file.txt"), nullptr);
    QCOMPARE(AgentRegistry::detect(""), nullptr);
}

QTEST_MAIN(TestAgent)
#include "tst_agent.moc"
