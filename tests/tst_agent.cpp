#include <QtTest>
#include "AgentRegistry.h"

using namespace qtmux;

class TestAgent : public QObject {
    Q_OBJECT
private slots:
    void detectsAgy();
    void detectsWithPathAndEnv();
    void ignoresUnknown();
    void detectsHermes();
    void resumeInsertsAfterCommandToken();
    void resumeIsIdempotent();
    void resumeLeavesUnknownAndFlagless();
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

// Hermes (QTMUX-85) gehört zu den bekannten Agenten.
void TestAgent::detectsHermes() {
    const AgentInfo *a = AgentRegistry::detect("hermes");
    QVERIFY(a != nullptr);
    QCOMPARE(a->id, QStringLiteral("hermes"));
    QCOMPARE(a->displayName, QStringLiteral("Hermes"));
    QCOMPARE(a->resumeArgs, QStringLiteral("--continue"));
}

// Das Fortsetzungs-Argument gehört DIREKT hinter den Kommando-Token, nicht ans Ende —
// sonst bräche eine Subkommando-Form, und Präfixe (env/Pfad) dürfen nicht verrutschen.
void TestAgent::resumeInsertsAfterCommandToken() {
    QCOMPARE(AgentRegistry::resumeCommand("claude"),
             QStringLiteral("claude --continue"));
    QCOMPARE(AgentRegistry::resumeCommand("claude --dangerously-skip-permissions"),
             QStringLiteral("claude --continue --dangerously-skip-permissions"));
    QCOMPARE(AgentRegistry::resumeCommand("env FOO=1 agy --model x"),
             QStringLiteral("env FOO=1 agy --continue --model x"));
    QCOMPARE(AgentRegistry::resumeCommand("/usr/local/bin/hermes chat"),
             QStringLiteral("/usr/local/bin/hermes --continue chat"));
}

// Zweimaliges Anwenden darf nichts aufaddieren — sonst sammelt sich über mehrere
// Neustarts "--continue --continue …" an.
void TestAgent::resumeIsIdempotent() {
    const QString once = AgentRegistry::resumeCommand("claude --model opus");
    QCOMPARE(AgentRegistry::resumeCommand(once), once);
    QCOMPARE(AgentRegistry::resumeCommand("claude --continue"),
             QStringLiteral("claude --continue"));
}

// Kein Agent bzw. kein verifiziertes Flag -> Zeile bleibt unangetastet (der Agent
// startet dann eben frisch, statt an einem geratenen Argument zu scheitern).
void TestAgent::resumeLeavesUnknownAndFlagless() {
    QCOMPARE(AgentRegistry::resumeCommand("ls -la"), QStringLiteral("ls -la"));
    QCOMPARE(AgentRegistry::resumeCommand(""), QString());
    QVERIFY(AgentRegistry::detect("gemini")->resumeArgs.isEmpty());
    QCOMPARE(AgentRegistry::resumeCommand("gemini --foo"), QStringLiteral("gemini --foo"));
}

QTEST_MAIN(TestAgent)
#include "tst_agent.moc"
