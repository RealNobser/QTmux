#pragma once

#include <QObject>
#include <QString>
#include <QStringList>

namespace qtmux {

/// Aktivitätswerte, wie sie `Session::Activity` verwendet — hier **gespiegelt**, damit
/// `PromptQueue` die Session **nicht kennen muss** (der Kern der Warteschlange soll ohne
/// laufende Session testbar sein; die Anbindung reicht die Zahl herein, genau wie
/// `shouldPreventSleep(bool, QList<int>)` in [SleepInhibitor.h](SleepInhibitor.h) es tut).
/// Die Zahlen sind in `Session.h` ausdrücklich als stabil festgeschrieben („bewusst
/// kompatibel mit der Sidebar-Ring-Logik: 1=grün, 2=gelb, 3=rot, 4=grau").
enum ActivityCode {
    ActivityIdle    = 0,
    ActivityRunning = 1,
    ActivityWaiting = 2,
    ActivityError   = 3,
    ActivityClosed  = 4,
};

/// Vorgabe-Ruhefenster für Sessions, die ihren Zustand **nie selbst melden** (s.
/// `mayDispatchNext`). 500 ms liegen deutlich über dem, was ein Prompt-Redraw oder das
/// Echo einer Shell an zusammenhängender Ausgabe erzeugt, und deutlich unter der Pause,
/// die ein laufender Build/Testlauf je macht.
inline constexpr qint64 kDefaultQuietMs = 500;

/// Alles, was die Abgabe-Entscheidung braucht. Bewusst eine **Struct mit designierten
/// Initialisierern** (C++20, wie die Einträge der `AgentRegistry`) statt fünf loser
/// Parameter: drei davon wären numerisch und ließen sich lautlos vertauschen.
struct DispatchContext {
    bool   queueNotEmpty     = false;   ///< Liegt überhaupt etwas an?
    bool   activityReported  = false;   ///< Hat die Session ihren Zustand je SELBST gemeldet?
    int    activity          = ActivityRunning;   ///< zuletzt gemeldeter/angenommener Zustand
    qint64 msSinceLastOutput = -1;      ///< ms seit der letzten Backend-Ausgabe; < 0 = noch keine
    qint64 quietMs           = kDefaultQuietMs;   ///< Ruhefenster für ungemeldete Sessions
};

/// **Die Abgabe-Bedingung** — reine Funktion, Gui-frei und ohne Zustand, damit sie ohne
/// Session und ohne Timing testbar ist (Muster: `shouldPreventSleep`, `gridFor`).
///
/// 🔑 **Die Falle, um die es hier geht (dieselbe wie in QTMUX-89):** `Activity` allein ist
/// als Auslöser UNBRAUCHBAR. Der Startwert einer Session ist `Running`, damit der
/// Sidebar-Ring sofort grün ist — und eine gewöhnliche Shell **ohne** Shell-Integration
/// meldet nie etwas. Wer nur auf den Übergang „busy → idle" wartet, hat eine
/// Warteschlange, die bei genau diesen Sessions **für immer stehen bleibt**: der Anwender
/// reiht ein, nichts passiert, und das sieht wie ein kaputtes Feature aus. Ungemeldet
/// heißt **unbekannt**, nicht „arbeitet".
///
/// Daraus zwei Zweige:
///
/// 1. **Die Session meldet selbst** (OSC 133 oder MCP `set_activity`) → ihrer Meldung wird
///    geglaubt, die Heuristik aus Zweig 2 wird gar nicht erst befragt. Abgegeben wird bei
///    jedem Zustand außer `Running`:
///    - `Idle` — Prompt zurück, der Normalfall.
///    - `Waiting` — der Agent fragt einen Menschen. Das ist der **wichtigste** Abgabefall,
///      denn der eingereihte Text ist meist genau die Antwort. (Umgekehrt als bei
///      `shouldPreventSleep`, wo `Waiting` bewusst NICHT als Arbeiten zählt — dort heißt
///      „wartet auf Menschen" „darf schlafen", hier „darf beliefert werden". Dieselbe
///      Lesart, andere Konsequenz.)
///    - `Error` — auch das ist ein Ruhezustand: der Agent hat aufgehört zu arbeiten, und
///      der eingereihte Text ist oft die Korrektur.
/// 2. **Die Session hat nie gemeldet** → der Aktivitätswert ist bedeutungslos und wird
///    ignoriert. Entschieden wird stattdessen über **Ruhe im Ausgabestrom**: erst wenn seit
///    der letzten Backend-Ausgabe `quietMs` vergangen sind, gilt die Session als frei. Das
///    ist keine Bildschirm-Auswertung (die Projektlinie aus QTMUX-30 verbietet, aus dem
///    *Inhalt* etwas abzuleiten) — gemessen wird nur, **ob** Bytes fließen.
///
/// Zwei Zustände blockieren immer:
/// - `Closed` — in eine tote Session schreibt niemand mehr, auch nicht nach langer Ruhe.
/// - `msSinceLastOutput < 0` (noch **nie** Ausgabe) in Zweig 2 — eine Shell, die noch kein
///   Lebenszeichen gegeben hat, ist nicht ruhig, sondern **noch nicht bereit**; ihr Prompt
///   kommt erst. Hineinzuschreiben ist dieselbe Falle wie beim Login-Script (QTMUX-98).
///   Gibt ein Backend nie etwas aus, bleibt die Warteschlange stehen — bewusst: das ist
///   ein sichtbar defektes Backend, kein Fall für stilles Draufloswerfen.
bool mayDispatchNext(const DispatchContext &ctx);

/// Warteschlange der Eingaben **einer** Session (QTMUX-90): Was der Anwender tippt,
/// während der Agent arbeitet, wird hier gesammelt und abgegeben, sobald er frei ist —
/// statt wie bisher mitten in dessen Ausgabe zu landen.
///
/// Gui-frei (nur Qt6::Core) und **ohne jede Kenntnis von `Session`**: Die Klasse hält
/// Texte, sonst nichts. Wann abgegeben werden darf, entscheidet die freie Funktion
/// `mayDispatchNext`; **wer** schreibt und **woher** die Zustandswerte kommen, ist Sache
/// der Anbindung. So bleibt der Kern ohne laufende Shell testbar.
///
/// Die Methoden sind `Q_INVOKABLE`, damit die spätere QML-Anbindung (Zähler und
/// Bearbeiten-Popup auf der Sidebar-Kachel) sie ohne Änderung erreicht.
class PromptQueue : public QObject {
    Q_OBJECT
    /// Zähler für die Sidebar-Kachel. Meldet über `changed`, weil jede Listenänderung
    /// den Zähler betreffen kann und ein zweites Signal nur zusätzlich verwaltet werden müsste.
    Q_PROPERTY(int count READ count NOTIFY changed)
public:
    explicit PromptQueue(QObject *parent = nullptr);

    // --- Lesen ---------------------------------------------------------------
    int count() const { return int(m_entries.size()); }
    Q_INVOKABLE bool isEmpty() const { return m_entries.isEmpty(); }
    /// Alle Einträge in Abgabereihenfolge (Index 0 = der nächste).
    Q_INVOKABLE QStringList entries() const { return m_entries; }
    /// Eintrag an `index`, oder leer bei ungültigem Index.
    Q_INVOKABLE QString at(int index) const;
    /// Der nächste Eintrag, **ohne** ihn zu entnehmen (leer, wenn nichts anliegt).
    Q_INVOKABLE QString peekNext() const { return m_entries.isEmpty() ? QString() : m_entries.first(); }

    // --- Ändern --------------------------------------------------------------
    /// Hängt hinten an. Liefert false, wenn der Text abgewiesen wurde (s. `isAcceptable`).
    Q_INVOKABLE bool enqueue(const QString &text);
    /// Fügt an `index` ein; der Index wird auf [0, count] **geklemmt** statt abgewiesen —
    /// die Aufrufstelle ist eine Liste zum Umsortieren, und ein um eins verrutschter Index
    /// darf den Eintrag nicht verschlucken.
    Q_INVOKABLE bool insert(int index, const QString &text);
    /// Entfernt einen Eintrag. Ungültiger Index → false, Liste unverändert, kein Signal.
    Q_INVOKABLE bool removeAt(int index);
    /// Leert die Liste und liefert die Anzahl der entfernten Einträge. Auf einer bereits
    /// leeren Liste passiert nichts (kein Signal).
    Q_INVOKABLE int clear();
    /// Verschiebt einen Eintrag. Ungültige oder gleiche Positionen → false, kein Signal.
    Q_INVOKABLE bool move(int from, int to);

    // --- Abgeben -------------------------------------------------------------
    /// Entnimmt den nächsten Eintrag (FIFO) und liefert ihn; leer, wenn nichts anliegt.
    ///
    /// 🔑 **Reihenfolge-Invariante:** Erst wird entnommen, **dann** `changed` gemeldet.
    /// Ein Slot, der auf `changed` hin selbst einreiht, hängt damit zwangsläufig **hinter**
    /// den noch wartenden Einträgen an — die Reihenfolge stimmt also auch dann, wenn
    /// während der Abgabe neu eingereiht wird.
    ///
    /// 🔑 **Reentranz-Wächter:** Ein `takeNext()` **aus** dem `changed`-Slot heraus liefert
    /// leer und tut nichts. Ohne den Wächter hätte der verschachtelte Aufruf den zweiten
    /// Eintrag geholt und abgesetzt, bevor der äußere Aufruf seinen ersten überhaupt
    /// zurückgegeben hat — auf der Leitung stünde #2 vor #1. Genau die Vertauschung, die
    /// eine Warteschlange verhindern soll.
    Q_INVOKABLE QString takeNext();

    /// Ist der Text als Eintrag zulässig? Verworfen wird nur, was **nach dem Trimmen**
    /// leer ist.
    ///
    /// 🔑 Begründung: Ein leerer Eintrag ginge als blankes Enter ins PTY. In einer Shell
    /// wäre das folgenlos, in einem Agenten-TUI aber **eine Handlung** — Claude Code
    /// bestätigt damit die vorausgewählte Option einer Rückfrage. Ein versehentlich
    /// eingereihtes leeres Feld würde also ungefragt zustimmen; abweisen ist die einzige
    /// Antwort, die nichts kaputt macht.
    /// 🔑 Gespeichert wird der Text **ungetrimmt**: führende Leerzeichen können bedeutsam
    /// sein (eingerückter Code, ein absichtliches Leerzeichen vor dem Befehl, das ihn in
    /// vielen Shells aus der History hält). Geprüft wird auf einer getrimmten Kopie,
    /// abgelegt das Original.
    static bool isAcceptable(const QString &text);

signals:
    /// Die Liste hat sich geändert (Einreihen, Löschen, Leeren, Umsortieren, Entnehmen).
    /// Treibt den Zähler auf der Sidebar-Kachel.
    void changed();

private:
    QStringList m_entries;
    bool m_dispatching = false;   ///< Reentranz-Wächter für takeNext (s. dort)
};

} // namespace qtmux
