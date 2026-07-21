#pragma once

#include <QObject>
#include <QHash>
#include <QList>
#include <QByteArray>
#include <QJsonValue>
#include <qqmlintegration.h>

#include "SessionModel.h"   // vollständiger Typ für Q_PROPERTY(SessionModel*)

class QTcpServer;
class QTcpSocket;
class QTimer;
class QJsonObject;

namespace qtmux {

/// Eingebetteter MCP-Server (Model Context Protocol) über HTTP/JSON-RPC 2.0,
/// gebunden an 127.0.0.1 — ein externer Agent kann QTmux fernsteuern, inkl.
/// einzelner Sessions (auflisten, erstellen, schließen, Eingaben senden, Schirm lesen).
///
/// Transport: MCP "Streamable HTTP" (POST /mcp mit JSON-RPC; Antwort als application/json).
/// Läuft im GUI-Thread → Session-Aufrufe sind thread-sicher.
class McpServer : public QObject {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(qtmux::SessionModel *sessions READ sessions WRITE setSessions NOTIFY sessionsChanged)
    Q_PROPERTY(int port READ port WRITE setPort NOTIFY portChanged)
    Q_PROPERTY(bool listening READ listening NOTIFY listeningChanged)
public:
    explicit McpServer(QObject *parent = nullptr);
    ~McpServer() override;

    SessionModel *sessions() const { return m_sessions; }
    void setSessions(SessionModel *m);
    int port() const { return m_port; }
    void setPort(int p);
    bool listening() const;

    /// Vorgabe-Port: `QTMUX_MCP_PORT` > Einstellung `mcp/port` > 7345. So lässt sich
    /// eine zweite Instanz zum Testen auf einem eigenen Port starten, ohne die
    /// produktive zu stören (s. auch `QTMUX_PROFILE` für getrennte Einstellungen).
    static int defaultPort();

    Q_INVOKABLE bool start();
    Q_INVOKABLE void stop();

    /// Ergebnis-Brücke für die Layout-/Profil-Signale (QTMUX-29): Der QML-Handler
    /// eines *Requested-Signals läuft SYNCHRON (gleicher Thread, Direct-Connection)
    /// und meldet sein Ergebnis hierüber zurück — nach dem `emit` liest callTool es
    /// aus. `text` ist je nach Tool eine Session-ID, ein Layout-JSON oder eine
    /// Fehlermeldung. Ruft kein Handler an (Signal unverbunden), bleibt die Brücke
    /// leer und das Tool meldet einen Fehler statt zu hängen.
    Q_INVOKABLE void provideResult(bool ok, const QString &text);

signals:
    void sessionsChanged();
    void portChanged();
    void listeningChanged();
    /// Vom MCP angeforderter Fokuswechsel auf eine Sidebar-Zeile (QML setzt currentRow).
    void focusRequested(int row);
    /// Vom MCP angeforderter Theme-Wechsel (0=System, 1=Hell, 2=Dunkel).
    void setThemeRequested(int mode);
    // --- Layout-/Profil-Steuerung (QTMUX-29). Handler antworten via provideResult. ---
    /// Layout-Baum als JSON liefern (Blatt: paneId/sessionId/active; Split: orientation/children).
    void layoutRequested();
    /// Aktives Pane teilen ("h" = nebeneinander, "v" = untereinander).
    void splitPaneRequested(const QString &orientation);
    /// Pane schließen (paneId < 0 = aktives Pane). Schließt wie die GUI auch die Session.
    void closePaneRequested(int paneId);
    /// Session (Sidebar-Zeile `row`) in ein Pane laden (paneId < 0 = aktives Pane).
    void assignPaneRequested(int row, int paneId);
    /// Verbindungsprofil per Name verbinden (Vault-Auflösung passiert im QML-Weg).
    void connectProfileRequested(const QString &name);

private:
    void onReadyRead(QTcpSocket *sock);
    void sendHttpJson(QTcpSocket *sock, const QByteArray &json, int status = 200);
    /// Ermittelt aus dem verbindenden Client-Prozess (TCP-Port → PID → Vorfahrenkette)
    /// die QTmux-Session, in deren Shell der Client läuft (sonst -1).
    int sessionIdForClientPort(quint16 clientPort) const;

    // JSON-RPC / MCP
    QJsonObject handleRpc(const QJsonObject &req, bool &isNotification);
    QJsonObject dispatchMethod(const QString &method, const QJsonObject &params,
                               bool &ok, QString &errMsg);
    QJsonObject callTool(const QString &name, const QJsonObject &args,
                         bool &isError, QString &text);
    QJsonObject toolsList() const;

    // Inter-Agenten-Benachrichtigung: Long-Poll wait_for_events. Der Socket bleibt
    // offen, bis ein passendes Ereignis vorliegt oder der Timeout greift.
    void beginLongPoll(QTcpSocket *sock, const QJsonValue &rpcId, const QJsonObject &args);
    void onHubEvent();                       // ein Ereignis kam → wartende Polls prüfen
    void completePoll(int index);            // Poll[index] mit seinen Events beantworten
    void removePollsForSocket(QTcpSocket *sock);
    QJsonObject pollResult(int subscriberSessionId, quint64 afterSeq) const;
    /// Zahl der Abo-Quellen, die bislang je ein Ereignis gemeldet haben (QTMUX-30).
    int eventCapableSources(int subscriberSessionId) const;
    int subscriberSessionId(const QJsonObject &args, quint16 clientPort) const;

    struct PendingPoll {
        QTcpSocket *sock = nullptr;
        QJsonValue  rpcId;
        int         subscriberSessionId = 0;
        quint64     afterSeq = 0;
        QTimer     *deadline = nullptr;
    };

    /// Setzt die Ergebnis-Brücke zurück, feuert `emitter` (synchroner QML-Handler)
    /// und liefert das via provideResult gemeldete Ergebnis in isError/text.
    template <typename Emit>
    void bridgedCall(Emit emitter, bool &isError, QString &text) {
        m_bridgeSet = false;
        m_bridgeOk = false;
        m_bridgeText.clear();
        emitter();
        if (!m_bridgeSet) {   // kein QML-Handler verbunden (z. B. headless)
            isError = true;
            text = QStringLiteral("UI nicht verbunden.");
            return;
        }
        isError = !m_bridgeOk;
        text = m_bridgeText;
    }

    QTcpServer *m_server = nullptr;
    SessionModel *m_sessions = nullptr;
    // Ergebnis-Brücke der *Requested-Signale (s. provideResult).
    bool m_bridgeSet = false;
    bool m_bridgeOk = false;
    QString m_bridgeText;
    int m_port = 7345;
    // Session-ID des aktuell verarbeiteten Aufrufers (für tools/call synchron gesetzt;
    // Fallback für post_event/subscribe_events ohne explizites sessionId-Argument).
    int m_callerSessionId = -1;
    QHash<QTcpSocket *, QByteArray> m_buffers;
    QList<PendingPoll> m_pendingPolls;
};

} // namespace qtmux
