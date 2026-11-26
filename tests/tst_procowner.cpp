#include <QtTest>
#include <QElapsedTimer>
#include <QProcess>
#include <QTcpServer>
#include <QTcpSocket>
#include <QThread>

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

        QScopedPointer<QProcess> client(spawn("QTMUX_PROCOWNER_CONNECT", QByteArray::number(serverPort)));
        QScopedPointer<QProcess> idle(spawn("QTMUX_PROCOWNER_IDLE", "1"));
        QVERIFY(client->waitForStarted(5000));
        QVERIFY(idle->waitForStarted(5000));
        QVERIFY(server.waitForNewConnection(10000));
        QTcpSocket *peer = server.nextPendingConnection();
        QVERIFY(peer);
        const quint16 clientPort = peer->peerPort();
        const qint64 clientPid = client->processId();

        // Gegenprobe mit der alten Suche: dieselbe Verbindung, derselbe Prozess.
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

        client->kill();
        idle->kill();
        client->waitForFinished(5000);
        idle->waitForFinished(5000);
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
