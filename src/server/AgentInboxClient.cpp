#include "AgentInboxClient.h"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QLocalSocket>

#if defined(Q_OS_UNIX)
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>
#if defined(Q_OS_MACOS)
#include <sys/ucred.h>
#endif
#endif

namespace qtmux {

namespace {

#if defined(Q_OS_UNIX)
// PID des Prozesses am anderen Ende eines verbundenen Unix-Sockets, -1 wenn nicht lesbar.
qint64 peerPidOf(qintptr fd) {
    if (fd < 0) return -1;
#if defined(Q_OS_MACOS)
    pid_t pid = 0;
    socklen_t len = sizeof(pid);
    if (::getsockopt(static_cast<int>(fd), SOL_LOCAL, LOCAL_PEERPID, &pid, &len) != 0) return -1;
    return pid > 0 ? pid : -1;
#elif defined(Q_OS_LINUX)
    struct ucred cred {};
    socklen_t len = sizeof(cred);
    if (::getsockopt(static_cast<int>(fd), SOL_SOCKET, SO_PEERCRED, &cred, &len) != 0) return -1;
    return cred.pid > 0 ? cred.pid : -1;
#else
    return -1;
#endif
}
#endif

} // namespace

InboxDelivery deliverToInbox(const QString &socketPath, const QByteArray &frame,
                             const std::function<bool(qint64)> &peerAccepted, int timeoutMs) {
    InboxDelivery r;
#if !defined(Q_OS_UNIX)
    Q_UNUSED(socketPath) Q_UNUSED(frame) Q_UNUSED(peerAccepted) Q_UNUSED(timeoutMs)
    r.error = QCoreApplication::translate("AgentInbox", "Der Nachrichten-Eingang wird auf dieser Plattform nicht unterstützt.");
    return r;
#else
    if (frame.isEmpty() || !frame.endsWith('\n')) {
        r.error = QCoreApplication::translate("AgentInbox", "Interner Fehler: Der Rahmen ist keine vollständige Zeile.");
        return r;
    }
    // Ein Pfad von außen ist eine Eingabe: Vor dem Verbinden prüfen, dass dort wirklich
    // ein Socket liegt und dass er diesem Benutzer gehört. (Ein FIFO oder eine Datei
    // ließe das Verbinden zwar ohnehin scheitern — so sagt die Meldung aber, warum.)
    struct stat st {};
    const QByteArray native = socketPath.toLocal8Bit();
    if (::lstat(native.constData(), &st) != 0) {
        r.error = QCoreApplication::translate("AgentInbox", "Der Socket existiert nicht (mehr) — der Agent ist vermutlich beendet.");
        return r;
    }
    if (!S_ISSOCK(st.st_mode)) {
        r.error = QCoreApplication::translate("AgentInbox", "Unter der gemeldeten Adresse liegt kein Socket.");
        return r;
    }
    if (st.st_uid != ::getuid()) {
        r.error = QCoreApplication::translate("AgentInbox", "Der Socket gehört einem anderen Benutzer.");
        return r;
    }

    QElapsedTimer clock;
    clock.start();
    const auto left = [&] { return qMax<qint64>(1, timeoutMs - clock.elapsed()); };

    QLocalSocket sock;
    sock.connectToServer(socketPath, QIODevice::WriteOnly);
    if (!sock.waitForConnected(static_cast<int>(left()))) {
        r.error = QCoreApplication::translate("AgentInbox", "Verbinden fehlgeschlagen: %1").arg(sock.errorString());
        return r;
    }
    r.peerPid = peerPidOf(sock.socketDescriptor());
    if (r.peerPid <= 0) {
        sock.abort();
        r.error = QCoreApplication::translate("AgentInbox", "Wer am Socket lauscht, ließ sich nicht feststellen — nichts gesendet.");
        return r;
    }
    if (peerAccepted && !peerAccepted(r.peerPid)) {
        sock.abort();
        r.error = QCoreApplication::translate("AgentInbox", "Am Socket lauscht ein Prozess (PID %1), der nicht zu dieser "
                          "Session gehört — nichts gesendet.").arg(r.peerPid);
        return r;
    }

    const qint64 written = sock.write(frame);
    if (written != frame.size()) {
        sock.abort();
        r.error = QCoreApplication::translate("AgentInbox", "Schreiben fehlgeschlagen: %1").arg(sock.errorString());
        return r;
    }
    while (sock.bytesToWrite() > 0) {
        if (clock.elapsed() >= timeoutMs || !sock.waitForBytesWritten(static_cast<int>(left()))) {
            if (sock.bytesToWrite() == 0) break;
            const QString why = sock.state() == QLocalSocket::ConnectedState
                ? QCoreApplication::translate("AgentInbox", "Zeitüberschreitung — der Empfänger liest nicht")
                : sock.errorString();
            sock.abort();
            r.error = QCoreApplication::translate("AgentInbox", "Schreiben fehlgeschlagen: %1").arg(why);
            return r;
        }
    }
    sock.disconnectFromServer();
    if (sock.state() != QLocalSocket::UnconnectedState)
        sock.waitForDisconnected(static_cast<int>(qMin<qint64>(left(), 200)));
    r.ok = true;
    return r;
#endif
}

} // namespace qtmux
