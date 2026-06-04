#pragma once

#include <QObject>
#include <QByteArray>
#include <QString>
#include <QStringList>

namespace qtmux {

/// Plattformneutrale Pseudo-Terminal-Abstraktion.
///
/// Implementierungen pro Plattform (per Praeprozessor ausgewaehlt):
///   - Unix (macOS/Linux): forkpty()  -> UnixPty.cpp
///   - Windows:            ConPTY      -> WindowsPty.cpp
///
/// Bewusst dependency-frei gehalten (kein ptyqt): ptyqt setzt auf das in Qt6
/// entfernte QProcess::setupChildProcess() und ist nicht Qt6-kompatibel.
class Pty : public QObject {
    Q_OBJECT
public:
    explicit Pty(QObject *parent = nullptr);
    ~Pty() override;

    /// Startet `program` mit `args` in einem PTY der Groesse cols x rows.
    /// `env` ergaenzt/ueberschreibt Umgebungsvariablen (Form: "KEY=VALUE").
    bool start(const QString &program, const QStringList &args,
               int cols, int rows, const QStringList &env = {});

    qint64 write(const QByteArray &data);
    void resize(int cols, int rows);
    void terminate();

    bool isRunning() const { return m_running; }
    qint64 pid() const { return m_pid; }
    QString lastError() const { return m_lastError; }

signals:
    void readyRead(const QByteArray &data);
    /// Prozess wurde beendet (exitCode, ob normal beendet).
    void finished(int exitCode);

private:
    void onMasterReadable();

    struct Private;            // plattformspezifische Felder
    Private *d = nullptr;

    bool m_running = false;
    qint64 m_pid = 0;
    QString m_lastError;
};

} // namespace qtmux
