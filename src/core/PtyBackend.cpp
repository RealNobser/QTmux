#include "PtyBackend.h"

#include <QProcessEnvironment>
#include <QProcess>
#include <QDir>

namespace qtmux {

PtyBackend::PtyBackend(QObject *parent) : ITerminalBackend(parent) {
    connect(&m_pty, &Pty::readyRead, this, &PtyBackend::dataReceived);
    connect(&m_pty, &Pty::finished, this, [this](int) {
        setState(BackendState::Closed);
    });
}

QString PtyBackend::defaultShell() {
    const QString shell = QProcessEnvironment::systemEnvironment().value(QStringLiteral("SHELL"));
    if (!shell.isEmpty()) return shell;
#if defined(Q_OS_WIN)
    return QStringLiteral("powershell.exe");
#elif defined(Q_OS_MACOS)
    return QStringLiteral("/bin/zsh");
#else
    return QStringLiteral("/bin/bash");
#endif
}

bool PtyBackend::start(int cols, int rows) {
    QString program = m_program.isEmpty() ? defaultShell() : m_program;
    QStringList args = m_args;

    // Wurden keine Argumente separat gesetzt, darf das Programm eine vollständige
    // Kommandozeile sein (z. B. die Clink-Shell `cmd.exe /k "<clink.bat>" inject
    // --quiet`, QTMUX-25). QProcess::splitCommand respektiert Anführungszeichen, so
    // bleiben Pfade mit Leerzeichen zusammen; ein einfacher Programmname/-pfad ergibt
    // genau ein Token und verhält sich unverändert. SshBackend setzt m_args explizit
    // und ist davon nicht betroffen.
    if (args.isEmpty()) {
        const QStringList parts = QProcess::splitCommand(program);
        if (parts.size() > 1) {
            program = parts.first();
            args = parts.mid(1);
        }
    }

    // Terminal-Typ setzen, damit Programme Farben/Capabilities erkennen.
    QStringList env = m_env;
    env << QStringLiteral("TERM=xterm-256color");

    // Leeres Startverzeichnis => Home-Verzeichnis des Nutzers.
    const QString workingDir = m_workingDir.isEmpty() ? QDir::homePath() : m_workingDir;

    setState(BackendState::Starting);
    if (!m_pty.start(program, args, cols, rows, env, workingDir)) {
        setState(BackendState::Error);
        return false;
    }
    setState(BackendState::Running);
    return true;
}

void PtyBackend::write(const QByteArray &data) { m_pty.write(data); }
void PtyBackend::resize(int cols, int rows) { m_pty.resize(cols, rows); }
void PtyBackend::terminate() { m_pty.terminate(); }

} // namespace qtmux
