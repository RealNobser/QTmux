#pragma once

#include <QString>
#include <QStringList>

namespace qtmux {

/// Ergebnis einer Installation der Shell-Helfer.
struct ShellIntegrationResult {
    bool ok = false;
    QString targetDir;      ///< tatsaechlich beschriebenes Verzeichnis
    QStringList written;    ///< neu geschrieben oder aktualisiert (Dateinamen)
    QStringList unchanged;  ///< lagen bereits inhaltsgleich vor
    QString error;          ///< nur gesetzt, wenn ok == false
};

/// Die mitgelieferten Shell-Helfer (QTMUX-38).
///
/// 🔑 Die Skripte stecken als **Qt-Ressource** im Binary und werden auf Wunsch an einen
/// stabilen Ort im Benutzerprofil geschrieben. Das loest zwei Probleme auf einmal:
///
/// 1. **Das AppImage hat keinen stabilen Pfad.** Es wird bei jedem Start unter
///    `/tmp/.mount_XXXXXX` gemountet — ein dorthin zeigender Hook-Eintrag in einer
///    `settings.json` ueberlebt keinen Neustart. Reines Mitpaketieren loest den
///    Linux-Fall daher grundsaetzlich nicht, egal wie sorgfaeltig man es macht.
/// 2. **Versionsdrift.** Aus der Ressource stammende Dateien passen zwangslaeufig zur
///    laufenden Fassung; wer sie einzeln von GitHub zieht, muss die richtige selbst treffen.
class ShellIntegration {
public:
    /// Dateinamen der mitgelieferten Helfer (ohne Pfad), stabil sortiert.
    static QStringList bundledFiles();

    /// Stabiles Standardziel, wenn der Aufrufer keines angibt:
    /// `<GenericDataLocation>/QTmux/shell-integration`.
    /// 🔑 Bewusst NICHT `AppDataLocation`: dort steckt der `applicationName`, und der
    /// traegt bei `--profile`/`QTMUX_PROFILE` ein Suffix — der Pfad wuerde also je nach
    /// gestartetem Profil wandern. Ein Hook-Eintrag muss aber fuer alle Profile gelten.
    static QString defaultTargetDir();

    /// Schreibt alle Helfer nach `targetDir` (leer = `defaultTargetDir()`).
    /// Bestehende Dateien werden ueberschrieben — die mitgelieferte Fassung ist die
    /// zur laufenden Version passende. Inhaltsgleiche Dateien werden nicht angefasst
    /// (dann bleibt auch der Zeitstempel stehen) und als `unchanged` gemeldet.
    /// Auf Unix bekommen die Skripte das Ausfuehrbar-Bit.
    static ShellIntegrationResult install(const QString &targetDir = QString());

    /// Die fertige Zeile fuer einen Stop-Hook — plattformrichtig, damit der Anwender
    /// sie kopieren kann statt einen Pfad zusammenzusuchen.
    static QString hookCommandExample(const QString &targetDir);
};

} // namespace qtmux
