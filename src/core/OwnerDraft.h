#pragma once

#include <QByteArray>

namespace qtmux {

/// Woher eine Eingabe des MENSCHEN stammt: ein Tastendruck (eine Bytefolge je Taste) oder
/// eine Einfügung aus der Zwischenablage. Alles, was nicht vom Menschen kommt — MCP,
/// Warteschlange, Login-Script —, erreicht diese Einheit gar nicht erst.
enum class HumanInputKind { Key, Paste };

/// **Merker „der Mensch hat getippt und noch nicht abgeschickt“** — als Wert, damit die
/// Regel ohne Session, ohne Terminal und ohne Zeit prüfbar ist (Muster: `mayDispatchNext`).
///
/// Wozu: Ein Agent, der einer anderen Session per MCP Text schickt, hängte ihn bisher an
/// das an, was der Mensch dort gerade tippte, und schickte beides zusammen ab. Solange
/// dieser Merker steht, schreibt QTmux darum nichts Fremdes in die Session.
///
/// 🔑 **Was QTmux weiß und was nicht.** QTmux sieht die Tasten des Menschen, aber nicht die
/// Eingabezeile des Programms: ob eine Taste dort Text erzeugt, eine Auswahl bestätigt oder
/// verpufft, entscheidet das Programm. Die Regel ist deshalb bewusst einseitig — im
/// Zweifel steht der Merker (lieber wartet eine Nachricht zu lange, als dass sie in einen
/// Entwurf geschrieben wird). Drei Zustände:
///
/// - `Clean` — nach allem, was QTmux gesehen hat, steht in der Zeile nichts vom Menschen.
/// - `Counted` — der Mensch hat **nur** einfache Zeichen getippt, und zwar `count` Stück.
///   Der Cursor steht am Ende, also nimmt jedes Zurücklöschen genau eines davon weg:
///   Sind alle wieder gelöscht, ist die Zeile nachweislich leer.
/// - `Open` — es steht (vielleicht) etwas da, und QTmux kann es nicht mehr mitzählen:
///   Einfügung, Cursor bewegt, Verlauf geholt, Vervollständigung, Zeichen außerhalb des
///   einfachen Bereichs.
///
/// Zurück nach `Clean` führen nur Ereignisse, nach denen die Zeile sicher leer ist:
/// **Enter** (abgeschickt), **Strg-C** (verworfen), das **gezählte Zurücklöschen** und
/// **Strg-U** aus `Counted` (eine einzelne, vollständig selbst getippte Zeile). Strg-U aus
/// `Open` setzt NICHT zurück: Ein mehrzeiliger Entwurf verliert damit nur eine Zeile.
/// Escape setzt nie zurück — in Agenten-TUIs bricht es die laufende Arbeit ab, die
/// Eingabezeile bleibt stehen. Einen Zeitablauf gibt es bewusst nicht: Ein halber Satz,
/// den der Mensch stehen lässt, ist genau der Fall, um den es geht.
///
/// ⚠️ **Bekannte Grenze (Dauer-Merker ohne Entwurf):** Verbraucht das Programm eine
/// einzelne Taste sofort (Ziffer in einem Auswahldialog, `q` in `less`), sieht das hier wie
/// ein angefangener Entwurf aus und bleibt stehen, bis der Mensch dort Enter oder Strg-C
/// drückt oder die Eingabe in der Oberfläche freigibt. Das ist die gewollte Richtung des
/// Irrtums; sichtbar wird er an der Kachel und in `list_sessions`.
struct OwnerDraftState {
    enum class Kind { Clean, Counted, Open };
    Kind kind = Kind::Clean;
    int count = 0;   ///< nur bei `Counted`: Zahl der getippten, noch nicht gelöschten Zeichen

    bool pending() const { return kind != Kind::Clean; }
    friend bool operator==(const OwnerDraftState &a, const OwnerDraftState &b) {
        return a.kind == b.kind && a.count == b.count;
    }
};

/// **Die Übergangsregel** — reine Funktion. `bytes` ist das, was die Oberfläche für EINE
/// Eingabe des Menschen ans Terminal schickt (bei `Key` die kodierte Taste, bei `Paste`
/// der eingefügte Text). Mehrere Tasten in einem Aufruf (IME, Broadcast) werden der Reihe
/// nach ausgewertet.
OwnerDraftState nextOwnerDraft(OwnerDraftState state, const QByteArray &bytes,
                               HumanInputKind kind);

/// Hat diese Eingabe die Zeile abgeschickt oder verworfen (Enter bzw. Strg-C als LETZTE
/// Taste)? Die Session stellt daraufhin ihre Ruhe-Uhr neu, damit eine wartende Nachricht
/// nicht im selben Augenblick hinterherfällt.
bool endsOwnerDraft(const QByteArray &bytes, HumanInputKind kind);

} // namespace qtmux
