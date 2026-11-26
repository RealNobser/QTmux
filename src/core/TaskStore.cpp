#include "TaskStore.h"

#include <QDateTime>
#include <QRandomGenerator>
#include <QTimeZone>

#include <algorithm>

namespace qtmux {

namespace {
// Kennungen: Präfix + 32 Hex-Zeichen (128 bit aus dem System-Zufall). Die Erweiterung
// erlaubt Kennungen als Bearer-Merkmal; ungeratbar ist darum Pflicht, nicht Zierde.
constexpr int kRandomHexChars = 32;
constexpr int kMaxIdLength = 64;

QString isoUtc(qint64 ms) {
    return QDateTime::fromMSecsSinceEpoch(ms, QTimeZone::UTC).toString(Qt::ISODate);
}
} // namespace

TaskStore::TaskStore() : TaskStore(Limits{}, Clock{}) {}

TaskStore::TaskStore(Limits limits, Clock clock)
    : m_limits(limits),
      m_clock(clock ? std::move(clock) : Clock([] { return QDateTime::currentMSecsSinceEpoch(); })) {}

bool TaskStore::isWellFormedId(const QString &id) {
    if (id.isEmpty() || id.size() > kMaxIdLength) return false;
    for (const QChar c : id) {
        const ushort u = c.unicode();
        const bool ok = (u >= '0' && u <= '9') || (u >= 'a' && u <= 'z') || u == '-';
        if (!ok) return false;
    }
    return true;
}

QString TaskStore::newId(const QString &prefix) const {
    static const char hex[] = "0123456789abcdef";
    for (;;) {
        QString s = prefix;
        s.reserve(prefix.size() + kRandomHexChars);
        quint32 words[kRandomHexChars / 8];
        QRandomGenerator::system()->fillRange(words);
        for (quint32 w : words)
            for (int i = 0; i < 8; ++i) s.append(QLatin1Char(hex[(w >> (i * 4)) & 0xF]));
        if (!m_tasks.contains(s) && !m_messages.contains(s)) return s;
    }
}

TaskStore::Task *TaskStore::findMutable(const QString &id) {
    auto it = m_tasks.find(id);
    return it == m_tasks.end() ? nullptr : &it.value();
}

const TaskStore::Task *TaskStore::find(const QString &id) const {
    if (!isWellFormedId(id)) return nullptr;
    auto it = m_tasks.constFind(id);
    return it == m_tasks.constEnd() ? nullptr : &it.value();
}

int TaskStore::openCount(int owner) const {
    int n = 0;
    for (const Task &t : m_tasks)
        if (t.owner == owner && t.status == Status::Working) ++n;
    return n;
}

bool TaskStore::isLive(const Task &t, qint64 now) const {
    return t.status == Status::Working
           && now - t.lastPolledAt <= qint64(m_limits.livePolls) * pollIntervalMs(t);
}

TaskStore::CreateResult TaskStore::create(Kind kind, int owner, bool bound, quint64 afterSeq) {
    CreateResult r;
    sweep();
    const qint64 now = m_clock();

    // Erst alles prüfen, dann ändern: Ein abgelehntes Anlegen lässt den Speicher, wie er war.
    // Je Session höchstens EIN offener Posteingang: der neue ersetzt den alten. Sonst
    // stünde nicht fest, welcher die nächste Nachricht bekommt.
    QStringList toReplace;
    if (kind == Kind::AwaitMessage)
        for (const QString &id : m_order) {
            const Task &t = m_tasks[id];
            if (t.owner == owner && t.kind == Kind::AwaitMessage && t.status == Status::Working)
                toReplace << id;
        }

    // Grenze je Session: den ältesten offenen OHNE Lebenszeichen abbrechen; lebt jeder,
    // wird abgelehnt (ein Client, der so viele Tasks wirklich abfragt, ist ein Fehler).
    QString victim;
    if (openCount(owner) - toReplace.size() >= m_limits.maxOpenPerOwner) {
        for (const QString &id : m_order) {
            const Task &t = m_tasks[id];
            if (t.owner == owner && t.status == Status::Working && !toReplace.contains(id)
                && !isLive(t, now)) {
                victim = id;
                break;
            }
        }
        if (victim.isEmpty()) {
            r.error = QStringLiteral("Zu viele offene Tasks für diese Session (höchstens %1).")
                          .arg(m_limits.maxOpenPerOwner);
            return r;
        }
    }

    // Grenze insgesamt: Platz schaffen nur aus beendeten (oder gleich beendeten) Tasks.
    int removable = toReplace.size() + (victim.isEmpty() ? 0 : 1);
    for (const Task &t : m_tasks)
        if (t.status != Status::Working) ++removable;
    if (m_tasks.size() - removable >= m_limits.maxTotal) {
        r.error = QStringLiteral("Zu viele Tasks insgesamt (höchstens %1).").arg(m_limits.maxTotal);
        return r;
    }

    for (const QString &id : toReplace)
        finish(m_tasks[id], Status::Cancelled, QStringLiteral("Ersetzt durch einen neuen Posteingangs-Task."));
    if (!toReplace.isEmpty()) r.replacedId = toReplace.last();
    if (!victim.isEmpty()) {
        finish(m_tasks[victim], Status::Cancelled,
               QStringLiteral("Abgebrochen: Grenze offener Tasks je Session erreicht."));
        r.evictedId = victim;
    }
    // Verwerfen: zuerst beendete ohne offene Zustellung (abgeholt, ohne Nachrichten oder
    // schon dem Rückfall übergeben), dann die übrigen beendeten; jeweils die ältesten.
    auto evictOne = [this](bool onlyHarmless) {
        for (int i = 0; i < m_order.size(); ++i) {
            const Task &t = m_tasks[m_order.at(i)];
            if (t.status == Status::Working) continue;
            const bool harmless = t.fetchedAt != 0 || t.messageIds.isEmpty() || t.released;
            if (onlyHarmless && !harmless) continue;
            m_tasks.remove(m_order.at(i));
            m_order.removeAt(i);
            return true;
        }
        return false;
    };
    while (m_tasks.size() >= m_limits.maxTotal)
        if (!evictOne(true) && !evictOne(false)) break;

    Task t;
    t.id = newId(QStringLiteral("qt-"));
    t.kind = kind;
    t.owner = owner;
    t.bound = bound;
    t.createdAt = t.lastUpdatedAt = t.lastPolledAt = now;
    t.afterSeq = afterSeq;
    m_tasks.insert(t.id, t);
    m_order.append(t.id);
    r.id = t.id;
    return r;
}

void TaskStore::finish(Task &t, Status st, const QString &statusMessage) {
    const qint64 now = m_clock();
    t.status = st;
    t.statusMessage = statusMessage;
    t.completedAt = now;
    t.lastUpdatedAt = now;
}

bool TaskStore::complete(const QString &id, const QJsonObject &result, const QString &statusMessage,
                         const QStringList &messageIds) {
    Task *t = findMutable(id);
    if (!t || t->status != Status::Working) return false;
    t->result = result;
    t->messageIds = messageIds;
    finish(*t, Status::Completed, statusMessage);
    if (!messageIds.isEmpty()) attachToTask(messageIds, id);
    return true;
}

bool TaskStore::cancel(const QString &id, const QString &reason) {
    Task *t = findMutable(id);
    if (!t || t->status != Status::Working) return false;
    finish(*t, Status::Cancelled, reason);
    return true;
}

bool TaskStore::fail(const QString &id, const QString &reason) {
    Task *t = findMutable(id);
    if (!t || t->status != Status::Working) return false;
    finish(*t, Status::Failed, reason);
    return true;
}

void TaskStore::setStatusMessage(const QString &id, const QString &msg) {
    Task *t = findMutable(id);
    if (t && t->status == Status::Working) t->statusMessage = msg;
}

QStringList TaskStore::unfetchedInboxes() const {
    QStringList out;
    const qint64 now = m_clock();
    for (const QString &id : m_order) {
        const Task &t = m_tasks[id];
        if (t.kind != Kind::AwaitMessage || t.status != Status::Completed || t.fetchedAt != 0
            || t.released || t.messageIds.isEmpty())
            continue;
        // Gleiche Schwelle wie „lebt“: so lange darf eine Abfrage höchstens ausbleiben.
        if (now - t.completedAt > qint64(m_limits.livePolls) * pollIntervalMs(t)) out << id;
    }
    return out;
}

void TaskStore::releaseUnfetched(const QString &id, const QJsonObject &replacement) {
    Task *t = findMutable(id);
    if (!t || t->fetchedAt != 0) return;
    t->result = replacement;
    t->released = true;
    t->lastUpdatedAt = m_clock();
}

void TaskStore::notePolled(const QString &id, bool deliveredEndState) {
    Task *t = findMutable(id);
    if (!t) return;
    const qint64 now = m_clock();
    t->lastPolledAt = now;
    // Nur eine Abfrage, die den Endzustand AUSLIEFERT (tasks/get), ist ein Zustellbeleg —
    // ein tasks/cancel auf einen schon beendeten Task ist keiner.
    if (deliveredEndState && t->status != Status::Working && t->fetchedAt == 0) {
        t->fetchedAt = now;
        if (t->released) return;   // die Nachrichten gingen schon über den Rückfall
        for (const QString &mid : t->messageIds) {
            auto it = m_messages.find(mid);
            if (it == m_messages.end()) continue;
            it->state = MessageState::Fetched;
            it->body.clear();
            it->changedAt = now;
        }
    }
}

QString TaskStore::liveInboxOf(int owner) const {
    const qint64 now = m_clock();
    for (const QString &id : m_order) {
        const Task &t = m_tasks[id];
        if (t.owner == owner && t.kind == Kind::AwaitMessage && isLive(t, now)) return id;
    }
    return {};
}

bool TaskStore::inboxRecentlySeen(int owner) const {
    const qint64 now = m_clock();
    for (const Task &t : m_tasks) {
        if (t.owner != owner || t.kind != Kind::AwaitMessage) continue;
        const qint64 last = std::max(t.lastPolledAt, t.completedAt);
        if (now - last <= m_limits.mailboxGraceMs) return true;
    }
    return false;
}

QStringList TaskStore::openTasks(Kind kind) const {
    QStringList out;
    for (const QString &id : m_order) {
        const Task &t = m_tasks.value(id);
        if (t.kind == kind && t.status == Status::Working) out << id;
    }
    return out;
}

QList<int> TaskStore::owners() const {
    QList<int> out;
    for (const Task &t : m_tasks)
        if (!out.contains(t.owner)) out << t.owner;
    for (auto it = m_buffers.constBegin(); it != m_buffers.constEnd(); ++it)
        if (!it.value().isEmpty() && !out.contains(it.key())) out << it.key();
    return out;
}

void TaskStore::ownerClosed(int owner) {
    for (Task &t : m_tasks)
        if (t.owner == owner && t.status == Status::Working)
            finish(t, Status::Cancelled, QStringLiteral("Die Session wurde geschlossen."));
    for (const QString &mid : m_buffers.take(owner))
        markFailed(mid, QStringLiteral("Die Ziel-Session wurde geschlossen, bevor sie die "
                                       "Nachricht abholte."));
}

void TaskStore::sweep() {
    const qint64 now = m_clock();
    for (int i = m_order.size() - 1; i >= 0; --i) {
        const QString id = m_order.at(i);
        Task &t = m_tasks[id];
        if (t.status == Status::Working) {
            if (now - t.lastPolledAt > m_limits.maxIdleWorkingMs)
                finish(t, Status::Cancelled,
                       QStringLiteral("Abgebrochen: seit 24 h nicht mehr abgefragt."));
            continue;
        }
        if (now - t.completedAt > m_limits.retainAfterDoneMs) {
            m_tasks.remove(id);
            m_order.removeAt(i);
        }
    }
    // Nachrichtenbelege: alte zuerst, gepufferte bleiben, bis der Puffer sie freigibt.
    for (int i = m_messageOrder.size() - 1; i >= 0; --i) {
        const QString id = m_messageOrder.at(i);
        const Message &m = m_messages[id];
        if (m.state == MessageState::Buffered) continue;
        if (now - m.changedAt > m_limits.retainMessageMs) {
            m_messages.remove(id);
            m_messageOrder.removeAt(i);
        }
    }
}

int TaskStore::pollIntervalMs(const Task &t) const {
    return (m_clock() - t.createdAt < m_limits.fastPollWindowMs) ? m_limits.fastPollMs
                                                                  : m_limits.slowPollMs;
}

QString TaskStore::statusName(Status s) {
    switch (s) {
    case Status::Working: return QStringLiteral("working");
    case Status::Completed: return QStringLiteral("completed");
    case Status::Failed: return QStringLiteral("failed");
    case Status::Cancelled: return QStringLiteral("cancelled");
    }
    return QStringLiteral("failed");
}

QJsonObject TaskStore::wire(const Task &t) const {
    QJsonObject o{
        {"taskId", t.id},
        {"status", statusName(t.status)},
        {"createdAt", isoUtc(t.createdAt)},
        {"lastUpdatedAt", isoUtc(t.lastUpdatedAt)},
        {"pollIntervalMs", pollIntervalMs(t)},
    };
    // ttlMs zählt ab Anlage. Offen: keine feste Frist (null); beendet: so lange, wie das
    // Ergebnis noch abrufbar ist. Die Erweiterung erlaubt, dass sich der Wert ändert.
    if (t.status == Status::Working)
        o.insert("ttlMs", QJsonValue::Null);
    else
        o.insert("ttlMs", static_cast<double>(t.completedAt - t.createdAt + m_limits.retainAfterDoneMs));
    if (!t.statusMessage.isEmpty()) o.insert("statusMessage", t.statusMessage);
    if (t.status == Status::Completed) o.insert("result", t.result);
    if (t.status == Status::Failed)
        o.insert("error", QJsonObject{{"code", -32603}, {"message", t.statusMessage}});
    return o;
}

// --- Nachrichten -------------------------------------------------------------

QString TaskStore::recordMessage(int sender, int target, const QString &text, const QString &body,
                                 const QString &from) {
    sweep();
    // Grenze der Belege: älteste nicht gepufferte zuerst verwerfen.
    while (m_messages.size() >= m_limits.maxMessages) {
        auto it = std::find_if(m_messageOrder.begin(), m_messageOrder.end(), [this](const QString &id) {
            return m_messages.value(id).state != MessageState::Buffered;
        });
        if (it == m_messageOrder.end()) break;
        m_messages.remove(*it);
        m_messageOrder.erase(it);
    }
    Message m;
    m.id = newId(QStringLiteral("msg-"));
    m.sender = sender;
    m.target = target;
    m.text = text;
    m.body = body;
    m.from = from;
    m.createdAt = m.changedAt = m_clock();
    m.state = MessageState::Failed;   // bis ein Weg feststeht
    m_messages.insert(m.id, m);
    m_messageOrder.append(m.id);
    return m.id;
}

const TaskStore::Message *TaskStore::message(const QString &id) const {
    if (!isWellFormedId(id)) return nullptr;
    auto it = m_messages.constFind(id);
    return it == m_messages.constEnd() ? nullptr : &it.value();
}

void TaskStore::markDirect(const QString &id, const QString &via, const QString &status, bool fellBack,
                           const QString &detail) {
    auto it = m_messages.find(id);
    if (it == m_messages.end()) return;
    it->state = fellBack ? MessageState::FellBack : MessageState::Direct;
    // Der Beleg braucht den Text nicht mehr (Befund der Durchsicht 2026-11-26: sonst blieben
    // je abgelehnter Riesen-Nachricht zwei Kopien eine Stunde lang liegen).
    it->text.clear();
    it->body.clear();
    it->via = via;
    it->status = status;
    it->detail = detail;
    it->changedAt = m_clock();
}

void TaskStore::markFailed(const QString &id, const QString &detail) {
    auto it = m_messages.find(id);
    if (it == m_messages.end()) return;
    it->state = MessageState::Failed;
    it->text.clear();
    it->body.clear();
    it->detail = detail;
    it->changedAt = m_clock();
}

bool TaskStore::buffer(const QString &id) {
    auto it = m_messages.find(id);
    if (it == m_messages.end()) return false;
    QStringList &buf = m_buffers[it->target];
    qint64 bytes = it->text.toUtf8().size();
    for (const QString &mid : buf) bytes += m_messages.value(mid).text.toUtf8().size();
    if (buf.size() >= m_limits.maxBufferedPerTarget || bytes > m_limits.maxBufferedBytesPerTarget)
        return false;
    buf.append(id);
    it->state = MessageState::Buffered;
    it->via = QStringLiteral("task");
    it->changedAt = m_clock();
    return true;
}

bool TaskStore::hasBuffered(int target) const {
    return !m_buffers.value(target).isEmpty();
}

QList<TaskStore::Message> TaskStore::takeBuffered(int target) {
    QList<Message> out;
    for (const QString &mid : m_buffers.take(target))
        if (m_messages.contains(mid)) out.append(m_messages.value(mid));
    return out;
}

QList<int> TaskStore::expiredBuffers() const {
    QList<int> out;
    const qint64 now = m_clock();
    for (auto it = m_buffers.constBegin(); it != m_buffers.constEnd(); ++it) {
        if (it.value().isEmpty()) continue;
        const Message oldest = m_messages.value(it.value().first());
        if (now - oldest.createdAt > m_limits.mailboxGraceMs) out << it.key();
    }
    return out;
}

void TaskStore::attachToTask(const QStringList &messageIds, const QString &taskId) {
    const qint64 now = m_clock();
    for (const QString &mid : messageIds) {
        auto it = m_messages.find(mid);
        if (it == m_messages.end()) continue;
        it->state = MessageState::InTask;
        // Der Text steht jetzt im Task-Ergebnis; den unformatierten behalten, bis der
        // Client abholt (der Rückfall bei Nicht-Abholen braucht ihn).
        it->text.clear();
        it->via = QStringLiteral("task");
        it->taskId = taskId;
        it->changedAt = now;
    }
}

QString TaskStore::stateName(MessageState s) {
    switch (s) {
    case MessageState::Buffered: return QStringLiteral("buffered");
    case MessageState::InTask: return QStringLiteral("completed");
    case MessageState::Fetched: return QStringLiteral("fetched");
    case MessageState::Direct: return QStringLiteral("direct");
    case MessageState::FellBack: return QStringLiteral("fell-back");
    case MessageState::Failed: return QStringLiteral("failed");
    }
    return QStringLiteral("failed");
}

} // namespace qtmux
