// Szene 15 (Proxy-Anmeldung) des Szenen-Bausteins im Update-Kern: Bedingung zum Aussetzen.
//
// Der Szenen-Baustein (third_party/updater/update/UpdateShowcase.cpp) startet einen
// Proxy-Server, der nur auf 127.0.0.1 lauscht, und nennt dem Ablauf als Proxy „localhost“.
// Auf dem GitHub-macOS-Runner kam die Szene in zwei Läufen nicht zustande (kein einziges
// Wort des Ablaufs im Log). Gemessen ist dort nur: „localhost“ löst zuerst auf ::1 auf.
// Das ist nicht hinreichend (der Windows-Runner löst genauso auf, und dort gelingt die
// Szene); die eigentliche Ursache (kein schneller Abweis auf ::1) ist vermutet, nicht
// gemessen. Darum misst diese Probe den Mechanismus selbst: Ein Server nur auf
// 127.0.0.1, eine Verbindung auf „localhost“, dieselbe Frist wie die Szene. Kommt keine
// Verbindung zustande, setzen die Tests, die Szene 15 brauchen, mit einer eigenen Meldung
// aus (ctest zeigt sie über SKIP_REGULAR_EXPRESSION als „Skipped“). Sonst laufen sie.
//
// Behoben im nächsten Stand des Update-Kerns (Proxy als 127.0.0.1). Rücknahme: beim
// nächsten Vendoring diese Datei und die beiden Szene-15-Tests in tests/CMakeLists.txt
// zurückbauen. Der Wächter test_scene15_skip_retired (tests/CheckScene15Skip.cmake)
// wird rot, sobald der vendierte Baustein kein „localhost“ mehr setzt und diese Datei
// noch existiert.

#pragma once

#include <QElapsedTimer>
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
    bool reached = false;
    qint64 ms = 0;
};

// Verbindet sich auf host:port und wartet höchstens deadlineMs.
inline Probe connectWithin(const QString &host, quint16 port, int deadlineMs)
{
    QTcpSocket socket;
    QElapsedTimer clock;
    clock.start();
    socket.connectToHost(host, port);
    Probe p;
    p.reached = socket.waitForConnected(deadlineMs);
    p.ms = clock.elapsed();
    socket.abort();
    return p;
}

// Die Meldung, mit der ausgesetzt wird; leer = nicht aussetzen. Rein, damit der Test
// die Entscheidung ohne Netz prüfen kann.
inline QString reasonFor(const Probe &probe, const QStringList &localhostOrder)
{
    if (probe.reached) return {};
    return QStringLiteral("%1: keine Verbindung zu localhost (Server nur auf 127.0.0.1) "
                          "in %2 ms (Frist %3 ms), localhost -> [%4]; behoben im naechsten "
                          "Stand des Update-Kerns")
        .arg(QLatin1String(kMarker))
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
        return reasonFor(connectWithin(QStringLiteral("localhost"), server.serverPort(),
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
