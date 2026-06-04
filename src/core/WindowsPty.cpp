#include "Pty.h"

#if defined(Q_OS_WIN)

// ConPTY-Implementierung (Windows 10 1809+ / Build 17763).
//
// STATUS: Skeleton — auf einer Windows-Maschine zu vervollständigen und zu testen.
// Geplante Umsetzung:
//   1. CreatePipe() für Input/Output.
//   2. CreatePseudoConsole(size, hInput, hOutput, 0, &hPC).
//   3. STARTUPINFOEX mit PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE.
//   4. CreateProcess() der Shell.
//   5. Lesen der Output-Pipe via QWinEventNotifier + Overlapped I/O
//      (QSocketNotifier funktioniert auf Windows nicht für Pipes).
//   6. resize -> ResizePseudoConsole(); terminate -> ClosePseudoConsole() + TerminateProcess().

namespace qtmux {

struct Pty::Private {};

Pty::Pty(QObject *parent) : QObject(parent), d(nullptr) {}
Pty::~Pty() {}

bool Pty::start(const QString &, const QStringList &, int, int, const QStringList &, const QString &) {
    m_lastError = QStringLiteral("ConPTY-Backend ist noch nicht implementiert (Windows).");
    return false;
}

qint64 Pty::write(const QByteArray &) { return -1; }
void Pty::resize(int, int) {}
void Pty::terminate() {}

} // namespace qtmux

#endif // Q_OS_WIN
