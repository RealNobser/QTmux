#pragma once

#include <QObject>
#include <QByteArray>
#include <QString>

namespace qtmux {

/// Zustand eines Backends — speist die Status-Ringe in der Sidebar (cmux-Stil).
enum class BackendState {
    Starting,
    Running,
    WaitingInput,
    Error,
    Closed
};

/// Einheitliche Schnittstelle fuer alles, was Terminal-Bytes liest/schreibt.
/// Implementierungen: PtyBackend (lokale Shell/Agenten), SshBackend, SerialBackend, AppBackend.
/// So funktionieren Sidebar, Rendering und Layout fuer alle Backend-Typen identisch.
class ITerminalBackend : public QObject {
    Q_OBJECT
public:
    explicit ITerminalBackend(QObject *parent = nullptr) : QObject(parent) {}
    ~ITerminalBackend() override = default;

    /// Startet die Verbindung/den Prozess mit der gegebenen Terminalgroesse.
    virtual bool start(int cols, int rows) = 0;
    /// Schreibt Nutzereingaben Richtung Backend.
    virtual void write(const QByteArray &data) = 0;
    /// Teilt dem Backend eine neue Terminalgroesse mit.
    virtual void resize(int cols, int rows) = 0;
    /// Beendet die Verbindung/den Prozess.
    virtual void terminate() = 0;

    /// Aktuelles Arbeitsverzeichnis (leer, wenn nicht zutreffend/ermittelbar).
    virtual QString currentWorkingDirectory() const { return {}; }

    BackendState state() const { return m_state; }

signals:
    /// Vom Backend empfangene Bytes (gehen in den VT-Parser).
    void dataReceived(const QByteArray &data);
    /// Zustandswechsel (fuer Status-Ringe / Notifications).
    void stateChanged(qtmux::BackendState state);

protected:
    void setState(BackendState s) {
        if (m_state != s) { m_state = s; emit stateChanged(s); }
    }

private:
    BackendState m_state = BackendState::Starting;
};

} // namespace qtmux
