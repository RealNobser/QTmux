#include "AgentEventHub.h"

#include <QDateTime>
#include <QVariantMap>

namespace qtmux {

AgentEventHub *AgentEventHub::instance() {
    static AgentEventHub hub;
    return &hub;
}

AgentEventHub::AgentEventHub(QObject *parent) : QObject(parent) {}

quint64 AgentEventHub::postEvent(int sourceSessionId, Kind kind, const QString &text) {
    Event ev;
    ev.sourceSessionId = sourceSessionId;
    ev.kind = kind;
    ev.text = text;
    ev.timestampMs = QDateTime::currentMSecsSinceEpoch();
    ev.seq = ++m_seq;
    m_events.append(ev);
    while (m_events.size() > kCap) m_events.removeFirst();
    emit eventPosted();
    return ev.seq;
}

bool AgentEventHub::matches(const Subscription &sub, const Event &ev) const {
    // Eine Session wird nicht über ihre EIGENEN Ereignisse benachrichtigt.
    if (ev.sourceSessionId == sub.subscriberSessionId) return false;
    if (!sub.sourceFilter.isEmpty() && !sub.sourceFilter.contains(ev.sourceSessionId))
        return false;
    if (!sub.kindFilter.isEmpty() && !sub.kindFilter.contains(ev.kind))
        return false;
    return true;
}

void AgentEventHub::subscribe(int subscriberSessionId, const QVariantList &sources,
                              const QStringList &kinds) {
    if (subscriberSessionId <= 0) return;
    Subscription sub;
    sub.subscriberSessionId = subscriberSessionId;
    for (const QVariant &v : sources) {
        bool ok = false;
        const int id = v.toInt(&ok);
        if (ok && id > 0) sub.sourceFilter << id;
    }
    for (const QString &k : kinds) {
        Kind kk;
        if (parseKind(k, kk) && !sub.kindFilter.contains(kk)) sub.kindFilter << kk;
    }
    // Upsert über die Subscriber-ID.
    for (int i = 0; i < m_subs.size(); ++i) {
        if (m_subs.at(i).subscriberSessionId == subscriberSessionId) {
            m_subs[i] = sub;
            emit subscriptionsChanged();
            return;
        }
    }
    m_subs << sub;
    emit subscriptionsChanged();
}

void AgentEventHub::unsubscribe(int subscriberSessionId) {
    for (int i = 0; i < m_subs.size(); ++i) {
        if (m_subs.at(i).subscriberSessionId == subscriberSessionId) {
            m_subs.removeAt(i);
            emit subscriptionsChanged();
            return;
        }
    }
}

bool AgentEventHub::hasSubscription(int subscriberSessionId) const {
    for (const Subscription &s : m_subs)
        if (s.subscriberSessionId == subscriberSessionId) return true;
    return false;
}

QVariantList AgentEventHub::subscriptions() const {
    QVariantList out;
    for (const Subscription &s : m_subs) {
        QVariantList src;
        for (int id : s.sourceFilter) src << id;
        QStringList kinds;
        for (Kind k : s.kindFilter) kinds << kindName(k);
        out << QVariantMap{
            {QStringLiteral("subscriberSessionId"), s.subscriberSessionId},
            {QStringLiteral("sources"), src},
            {QStringLiteral("kinds"), kinds},
        };
    }
    return out;
}

QList<AgentEventHub::Event> AgentEventHub::eventsFor(int subscriberSessionId,
                                                     quint64 afterSeq) const {
    QList<Event> out;
    int idx = -1;
    for (int i = 0; i < m_subs.size(); ++i) {
        if (m_subs.at(i).subscriberSessionId == subscriberSessionId) { idx = i; break; }
    }
    if (idx < 0) return out;   // kein Abo → keine Ereignisse
    const Subscription &sub = m_subs.at(idx);
    for (const Event &ev : m_events) {
        if (ev.seq > afterSeq && matches(sub, ev)) out << ev;
    }
    return out;
}

AgentEventHub::Event AgentEventHub::latestFrom(int sourceSessionId) const {
    for (int i = m_events.size() - 1; i >= 0; --i) {
        if (m_events.at(i).sourceSessionId == sourceSessionId) return m_events.at(i);
    }
    return Event{};   // seq == 0 → keines
}

void AgentEventHub::sessionClosed(int sessionId) {
    // Abo der geschlossenen Session entfernen (Events bleiben im Ringpuffer, harmlos).
    unsubscribe(sessionId);
}

bool AgentEventHub::parseKind(const QString &s, Kind &out) {
    const QString k = s.trimmed().toLower();
    if (k == QLatin1String("done"))     { out = Kind::Done;     return true; }
    if (k == QLatin1String("question")) { out = Kind::Question; return true; }
    if (k == QLatin1String("error"))    { out = Kind::Error;    return true; }
    if (k == QLatin1String("info"))     { out = Kind::Info;     return true; }
    return false;
}

AgentEventHub::Kind AgentEventHub::kindFromString(const QString &s) {
    Kind k;
    return parseKind(s, k) ? k : Kind::Info;
}

QString AgentEventHub::kindName(Kind k) {
    switch (k) {
    case Kind::Done:     return QStringLiteral("done");
    case Kind::Question: return QStringLiteral("question");
    case Kind::Error:    return QStringLiteral("error");
    case Kind::Info:     break;
    }
    return QStringLiteral("info");
}

} // namespace qtmux
