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
void driveUpdateSelfTest(UpdateBridge &bridge);

} // namespace qtmux
