#pragma once

#include <QString>
#include <QStringList>
#include <QList>

namespace qtmux {

/// Metadaten zu einem bekannten KI-Agenten-CLI.
struct AgentInfo {
    QString id;          // stabiler Schlüssel (z. B. "antigravity") — QML mappt darauf Icon/Farbe
    QString command;     // AKTUELLER Kommandoname (z. B. "agy")
    QString displayName; // Anzeigename in der Sidebar (z. B. "AntiGravity")

    /// Weitere Kommandonamen desselben Agenten (QTMUX-88): frühere Namen nach einer
    /// Umbenennung, Wrapper-Skripte. Nur **belegte** Namen eintragen — ein geratener
    /// Alias erkennt entweder nie etwas oder, schlimmer, ein fremdes Programm.
    QStringList aliases;

    // --- Fortsetzen einer Unterhaltung (QTMUX-85/98) ------------------------
    // Je Weg eine Argument-Vorlage. LEER heißt immer: dieser Agent kann das nicht
    // bzw. es ist nicht am `--help` verifiziert — dann startet er frisch, statt an
    // einem geratenen Flag zu scheitern. Nur EINE der drei wird verwendet, je nach
    // eingestelltem Modus.
    QString resumeLastArgs;  // jüngste Unterhaltung im Verzeichnis (z. B. "--continue")
    QString resumePickArgs;  // interaktive Auswahl beim Start (z. B. "--resume" ohne Wert)
    QString resumeIdArgs;    // gezielt per ID; MUSS den Platzhalter {id} enthalten

    // --- Mausrad in der Vollbild-Oberfläche des Agenten ---------------------
    // Tastenfolge, mit der DIESER Agent seinen eigenen Verlauf scrollt. LEER heißt:
    // die xterm-üblichen Cursor-Tasten (ESC[A/ESC[B) verwenden, so wie DECSET 1007 es
    // vorsieht. Einen Eintrag bekommt nur, wer nachweislich etwas anderes braucht —
    // ein geratenes Kürzel landet mitten in der Oberfläche des Agenten.
    QByteArray scrollUpKeys;
    QByteArray scrollDownKeys;

    /// Passt ein Kommando-Basisname (ohne Pfad und ohne .exe/.cmd/.bat) auf diesen
    /// Agenten? Vergleicht `command` UND alle `aliases`, Groß-/Kleinschreibung egal.
    bool matches(const QString &base) const;
};

/// Fortsetzungs-Weg beim Wiederherstellen (QTMUX-98). Die Werte sind persistiert
/// (`window/resumeAgentMode`) — Reihenfolge daher nicht ändern, nur anhängen.
enum class ResumeMode {
    None = 0,    // gar nicht fortsetzen (Vorgabe)
    Last = 1,    // jüngste Unterhaltung im Arbeitsverzeichnis — ⚠️ bei mehreren
                 // Agenten im selben Ordner bekommen alle dieselbe
    Pick = 2,    // Auswahl beim Start; nichts wird geraten, kostet einen Klick je Pane
    Reported = 3 // exakt die vom Agenten gemeldete Sitzung (set_agent_session)
};

/// Registry bekannter Agenten-CLIs. Erweiterbar — neue Agenten hier eintragen.
/// Erkennung erfolgt über den ersten Token einer getippten Kommandozeile.
class AgentRegistry {
public:
    static const QList<AgentInfo> &all();

    /// Liefert den Agenten zu einer Kommandozeile, oder nullptr wenn keiner passt.
    /// Berücksichtigt führende Pfade und Umgebungs-Präfixe (z. B. "env FOO=1 agy").
    /// `tokenIndex` (optional) erhält den Index des erkannten Kommando-Tokens —
    /// nötig, weil Präfixe wie `env FOO=1` davor stehen können.
    static const AgentInfo *detect(const QString &commandLine, int *tokenIndex = nullptr);

    /// Ergänzt eine erkannte Agenten-Kommandozeile um die Fortsetzungs-Argumente des
    /// gewählten Modus (QTMUX-85/98). `sessionRef` wird nur bei ResumeMode::Reported
    /// gebraucht und ersetzt dort den Platzhalter {id}.
    ///
    /// Eingefügt wird DIREKT hinter dem Kommando-Token, nicht am Ende — nur so bleibt
    /// sowohl `claude --resume <id> --foo` als auch eine Subkommando-Form
    /// (`agent resume --last --foo`) gültig.
    ///
    /// Unverändert zurück, wenn kein Agent erkannt wird, die Vorlage des Modus leer ist,
    /// bei Reported keine `sessionRef` vorliegt, oder der erste Token der Vorlage schon
    /// in der Zeile steht (Idempotenz über mehrere Neustarts).
    static QString resumeCommand(const QString &commandLine, ResumeMode mode,
                                 const QString &sessionRef = {});

    /// Kann dieser Agent den Modus überhaupt? (leere Vorlage = nein) — die UI kann
    /// damit erklären, warum ein Agent trotz gesetztem Modus frisch startet.
    static bool supportsResumeMode(const AgentInfo &a, ResumeMode mode);

    /// Tastenfolge, mit der der Agent `agentId` seinen Verlauf um eine Zeile scrollt
    /// (`up` = nach oben). **Leer** heißt „nichts Besonderes bekannt" — der Aufrufer
    /// nimmt dann die xterm-üblichen Cursor-Tasten, wie DECSET 1007 sie vorsieht.
    ///
    /// 🔑 Warum das überhaupt je Agent nötig ist: Codex fordert per 1007 Cursor-Tasten
    /// an, reagiert aber nur auf **Shift+Pfeil** (am laufenden Programm gemessen). Wer
    /// hier einen Eintrag ergänzt, misst ihn vorher — ein geratenes Kürzel landet
    /// mitten in der Oberfläche des Agenten und richtet dort Schaden an.
    static QByteArray scrollKeysFor(const QString &agentId, bool up);
};

} // namespace qtmux
