#pragma once

#include <QString>
#include <QList>

namespace qtmux {

/// Metadaten zu einem bekannten KI-Agenten-CLI.
struct AgentInfo {
    QString id;          // stabiler Schlüssel (z. B. "antigravity") — QML mappt darauf Icon/Farbe
    QString command;     // erwarteter Kommandoname (z. B. "agy")
    QString displayName; // Anzeigename in der Sidebar (z. B. "AntiGravity")
    // Argumente, die den Agenten seine VORHERIGE Unterhaltung fortsetzen lassen
    // (QTMUX-85, z. B. "--continue"). Leer = der Agent kann das nicht bzw. das Flag
    // ist nicht verifiziert; dann startet er beim Wiederherstellen frisch.
    QString resumeArgs;
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

    /// Ergänzt eine erkannte Agenten-Kommandozeile um die Fortsetzungs-Argumente
    /// (QTMUX-85). Eingefügt wird DIREKT hinter dem Kommando-Token, nicht am Ende —
    /// nur so bleibt sowohl `claude --continue --foo` als auch eine mögliche
    /// Subkommando-Form (`agent resume --last --foo`) gültig.
    /// Unverändert zurück, wenn kein Agent erkannt wird, `resumeArgs` leer ist oder
    /// dessen erster Token bereits in der Zeile steht (Idempotenz über Neustarts).
    static QString resumeCommand(const QString &commandLine);
};

} // namespace qtmux
