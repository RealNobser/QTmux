#include "SessionModel.h"
#include "Session.h"
#include "PtyBackend.h"
#include "SerialBackend.h"

#include <QSerialPortInfo>

namespace qtmux {

SessionModel::SessionModel(QObject *parent) : QAbstractListModel(parent) {}

int SessionModel::rowCount(const QModelIndex &parent) const {
    return parent.isValid() ? 0 : count();
}

QVariant SessionModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= count()) return {};
    Session *s = m_sessions.at(index.row());
    switch (role) {
    case TitleRole:   return s->title();
    case StateRole:   return s->activityInt();   // Sidebar-Ring folgt der Aktivität
    case TypeRole:    return static_cast<int>(s->type());
    case AgentRole:   return s->agentId();
    case AttentionRole: return s->needsAttention();
    case NotificationRole: return s->lastNotification();
    case SessionRole: return QVariant::fromValue(static_cast<QObject *>(s));
    default:          return {};
    }
}

QHash<int, QByteArray> SessionModel::roleNames() const {
    return {
        {TitleRole,   "title"},
        {StateRole,   "runState"},
        {TypeRole,    "sessionType"},
        {AgentRole,   "agentId"},
        {AttentionRole, "needsAttention"},
        {NotificationRole, "lastNotification"},
        {SessionRole, "session"},
    };
}

void SessionModel::wireSession(Session *s, int row) {
    // Bei Titel-/Zustandswechsel die betroffene Sidebar-Zeile aktualisieren.
    auto refresh = [this, s]() {
        const int r = static_cast<int>(m_sessions.indexOf(s));
        if (r >= 0) {
            const QModelIndex idx = index(r);
            emit dataChanged(idx, idx,
                {TitleRole, StateRole, AgentRole, AttentionRole, NotificationRole});
        }
    };
    connect(s, &Session::titleChanged, this, refresh);
    connect(s, &Session::stateChanged, this, refresh);
    connect(s, &Session::agentChanged, this, refresh);
    connect(s, &Session::activityChanged, this, refresh);
    connect(s, &Session::notificationChanged, this, refresh);
    connect(s, &Session::attentionChanged, this, refresh);

    // Steigt die Aufmerksamkeit, das Fenster informieren (Dock-/Taskbar-Alert).
    connect(s, &Session::attentionChanged, this, [this, s]() {
        if (s->needsAttention()) {
            const int r = static_cast<int>(m_sessions.indexOf(s));
            if (r >= 0) emit attentionRaised(r);
        }
    });

    // Endet die zugrundeliegende Shell/Verbindung, die Session automatisch entfernen.
    // Verzögert (QueuedConnection), um nicht während der Signalauslösung zu löschen.
    connect(s, &Session::stateChanged, this, [this, s]() {
        if (s->state() == BackendState::Closed) {
            QMetaObject::invokeMethod(this, [this, s]() {
                const int r = static_cast<int>(m_sessions.indexOf(s));
                if (r >= 0) closeSession(r);
            }, Qt::QueuedConnection);
        }
    });
    Q_UNUSED(row);
}

int SessionModel::createShellSession() {
    auto *s = new Session(this);
    auto *pty = new PtyBackend();
    s->attachBackend(pty, Session::Type::Shell, 80, 24);

    const int row = count();
    beginInsertRows({}, row, row);
    m_sessions.append(s);
    endInsertRows();

    wireSession(s, row);
    s->start(80, 24);
    emit countChanged();
    return row;
}

int SessionModel::createSerialSession(const QString &portName, int baud) {
    auto *s = new Session(this);
    auto *serial = new SerialBackend();
    serial->setPortName(portName);
    serial->setBaudRate(baud);
    s->attachBackend(serial, Session::Type::Serial, 80, 24);
    s->setTitle(portName);

    const int row = count();
    beginInsertRows({}, row, row);
    m_sessions.append(s);
    endInsertRows();

    wireSession(s, row);
    s->start(80, 24);
    emit countChanged();
    return row;
}

QStringList SessionModel::availableSerialPorts() const {
    QStringList ports;
    const auto infos = QSerialPortInfo::availablePorts();
    for (const QSerialPortInfo &info : infos) ports << info.portName();
    return ports;
}

QObject *SessionModel::sessionAt(int row) const {
    if (row < 0 || row >= count()) return nullptr;
    return m_sessions.at(row);
}

void SessionModel::setActiveRow(int row) {
    for (int i = 0; i < count(); ++i) {
        m_sessions.at(i)->setActive(i == row);
    }
}

void SessionModel::closeSession(int row) {
    if (row < 0 || row >= count()) return;
    beginRemoveRows({}, row, row);
    Session *s = m_sessions.takeAt(row);
    endRemoveRows();
    s->deleteLater();
    emit countChanged();
}

} // namespace qtmux
