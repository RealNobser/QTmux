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
    /// `workingDir` ist das Startverzeichnis (leer = vom Elternprozess erben).
    /// `argv0` ueberschreibt — wenn nicht leer — den an den Kindprozess
    /// uebergebenen Namen (argv[0]), waehrend `program` weiterhin der zu
    /// suchende/auszufuehrende Pfad bleibt. Genutzt fuer Login-Shells
    /// (argv[0] mit fuehrendem '-', wie login(1)/Terminal.app). Nur Unix;
    /// auf Windows ignoriert (ConPTY kennt kein argv[0]-Login-Konzept).
    bool start(const QString &program, const QStringList &args,
               int cols, int rows, const QStringList &env = {},
               const QString &workingDir = {}, const QString &argv0 = {});

    /// Schreibt `data` Richtung Kindprozess. Der PTY-Master ist nicht-blockierend und
    /// nimmt pro ::write() nur den Platz im Kernel-Puffer (~1 KB) auf — der Rest wird
    /// daher gepuffert und ueber einen Write-Notifier nachgeliefert (QTMUX-28; frueher
    /// ging alles ueber den Puffer hinaus still verloren). Rueckgabe: die uebernommenen
    /// Bytes (== data.size(), da vollstaendig gepuffert) bzw. -1, wenn kein PTY laeuft.
    qint64 write(const QByteArray &data);
    void resize(int cols, int rows);
    void terminate();

    bool isRunning() const { return m_running; }
    qint64 pid() const { return m_pid; }
    QString lastError() const { return m_lastError; }

    /// Aktuelles Arbeitsverzeichnis des Kindprozesses (leer, wenn nicht ermittelbar).
    QString currentWorkingDirectory() const;

    /// App-Quit-Modus: terminate() beendet dann synchron + nicht-blockierend
    /// (SIGKILL an den ganzen Baum, KEIN waitpid — der Prozess endet ohnehin und
    /// das OS reapt die Zombies). So sterben auch HUP-ignorierende Nachfahren
    /// garantiert VOR dem Prozess-Exit. Im Normalbetrieb (false) reapt terminate()
    /// asynchron, um den GUI-Thread nie zu blockieren. Vor shutdownAll() setzen.
    static void setQuitting(bool on) { s_quitting = on; }
    static bool quitting() { return s_quitting; }

signals:
    void readyRead(const QByteArray &data);
    /// Prozess wurde beendet (exitCode, ob normal beendet).
    void finished(int exitCode);

private:
    void onMasterReadable();
    /// Schreibt so viel wie moeglich aus dem Ausgangspuffer und (de)aktiviert den
    /// Write-Notifier entsprechend. Auf Windows ein No-Op (WriteFile schreibt voll).
    void flushPendingWrites();

    struct Private;            // plattformspezifische Felder
    Private *d = nullptr;

    bool m_running = false;
    qint64 m_pid = 0;
    QString m_lastError;

    static bool s_quitting;   // App-Quit: synchroner, nicht-blockierender terminate()
};

} // namespace qtmux
