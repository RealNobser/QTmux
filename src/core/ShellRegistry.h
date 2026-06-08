#pragma once

#include <QList>
#include <QString>

namespace qtmux {

/// Eine auswählbare lokale Shell (Programmname + Anzeigename).
struct ShellInfo {
    QString program;   ///< Ausführbares Programm, z. B. "powershell.exe"
    QString name;      ///< Anzeigename, z. B. "PowerShell"
};

/// Bekannte/erkannte lokale Shells je Plattform.
class ShellRegistry {
public:
    /// Auswählbare Shells dieser Plattform.
    ///   - Windows: PowerShell + Eingabeaufforderung (cmd); zusätzlich PowerShell 7
    ///     (pwsh), falls im PATH gefunden.
    ///   - Unix: die Login-Shell als einzelner Eintrag.
    /// Das erste Element ist die plattformübliche Vorgabe.
    static QList<ShellInfo> available();
};

} // namespace qtmux
