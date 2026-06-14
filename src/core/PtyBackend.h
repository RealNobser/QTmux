#pragma once

#include "ITerminalBackend.h"
#include "Pty.h"
#include <QString>
#include <QStringList>

namespace qtmux {

/// Backend für lokale Shells und CLI-Agenten (Claude Code, Codex, …) über ein PTY.
class PtyBackend : public ITerminalBackend {
    Q_OBJECT
public:
    explicit PtyBackend(QObject *parent = nullptr);

    /// Standard-Login-Shell des Nutzers ($SHELL bzw. plattform-Fallback).
    static QString defaultShell();

    void setProgram(const QString &program) { m_program = program; }
    void setArguments(const QStringList &args) { m_args = args; }
    void setExtraEnv(const QStringList &env) { m_env = env; }
    /// Startverzeichnis (leer = Home-Verzeichnis des Nutzers).
    void setWorkingDirectory(const QString &dir) { m_workingDir = dir; }
    /// Als Login-Shell starten (Unix): argv[0] mit fuehrendem '-', damit zsh/bash
    /// ihre Login-Startupdateien laden (~/.zprofile, /etc/zprofile → path_helper/
    /// Homebrew-PATH, ~/.bash_profile …). Nur fuer interaktive Default-Shells ohne
    /// eigene Argumente sinnvoll; greift daher nur, wenn keine Args vorliegen.
    void setLoginShell(bool on) { m_loginShell = on; }

    bool start(int cols, int rows) override;
    void write(const QByteArray &data) override;
    void resize(int cols, int rows) override;
    void terminate() override;
    QString currentWorkingDirectory() const override { return m_pty.currentWorkingDirectory(); }
    qint64 processId() const override { return m_pty.pid(); }

private:
    Pty m_pty;
    QString m_program;
    QStringList m_args;
    QStringList m_env;
    QString m_workingDir;   // leer => Home (siehe start())
    bool m_loginShell = false;
};

} // namespace qtmux
