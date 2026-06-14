#include "Pty.h"

#if defined(Q_OS_UNIX)

#include <QSocketNotifier>

#include "ProcessInfo.h"

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

#if defined(Q_OS_MACOS)
#  include <libproc.h>
#elif defined(Q_OS_LINUX)
#  include <limits.h>
#endif

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
                int cols, int rows, const QStringList &env,
                const QString &workingDir, const QString &argv0) {
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
        // --- Child: Startverzeichnis, Umgebung setzen und Shell exec'en ---
        if (!workingDir.isEmpty()) {
            if (::chdir(workingDir.toLocal8Bit().constData()) != 0) {
                // Fehlschlag ist nicht fatal — dann startet die Shell im geerbten Verzeichnis.
            }
        }
        for (const QString &kv : env) {
            const int eq = kv.indexOf('=');
            if (eq > 0) {
                qputenv(kv.left(eq).toLocal8Bit(), kv.mid(eq + 1).toLocal8Bit());
            }
        }

        // Such-/Ausfuehrungspfad bleibt `program`; argv[0] kann fuer Login-Shells
        // abweichen (fuehrendes '-', wie login(1)/Terminal.app). Daher execvp mit
        // dem echten Pfad als Such-Argument, argv[0] separat aus argvStore.
        const QByteArray execPath = program.toLocal8Bit();
        std::vector<QByteArray> argvStore;
        argvStore.push_back(argv0.isEmpty() ? execPath : argv0.toLocal8Bit());
        for (const QString &a : args) argvStore.push_back(a.toLocal8Bit());

        std::vector<char *> argv;
        for (auto &a : argvStore) argv.push_back(a.data());
        argv.push_back(nullptr);

        ::execvp(execPath.constData(), argv.data());
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

    const pid_t pid = static_cast<pid_t>(m_pid);

    // Nachfahren (z. B. ein gestarteter Agent) ERFASSEN, solange der Baum intakt
    // ist — nach dem Tod der Shell reparenten sie zu init/launchd. So beenden wir
    // auch Prozesse, die das PTY-HUP ignorieren (Agent läuft sonst verwaist weiter).
    QList<qint64> tree;
    if (pid > 0) tree = procinfo::descendantPids(pid);

    // Höflich beenden: SIGHUP an Shell + alle Nachfahren, dann Master schließen
    // (löst zusätzlich SIGHUP an die Vordergrund-Prozessgruppe aus).
    if (pid > 0) ::kill(pid, SIGHUP);
    for (qint64 p : tree) if (p > 0) ::kill(static_cast<pid_t>(p), SIGHUP);

    if (d->masterFd >= 0) {
        ::close(d->masterFd);
        d->masterFd = -1;
    }

    // Kurze Gnadenfrist, dann hart beenden, was noch lebt.
    int status = 0;
    if (pid > 0) {
        bool reaped = false;
        for (int i = 0; i < 20; ++i) {                 // ~200 ms
            if (::waitpid(pid, &status, WNOHANG) == pid) { reaped = true; break; }
            ::usleep(10 * 1000);
        }
        if (!reaped) {
            ::kill(pid, SIGKILL);
            ::waitpid(pid, &status, 0);
        }
    }
    // Überlebende Nachfahren hart beenden (sie sind nicht unsere direkten Kinder
    // und werden von init reaped — SIGKILL genügt).
    for (qint64 p : tree) if (p > 0) ::kill(static_cast<pid_t>(p), SIGKILL);

    const int exitCode = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
    m_pid = 0;
    emit finished(exitCode);
}

QString Pty::currentWorkingDirectory() const {
    if (m_pid <= 0) return {};
#if defined(Q_OS_MACOS)
    struct proc_vnodepathinfo vpi;
    const int ret = ::proc_pidinfo(static_cast<int>(m_pid), PROC_PIDVNODEPATHINFO, 0,
                                   &vpi, sizeof(vpi));
    if (ret == static_cast<int>(sizeof(vpi)))
        return QString::fromLocal8Bit(vpi.pvi_cdir.vip_path);
    return {};
#elif defined(Q_OS_LINUX)
    char buf[PATH_MAX];
    const QByteArray link = "/proc/" + QByteArray::number(m_pid) + "/cwd";
    const ssize_t n = ::readlink(link.constData(), buf, sizeof(buf) - 1);
    if (n > 0) return QString::fromLocal8Bit(buf, static_cast<int>(n));
    return {};
#else
    return {};
#endif
}

} // namespace qtmux

#endif // Q_OS_UNIX
