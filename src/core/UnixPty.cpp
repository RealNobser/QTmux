#include "Pty.h"

#if defined(Q_OS_UNIX)

#include <QSocketNotifier>

#if defined(Q_OS_MACOS) || defined(Q_OS_BSD4)
#  include <util.h>          // forkpty auf macOS/BSD
#else
#  include <pty.h>           // forkpty auf Linux
#endif

#include <unistd.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <signal.h>
#include <fcntl.h>
#include <cerrno>
#include <cstring>
#include <vector>

namespace qtmux {

struct Pty::Private {
    int masterFd = -1;
    QSocketNotifier *notifier = nullptr;
};

Pty::Pty(QObject *parent) : QObject(parent), d(new Private) {}

Pty::~Pty() {
    terminate();
    delete d;
}

bool Pty::start(const QString &program, const QStringList &args,
                int cols, int rows, const QStringList &env) {
    if (m_running) {
        m_lastError = QStringLiteral("PTY läuft bereits");
        return false;
    }

    struct winsize ws {};
    ws.ws_col = static_cast<unsigned short>(cols > 0 ? cols : 80);
    ws.ws_row = static_cast<unsigned short>(rows > 0 ? rows : 24);

    int master = -1;
    pid_t pid = forkpty(&master, nullptr, nullptr, &ws);

    if (pid < 0) {
        m_lastError = QStringLiteral("forkpty fehlgeschlagen: %1").arg(QString::fromLocal8Bit(strerror(errno)));
        return false;
    }

    if (pid == 0) {
        // --- Child: Umgebung setzen und Shell exec'en ---
        for (const QString &kv : env) {
            const int eq = kv.indexOf('=');
            if (eq > 0) {
                qputenv(kv.left(eq).toLocal8Bit(), kv.mid(eq + 1).toLocal8Bit());
            }
        }

        std::vector<QByteArray> argvStore;
        argvStore.push_back(program.toLocal8Bit());
        for (const QString &a : args) argvStore.push_back(a.toLocal8Bit());

        std::vector<char *> argv;
        for (auto &a : argvStore) argv.push_back(a.data());
        argv.push_back(nullptr);

        ::execvp(argv[0], argv.data());
        // Nur erreichbar, wenn exec fehlschlägt:
        ::perror("execvp");
        ::_exit(127);
    }

    // --- Parent ---
    m_pid = pid;
    d->masterFd = master;

    // Master-FD nicht-blockierend lesen.
    int flags = ::fcntl(master, F_GETFL, 0);
    ::fcntl(master, F_SETFL, flags | O_NONBLOCK);

    d->notifier = new QSocketNotifier(master, QSocketNotifier::Read, this);
    connect(d->notifier, &QSocketNotifier::activated, this, [this]() { onMasterReadable(); });

    m_running = true;
    return true;
}

void Pty::onMasterReadable() {
    char buf[8192];
    for (;;) {
        const ssize_t n = ::read(d->masterFd, buf, sizeof(buf));
        if (n > 0) {
            emit readyRead(QByteArray(buf, static_cast<int>(n)));
            if (n < static_cast<ssize_t>(sizeof(buf))) break; // wahrscheinlich leergelesen
        } else if (n < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) break; // nichts mehr da
            if (errno == EINTR) continue;
            // Lesefehler/EOF (Shell beendet)
            terminate();
            return;
        } else { // n == 0 -> EOF
            terminate();
            return;
        }
    }
}

qint64 Pty::write(const QByteArray &data) {
    if (!m_running || d->masterFd < 0) return -1;
    const ssize_t n = ::write(d->masterFd, data.constData(), static_cast<size_t>(data.size()));
    return static_cast<qint64>(n);
}

void Pty::resize(int cols, int rows) {
    if (!m_running || d->masterFd < 0) return;
    struct winsize ws {};
    ws.ws_col = static_cast<unsigned short>(cols > 0 ? cols : 80);
    ws.ws_row = static_cast<unsigned short>(rows > 0 ? rows : 24);
    ::ioctl(d->masterFd, TIOCSWINSZ, &ws);
}

void Pty::terminate() {
    if (!m_running) return;
    m_running = false;

    if (d->notifier) {
        d->notifier->setEnabled(false);
        d->notifier->deleteLater();
        d->notifier = nullptr;
    }
    if (d->masterFd >= 0) {
        ::close(d->masterFd);
        d->masterFd = -1;
    }

    int status = 0;
    if (m_pid > 0) {
        ::kill(static_cast<pid_t>(m_pid), SIGHUP);
        ::waitpid(static_cast<pid_t>(m_pid), &status, 0);
    }
    const int exitCode = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
    m_pid = 0;
    emit finished(exitCode);
}

} // namespace qtmux

#endif // Q_OS_UNIX
