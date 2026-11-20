#pragma once

// „Beenden und aktualisieren“ (Vertrag V2 §4.3, K2): QTmux' Gegenstück zum
// Beenden-Rückruf der Widgets-Apps (dort `close()` + `closeAllWindows()` +
// `allQuitBlockingWindowsClosed()`).
//
// QTmux beendet sich über das Schließen seines Hauptfensters (Main.qml,
// `onClosing`): Dort werden Fenster, Layout und Verlauf gesichert und alle
// Sitzungen beendet. Auf dem Update-Weg entfällt NUR die eigene Rückfrage
// „Beenden?“ (`quitConfirmed`): Der Zustand „Bereit“ nennt schon die Zahl der
// laufenden Sitzungen, und der Knopf ist die bewusste Entscheidung des Anwenders.
// Die Schwester-Apps verfahren auf demselben Weg genauso. Alles andere im
// Schließen läuft unverändert.
//
// 🔑 Warum synchron: Der Ablauf verlangt sofort die Antwort, ob die App geht
// (`Config::requestQuit`). Die eigene Rückfrage von QTmux ist ein QML-Dialog und
// damit asynchron — sie würde mit der 15-s-Frist des Ablaufs kollidieren.
// `QWindow::close()` dagegen schickt das Schließ-Ereignis sofort und meldet, ob
// es angenommen wurde.

class QQmlApplicationEngine;

namespace qtmux {

/// Schließt das Hauptfenster ohne QTmux' eigene Beenden-Rückfrage und beendet
/// danach die App. Liefert false (und setzt die Rückfrage zurück), wenn das
/// Fenster das Schließen ablehnt oder fehlt — dann passiert nichts weiter.
bool quitForUpdate(QQmlApplicationEngine &engine);

} // namespace qtmux
