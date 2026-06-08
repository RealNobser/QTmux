#pragma once

#include <QAbstractListModel>
#include <QList>
#include <QVariantList>
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
        McpControllerRole,
        SessionRole,
    };
    Q_ENUM(Roles)

    explicit SessionModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    int count() const { return static_cast<int>(m_sessions.size()); }

    /// Erzeugt eine lokale Shell-Session und gibt die Zeile zurück.
    /// `workingDir` leer = Home; `program` leer = plattformübliche Standard-Shell.
    Q_INVOKABLE int createShellSession(const QString &workingDir = {},
                                       const QString &program = {});
    /// Auf dieser Plattform auswählbare Shells als Liste von {program, name}-Maps
    /// (für die Shell-Auswahl in der UI). Siehe qtmux::ShellRegistry.
    Q_INVOKABLE QVariantList availableShells() const;
    /// Erzeugt eine serielle Session (Port + Baudrate) und gibt deren Zeilenindex zurück.
    Q_INVOKABLE int createSerialSession(const QString &portName, int baud);
    /// Erzeugt eine SSH-Session (System-ssh) und gibt deren Zeilenindex zurück.
    Q_INVOKABLE int createSshSession(const QString &host, int port,
                                     const QString &user, const QString &identityFile = {});
    /// Verfügbare serielle Ports (z. B. "/dev/cu.usbserial-XYZ").
    Q_INVOKABLE QStringList availableSerialPorts() const;
    /// Session-Objekt einer Zeile (für Binding an TerminalItem.session).
    Q_INVOKABLE QObject *sessionAt(int row) const;
    /// Zeilenindex einer Session-ID, oder -1.
    int rowForId(int id) const;
    /// Session-Objekt zu einer stabilen ID (für externe Steuerung / MCP).
    Session *sessionById(int id) const;
    const QList<Session *> &sessions() const { return m_sessions; }
    Q_INVOKABLE void closeSession(int row);
    /// Verschiebt die Session von `from` an die Zielposition `to` (Drag-Reorder
    /// in der Sidebar). Persistiert die neue Reihenfolge.
    Q_INVOKABLE void moveSession(int from, int to);
    /// Beendet alle laufenden Prozesse/Verbindungen (beim App-Quit aufzurufen,
    /// nach saveState()). Verhindert verwaiste Shells/Agenten.
    Q_INVOKABLE void shutdownAll();
    /// Markiert die Zeile als aktiv/fokussiert (alle anderen inaktiv) — löscht deren Attention.
    Q_INVOKABLE void setActiveRow(int row);

    /// Stellt die Sessions aus den persistierten Einstellungen wieder her.
    /// Gibt die wiederherzustellende aktive Zeile zurück (oder -1, wenn nichts gespeichert).
    Q_INVOKABLE int restoreState();
    /// Schreibt den aktuellen Zustand (Session-Liste + aktive Zeile) in die Einstellungen.
    Q_INVOKABLE void saveState() const;

signals:
    void countChanged();
    /// Eine (nicht-fokussierte) Session fordert Aufmerksamkeit — für Fenster-Alert.
    void attentionRaised(int row);

private:
    void wireSession(Session *s, int row);

    /// Persistierbare Beschreibung einer Session (Inhalt ist nicht wiederherstellbar).
    struct SessionConfig {
        int type = 0;        // qtmux::Session::Type
        QString serialPort;  // nur bei Seriell
        int baud = 115200;   // nur bei Seriell
        QString workingDir;  // letztes Arbeitsverzeichnis (Shell), live in saveState() erfasst
        QString program;     // gewählte Shell (Shell), leer = Standard-Shell
        QString host;        // SSH
        int sshPort = 22;    // SSH
        QString user;        // SSH
        QString identity;    // SSH (Identity-Datei, optional)
    };

    QList<Session *> m_sessions;
    QList<SessionConfig> m_configs;   // parallel zu m_sessions
    int m_activeRow = -1;
    bool m_restoring = false;         // unterdrückt Persistierung während restoreState()
};

} // namespace qtmux
