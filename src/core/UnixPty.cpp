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
#include <thread>

#if defined(Q_OS_MACOS)
#  include <libproc.h>
#elif defined(Q_OS_LINUX)
#  include <limits.h>
#endif

namespace qtmux {

struct Pty::Private {
    int masterFd = -1;
    QSocketNotifier *notifier = nullptr;

    // Ausgangspuffer (QTMUX-28): der Master ist O_NONBLOCK und nimmt pro ::write()
    // nur den freien Kernel-Puffer (~1 KB) auf. Der Rest bleibt hier liegen und wird
    // ueber `writeNotifier` nachgeliefert, sobald der FD wieder schreibbereit ist.
    // `pendingPos` ist der Lesezeiger, damit nicht bei jedem Teilschreibvorgang der
    // Pufferanfang umkopiert werden muss (sonst O(n^2) bei grossen Einfuegungen).
    QByteArray pending;
    int pendingPos = 0;
    QSocketNotifier *writeNotifier = nullptr;
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

    // Write-Notifier: nur aktiv, solange etwas im Ausgangspuffer wartet (sonst wuerde
    // er dauernd feuern, da ein leerer FD praktisch immer schreibbereit ist).
    d->writeNotifier = new QSocketNotifier(master, QSocketNotifier::Write, this);
    d->writeNotifier->setEnabled(false);
    connect(d->writeNotifier, &QSocketNotifier::activated, this,
            [this]() { flushPendingWrites(); });

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
    if (data.isEmpty()) return 0;
    // Immer erst puffern, dann so viel wie moeglich rausschreiben. Ein einzelnes
    // ::write() wuerde bei O_NONBLOCK nur den freien Kernel-Puffer aufnehmen und den
    // Rest verlieren (QTMUX-28) — der Aufrufer prueft den Rueckgabewert nirgends.
    d->pending.append(data);
    flushPendingWrites();
    return data.size();
}

void Pty::flushPendingWrites() {
    if (d->masterFd < 0) return;

    while (d->pendingPos < d->pending.size()) {
        const char *p = d->pending.constData() + d->pendingPos;
        const size_t remaining = static_cast<size_t>(d->pending.size() - d->pendingPos);
        const ssize_t n = ::write(d->masterFd, p, remaining);

        if (n > 0) {
            d->pendingPos += static_cast<int>(n);
            continue;
        }
        if (n < 0) {
            if (errno == EINTR) continue;               // unterbrochen -> erneut versuchen
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // Kernel-Puffer voll: Rest liegen lassen und aufwachen, sobald wieder
                // Platz ist. Ohne das ginge der Rest verloren (bzw. wir wuerden spinnen).
                if (d->writeNotifier) d->writeNotifier->setEnabled(true);
                return;
            }
            // Echter Schreibfehler (z. B. Gegenseite weg) -> Puffer verwerfen.
            d->pending.clear();
            d->pendingPos = 0;
            if (d->writeNotifier) d->writeNotifier->setEnabled(false);
            return;
        }
        break;                                          // n == 0: nichts geschrieben
    }

    if (d->pendingPos >= d->pending.size()) {           // alles raus
        d->pending.clear();
        d->pendingPos = 0;
        if (d->writeNotifier) d->writeNotifier->setEnabled(false);
    } else if (d->pendingPos > 65536) {                 // Vorderteil gelegentlich abschneiden
        d->pending.remove(0, d->pendingPos);
        d->pendingPos = 0;
    }
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
    if (d->writeNotifier) {
        d->writeNotifier->setEnabled(false);
        d->writeNotifier->deleteLater();
        d->writeNotifier = nullptr;
    }
    d->pending.clear();
    d->pendingPos = 0;

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

    if (s_quitting) {
        // App-Quit: synchron + NICHT blockierend. SIGKILL sofort an Shell + Baum
        // (kein waitpid — der Prozess endet ohnehin, das OS reapt die Zombies). So
        // sterben auch HUP-ignorierende Nachfahren garantiert vor dem Prozess-Exit;
        // ein detached Thread liefe sonst evtl. nicht mehr, bevor main() endet.
        if (pid > 0) ::kill(pid, SIGKILL);
        for (qint64 p : tree) if (p > 0) ::kill(static_cast<pid_t>(p), SIGKILL);
    } else {
        // Normalbetrieb: Gnadenfrist, Hart-Beenden und das ABERNTEN (waitpid) laufen
        // in einem detached Thread — NIEMALS im aufrufenden (GUI-)Thread. Das
        // blockierende waitpid kann Sekunden dauern: ein per SIGKILL beendeter
        // Prozess mit vielen Threads/Mach-Ports (z. B. ein node-/Agent-Baum) hängt
        // im Kernel-Teardown ("exiting"), und waitpid kehrt erst danach zurück.
        // Würde das im GUI-Thread passieren (terminate wird u. a. aus ~Session via
        // deleteLater gerufen), fröre die ganze App ein (bunter Kreisel), sobald man
        // mehrere solche Sessions schließt. Der Thread reapt unsere direkte Shell
        // (sonst bliebe sie Zombie); die reparenteten Nachfahren erledigt launchd
        // nach dem SIGKILL.
        if (pid > 0 || !tree.isEmpty()) {
            std::thread([pid, tree]() {
                int status = 0;
                if (pid > 0) {
                    bool reaped = false;
                    for (int i = 0; i < 20; ++i) {                 // ~200 ms
                        if (::waitpid(pid, &status, WNOHANG) == pid) { reaped = true; break; }
                        ::usleep(10 * 1000);
                    }
                    if (!reaped) {
                        ::kill(pid, SIGKILL);
                        ::waitpid(pid, &status, 0);   // blockiert NUR diesen Thread
                    }
                }
                for (qint64 p : tree) if (p > 0) ::kill(static_cast<pid_t>(p), SIGKILL);
            }).detach();
        }
    }

    // exitCode wird beim asynchronen Abernten nicht mehr ausgewertet (der Closed-
    // Zustand hängt nicht vom genauen Code ab); -1 = "ohne bekannten Exit-Code".
    m_pid = 0;
    emit finished(-1);
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
