#pragma once

#include <QObject>
#include <QString>
#include <memory>

#include "ITerminalBackend.h"

namespace qtmux {

class VtScreen;

/// Eine Terminal-Session: bündelt ein Backend (PTY/SSH/Serial/App) mit seinem
/// VT-Screen und verdrahtet beide. Backend-agnostisch — die UI spricht nur mit Session.
class Session : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString title READ title NOTIFY titleChanged)
    Q_PROPERTY(int state READ stateInt NOTIFY stateChanged)
    Q_PROPERTY(QString agentId READ agentId NOTIFY agentChanged)
    Q_PROPERTY(bool needsAttention READ needsAttention NOTIFY attentionChanged)
    Q_PROPERTY(int activity READ activityInt NOTIFY activityChanged)
    Q_PROPERTY(QString lastNotification READ lastNotification NOTIFY notificationChanged)
public:
    enum class Type { Shell, Ssh, Serial, App };
    Q_ENUM(Type)

    // Werte bewusst kompatibel mit der Sidebar-Ring-Logik (1=grün, 2=gelb, 3=rot, 4=grau).
    enum class Activity { Idle = 0, Running = 1, Waiting = 2, Error = 3, Closed = 4 };
    Q_ENUM(Activity)

    explicit Session(QObject *parent = nullptr);
    ~Session() override;

    /// Übernimmt Besitz des Backends und verbindet es mit einem frischen VtScreen.
    void attachBackend(ITerminalBackend *backend, Type type, int cols, int rows);

    int id() const { return m_id; }
    Type type() const { return m_type; }
    QString title() const { return m_title; }
    /// Sichtbarer Bildschirm als Klartext (für externe Steuerung / MCP).
    QString screenText() const;
    /// Aktuelles Arbeitsverzeichnis des Backends (für Persistenz).
    QString currentWorkingDirectory() const {
        return m_backend ? m_backend->currentWorkingDirectory() : QString();
    }
    QString agentId() const { return m_agentId; }
    bool needsAttention() const { return m_needsAttention; }
    Activity activity() const { return m_activity; }
    int activityInt() const { return static_cast<int>(m_activity); }
    QString lastNotification() const { return m_lastNotification; }

    /// Markiert die Session als aktiv (sichtbar/fokussiert). Aktivieren löscht
    /// einen anstehenden Aufmerksamkeits-Hinweis.
    void setActive(bool active);

    /// Setzt den Anzeigetitel (z. B. Portname für serielle Sessions).
    void setTitle(const QString &t);
    BackendState state() const { return m_backend ? m_backend->state() : BackendState::Closed; }
    int stateInt() const { return static_cast<int>(state()); }

    VtScreen *screen() const { return m_screen.get(); }

    void start(int cols, int rows);
    void write(const QByteArray &data);
    void resize(int cols, int rows);

signals:
    void titleChanged(const QString &title);
    void stateChanged();
    void agentChanged();
    void attentionChanged();
    void activityChanged();
    void notificationChanged();
    void bell();

private:
    void setActivity(Activity a);
    void raiseAttention();                      // setzt needsAttention (wenn inaktiv)
    void observeInput(const QByteArray &data);  // erkennt getippte Agenten-Kommandos
    void onBell();                              // Bell -> Aufmerksamkeit, wenn inaktiv
    void onNotify(const QString &text);         // OSC 9/777
    void onPromptMarker(char kind, int exitCode); // OSC 133

    std::unique_ptr<ITerminalBackend> m_backend;
    std::unique_ptr<VtScreen> m_screen;
    Type m_type = Type::Shell;
    QString m_title = QStringLiteral("Shell");
    QString m_agentId;
    QString m_inputLine;       // Puffer der aktuell getippten Zeile
    bool m_titleFromAgent = false;
    bool m_active = false;
    bool m_needsAttention = false;
    Activity m_activity = Activity::Running;
    QString m_lastNotification;
    bool m_commandRunning = false;   // zwischen OSC 133;C und ;D
    int m_cols = 80;
    int m_rows = 24;
    int m_id = nextId();
    static int nextId();
};

} // namespace qtmux
