#include "ProcessInfo.h"

#include <QHash>

// Plattformspezifische Prozess-Inspektion. macOS: libproc + sysctl.
// Linux: /proc. Windows: aktuell Stubs (PTY-Backend dort noch Skelett).

#if defined(Q_OS_MACOS)
#  include <libproc.h>
#  include <sys/proc_info.h>
#  include <arpa/inet.h>   // ntohs
#  include <vector>
#elif defined(Q_OS_LINUX)
#  include <QByteArray>
#  include <QDir>
#  include <QFile>
#  include <QFileInfo>
#  include <dirent.h>
#elif defined(Q_OS_WIN)
#  include <winsock2.h>      // vor windows.h, sonst Konflikt mit winsock.h
#  include <ws2tcpip.h>
#  include <windows.h>
#  include <tlhelp32.h>
#  include <iphlpapi.h>
#endif

namespace qtmux::procinfo {

#if defined(Q_OS_MACOS)

static std::vector<pid_t> allPids() {
    int n = proc_listpids(PROC_ALL_PIDS, 0, nullptr, 0);
    if (n <= 0) return {};
    std::vector<pid_t> pids(n / sizeof(pid_t) + 32, 0);
    n = proc_listpids(PROC_ALL_PIDS, 0, pids.data(),
                      static_cast<int>(pids.size() * sizeof(pid_t)));
    if (n <= 0) return {};
    pids.resize(n / sizeof(pid_t));
    return pids;
}

qint64 pidOfTcpClient(quint16 clientPort, quint16 serverPort) {
    for (pid_t pid : allPids()) {
        if (pid <= 0) continue;
        int bufsz = proc_pidinfo(pid, PROC_PIDLISTFDS, 0, nullptr, 0);
        if (bufsz <= 0) continue;
        std::vector<proc_fdinfo> fds(bufsz / sizeof(proc_fdinfo) + 8);
        bufsz = proc_pidinfo(pid, PROC_PIDLISTFDS, 0, fds.data(),
                             static_cast<int>(fds.size() * sizeof(proc_fdinfo)));
        const int fdcount = bufsz > 0 ? bufsz / static_cast<int>(sizeof(proc_fdinfo)) : 0;
        for (int f = 0; f < fdcount; ++f) {
            if (fds[f].proc_fdtype != PROX_FDTYPE_SOCKET) continue;
            struct socket_fdinfo si {};
            const int r = proc_pidfdinfo(pid, fds[f].proc_fd, PROC_PIDFDSOCKETINFO,
                                         &si, sizeof(si));
            if (r != sizeof(si)) continue;
            if (si.psi.soi_kind != SOCKINFO_TCP) continue;
            const auto &in = si.psi.soi_proto.pri_tcp.tcpsi_ini;
            const quint16 lport = ntohs(static_cast<uint16_t>(in.insi_lport));
            const quint16 fport = ntohs(static_cast<uint16_t>(in.insi_fport));
            if (lport == clientPort && fport == serverPort) return pid;
        }
    }
    return -1;
}

QList<qint64> ancestorPids(qint64 pid) {
    QList<qint64> chain;
    qint64 cur = pid;
    for (int guard = 0; cur > 1 && guard < 64; ++guard) {
        chain.append(cur);
        struct proc_bsdinfo bsd {};
        if (proc_pidinfo(static_cast<int>(cur), PROC_PIDTBSDINFO, 0, &bsd, sizeof(bsd))
            != sizeof(bsd))
            break;
        cur = bsd.pbi_ppid;
    }
    return chain;
}

QList<qint64> descendantPids(qint64 root) {
    QHash<qint64, qint64> parentOf;
    for (pid_t pid : allPids()) {
        if (pid <= 0) continue;
        struct proc_bsdinfo bsd {};
        const int r = proc_pidinfo(pid, PROC_PIDTBSDINFO, 0, &bsd, sizeof(bsd));
        if (r == sizeof(bsd)) parentOf.insert(pid, bsd.pbi_ppid);
    }
    QList<qint64> out;
    QList<qint64> stack{root};
    while (!stack.isEmpty()) {
        const qint64 cur = stack.takeLast();
        for (auto it = parentOf.constBegin(); it != parentOf.constEnd(); ++it) {
            if (it.value() == cur && it.key() != root && !out.contains(it.key())) {
                out.append(it.key());
                stack.append(it.key());
            }
        }
    }
    return out;
}

#elif defined(Q_OS_LINUX)

static qint64 ppidOf(qint64 pid) {
    QFile f(QStringLiteral("/proc/%1/stat").arg(pid));
    if (!f.open(QIODevice::ReadOnly)) return -1;
    const QByteArray s = f.readAll();
    // Format: pid (comm) state ppid ...  — comm kann Leerzeichen/Klammern enthalten.
    const int rp = s.lastIndexOf(')');
    if (rp < 0) return -1;
    const QList<QByteArray> rest = s.mid(rp + 2).split(' ');
    if (rest.size() < 2) return -1;
    return rest.at(1).toLongLong();   // Feld 4 (ppid): state, ppid
}

qint64 pidOfTcpClient(quint16 clientPort, quint16 serverPort) {
    // Inode der Verbindung 127.0.0.1:clientPort -> 127.0.0.1:serverPort suchen.
    auto findInode = [&](const QString &path) -> qint64 {
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly)) return -1;
        f.readLine();  // Kopfzeile
        while (!f.atEnd()) {
            const QList<QByteArray> col = f.readLine().simplified().split(' ');
            if (col.size() < 10) continue;
            const QList<QByteArray> local = col.at(1).split(':');
            const QList<QByteArray> rem = col.at(2).split(':');
            if (local.size() != 2 || rem.size() != 2) continue;
            const quint16 lport = local.at(1).toUShort(nullptr, 16);
            const quint16 rport = rem.at(1).toUShort(nullptr, 16);
            if (lport == clientPort && rport == serverPort)
                return col.at(9).toLongLong();   // inode
        }
        return -1;
    };
    qint64 inode = findInode(QStringLiteral("/proc/net/tcp"));
    if (inode < 0) inode = findInode(QStringLiteral("/proc/net/tcp6"));
    if (inode < 0) return -1;

    const QByteArray needle = "socket:[" + QByteArray::number(inode) + "]";
    QDir proc(QStringLiteral("/proc"));
    for (const QString &entry : proc.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        bool isPid = false;
        const qint64 pid = entry.toLongLong(&isPid);
        if (!isPid) continue;
        QDir fds(QStringLiteral("/proc/%1/fd").arg(pid));
        for (const QString &fd : fds.entryList(QDir::NoDotAndDotDot)) {
            const QByteArray target =
                QFile::symLinkTarget(fds.filePath(fd)).toLocal8Bit();
            if (target == needle) return pid;
        }
    }
    return -1;
}

QList<qint64> ancestorPids(qint64 pid) {
    QList<qint64> chain;
    qint64 cur = pid;
    for (int guard = 0; cur > 1 && guard < 64; ++guard) {
        chain.append(cur);
        const qint64 ppid = ppidOf(cur);
        if (ppid < 0) break;
        cur = ppid;
    }
    return chain;
}

QList<qint64> descendantPids(qint64 root) {
    QHash<qint64, qint64> parentOf;
    QDir proc(QStringLiteral("/proc"));
    for (const QString &entry : proc.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        bool isPid = false;
        const qint64 pid = entry.toLongLong(&isPid);
        if (!isPid) continue;
        const qint64 ppid = ppidOf(pid);
        if (ppid >= 0) parentOf.insert(pid, ppid);
    }
    QList<qint64> out;
    QList<qint64> stack{root};
    while (!stack.isEmpty()) {
        const qint64 cur = stack.takeLast();
        for (auto it = parentOf.constBegin(); it != parentOf.constEnd(); ++it) {
            if (it.value() == cur && it.key() != root && !out.contains(it.key())) {
                out.append(it.key());
                stack.append(it.key());
            }
        }
    }
    return out;
}

#elif defined(Q_OS_WIN)

// Eltern-Map (pid -> ppid) per ToolHelp-Snapshot aufbauen.
static QHash<qint64, qint64> parentMap() {
    QHash<qint64, qint64> parentOf;
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return parentOf;
    PROCESSENTRY32W pe {};
    pe.dwSize = sizeof(pe);
    if (Process32FirstW(snap, &pe)) {
        do {
            parentOf.insert(static_cast<qint64>(pe.th32ProcessID),
                            static_cast<qint64>(pe.th32ParentProcessID));
        } while (Process32NextW(snap, &pe));
    }
    CloseHandle(snap);
    return parentOf;
}

qint64 pidOfTcpClient(quint16 clientPort, quint16 serverPort) {
    ULONG size = 0;
    GetExtendedTcpTable(nullptr, &size, FALSE, AF_INET,
                        TCP_TABLE_OWNER_PID_CONNECTIONS, 0);
    if (size == 0) return -1;
    QByteArray buf(static_cast<int>(size), Qt::Uninitialized);
    auto *table = reinterpret_cast<MIB_TCPTABLE_OWNER_PID *>(buf.data());
    if (GetExtendedTcpTable(table, &size, FALSE, AF_INET,
                            TCP_TABLE_OWNER_PID_CONNECTIONS, 0) != NO_ERROR)
        return -1;
    for (DWORD i = 0; i < table->dwNumEntries; ++i) {
        const auto &row = table->table[i];
        const quint16 lport = ntohs(static_cast<u_short>(row.dwLocalPort & 0xFFFF));
        const quint16 rport = ntohs(static_cast<u_short>(row.dwRemotePort & 0xFFFF));
        if (lport == clientPort && rport == serverPort)
            return static_cast<qint64>(row.dwOwningPid);
    }
    return -1;
}

QList<qint64> ancestorPids(qint64 pid) {
    const QHash<qint64, qint64> parentOf = parentMap();
    QList<qint64> chain;
    qint64 cur = pid;
    for (int guard = 0; cur > 0 && guard < 64; ++guard) {
        chain.append(cur);
        if (!parentOf.contains(cur)) break;
        const qint64 ppid = parentOf.value(cur);
        if (ppid <= 0 || ppid == cur) break;
        cur = ppid;
    }
    return chain;
}

QList<qint64> descendantPids(qint64 root) {
    const QHash<qint64, qint64> parentOf = parentMap();
    QList<qint64> out;
    QList<qint64> stack{root};
    while (!stack.isEmpty()) {
        const qint64 cur = stack.takeLast();
        for (auto it = parentOf.constBegin(); it != parentOf.constEnd(); ++it) {
            if (it.value() == cur && it.key() != root && !out.contains(it.key())) {
                out.append(it.key());
                stack.append(it.key());
            }
        }
    }
    return out;
}

#else  // andere Plattformen: nicht implementiert

qint64 pidOfTcpClient(quint16, quint16) { return -1; }
QList<qint64> ancestorPids(qint64) { return {}; }
QList<qint64> descendantPids(qint64) { return {}; }

#endif

} // namespace qtmux::procinfo
