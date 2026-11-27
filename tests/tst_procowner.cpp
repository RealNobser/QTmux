#include <QtTest>
#include <QElapsedTimer>
#include <QProcess>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QThread>

#if defined(Q_OS_UNIX)
#  include <dirent.h>
#  include <unistd.h>
#endif
#if defined(Q_OS_LINUX)
#  include <fstream>
#  include <string>
#endif

#include "ProcessInfo.h"

// procinfo::tcpClientBelongsTo (QTMUX-162): Gehört der TCP-Client am Port X zum
// Prozessbaum unter einer gegebenen Wurzel? Gemessen an einem echten Kindprozess (dieses
// Programm selbst in einer Hilfsrolle), der sich mit einem Server im Test verbindet.
//
// Hilfsrollen (Umgebung): QTMUX_PROCOWNER_CONNECT=<port> verbindet und wartet;
// QTMUX_PROCOWNER_IDLE=1 wartet nur (unbeteiligter Prozess).
namespace {

int helperMain() {
    const QByteArray port = qgetenv("QTMUX_PROCOWNER_CONNECT");
    QTcpSocket s;
    if (!port.isEmpty()) {
        s.connectToHost(QHostAddress::LocalHost, static_cast<quint16>(port.toUInt()));
        if (!s.waitForConnected(5000)) return 2;
        s.write("x");
        s.waitForBytesWritten(2000);
    }
    // Warten, bis der Test uns beendet (höchstens 30 s).
    for (int i = 0; i < 300; ++i) QThread::msleep(100);
    return 0;
}

// Ein Kindprozess wird auch dann beendet und abgewartet, wenn ein Vergleich vorher scheitert
// (sonst „QProcess: Destroyed while process … is still running“).
struct KillAndWait {
    static void cleanup(QProcess *p) {
        if (!p) return;
        if (p->state() != QProcess::NotRunning) {
            p->kill();
            p->waitForFinished(5000);
        }
        delete p;
    }
};
using ChildProcess = QScopedPointer<QProcess, KillAndWait>;

#if defined(Q_OS_LINUX)
// Diagnose für den CI-Lauf, unabhängig vom Produktcode: die Zeilen der Kernel-Tabellen mit
// dem Client-Port und die Socket-Einträge des Clients. Keine Pfade, keine Rechnernamen.
void printLinuxDiagnosis(qint64 clientPid, quint16 clientPort, quint16 serverPort) {
    const QByteArray portHex = ":" + QByteArray::number(clientPort, 16).toUpper().rightJustified(4, '0');
    qInfo("Diagnose: Client-Port %u (%s), Server-Port %u", clientPort, portHex.constData() + 1, serverPort);
    for (const char *table : {"/proc/net/tcp", "/proc/net/tcp6"}) {
        std::ifstream in(table);
        std::string line;
        while (std::getline(in, line)) {
            if (line.find(portHex.constData()) != std::string::npos)
                qInfo("Diagnose %s: %s", table + 10, QByteArray(line.c_str()).simplified().constData());
        }
    }
    const QByteArray dir = "/proc/" + QByteArray::number(clientPid) + "/fd";
    if (DIR *d = ::opendir(dir.constData())) {
        while (dirent *e = ::readdir(d)) {
            char buf[128];
            const ssize_t n = ::readlink((dir + '/' + e->d_name).constData(), buf, sizeof(buf) - 1);
            if (n > 0 && QByteArray(buf, n).startsWith("socket:"))
                qInfo("Diagnose fd %s -> %s", e->d_name, QByteArray(buf, n).constData());
        }
        ::closedir(d);
    } else {
        qInfo("Diagnose: fd-Verzeichnis des Clients nicht lesbar");
    }
}
#endif

} // namespace

class TestProcOwner : public QObject {
    Q_OBJECT

    static QProcess *spawn(const QByteArray &var, const QByteArray &value) {
        auto *p = new QProcess;
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        env.insert(QString::fromLatin1(var), QString::fromLatin1(value));
        p->setProcessEnvironment(env);
        p->start(QCoreApplication::applicationFilePath(), {});
        return p;
    }

private slots:
    void belongsToItsTreeOnly() {
#if !defined(Q_OS_MACOS) && !defined(Q_OS_LINUX) && !defined(Q_OS_WIN)
        QSKIP("Prozessliste auf dieser Plattform nicht lesbar");
#endif
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost, 0));
        const quint16 serverPort = server.serverPort();

        ChildProcess client(spawn("QTMUX_PROCOWNER_CONNECT", QByteArray::number(serverPort)));
        ChildProcess idle(spawn("QTMUX_PROCOWNER_IDLE", "1"));
        QVERIFY(client->waitForStarted(5000));
        QVERIFY(idle->waitForStarted(5000));
        QVERIFY(server.waitForNewConnection(10000));
        QTcpSocket *peer = server.nextPendingConnection();
        QVERIFY(peer);
        const quint16 clientPort = peer->peerPort();
        const qint64 clientPid = client->processId();

        // Gegenprobe mit der alten Suche: dieselbe Verbindung, derselbe Prozess. Unabhängig
        // ist dabei der Vergleich mit der PID, die QProcess meldet. ⚠️ Unter Windows ist
        // tcpClientBelongsTo selbst aus pidOfTcpClient + ancestorPids gebaut; die Prüfungen
        // im Baum sind dort also keine unabhängige Gegenprobe, sondern nur die
        // Ja-/Nein-Fälle derselben Mechanik.
#if defined(Q_OS_LINUX)
        printLinuxDiagnosis(clientPid, clientPort, serverPort);
#endif
        QElapsedTimer t;
        t.start();
        QCOMPARE(qtmux::procinfo::pidOfTcpClient(clientPort, serverPort), clientPid);
        const qint64 searchNs = t.nsecsElapsed();

        t.restart();
        QVERIFY(qtmux::procinfo::tcpClientBelongsTo(clientPid, clientPort, serverPort));
        const qint64 checkNs = t.nsecsElapsed();
        // Der Test ist Vorfahr des Clients: auch das ist „im Baum“. Das ist der Fall im
        // Betrieb (Wurzel = Shell der Session, Client = ihr Nachfahre).
        t.restart();
        QVERIFY(qtmux::procinfo::tcpClientBelongsTo(QCoreApplication::applicationPid(), clientPort, serverPort));
        const qint64 treeNs = t.nsecsElapsed();
        // Ein unbeteiligter Prozess, ein falscher Port, eine ungültige Wurzel: nein.
        QVERIFY(!qtmux::procinfo::tcpClientBelongsTo(idle->processId(), clientPort, serverPort));
        QVERIFY(!qtmux::procinfo::tcpClientBelongsTo(clientPid, static_cast<quint16>(clientPort + 1), serverPort));
        QVERIFY(!qtmux::procinfo::tcpClientBelongsTo(clientPid, clientPort, static_cast<quint16>(serverPort + 1)));
        QVERIFY(!qtmux::procinfo::tcpClientBelongsTo(0, clientPort, serverPort));
        // Ungünstigster Fall der alten Suche: niemand hält den Port, alle Prozesse samt
        // Deskriptoren werden angesehen (so läuft sie bei jedem Fehlgriff).
        t.restart();
        QCOMPARE(qtmux::procinfo::pidOfTcpClient(static_cast<quint16>(clientPort + 1), serverPort), qint64(-1));
        const qint64 fullSearchNs = t.nsecsElapsed();
        qInfo("Suche über alle Prozesse: %lld µs (Treffer), %lld µs (voll); Prüfung im Baum: "
              "%lld µs (Wurzel hält den Socket), %lld µs (Nachfahre hält ihn)",
              searchNs / 1000, fullSearchNs / 1000, checkNs / 1000, treeNs / 1000);

    }

    // Unter Linux sucht QTmux den Socket über die Einträge in /proc/<pid>/fd. Deren Ziel
    // („socket:[4711]“) ist kein Pfad: Es zählt das wörtliche Linkziel, und ein Eintrag ohne
    // existierendes Ziel muss mitgezählt werden. Prüfbar auf jedem Unix an einem
    // nachgebauten Verzeichnis.
    void fdDirectoryMatchesSocketLinkLiterally() {
#if !defined(Q_OS_UNIX)
        QSKIP("Symlinks im fd-Verzeichnis gibt es nur unter Unix");
#else
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const QByteArray dir = QFile::encodeName(tmp.path());
        QCOMPARE(::symlink("socket:[47110]", (dir + "/3").constData()), 0);
        QCOMPARE(::symlink("pipe:[4711]", (dir + "/4").constData()), 0);
        QFile plain(tmp.filePath(QStringLiteral("5")));
        QVERIFY(plain.open(QIODevice::WriteOnly));
        plain.close();
        QVERIFY(!qtmux::procinfo::detail::dirHasLinkTo(dir, "socket:[4711]"));
        QCOMPARE(::symlink("socket:[4711]", (dir + "/6").constData()), 0);
        QVERIFY(qtmux::procinfo::detail::dirHasLinkTo(dir, "socket:[4711]"));
        QVERIFY(qtmux::procinfo::detail::dirHasLinkTo(dir, "socket:[47110]"));
        QVERIFY(!qtmux::procinfo::detail::dirHasLinkTo(dir + "/fehlt", "socket:[4711]"));
#endif
    }

    // Die Kernel-Tabelle führt eine Verbindung zweimal: Client-Seite (lokal = Client-Port)
    // und Server-Seite (lokal = Server-Port). Gesucht ist die Client-Seite, auch wenn sie
    // weit hinten steht (hier hinter 300 fremden Zeilen, also jenseits eines Lesepuffers).
    void kernelTableFindsClientSide() {
        QByteArray table = "  sl  local_address rem_address   st tx_queue rx_queue tr tm->when retrnsmt   uid  timeout inode\n";
        auto row = [](int sl, quint16 lport, quint16 rport, qint64 inode) {
            return QByteArray::number(sl).rightJustified(4, ' ') + ": 0100007F:"
                   + QByteArray::number(lport, 16).toUpper().rightJustified(4, '0') + " 0100007F:"
                   + QByteArray::number(rport, 16).toUpper().rightJustified(4, '0')
                   + " 01 00000000:00000000 00:00000000 00000000  1001        0 "
                   + QByteArray::number(inode) + " 1 0000000000000000 20 4 30 10 -1\n";
        };
        for (int i = 0; i < 300; ++i) table += row(i, static_cast<quint16>(20000 + i), 443, 100000 + i);
        table += row(300, 7346, 51234, 222);   // Server-Seite
        table += row(301, 51234, 7346, 333);   // Client-Seite
        QCOMPARE(qtmux::procinfo::detail::tcpTableInode(table, 51234, 7346), qint64(333));
        QCOMPARE(qtmux::procinfo::detail::tcpTableInode(table, 7346, 51234), qint64(222));
        QCOMPARE(qtmux::procinfo::detail::tcpTableInode(table, 51235, 7346), qint64(-1));
        QCOMPARE(qtmux::procinfo::detail::tcpTableInode(QByteArray(), 51234, 7346), qint64(-1));
    }
};

int main(int argc, char **argv) {
    if (qEnvironmentVariableIsSet("QTMUX_PROCOWNER_CONNECT") || qEnvironmentVariableIsSet("QTMUX_PROCOWNER_IDLE")) {
        QCoreApplication app(argc, argv);
        return helperMain();
    }
    QCoreApplication app(argc, argv);
    TestProcOwner t;
    return QTest::qExec(&t, argc, argv);
}

#include "tst_procowner.moc"
