#pragma once

namespace qtmux {

/// Umfang der Wiederherstellung beim Start (QTMUX-99). Die Werte sind persistiert
/// (`window/restoreSessionMode`) — Reihenfolge daher nicht ändern, nur anhängen.
///
/// Bewusst eine **Wahl** und kein Schalter, aus demselben Grund wie bei `ResumeMode`
/// (QTMUX-98): Der teure Teil der Wiederherstellung ist der farbige Scrollback, der
/// nützliche Teil sind Fenster, Panes und Arbeitsverzeichnisse. Wer nur Ersteres
/// loswerden will, müsste sonst auf Letzteres mit verzichten.
enum class RestoreMode {
    None           = 0,  ///< nichts wiederherstellen; QTmux startet mit einer leeren Session
    WithoutHistory = 1,  ///< Fenster, Panes und Arbeitsverzeichnisse, aber ohne Scrollback
    Full           = 2   ///< zusätzlich der farbige Verlauf je Pane (Vorgabe = bisheriges Verhalten)
};

/// Liest einen persistierten Zahlenwert als Modus.
///
/// ⚠️ Unbekannte und negative Werte ergeben **`Full`**, nicht `None`. Das ist die einzige
/// sichere Richtung: `None` unterdrückt zusätzlich das **Speichern** (s. `persistsOnQuit`).
/// Würde ein defekter oder aus einer neueren Version stammender Wert als `None` gelesen,
/// startete QTmux leer und schriebe den letzten Stand nie mehr fort — der Anwender sähe
/// einen Totalverlust, obwohl nur eine Zahl nicht verstanden wurde.
constexpr RestoreMode restoreModeFromInt(int value) {
    switch (value) {
    case 0:  return RestoreMode::None;
    case 1:  return RestoreMode::WithoutHistory;
    default: return RestoreMode::Full;
    }
}

/// Sollen Fenster, Panes und deren Arbeitsverzeichnisse zurückkommen?
constexpr bool restoresLayout(RestoreMode mode) { return mode != RestoreMode::None; }

/// Soll zusätzlich der farbige Scrollback je Pane geladen werden?
constexpr bool restoresHistory(RestoreMode mode) { return mode == RestoreMode::Full; }

/// Darf der aktuelle Stand beim Beenden gespeichert werden?
///
/// ⚠️ Bei `None` **nein** — und das ist der Kern des Tickets, nicht ein Detail: Wer nie
/// wiederherstellt, würde beim ersten Beenden seinen gespeicherten Stand mit der einen
/// frisch geöffneten Session überschreiben. Ein einmaliges Umstellen wäre damit
/// unwiderruflich. Stattdessen bleibt der letzte Stand eingefroren, bis wieder ein Modus
/// gewählt wird, der ihn auch lädt. Aus demselben Grund unterbleibt bei `None` das
/// Aufräumen verwaister Scrollback-Dumps: die gehören zum eingefrorenen Stand.
/// Dieselbe Klasse Fehler wie `sessionConfig()` in QTMUX-85.
constexpr bool persistsOnQuit(RestoreMode mode) { return mode != RestoreMode::None; }

} // namespace qtmux
