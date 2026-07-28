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
    void resumeModeNoneNeverTouches();
    void resumeModePickOnlyWhereSupported();
    void resumeModeReportedNeedsRef();
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
    QCOMPARE(a->resumeLastArgs, QStringLiteral("--continue"));
}

// Das Fortsetzungs-Argument gehört DIREKT hinter den Kommando-Token, nicht ans Ende —
// sonst bräche eine Subkommando-Form, und Präfixe (env/Pfad) dürfen nicht verrutschen.
void TestAgent::resumeInsertsAfterCommandToken() {
    const auto L = ResumeMode::Last;
    QCOMPARE(AgentRegistry::resumeCommand("claude", L),
             QStringLiteral("claude --continue"));
    QCOMPARE(AgentRegistry::resumeCommand("claude --dangerously-skip-permissions", L),
             QStringLiteral("claude --continue --dangerously-skip-permissions"));
    QCOMPARE(AgentRegistry::resumeCommand("env FOO=1 agy --model x", L),
             QStringLiteral("env FOO=1 agy --continue --model x"));
    QCOMPARE(AgentRegistry::resumeCommand("/usr/local/bin/hermes chat", L),
             QStringLiteral("/usr/local/bin/hermes --continue chat"));
}

// Zweimaliges Anwenden darf nichts aufaddieren — sonst sammelt sich über mehrere
// Neustarts "--continue --continue …" an.
void TestAgent::resumeIsIdempotent() {
    const auto L = ResumeMode::Last;
    const QString once = AgentRegistry::resumeCommand("claude --model opus", L);
    QCOMPARE(AgentRegistry::resumeCommand(once, L), once);
    QCOMPARE(AgentRegistry::resumeCommand("claude --continue", L),
             QStringLiteral("claude --continue"));
}

// Kein Agent bzw. keine verifizierte Vorlage -> Zeile bleibt unangetastet (der Agent
// startet dann eben frisch, statt an einem geratenen Argument zu scheitern).
void TestAgent::resumeLeavesUnknownAndFlagless() {
    const auto L = ResumeMode::Last;
    QCOMPARE(AgentRegistry::resumeCommand("ls -la", L), QStringLiteral("ls -la"));
    QCOMPARE(AgentRegistry::resumeCommand("", L), QString());
    QVERIFY(AgentRegistry::detect("gemini")->resumeLastArgs.isEmpty());
    QCOMPARE(AgentRegistry::resumeCommand("gemini --foo", L), QStringLiteral("gemini --foo"));
}

// QTMUX-98: Modus None laesst die Zeile immer unberuehrt - sonst liefe beim
// abgeschalteten Fortsetzen doch etwas mit.
void TestAgent::resumeModeNoneNeverTouches() {
    QCOMPARE(AgentRegistry::resumeCommand("claude --foo", ResumeMode::None),
             QStringLiteral("claude --foo"));
    QCOMPARE(AgentRegistry::resumeCommand("claude", ResumeMode::None, "abc"),
             QStringLiteral("claude"));
}

// QTMUX-98: Auswahl beim Start. Einen dokumentierten Picker hat bislang nur Claude
// Code - die anderen duerfen deshalb NICHT auf einen anderen Weg ausweichen.
void TestAgent::resumeModePickOnlyWhereSupported() {
    const auto P = ResumeMode::Pick;
    QCOMPARE(AgentRegistry::resumeCommand("claude --model opus", P),
             QStringLiteral("claude --resume --model opus"));
    QVERIFY(AgentRegistry::supportsResumeMode(*AgentRegistry::detect("claude"), P));

    for (const char *cmd : {"agy", "opencode", "hermes"}) {
        const AgentInfo *a = AgentRegistry::detect(cmd);
        QVERIFY(a != nullptr);
        QVERIFY2(!AgentRegistry::supportsResumeMode(*a, P), cmd);
        QCOMPARE(AgentRegistry::resumeCommand(QString::fromLatin1(cmd), P),
                 QString::fromLatin1(cmd));      // unveraendert = frischer Start
    }
}

// QTMUX-98: gemeldete Sitzung. Der Platzhalter {id} wird ersetzt; OHNE Referenz wird
// die Zeile NICHT angefasst - lieber frisch starten als eine fremde Unterhaltung.
void TestAgent::resumeModeReportedNeedsRef() {
    const auto R = ResumeMode::Reported;
    QCOMPARE(AgentRegistry::resumeCommand("claude", R, "abc-123"),
             QStringLiteral("claude --resume abc-123"));
    QCOMPARE(AgentRegistry::resumeCommand("agy --model x", R, "cid-9"),
             QStringLiteral("agy --conversation cid-9 --model x"));
    QCOMPARE(AgentRegistry::resumeCommand("opencode", R, "s7"),
             QStringLiteral("opencode --session s7"));
    QCOMPARE(AgentRegistry::resumeCommand("hermes", R, "h1"),
             QStringLiteral("hermes --resume h1"));

    // Ohne Referenz bzw. ohne Vorlage: unveraendert.
    QCOMPARE(AgentRegistry::resumeCommand("claude", R), QStringLiteral("claude"));
    QCOMPARE(AgentRegistry::resumeCommand("claude", R, "   "), QStringLiteral("claude"));
    QCOMPARE(AgentRegistry::resumeCommand("gemini", R, "x"), QStringLiteral("gemini"));
}

QTEST_MAIN(TestAgent)
#include "tst_agent.moc"
