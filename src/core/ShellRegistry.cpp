#include "ShellRegistry.h"
#include "PtyBackend.h"

#include <QCoreApplication>
#include <QStandardPaths>

#if defined(Q_OS_WIN)
#include <QDir>
#include <QFileInfo>
#include <QProcessEnvironment>
#include <QSettings>
#include <utility>   // std::as_const
#endif

namespace qtmux {

#if defined(Q_OS_WIN)
namespace {

/// Pfad zum Clink-Launcher (clink.bat) oder leer, falls Clink nicht gefunden wird.
/// Clink (https://github.com/chrisant996/clink) bringt Readline-Completion/History
/// in cmd.exe. Es wird NICHT mitgeliefert (GPL-3.0) — nur eine installierte Version
/// erkannt. Suche: PATH-Shim (scoop/winget/manuelles PATH) zuerst, dann die üblichen
/// Installationsorte. (QTMUX-25)
QString findClinkLauncher() {
    // 1) Im PATH (deckt scoop-/winget-Shims und manuelle PATH-Einträge ab).
    //    findExecutable findet clink.bat oder clink.exe.
    QString hit = QStandardPaths::findExecutable(QStringLiteral("clink"));
    if (!hit.isEmpty()) {
        const QFileInfo fi(hit);
        // clink.bat direkt nutzen; bei clink.exe nach der .bat im selben Ordner sehen.
        if (fi.suffix().compare(QLatin1String("bat"), Qt::CaseInsensitive) == 0)
            return QDir::toNativeSeparators(hit);
        const QString bat = fi.absoluteDir().filePath(QStringLiteral("clink.bat"));
        if (QFileInfo::exists(bat)) return QDir::toNativeSeparators(bat);
        return QDir::toNativeSeparators(hit);   // clink.exe akzeptiert `inject` ebenfalls
    }

    // 2) Bekannte Installationsorte (Installer / scoop), clink.bat erwartet.
    const QProcessEnvironment e = QProcessEnvironment::systemEnvironment();
    QStringList candidates;
    const QString localApp = e.value(QStringLiteral("LOCALAPPDATA"));
    if (!localApp.isEmpty()) {
        candidates << QDir(localApp).filePath(QStringLiteral("clink/clink.bat"));
        candidates << QDir(localApp).filePath(QStringLiteral("Programs/clink/clink.bat"));
    }
    const QString userProfile = e.value(QStringLiteral("USERPROFILE"));
    if (!userProfile.isEmpty())
        candidates << QDir(userProfile).filePath(
            QStringLiteral("scoop/apps/clink/current/clink.bat"));
    for (const QString &var : {QStringLiteral("ProgramFiles"),
                               QStringLiteral("ProgramFiles(x86)")}) {
        const QString pf = e.value(var);
        if (!pf.isEmpty()) candidates << QDir(pf).filePath(QStringLiteral("clink/clink.bat"));
    }
    for (const QString &c : std::as_const(candidates))
        if (QFileInfo::exists(c)) return QDir::toNativeSeparators(c);

    return QString();
}

/// True, wenn Clink bereits global per cmd.exe-AutoRun injiziert wird
/// (HKCU/HKLM \Software\Microsoft\Command Processor\AutoRun verweist auf clink).
/// Dann lädt JEDE cmd.exe Clink ohnehin — eine eigene "Eingabeaufforderung (Clink)"-
/// Shell wäre überflüssig und ihr zusätzliches `clink inject` meldet nur
/// "Clink already loaded in process N". In diesem Fall bieten wir sie nicht an.
bool clinkAutoRunActive() {
    for (auto scope : {QSettings::UserScope, QSettings::SystemScope}) {
        QSettings s(QSettings::NativeFormat, scope,
                    QStringLiteral("Microsoft"), QStringLiteral("Command Processor"));
        if (s.value(QStringLiteral("AutoRun")).toString()
                .contains(QLatin1String("clink"), Qt::CaseInsensitive))
            return true;
    }
    return false;
}

} // namespace
#endif

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

    // Clink-Shell nur anbieten, wenn Clink installiert ist (QTMUX-25) UND nicht schon
    // per AutoRun global injiziert wird (sonst doppeltes inject -> "already loaded").
    // Das "program" ist hier eine vollständige Kommandozeile; PtyBackend::start zerlegt
    // sie. Anzeigename "(Clink)"; prettifyTitle erkennt die Kommandozeile ebenfalls.
    const QString clink = findClinkLauncher();
    if (!clink.isEmpty() && !clinkAutoRunActive()) {
        const QString cmdLine =
            QStringLiteral("cmd.exe /k \"%1\" inject --quiet").arg(clink);
        list.append({cmdLine,
                     QCoreApplication::translate("Shells", "Eingabeaufforderung (Clink)")});
    }
    return list;
#else
    // Unix: die Login-Shell ($SHELL bzw. plattformüblicher Fallback).
    return {{PtyBackend::defaultShell(), QStringLiteral("Shell")}};
#endif
}

} // namespace qtmux
