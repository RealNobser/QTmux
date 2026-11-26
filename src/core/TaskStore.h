#pragma once

#include <QHash>
#include <QJsonObject>
#include <QList>
#include <QString>
#include <QStringList>
#include <QtGlobal>

#include <functional>

namespace qtmux {

/// Gui-freier Speicher für MCP-Tasks (Erweiterung `io.modelcontextprotocol/tasks`,
/// Revision 2026-07-28) und für Nachrichten, die über einen Posteingangs-Task zugestellt
/// werden. Kennt weder Netz noch Sessions, nur Kennungen, Zeiten und Grenzen; der
/// McpServer entscheidet, wann ein Task endet.
///
/// Zwei Arten:
///  - **AwaitEvents**: endet, sobald für die Besitzer-Session ein abonniertes Ereignis
///    vorliegt (Task-Form von `wait_for_events`).
///  - **AwaitMessage**: Posteingang. Endet mit der nächsten an die Besitzer-Session
///    gerichteten Nachricht. Je Session höchstens EIN offener.
///
/// 🔑 Alles liegt nur im Speicher. Nach einem Neustart von QTmux sind Tasks und
/// Nachrichtenbelege fort; eine Abfrage endet dann mit „unbekannt“ (-32602).
class TaskStore {
public:
    enum class Kind { AwaitEvents, AwaitMessage };
    enum class Status { Working, Completed, Failed, Cancelled };

    struct Task {
        QString id;
        Kind kind = Kind::AwaitEvents;
        int owner = -1;            ///< QTmux-Session-ID des Besitzers
        bool bound = false;        ///< Besitz am Prozessbaum geprüft (sonst nur die Kennung)
        Status status = Status::Working;
        qint64 createdAt = 0;      ///< ms seit Epoche (UTC)
        qint64 lastUpdatedAt = 0;
        qint64 completedAt = 0;    ///< 0 = offen
        qint64 lastPolledAt = 0;   ///< letzte Abfrage (Lebenszeichen des Clients)
        qint64 fetchedAt = 0;      ///< erste Abfrage, die den Endzustand ausgeliefert hat
        QJsonObject result;        ///< CallToolResult, nur bei Completed
        QString statusMessage;
        quint64 afterSeq = 0;      ///< nur AwaitEvents: Cursor in den AgentEventHub
        QStringList messageIds;    ///< nur AwaitMessage: zugestellte Nachrichten
        bool released = false;     ///< Nachrichten nach Nicht-Abholen dem Rückfall übergeben
    };

    /// Zustand einer Nachricht aus Sicht des Absenders (`message_status`).
    enum class MessageState {
        Buffered,   ///< wartet auf den nächsten Posteingangs-Task
        InTask,     ///< im Ergebnis eines Tasks, vom Client noch NICHT abgeholt
        Fetched,    ///< Client hat den Endzustand abgeholt (Zustellbeleg, kein Lesebeleg)
        Direct,     ///< ging direkt über Sitzungs-Socket oder Warteschlange (`via`)
        FellBack,   ///< Puffer lief ab, dann Socket/Warteschlange (`via`)
        Failed,     ///< nicht zugestellt (`detail` nennt den Grund)
    };

    struct Message {
        QString id;
        int sender = -1;
        int target = -1;
        QString text;              ///< für den Task fertig formatiert (mit Absender)
        QString body;              ///< Text, wie ihn der Absender schrieb (für den Rückfall)
        QString from;              ///< Absender-Bezeichnung wie in send_message
        qint64 createdAt = 0;
        qint64 changedAt = 0;
        MessageState state = MessageState::Buffered;
        QString via;               ///< "task" | "inbox" | "queue"
        QString status;            ///< Status des Direktwegs (handed-over, queued, …)
        QString detail;            ///< Grund bei Failed/FellBack
        QString taskId;
    };

    struct Limits {
        int maxOpenPerOwner = 8;
        int maxTotal = 256;
        qint64 retainAfterDoneMs = 60LL * 60 * 1000;          ///< 1 h nach Abschluss
        qint64 maxIdleWorkingMs = 24LL * 60 * 60 * 1000;      ///< offen ohne Abfrage
        /// „Lebt“: letzte Abfrage höchstens so viele Takte (pollIntervalMs) alt. Gemessen
        /// 2026-11-26: Claude Code fragt im Takt nach, auch während langer Züge und
        /// /compact (größter Abstand 2,04 s bei 2000, 5,01 s bei 5000).
        int livePolls = 3;
        qint64 mailboxGraceMs = 60 * 1000;                    ///< Puffer wartet so lange
        int maxBufferedPerTarget = 32;
        qint64 maxBufferedBytesPerTarget = 256 * 1024;
        int maxMessages = 1024;                               ///< Belege insgesamt
        qint64 retainMessageMs = 60LL * 60 * 1000;
        qint64 fastPollWindowMs = 60 * 1000;
        int fastPollMs = 2000;
        int slowPollMs = 5000;
    };

    using Clock = std::function<qint64()>;

    TaskStore();
    TaskStore(Limits limits, Clock clock);

    const Limits &limits() const { return m_limits; }
    qint64 now() const { return m_clock(); }

    /// Hat `id` die Form einer von uns vergebenen Kennung? Vorprüfung ohne Suche, damit
    /// fremde Eingaben (Länge, Zeichen) gar nicht erst in eine Hash-Abfrage gehen.
    static bool isWellFormedId(const QString &id);

    struct CreateResult {
        QString id;          ///< leer = abgelehnt (`error`)
        QString error;
        QString replacedId;  ///< AwaitMessage: ersetzter offener Posteingangs-Task
        QString evictedId;   ///< wegen der Grenze je Session abgebrochener Task
    };
    CreateResult create(Kind kind, int owner, bool bound, quint64 afterSeq = 0);

    const Task *find(const QString &id) const;

    /// Endzustände. Liefern false, wenn der Task unbekannt oder schon beendet ist.
    bool complete(const QString &id, const QJsonObject &result, const QString &statusMessage = {},
                  const QStringList &messageIds = {});
    bool cancel(const QString &id, const QString &reason);
    bool fail(const QString &id, const QString &reason);
    /// Statusmeldung eines offenen Tasks setzen.
    void setStatusMessage(const QString &id, const QString &msg);

    /// Abfrage durch den Client vermerken; liefert er dabei den Endzustand aus, gilt der
    /// Task (und jede Nachricht in ihm) als abgeholt.
    void notePolled(const QString &id, bool deliveredEndState = true);
    /// Beendete Posteingänge mit Nachrichten, die seit mehr als 3 Takten niemand abholt.
    QStringList unfetchedInboxes() const;
    /// Ergebnis eines nicht abgeholten Posteingangs ersetzen, nachdem seine Nachrichten
    /// über den Rückfall gingen (holt der Client später doch ab, kommt nichts doppelt).
    void releaseUnfetched(const QString &id, const QJsonObject &replacement);

    /// Offener, lebender Posteingangs-Task der Session (letzte Abfrage höchstens
    /// `livePolls` Takte alt); leer = keiner.
    QString liveInboxOf(int owner) const;
    /// Hatte die Session in den letzten `mailboxGraceMs` einen Posteingangs-Task
    /// (offen, gerade abgefragt oder gerade beendet)? Dann lohnt es, kurz zu puffern.
    bool inboxRecentlySeen(int owner) const;
    /// Offene Tasks einer Art (für das Wecken von AwaitEvents).
    QStringList openTasks(Kind kind) const;
    /// Alle Besitzer, die noch Tasks oder gepufferte Nachrichten haben.
    QList<int> owners() const;

    /// Besitzer-Session wurde geschlossen: offene Tasks abbrechen, Puffer verwerfen.
    void ownerClosed(int owner);

    /// Aufräumen: abgelaufene Endzustände löschen, offene Tasks ohne Lebenszeichen
    /// abbrechen, alte Nachrichtenbelege löschen.
    void sweep();

    /// Task in der Drahtform der Erweiterung (ohne `resultType` und `_meta`).
    QJsonObject wire(const Task &t) const;
    int pollIntervalMs(const Task &t) const;

    // --- Nachrichten ---------------------------------------------------------
    QString recordMessage(int sender, int target, const QString &text, const QString &body,
                          const QString &from);
    const Message *message(const QString &id) const;
    void markDirect(const QString &id, const QString &via, const QString &status, bool fellBack,
                    const QString &detail = {});
    void markFailed(const QString &id, const QString &detail);
    /// In den Puffer der Ziel-Session legen; false = Puffer voll (Nachricht bleibt dann
    /// unverändert, der Aufrufer nimmt den nächsten Weg).
    bool buffer(const QString &id);
    bool hasBuffered(int target) const;
    /// Puffer der Ziel-Session leeren (Reihenfolge wie eingegangen).
    QList<Message> takeBuffered(int target);
    /// Ziel-Sessions, deren ältester gepufferter Eintrag älter als `mailboxGraceMs` ist.
    QList<int> expiredBuffers() const;
    /// Nachrichten in einen Task legen (Zustand InTask).
    void attachToTask(const QStringList &messageIds, const QString &taskId);

    static QString stateName(MessageState s);
    static QString statusName(Status s);

private:
    QString newId(const QString &prefix) const;
    Task *findMutable(const QString &id);
    int openCount(int owner) const;
    bool isLive(const Task &t, qint64 now) const;
    void finish(Task &t, Status st, const QString &statusMessage);

    Limits m_limits;
    Clock m_clock;
    QHash<QString, Task> m_tasks;
    QList<QString> m_order;                 // Anlagereihenfolge (für Grenzen und Aufräumen)
    QHash<QString, Message> m_messages;
    QList<QString> m_messageOrder;
    QHash<int, QStringList> m_buffers;      // Ziel-Session -> gepufferte Nachrichten-IDs
};

} // namespace qtmux
