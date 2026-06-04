#include "SessionModel.h"
#include "Session.h"
#include "PtyBackend.h"

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
    case StateRole:   return s->stateInt();
    case TypeRole:    return static_cast<int>(s->type());
    case SessionRole: return QVariant::fromValue(static_cast<QObject *>(s));
    default:          return {};
    }
}

QHash<int, QByteArray> SessionModel::roleNames() const {
    return {
        {TitleRole,   "title"},
        {StateRole,   "runState"},
        {TypeRole,    "sessionType"},
        {SessionRole, "session"},
    };
}

void SessionModel::wireSession(Session *s, int row) {
    // Bei Titel-/Zustandswechsel die betroffene Sidebar-Zeile aktualisieren.
    auto refresh = [this, s]() {
        const int r = static_cast<int>(m_sessions.indexOf(s));
        if (r >= 0) {
            const QModelIndex idx = index(r);
            emit dataChanged(idx, idx, {TitleRole, StateRole});
        }
    };
    connect(s, &Session::titleChanged, this, refresh);
    connect(s, &Session::stateChanged, this, refresh);
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

QObject *SessionModel::sessionAt(int row) const {
    if (row < 0 || row >= count()) return nullptr;
    return m_sessions.at(row);
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
