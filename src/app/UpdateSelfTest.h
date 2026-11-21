#pragma once

// Selbsttest des Update-Wegs am ECHTEN Programm (K2, Vertrag V2 §8).
//
// Start: `qtmux --selftest-update <ordner> --update-base-url <file-url>`.
// Nur in diesem Lauf darf der Ablauf die Update-Quelle umstellen
// (`Config::allowSourceOverrideForSelfTest`, entschieden hier im Code, nie über
// eine Einstellung); dann ist JEDE Installation ein Trockenlauf — der Ablauf
// schreibt „dry run (update source overridden), would start: …“ ins Log, statt
// einen Helfer zu starten. Download und Ergebnisdatei liegen im <ordner>, nie
// in „Downloads“.
//
// Der Treiber fährt den Weg, den ein Anwender per Klick fährt: prüfen →
// „Herunterladen“ → „Beenden und aktualisieren“. Beendet wird über denselben
// Rückruf wie im Betrieb (UpdateQuit.cpp), mit dem echten Main.qml-`onClosing`.
// Wächter: tests/tst_updatequit.cpp startet das Programm als Kindprozess.

#include <QString>

namespace qtmux {

class UpdateBridge;

/// Hängt den Treiber an `bridge` und stößt die Prüfung an. Nur im Selbsttest.
///
/// `systemInstaller` (Schalter `--selftest-system-installer`): Der Treiber setzt dem
/// Ablauf den Installationsweg von Stufe 1, und zwar NUR, wenn die Quelle umgestellt
/// ist (dann ist jede Installation ein Trockenlauf). Grund: Ein ad hoc signierter
/// Entwicklerbau (macOS) bzw. ein Bau außerhalb des installierten Pakets (Windows)
/// bekommt vom Selbst-Ersetzen nur „von Hand“, und der Test des Beenden-Wegs sähe
/// „Beenden und aktualisieren“ nie. Im normalen Start setzt allein die Bridge den
/// Weg, und zwar immer die Wahl des Hubs (Wächter test_update_consumer_guard).
void driveUpdateSelfTest(UpdateBridge &bridge, bool systemInstaller);

} // namespace qtmux
