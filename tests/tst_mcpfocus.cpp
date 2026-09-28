#include <QtTest>
#include <QSettings>
#include <QJsonArray>
#include <QJsonObject>
#include "McpServer.h"
#include "SessionModel.h"
#include "WindowModel.h"

// Owner-Vorgabe 2026-09-28: Eine per MCP erzeugte Session darf den Fokus NICHT an sich
// ziehen — tippt der Mensch gerade in einer anderen Session, landeten seine Tastendrücke
// sonst in der neuen. Geprüft wird hier die C++-Hälfte des Vertrags: welche Signale
// callTool feuert und mit welchem focus-Wert. Die QML-Hälfte (openWindowWithSession mit
// activate=false) ist am laufenden Objekt nachgewiesen, s. docs/MCP.md.
//
// Serielle Sessions auf einem nicht existierenden Port: Das Model legt die Zeile trotzdem
// an (der Fehler kommt asynchron vom Backend), und es startet kein Prozess — der Test
// läuft damit auf allen drei Plattformen ohne PTY/ConPTY.
namespace qtmux {

class TestMcpFocus : public QObject {
    Q_OBJECT
private slots:
    void initTestCase();
    void init();
    void createSessionStaysInBackground();
    void createSessionFocusIsOptIn();
    void windowToolsDefaultToBackground();
    void schemaDocumentsFocus();

private:
    static QJsonObject serialArgs() {
        return QJsonObject{{"type", "serial"}, {"port", "qtmux-test-kein-port"}};
    }
    static QJsonObject call(McpServer &mcp, const QString &name, const QJsonObject &args,
                            bool &isError, QString &text) {
        isError = false;
        text.clear();
        return mcp.callTool(name, args, isError, text);
    }
};

void TestMcpFocus::initTestCase() {
    // Eigene Settings-Domain: SessionModel::saveState darf keine echte Liste überschreiben.
    QCoreApplication::setOrganizationName(QStringLiteral("QTmux"));
    QCoreApplication::setApplicationName(QStringLiteral("QTmuxMcpFocusTest"));
}

void TestMcpFocus::init() { QSettings().clear(); }

// Kern der Abnahme: create_session ohne focus → KEIN focusRequested, sessionCreated mit
// focus=false, das aktive Window unverändert. Mutationsprobe: `emit focusRequested(row)`
// zurück in den Handler → dieser Fall wird rot.
void TestMcpFocus::createSessionStaysInBackground() {
    SessionModel sessions;
    WindowModel windows;
    McpServer mcp;
    mcp.setSessions(&sessions);
    mcp.setWindows(&windows);

    // Ausgangslage wie in der App: ein Window, das der Mensch gerade benutzt.
    const int activeRow = windows.createWindow(QStringLiteral("Mensch"));
    windows.setActiveRow(activeRow);
    const int sessionsBefore = sessions.count();

    QSignalSpy focusSpy(&mcp, &McpServer::focusRequested);
    QSignalSpy createdSpy(&mcp, &McpServer::sessionCreated);

    bool isError = false;
    QString text;
    call(mcp, QStringLiteral("create_session"), serialArgs(), isError, text);
    QVERIFY2(!isError, qPrintable(text));
    QCOMPARE(sessions.count(), sessionsBefore + 1);

    QCOMPARE(focusSpy.count(), 0);
    QCOMPARE(createdSpy.count(), 1);
    QCOMPARE(createdSpy.at(0).at(0).toInt(), sessions.count() - 1);
    QCOMPARE(createdSpy.at(0).at(1).toBool(), false);
    QCOMPARE(windows.activeRow(), activeRow);
    // Die zurückgegebene ID gehört zur neuen Zeile (Aufrufer adressieren über sie).
    QCOMPARE(sessions.rowForId(text.toInt()), sessions.count() - 1);

    // Explizites focus=false verhält sich wie die Vorgabe.
    QJsonObject args = serialArgs();
    args.insert(QStringLiteral("focus"), false);
    call(mcp, QStringLiteral("create_session"), args, isError, text);
    QVERIFY2(!isError, qPrintable(text));
    QCOMPARE(focusSpy.count(), 0);
    QCOMPARE(createdSpy.count(), 2);
    QCOMPARE(createdSpy.at(1).at(1).toBool(), false);
}

// Gegenprobe: Mit focus=true kommt der Wunsch an — sonst wäre „false" oben nur ein
// konstanter Wert, den der Test nicht von einer abgeschalteten Weitergabe unterscheidet.
// Der Fokus läuft auch dann NICHT über focusRequested (das bleibt focus_session).
void TestMcpFocus::createSessionFocusIsOptIn() {
    SessionModel sessions;
    McpServer mcp;
    mcp.setSessions(&sessions);
    QSignalSpy focusSpy(&mcp, &McpServer::focusRequested);
    QSignalSpy createdSpy(&mcp, &McpServer::sessionCreated);

    QJsonObject args = serialArgs();
    args.insert(QStringLiteral("focus"), true);
    bool isError = false;
    QString text;
    call(mcp, QStringLiteral("create_session"), args, isError, text);
    QVERIFY2(!isError, qPrintable(text));
    QCOMPARE(createdSpy.count(), 1);
    QCOMPARE(createdSpy.at(0).at(1).toBool(), true);
    QCOMPARE(focusSpy.count(), 0);

    // Fehlschlag (unbekanntes Plugin) meldet nichts an die Oberfläche.
    call(mcp, QStringLiteral("create_session"),
         QJsonObject{{"type", "plugin"}, {"pluginId", "gibt-es-nicht"}, {"typeId", "x"}},
         isError, text);
    QVERIFY(isError);
    QCOMPARE(createdSpy.count(), 1);
}

// new_window und split_pane erzeugen ebenfalls Sessions (über den QML-Weg). Der Handler
// hier steht für QML: Er protokolliert den focus-Wert und antwortet über die Brücke.
void TestMcpFocus::windowToolsDefaultToBackground() {
    SessionModel sessions;
    McpServer mcp;
    mcp.setSessions(&sessions);
    QList<bool> newWindowFocus, splitFocus;
    connect(&mcp, &McpServer::newWindowRequested, this, [&](bool focus) {
        newWindowFocus << focus;
        mcp.provideResult(true, QStringLiteral("1"));
    });
    connect(&mcp, &McpServer::splitPaneRequested, this,
            [&](const QString &, bool focus) {
                splitFocus << focus;
                mcp.provideResult(true, QStringLiteral("2"));
            });

    bool isError = false;
    QString text;
    call(mcp, QStringLiteral("new_window"), {}, isError, text);
    QVERIFY2(!isError, qPrintable(text));
    call(mcp, QStringLiteral("new_window"), QJsonObject{{"focus", true}}, isError, text);
    QVERIFY2(!isError, qPrintable(text));
    QCOMPARE(newWindowFocus, (QList<bool>{false, true}));

    call(mcp, QStringLiteral("split_pane"), QJsonObject{{"orientation", "h"}}, isError, text);
    QVERIFY2(!isError, qPrintable(text));
    call(mcp, QStringLiteral("split_pane"),
         QJsonObject{{"orientation", "v"}, {"focus", true}}, isError, text);
    QVERIFY2(!isError, qPrintable(text));
    QCOMPARE(splitFocus, (QList<bool>{false, true}));
}

// Ein Agent erfährt vom Parameter nur über das Schema — fehlt er dort, gibt es ihn für
// den Aufrufer nicht. Alle vier Session-erzeugenden Tools müssen ihn anbieten.
void TestMcpFocus::schemaDocumentsFocus() {
    McpServer mcp;
    const QJsonArray tools = mcp.toolsList().value(QStringLiteral("tools")).toArray();
    const QStringList wanted{QStringLiteral("create_session"), QStringLiteral("new_window"),
                             QStringLiteral("split_pane"), QStringLiteral("connect_profile")};
    QStringList found;
    for (const QJsonValue &v : tools) {
        const QJsonObject t = v.toObject();
        const QString name = t.value(QStringLiteral("name")).toString();
        if (!wanted.contains(name)) continue;
        const QJsonObject props = t.value(QStringLiteral("inputSchema")).toObject()
                                      .value(QStringLiteral("properties")).toObject();
        QVERIFY2(props.value(QStringLiteral("focus")).toObject()
                         .value(QStringLiteral("type")).toString() == QLatin1String("boolean"),
                 qPrintable(name));
        found << name;
    }
    found.sort();
    QStringList expected = wanted;
    expected.sort();
    QCOMPARE(found, expected);
}

} // namespace qtmux

QTEST_MAIN(qtmux::TestMcpFocus)
#include "tst_mcpfocus.moc"
