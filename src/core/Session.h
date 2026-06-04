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
public:
    enum class Type { Shell, Ssh, Serial, App };
    Q_ENUM(Type)

    explicit Session(QObject *parent = nullptr);
    ~Session() override;

    /// Übernimmt Besitz des Backends und verbindet es mit einem frischen VtScreen.
    void attachBackend(ITerminalBackend *backend, Type type, int cols, int rows);

    Type type() const { return m_type; }
    QString title() const { return m_title; }
    QString agentId() const { return m_agentId; }
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
    void bell();

private:
    void setTitle(const QString &t);
    void observeInput(const QByteArray &data);  // erkennt getippte Agenten-Kommandos

    std::unique_ptr<ITerminalBackend> m_backend;
    std::unique_ptr<VtScreen> m_screen;
    Type m_type = Type::Shell;
    QString m_title = QStringLiteral("Shell");
    QString m_agentId;
    QString m_inputLine;       // Puffer der aktuell getippten Zeile
    bool m_titleFromAgent = false;
    int m_cols = 80;
    int m_rows = 24;
};

} // namespace qtmux
