#pragma once

#include <QString>
#include <QList>

namespace qtmux {

/// Metadaten zu einem bekannten KI-Agenten-CLI.
struct AgentInfo {
    QString id;          // stabiler Schlüssel (z. B. "antigravity") — QML mappt darauf Icon/Farbe
    QString command;     // erwarteter Kommandoname (z. B. "agy")
    QString displayName; // Anzeigename in der Sidebar (z. B. "AntiGravity")
};

/// Registry bekannter Agenten-CLIs. Erweiterbar — neue Agenten hier eintragen.
/// Erkennung erfolgt über den ersten Token einer getippten Kommandozeile.
class AgentRegistry {
public:
    static const QList<AgentInfo> &all();

    /// Liefert den Agenten zu einer Kommandozeile, oder nullptr wenn keiner passt.
    /// Berücksichtigt führende Pfade und Umgebungs-Präfixe (z. B. "env FOO=1 agy").
    static const AgentInfo *detect(const QString &commandLine);
};

} // namespace qtmux
