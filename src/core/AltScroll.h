#pragma once

namespace qtmux {

/// Was das **Mausrad** tun soll, wenn eine Anwendung den Sichtbereich für sich
/// beansprucht und ihren Verlauf selbst zeichnet. Die Werte sind persistiert
/// (`window/altScrollMode`) — Reihenfolge daher nicht ändern, nur anhängen.
///
/// Der Ausgangspunkt: QTmux kann nur scrollen, was in **seinem** Scrollback liegt, und
/// dorthin gelangen ausschließlich Zeilen, die oben aus dem Primary Screen
/// herausgeschoben wurden. Zwei verbreitete Fälle füllen ihn nie:
///   * Ein Vollbild-TUI im **Alternate Screen** (`vim`, `less`, `htop`) — libvterm
///     schiebt Alt-Screen-Zeilen bewusst nicht in den Scrollback.
///   * Eine Anwendung, die im Primary Screen einen festen Sichtbereich **an Ort und
///     Stelle neu zeichnet** und ihren Verlauf selbst verwaltet. **Genau das tut der
///     Codex-Agent** (am laufenden Programm gemessen, 2026-08-03: `altScreen` bleibt
///     false, der Scrollback bleibt 0).
/// In beiden Fällen ist `scrollByLines` ein No-op — das Rad wirkt „festgenagelt". Helfen
/// kann nur, der Anwendung eine Taste zu schicken, mit der SIE scrollt.
enum class AltScrollMode {
    /// Nur wenn die Anwendung es per DECSET **1007** verlangt (Vorgabe).
    ///
    /// Dieselbe Linie wie bei QTMUX-30/37 und QTMUX-89: QTmux leitet nichts ab, sondern
    /// handelt auf eine ausdrückliche Meldung. Kein Kollateralschaden — `vim` und `nano`
    /// bleiben unberührt, wo das Rad als Cursor-Taste den **Cursor** bewegen (und damit
    /// im Zweifel den Text ändern) würde, statt zu scrollen.
    OnlyWhenRequested = 0,
    /// Immer, sobald ein Alternate Screen läuft und die Maus nicht gegriffen ist.
    ///
    /// Deckt zusätzlich die Anzeigeprogramme ab, die 1007 nicht senden (`less`, `man`,
    /// `git log`) — so verhält sich iTerm2 in seiner Voreinstellung. Preis ist genau der
    /// oben genannte Fall: in `vim`/`nano` wandert dann der Cursor.
    AlwaysInAltScreen = 1
};

/// Liest einen persistierten Zahlenwert als Modus.
///
/// Unbekannte und negative Werte ergeben **`OnlyWhenRequested`** — die zurückhaltende
/// Richtung: Ein defekter oder aus einer neueren Version stammender Wert darf nicht dazu
/// führen, dass QTmux ungefragt Tasten in fremde Anwendungen schickt.
constexpr AltScrollMode altScrollModeFromInt(int value) {
    switch (value) {
    case 1:  return AltScrollMode::AlwaysInAltScreen;
    default: return AltScrollMode::OnlyWhenRequested;
    }
}

/// Soll die Rad-Rastung als Tastendruck an die Anwendung gehen, statt den lokalen
/// Scrollback zu bewegen?
///
/// `altScreen`     — läuft ein Vollbild-TUI (DECSET 1049)?
/// `mouseTracking` — Maus-Tracking-Modus der App (0 = aus; s. `VtScreen::mouseTracking`)
/// `appRequested`  — hat die App DECSET 1007 gesetzt (`VtScreen::altScroll`)?
/// `hasScrollback` — hat QTmux selbst Verlauf zu zeigen (`scrollbackCount() > 0`)?
/// `hasAgentKeys`  — ist für den laufenden Agenten eine **gemessene** Scroll-Taste
///                   hinterlegt (`AgentRegistry::scrollKeysFor`)?
///
/// Die Reihenfolge der Klauseln ist die Begründung:
///
/// 🔑 **Maus-Tracking hat Vorrang** und wird nur ausgeschlossen: Greift die App die Maus,
/// ist der Rad-als-Taste-4/5-Weg der richtige (er meldet auch die Zelle, QTMUX-104).
/// Beides zu senden hieße, dieselbe Rastung doppelt zu melden.
///
/// 🔑 **Im Alternate Screen** entscheidet allein die Wahl bzw. die 1007-Anforderung. Der
/// lokale Scrollback wird hier absichtlich NICHT geprüft: Was dort liegt, stammt vom
/// Primary Screen (die Shell vor dem TUI) — es wäre der falsche Inhalt.
///
/// 🔑 **Im Primary Screen** wird nur delegiert, wenn QTmux **nichts Eigenes zu bieten**
/// hat (`!hasScrollback`) und für den Agenten eine gemessene Taste vorliegt. Beide
/// Bedingungen tragen: Der eigene Verlauf des Anwenders muss immer Vorrang haben, und
/// eine Taste ins Blaue hinein wäre gefährlich — an einem Shell-Prompt blättern
/// Cursor-Tasten die Befehls-Historie durch. Die Scrollback-Klausel heilt zugleich den
/// Fall „Agent beendet, Shell wieder da": Sobald die Shell Ausgabe produziert, die oben
/// herausläuft, gewinnt wieder der lokale Weg.
///
/// 🔑 Der Agenten-Weg hängt bewusst **nicht** am Modus: Er beruht nicht auf einer
/// Ableitung, sondern auf einer Messung an genau diesem Programm — es gibt nichts,
/// wovor die zurückhaltende Vorgabe schützen müsste.
constexpr bool wheelGoesToApp(AltScrollMode mode, bool altScreen, int mouseTracking,
                              bool appRequested, bool hasScrollback, bool hasAgentKeys) {
    if (mouseTracking != 0) return false;
    if (altScreen)
        return mode == AltScrollMode::AlwaysInAltScreen || appRequested;
    return hasAgentKeys && !hasScrollback;
}

/// Zeilen (= Tastendrücke) je Rad-Rastung. Bewusst dieselbe Zahl wie beim lokalen
/// Scrollback-Scrollen (`TerminalItem::scrollByLines(±3)`), damit sich das Rad in beiden
/// Fällen gleich schnell anfühlt; xterm verwendet für `alternateScroll` denselben Wert.
constexpr int kAltScrollLinesPerNotch = 3;

} // namespace qtmux
