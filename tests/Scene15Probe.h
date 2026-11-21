// Szene 15 (Proxy-Anmeldung) des Szenen-Bausteins im Update-Kern: Bedingung zum Aussetzen.
//
// Der Szenen-Baustein (third_party/updater/update/UpdateShowcase.cpp) startet einen
// Proxy-Server, der nur auf 127.0.0.1 lauscht, und nennt dem Ablauf als Proxy „localhost“.
// Auf dem GitHub-macOS-Runner kam die Szene in drei Läufen nicht zustande (kein einziges
// Wort des Ablaufs im Log). Gemessen dort: „localhost“ löst zuerst auf ::1 auf, UND eine
// Verbindung zu einem Loopback-Port, an dem niemand lauscht, gilt dort als verbunden
// (lokal, unter Linux und unter Windows wird sie abgewiesen). Die Verbindung auf ::1
// landet also nicht beim Szenen-Server, wird aber auch nicht abgewiesen, und Qt versucht
// 127.0.0.1 nie. Die Reihenfolge allein ist nicht hinreichend: Der Windows-Runner löst
// genauso auf, weist ::1 aber ab, und dort gelingt die Szene.
//
// Darum misst diese Probe den Mechanismus selbst, und zwar die ANKUNFT, nicht das
// Verbinden: ein Server nur auf 127.0.0.1, eine Verbindung auf „localhost“, dieselbe Frist
// wie die Szene. Nimmt der Server in der Frist keine Verbindung an, setzen die Tests, die
// Szene 15 brauchen, mit einer eigenen Meldung aus (ctest zeigt sie über
// SKIP_REGULAR_EXPRESSION als „Skipped“). Sonst laufen sie.
//
// Behoben im nächsten Stand des Update-Kerns (Proxy als 127.0.0.1). Rücknahme: beim
// nächsten Vendoring diese Datei und die beiden Szene-15-Tests in tests/CMakeLists.txt
// zurückbauen. Der Wächter test_scene15_skip_retired (tests/CheckScene15Skip.cmake)
// wird rot, sobald der vendierte Baustein kein „localhost“ mehr setzt und diese Datei
// noch existiert.

#pragma once

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QHostAddress>
#include <QHostInfo>
#include <QString>
#include <QStringList>
#include <QTcpServer>
#include <QTcpSocket>

namespace scene15 {

// Wie finishWhen(…, 15000) für Szene 15 in UpdateShowcase.cpp.
inline constexpr int kDeadlineMs = 15000;

// Erkennungsmarke der Meldung, zugleich der SKIP_REGULAR_EXPRESSION in tests/CMakeLists.txt.
// Nur ASCII: Die Meldung läuft unter Windows durch cmd.exe und ctest.
inline constexpr char kMarker[] = "SZENE15-AUSGESETZT";

struct Probe {
    bool arrived = false;          // der Server hat die Verbindung angenommen
    bool clientConnected = false;  // der Client hielt sich für verbunden
    qint64 ms = 0;
};

// Verbindet sich auf host:port und wartet höchstens deadlineMs darauf, dass `server`
// eine Verbindung annimmt. Zählt die Ankunft, nicht das Verbinden.
inline Probe arrivesWithin(QTcpServer &server, const QString &host, quint16 port, int deadlineMs)
{
    QTcpSocket socket;
    QElapsedTimer clock;
    clock.start();
    socket.connectToHost(host, port);
    Probe p;
    while (clock.elapsed() < deadlineMs) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
        if (server.hasPendingConnections() || server.waitForNewConnection(20)) {
            p.arrived = true;
            break;
        }
    }
    p.ms = clock.elapsed();
    p.clientConnected = socket.state() == QAbstractSocket::ConnectedState;
    socket.abort();
    while (QTcpSocket *s = server.nextPendingConnection()) delete s;
    return p;
}

// Die Meldung, mit der ausgesetzt wird; leer = nicht aussetzen. Rein, damit der Test
// die Entscheidung ohne Netz prüfen kann.
inline QString reasonFor(const Probe &probe, const QStringList &localhostOrder)
{
    if (probe.arrived) return {};
    return QStringLiteral("%1: Verbindung auf localhost kam nicht beim Server auf 127.0.0.1 an "
                          "(Client verbunden: %2) in %3 ms (Frist %4 ms), localhost -> [%5]; "
                          "behoben im naechsten Stand des Update-Kerns")
        .arg(QLatin1String(kMarker))
        .arg(probe.clientConnected ? QStringLiteral("ja") : QStringLiteral("nein"))
        .arg(probe.ms)
        .arg(kDeadlineMs)
        .arg(localhostOrder.join(QStringLiteral(", ")));
}

// Einmal je Prozess gemessen (die Esc-Prüfung hat drei Zeilen).
inline const QString &skipReasonHere()
{
    static const QString reason = [] {
        QTcpServer server;
        if (!server.listen(QHostAddress::LocalHost, 0))
            return QStringLiteral("%1: kein Server auf 127.0.0.1 moeglich (%2)")
                .arg(QLatin1String(kMarker), server.errorString());
        QStringList order;
        for (const QHostAddress &a : QHostInfo::fromName(QStringLiteral("localhost")).addresses())
            order << a.toString();
        return reasonFor(arrivesWithin(server, QStringLiteral("localhost"), server.serverPort(),
                                       kDeadlineMs),
                         order);
    }();
    return reason;
}

// Der Hauptlauf eines Tests setzt diese Variable: Dort laufen die Szene-15-Fälle nicht,
// sie haben ihren eigenen ctest-Test (nur der trägt den Skip-Ausdruck, damit hinter ihm
// kein roter Fall verschwindet).
inline bool runsInItsOwnTest() { return qEnvironmentVariableIsSet("QTMUX_SCENE15_OWN_TEST"); }

} // namespace scene15
