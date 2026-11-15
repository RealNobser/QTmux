#pragma once

#include <QByteArray>
#include <QString>
#include <functional>

namespace qtmux {

/// Ergebnis eines Zustellversuchs an den Nachrichten-Eingang eines Agenten.
struct InboxDelivery {
    bool ok = false;        ///< der ganze Rahmen ist an den Socket übergeben
    QString error;          ///< bei !ok: ein Satz, warum nicht
    qint64 peerPid = -1;    ///< Prozess, der am Socket lauscht (-1 = nicht lesbar)
};

/// Schreibt `frame` (eine vollständige Zeile) auf den Unix-Socket `socketPath` und trennt.
///
/// 🔑 **Vor dem Schreiben wird gefragt, WER dort lauscht.** `peerAccepted` bekommt die PID
/// des Prozesses am anderen Ende und entscheidet, ob er der gemeinte ist — der Aufrufer
/// bindet die Zustellung so an die Session (der Prozess muss in ihrem Prozessbaum
/// liegen) statt an einen Pfad, der einmal gestimmt hat: Socket-Namen tragen die PID, und
/// PIDs werden wiederverwendet. Lässt sich die PID nicht lesen, wird NICHT geschrieben.
///
/// „ok“ heißt: übergeben, nicht gelesen. Der Socket antwortet nicht; ob der Empfänger die
/// Nachricht annimmt, zurückhält oder ablehnt, erfährt der Absender hier nicht.
///
/// Blockiert höchstens `timeoutMs` (Verbinden und Schreiben zusammen). Ein nur teilweise
/// geschriebener Rahmen ist für den Empfänger keine Nachricht (ihm fehlt das Zeilenende),
/// ein Fehlschlag hinterlässt also nichts Halbes.
///
/// Nur Unix (macOS/Linux). Unter Windows liefert der Aufruf sofort einen Fehler: Dort ist
/// der Eingang eine Named Pipe mit Anmeldepflicht — nicht gemessen, darum nicht gebaut.
InboxDelivery deliverToInbox(const QString &socketPath, const QByteArray &frame,
                             const std::function<bool(qint64 peerPid)> &peerAccepted,
                             int timeoutMs = 1500);

} // namespace qtmux
