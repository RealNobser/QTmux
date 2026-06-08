#include "ShellRegistry.h"
#include "PtyBackend.h"

#include <QCoreApplication>
#include <QStandardPaths>

namespace qtmux {

QList<ShellInfo> ShellRegistry::available() {
#if defined(Q_OS_WIN)
    // Eigennamen (PowerShell …) bleiben unübersetzt; "Eingabeaufforderung" ist
    // lokalisierbar (Kontext "Shells", engl. "Command Prompt").
    QList<ShellInfo> list = {
        {QStringLiteral("powershell.exe"), QStringLiteral("PowerShell")},
        {QStringLiteral("cmd.exe"),
         QCoreApplication::translate("Shells", "Eingabeaufforderung")},
    };
    // PowerShell 7 nur anbieten, wenn es tatsächlich installiert ist (nicht im
    // Windows-Lieferumfang). pwsh.exe liegt typischerweise in %ProgramFiles%\PowerShell.
    const QString pwsh = QStandardPaths::findExecutable(QStringLiteral("pwsh"));
    if (!pwsh.isEmpty())
        list.append({QStringLiteral("pwsh.exe"), QStringLiteral("PowerShell 7")});
    return list;
#else
    // Unix: die Login-Shell ($SHELL bzw. plattformüblicher Fallback).
    return {{PtyBackend::defaultShell(), QStringLiteral("Shell")}};
#endif
}

} // namespace qtmux
