#include "ShellIntegration.h"
#include "SafeFileRead.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>

namespace qtmux {

namespace {
// Die mitgelieferten Skripte sind wenige KB gross; 1 MB ist grosszuegig
// und faengt trotzdem jede Gerätedatei und jeden Unfall ab.
constexpr qint64 kMaxScriptBytes = 1024 * 1024;
}  // namespace

namespace {

constexpr const char *kResourcePrefix = ":/shell-integration/";

// Reihenfolge: erst die OSC-133-Shell-Integration, dann die Ereignis-Helfer, dann die
// Anleitung. Wer die Ausgabe des Installationsbefehls liest, sieht sie in dieser Ordnung.
const QStringList &fileList() {
    static const QStringList kFiles = {
        QStringLiteral("qtmux.bash"),   QStringLiteral("qtmux.zsh"),
        QStringLiteral("qtmux.ps1"),    QStringLiteral("qtmux-event.cmd"),
        QStringLiteral("qtmux-emit.sh"), QStringLiteral("qtmux-emit.ps1"),
        QStringLiteral("qtmux-emit.cmd"),
        QStringLiteral("qtmux-wait.sh"), QStringLiteral("qtmux-wait.ps1"),
        QStringLiteral("qtmux-wait.cmd"),
        QStringLiteral("README.md"),
    };
    return kFiles;
}

// Skripte brauchen unter Unix das Ausfuehrbar-Bit — ohne das scheitert der Hook mit
// "Permission denied", und zwar erst dann, wenn der Agent fertig ist. Qt schreibt neue
// Dateien mit 0644, die Ressource traegt keine Rechte.
bool needsExecBit(const QString &name) {
    return name.endsWith(QLatin1String(".sh"));
}

} // namespace

QStringList ShellIntegration::bundledFiles() { return fileList(); }

QString ShellIntegration::defaultTargetDir() {
    const QString base =
        QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
    return base + QStringLiteral("/QTmux/shell-integration");
}

ShellIntegrationResult ShellIntegration::install(const QString &targetDir) {
    ShellIntegrationResult r;
    r.targetDir = targetDir.trimmed().isEmpty() ? defaultTargetDir()
                                                : QDir::cleanPath(targetDir.trimmed());

    QDir dir;
    if (!dir.mkpath(r.targetDir)) {
        r.error = QStringLiteral("Verzeichnis lässt sich nicht anlegen: %1").arg(r.targetDir);
        return r;
    }

    for (const QString &name : fileList()) {
        QFile src(QLatin1String(kResourcePrefix) + name);
        if (!src.open(QIODevice::ReadOnly)) {
            // Kein stiller Teilerfolg: fehlt eine Datei in der Ressource, ist das ein
            // Baufehler und keine Umgebungsfrage.
            r.error = QStringLiteral("Mitgelieferte Datei fehlt: %1").arg(name);
            return r;
        }
        const QByteArray data = src.readAll();
        const QString path = r.targetDir + QLatin1Char('/') + name;

        QFile dst(path);
        if (dst.exists()) {
            // Gedeckelt vergleichen (s. SafeFileRead.h): Zeigt die Zieldatei per
            // Symlink auf /dev/zero, läse ein blankes readAll() unendlich. Schlägt
            // die Prüfung fehl, gilt die Datei als „nicht identisch" und wird
            // überschrieben — genau das soll der Befehl ja tun.
            const auto rd = safefile::read(path, kMaxScriptBytes);
            const bool same = rd.ok && rd.data == data;
            if (same) {
                r.unchanged << name;
                continue;   // Zeitstempel stehen lassen
            }
        }
        if (!dst.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            r.error = QStringLiteral("Schreiben fehlgeschlagen: %1").arg(path);
            return r;
        }
        if (dst.write(data) != data.size()) {
            r.error = QStringLiteral("Unvollständig geschrieben: %1").arg(path);
            return r;
        }
        dst.close();

        if (needsExecBit(name)) {
            const QFileDevice::Permissions p = dst.permissions();
            dst.setPermissions(p | QFileDevice::ExeOwner | QFileDevice::ExeGroup
                               | QFileDevice::ExeOther);
        }
        r.written << name;
    }

    r.ok = true;
    return r;
}

QString ShellIntegration::hookCommandExample(const QString &targetDir) {
    const QString dir = targetDir.trimmed().isEmpty() ? defaultTargetDir() : targetDir;
#if defined(Q_OS_WIN)
    // Auf Windows fuehrt der Weg ueber die .cmd-Huelle: eine PowerShell-Datei laesst sich
    // nicht ohne Interpreter als Hook-Kommando eintragen, und die Ausfuehrungsrichtlinie
    // wuerde sie je nach Maschine ohnehin blocken.
    return QDir::toNativeSeparators(dir + QStringLiteral("/qtmux-emit.cmd"))
           + QStringLiteral(" done \"Aufgabe erledigt\"");
#else
    return dir + QStringLiteral("/qtmux-emit.sh done \"Aufgabe erledigt\"");
#endif
}

} // namespace qtmux
