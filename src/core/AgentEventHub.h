#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QList>
#include <QVariantList>

namespace qtmux {

/// Zentraler Ereignis-Bus für Inter-Agenten-Benachrichtigung. Gui-frei (nur Qt Core):
/// ein Agent in Shell A meldet „fertig"/„Frage" (per OSC-Hook oder MCP post_event), ein
/// abonnierender Agent in Shell B wird benachrichtigt und erhält die Quell-Session-ID,
/// um dort per MCP weiterzuarbeiten.
///
/// Producer: Session::onAgentEvent (OSC 777;qtmux-event) und McpServer (post_event).
/// Consumer: McpServer (Long-Poll wait_for_events + list_sessions-Anreicherung).
///
/// MVP: Events liegen in einem Ringpuffer (Laufzeit), Abos sind laufzeit-flüchtig — die
/// Session-ID ist nur zur Laufzeit eindeutig (Counter), daher wäre Persistenz über die
/// numerische ID bedeutungslos. `seq` ist ein monoton steigender Cursor je Hub-Lauf.
class AgentEventHub : public QObject {
    Q_OBJECT
public:
    enum class Kind { Info = 0, Done = 1, Question = 2, Error = 3 };
    Q_ENUM(Kind)

    /// Ein Ereignis (transient). `seq` ist hub-global monoton → Long-Poll-Cursor.
    struct Event {
        int     sourceSessionId = 0;
        Kind    kind = Kind::Info;
        QString text;
        qint64  timestampMs = 0;
        quint64 seq = 0;
    };

    /// Ein Abo je Subscriber-Session. Leerer Filter = „alle".
    struct Subscription {
        int          subscriberSessionId = 0;
        QList<int>   sourceFilter;   ///< leer = Ereignisse aller Quell-Sessions
        QList<Kind>  kindFilter;     ///< leer = alle Ereignisarten
    };

    /// Prozessweite Instanz (in main.cpp als Context-Property `AgentEvents` registriert).
    static AgentEventHub *instance();

    // --- Producer (C++; Session/McpServer) ---------------------------------
    /// Hängt ein Ereignis an, vergibt `seq`, meldet `eventPosted`. Gibt `seq` zurück.
    quint64 postEvent(int sourceSessionId, Kind kind, const QString &text);

    // --- Abos (QML-Dialog + McpServer) -------------------------------------
    /// Legt/aktualisiert das Abo der Subscriber-Session (Upsert über die ID).
    /// `sources`: Liste von Quell-Session-IDs (leer = alle). `kinds`: Liste aus
    /// "done"/"question"/"error"/"info" (leer = alle); unbekannte werden ignoriert.
    Q_INVOKABLE void subscribe(int subscriberSessionId, const QVariantList &sources,
                               const QStringList &kinds);
    /// Entfernt das Abo der Subscriber-Session.
    Q_INVOKABLE void unsubscribe(int subscriberSessionId);
    /// True, wenn die Session ein Abo hat.
    Q_INVOKABLE bool hasSubscription(int subscriberSessionId) const;
    /// Alle Abos als QVariantList ({subscriberSessionId, sources:[int], kinds:[string]}).
    Q_INVOKABLE QVariantList subscriptions() const;

    // --- Abfrage (McpServer) -----------------------------------------------
    /// Ereignisse für eine Subscriber-Session, gefiltert + nur mit `seq` > afterSeq.
    QList<Event> eventsFor(int subscriberSessionId, quint64 afterSeq) const;
    /// Jüngstes Ereignis einer Quell-Session (seq 0 = keines) — für list_sessions.
    Event latestFrom(int sourceSessionId) const;
    /// Aktueller Höchst-`seq` (Cursor-Startwert „ab jetzt").
    quint64 lastSeq() const { return m_seq; }

    /// Aufräumen, wenn eine Session schließt: ihr Abo entfernen.
    void sessionClosed(int sessionId);

    // --- Konvertierung ------------------------------------------------------
    static Kind kindFromString(const QString &s);   ///< unbekannt → Info
    static QString kindName(Kind k);                 ///< "done"/"question"/"error"/"info"

signals:
    /// Ein Ereignis wurde eingespeist — der McpServer prüft seine wartenden Long-Polls.
    /// (Ohne Argument: Hub und Server laufen im selben GUI-Thread, der Slot re-queryt.)
    void eventPosted();
    void subscriptionsChanged();

private:
    explicit AgentEventHub(QObject *parent = nullptr);
    bool matches(const Subscription &sub, const Event &ev) const;
    static bool parseKind(const QString &s, Kind &out);

    QList<Event>        m_events;   // Ringpuffer (Cap kCap)
    QList<Subscription> m_subs;     // ein Eintrag je Subscriber-Session
    quint64             m_seq = 0;  // monoton, Long-Poll-Cursor
    static constexpr int kCap = 256;
};

} // namespace qtmux
