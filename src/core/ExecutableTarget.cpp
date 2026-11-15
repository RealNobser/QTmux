#include "ExecutableTarget.h"

#include <QFile>
#include <QFileInfo>
#include <QStringList>

namespace ExecutableTarget {

namespace {

using FileManagerReveal::Platform;

// Endung des Dateinamens in Kleinbuchstaben mit Punkt (`.bat`), leer ohne Punkt im Namen. Nur der
// letzte Abschnitt zählt: Bei `bericht.pdf.exe` entscheidet `.exe`. Punkte, Leerzeichen und
// Trenner am Ende fallen vorher weg: Windows ignoriert die ersten beiden beim Öffnen
// (`werkzeug.bat.` ist `werkzeug.bat`), und `Rechner.app/` ist das Bündel. Auch eine Datei, die
// nur `.bat` heißt, hat diese Endung. Ein Punkt in einem Ordnernamen ist keine Endung der Datei.
QString suffixOf(const QString &path) {
    QString p = path;
    while (p.endsWith(QLatin1Char('.')) || p.endsWith(QLatin1Char(' '))
           || p.endsWith(QLatin1Char('/')) || p.endsWith(QLatin1Char('\\')))
        p.chop(1);
    const qsizetype slash = qMax(p.lastIndexOf(QLatin1Char('/')), p.lastIndexOf(QLatin1Char('\\')));
    const QString name = p.mid(slash + 1);
    const qsizetype dot = name.lastIndexOf(QLatin1Char('.'));
    return dot < 0 ? QString() : name.mid(dot).toLower();
}

// Was Windows beim Öffnen ausführt oder installiert. Die Vorgabe von PATHEXT gilt immer, auch
// wenn die Variable auf dem Rechner gekürzt wurde; was sie zusätzlich nennt, kommt hinzu.
bool windowsSuffixRuns(const QString &suffix, const QString &pathExt) {
    static const QStringList builtin{
        // Vorgabe von PATHEXT
        QStringLiteral(".com"), QStringLiteral(".exe"), QStringLiteral(".bat"), QStringLiteral(".cmd"),
        QStringLiteral(".vbs"), QStringLiteral(".vbe"), QStringLiteral(".js"),  QStringLiteral(".jse"),
        QStringLiteral(".wsf"), QStringLiteral(".wsh"), QStringLiteral(".msc"),
        // Installer und Pakete
        QStringLiteral(".msi"), QStringLiteral(".msp"), QStringLiteral(".msix"),
        QStringLiteral(".msixbundle"), QStringLiteral(".appx"), QStringLiteral(".appxbundle"),
        QStringLiteral(".appinstaller"), QStringLiteral(".application"), QStringLiteral(".appref-ms"),
        QStringLiteral(".gadget"), QStringLiteral(".diagcab"),
        // Abbilder: beim Öffnen eingehängt, ihr Inhalt gilt dann als lokal
        QStringLiteral(".iso"), QStringLiteral(".img"), QStringLiteral(".vhd"), QStringLiteral(".vhdx"),
        // Verknüpfungen: zeigen auf ein Programm oder tragen einen Befehl (`.lnk`, `.url` und
        // `.pif` stehen in asksFor, sie gelten auch für ein Verzeichnis)
        QStringLiteral(".settingcontent-ms"), QStringLiteral(".website"),
        QStringLiteral(".library-ms"), QStringLiteral(".search-ms"), QStringLiteral(".searchconnector-ms"),
        // Programme unter anderem Namen
        QStringLiteral(".scr"), QStringLiteral(".cpl"), QStringLiteral(".xll"),
        // Skripte und Anwendungen der Skript-Hosts; `.py` läuft, sobald Python installiert ist
        QStringLiteral(".ps1"), QStringLiteral(".hta"), QStringLiteral(".jar"), QStringLiteral(".jnlp"),
        QStringLiteral(".wsc"), QStringLiteral(".sct"), QStringLiteral(".chm"),
        QStringLiteral(".py"),  QStringLiteral(".pyw"), QStringLiteral(".ws"),  QStringLiteral(".psc1"),
        // schreibt beim Öffnen in die Registrierung
        QStringLiteral(".reg"),
    };
    if (builtin.contains(suffix)) return true;
    const QStringList extra = pathExt.toLower().split(QLatin1Char(';'), Qt::SkipEmptyParts);
    for (const QString &e : extra)
        if (e.trimmed() == suffix) return true;
    return false;
}

// macOS: Programme, Installer und Erweiterungen sind dort oft Bündel, also Verzeichnisse. Die
// Endung zählt darum auch für ein Verzeichnis.
bool macSuffixRuns(const QString &suffix) {
    static const QStringList runs{
        QStringLiteral(".app"),                                   // Programm
        QStringLiteral(".command"), QStringLiteral(".tool"),      // Terminal führt sie aus
        QStringLiteral(".terminal"),                              // Terminal-Einstellung mit Startbefehl
        QStringLiteral(".pkg"), QStringLiteral(".mpkg"),          // Installer
        QStringLiteral(".prefpane"), QStringLiteral(".saver"),    // werden beim Öffnen installiert
        QStringLiteral(".workflow"), QStringLiteral(".action"),   // Automator
        QStringLiteral(".fileloc"), QStringLiteral(".inetloc"),   // Verweis, auch auf ein Programm
        QStringLiteral(".webloc"),
        QStringLiteral(".dmg"),                                   // Abbild, wird eingehängt
        QStringLiteral(".jar"),                                   // läuft, sobald Java installiert ist
    };
    return runs.contains(suffix);
}

bool linuxSuffixRuns(const QString &suffix) {
    static const QStringList runs{
        QStringLiteral(".desktop"),                               // Starter mit Befehlszeile
        QStringLiteral(".appimage"), QStringLiteral(".run"), QStringLiteral(".sh"),
        QStringLiteral(".jar"),
        QStringLiteral(".deb"), QStringLiteral(".rpm"), QStringLiteral(".flatpakref"),  // Installer
        QStringLiteral(".flatpak"), QStringLiteral(".snap"),
    };
    return runs.contains(suffix);
}

bool asksFor(const QString &path, const Facts &facts, Platform platform, const QString &pathExt) {
    const QString suffix = suffixOf(path);
    switch (platform) {
    case Platform::Windows:
        // Kein Ausführungsrecht unter Windows: Dort entscheidet der Name.
        // Verknüpfungen zuerst: Qt folgt einer `.lnk` und meldet für eine Verknüpfung auf einen
        // Ordner ein Verzeichnis. Was die Shell beim Öffnen daraus macht, steht in der Datei.
        if (suffix == QLatin1String(".lnk") || suffix == QLatin1String(".url")
            || suffix == QLatin1String(".pif"))
            return true;
        if (facts.isDir) return false;
        // Ohne Endung gibt es kein verknüpftes Programm, und die Shell hängt beim Öffnen selbst
        // eine Programm-Endung an: `werkzeug` startet `werkzeug.exe`, wenn es daneben liegt.
        if (suffix.isEmpty()) return true;
        // Ein Doppelpunkt hinter dem Laufwerk nennt einen Datenstrom (`notiz.txt:x.exe`,
        // `werkzeug.exe::$DATA`). Was die Shell daraus macht, sagt die Endung nicht.
        if (path.indexOf(QLatin1Char(':'), 2) >= 0) return true;
        return windowsSuffixRuns(suffix, pathExt);
    case Platform::MacOS:
        if (macSuffixRuns(suffix)) return true;                    // auch als Verzeichnis (Bündel)
        if (facts.isDir) return false;
        return facts.anyExecBit;
    case Platform::Linux:
        if (facts.isDir) return false;
        return linuxSuffixRuns(suffix) || facts.anyExecBit;
    }
    return true;                                                   // unbekannte Plattform: fragen
}

} // namespace

Facts factsFor(const QString &absPath) {
    Facts f;
    const QFileInfo fi(absPath);                 // folgt symbolischen Verknüpfungen
    if (!fi.exists()) return f;
    f.isDir = fi.isDir();
    const QFile::Permissions exec = QFile::ExeOwner | QFile::ExeUser | QFile::ExeGroup | QFile::ExeOther;
    f.anyExecBit = (fi.permissions() & exec) != QFile::Permissions();
    f.resolved = fi.canonicalFilePath();
    return f;
}

bool asksBeforeOpen(const QString &absPath, const Facts &facts, Platform platform,
                    const QString &pathExt) {
    if (asksFor(absPath, facts, platform, pathExt)) return true;
    // Dasselbe am aufgelösten Ziel: Der Name der Verknüpfung sagt nicht, was geöffnet wird.
    return !facts.resolved.isEmpty() && facts.resolved != absPath
        && asksFor(facts.resolved, facts, platform, pathExt);
}

bool asksBeforeOpen(const QString &absPath) {
    return asksBeforeOpen(absPath, factsFor(absPath), FileManagerReveal::currentPlatform(),
                          qEnvironmentVariable("PATHEXT"));
}

} // namespace ExecutableTarget
