#pragma once

#include "PtyBackend.h"

namespace qtmux {

/// Backend für SSH-Verbindungen über den System-`ssh`-Client in einem PTY.
/// Dadurch funktionieren Passwort-/Key-Abfrage, known_hosts, ~/.ssh/config,
/// Agent-Forwarding und Resize wie im echten Terminal — der gesamte PTY-Stack wird genutzt.
class SshBackend : public PtyBackend {
    Q_OBJECT
public:
    explicit SshBackend(QObject *parent = nullptr);

    void setHost(const QString &h) { m_host = h; }
    void setPort(int p) { m_port = p; }
    void setUser(const QString &u) { m_user = u; }
    void setIdentityFile(const QString &f) { m_identity = f; }

    QString host() const { return m_host; }
    int port() const { return m_port; }
    QString user() const { return m_user; }
    QString identityFile() const { return m_identity; }
    /// Anzeigeziel, z. B. "user@host".
    QString target() const { return m_user.isEmpty() ? m_host : m_user + QLatin1Char('@') + m_host; }

    bool start(int cols, int rows) override;

private:
    QString m_host;
    int m_port = 22;
    QString m_user;
    QString m_identity;
};

} // namespace qtmux
