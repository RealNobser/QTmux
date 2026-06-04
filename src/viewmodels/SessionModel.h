#pragma once

#include <QAbstractListModel>
#include <QList>
#include <qqmlintegration.h>

namespace qtmux {

class Session;

/// Liste aller Sessions — speist die vertikale Sidebar und liefert Session-Objekte
/// zum Binden an TerminalItem.session.
class SessionModel : public QAbstractListModel {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(int count READ count NOTIFY countChanged)
public:
    enum Roles {
        TitleRole = Qt::UserRole + 1,
        StateRole,
        TypeRole,
        AgentRole,
        AttentionRole,
        NotificationRole,
        SessionRole,
    };
    Q_ENUM(Roles)

    explicit SessionModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    int count() const { return static_cast<int>(m_sessions.size()); }

    /// Erzeugt eine lokale Shell-Session und gibt deren Zeilenindex zurück.
    Q_INVOKABLE int createShellSession();
    /// Session-Objekt einer Zeile (für Binding an TerminalItem.session).
    Q_INVOKABLE QObject *sessionAt(int row) const;
    Q_INVOKABLE void closeSession(int row);
    /// Markiert die Zeile als aktiv/fokussiert (alle anderen inaktiv) — löscht deren Attention.
    Q_INVOKABLE void setActiveRow(int row);

signals:
    void countChanged();
    /// Eine (nicht-fokussierte) Session fordert Aufmerksamkeit — für Fenster-Alert.
    void attentionRaised(int row);

private:
    void wireSession(Session *s, int row);

    QList<Session *> m_sessions;
};

} // namespace qtmux
