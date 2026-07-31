#include "GitInfo.h"

#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QFileInfo>

namespace qtmux {
namespace {

/// Obergrenze für die Aufwärtssuche. Die Abbruchbedingung ist eigentlich `cdUp() == false`
/// an der Dateisystemwurzel; der Zähler ist der Gürtel zum Hosenträger für pathologische
/// Pfade (Symlink-Ringe, Netzlaufwerke) — er darf nie greifen, kostet aber nichts.
constexpr int kMaxDepth = 128;

/// Liest die erste Zeile einer Datei und schneidet Leerraum ab (inkl. CR — eine unter
/// Windows geschriebene `HEAD` endet auf CRLF). Nicht existierend/unlesbar → leer.
/// Die Längenbegrenzung schützt davor, versehentlich eine große Datei einzulesen, falls
/// an der Stelle etwas anderes liegt als erwartet.
QString firstLineOf(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))        // bewusst still: „kein Repo" ist der Normalfall
        return QString();
    const QByteArray line = f.readLine(4096);
    return QString::fromUtf8(line).trimmed();
}

/// Ist `s` eine rohe Commit-ID? git schreibt sie in `HEAD`, wenn kein Branch ausgecheckt
/// ist (detached HEAD). 40 Zeichen für SHA-1, 64 für SHA-256-Repositories.
bool isRawSha(const QString &s)
{
    if (s.size() != 40 && s.size() != 64)
        return false;
    for (const QChar c : s) {
        if (!((c >= u'0' && c <= u'9') || (c >= u'a' && c <= u'f') || (c >= u'A' && c <= u'F')))
            return false;
    }
    return true;
}

/// Wertet eine `.git`-**Datei** aus (Worktree oder Submodul): Inhalt `gitdir: <pfad>`.
/// Der Pfad darf relativ sein — bei Submodulen ist das der Normalfall (`gitdir:
/// ../.git/modules/foo`) — und wird dann gegen das Verzeichnis der `.git`-Datei aufgelöst.
/// Zeigt er ins Leere, ist das Ergebnis leer (→ ungültig), NICHT ein Weitersuchen nach oben:
/// Ein kaputter Zeiger ist ein Fehler, kein „hier ist kein Repo".
QString gitDirFromFile(const QString &gitFilePath, const QDir &containingDir)
{
    const QString line = firstLineOf(gitFilePath);
    const QLatin1String marker("gitdir:");
    if (!line.startsWith(marker))
        return QString();

    const QString target = line.mid(marker.size()).trimmed();
    if (target.isEmpty())
        return QString();

    // absoluteFilePath() lässt einen bereits absoluten Pfad unverändert.
    const QString resolved = QDir::cleanPath(containingDir.absoluteFilePath(target));
    return QFileInfo(resolved).isDir() ? resolved : QString();
}

/// Sucht von `dir` aus nach oben das Git-Verzeichnis. Rückgabe ist der Pfad, in dem `HEAD`
/// liegt — bei einem Worktree also `<repo>/.git/worktrees/<name>`, nicht `<repo>/.git`.
QString findGitDir(const QString &dir)
{
    if (dir.isEmpty())
        return QString();

    QDir d(QDir(dir).absolutePath());
    if (!d.exists())
        return QString();

    for (int depth = 0; depth < kMaxDepth; ++depth) {
        const QString candidate = d.filePath(QStringLiteral(".git"));
        const QFileInfo fi(candidate);
        if (fi.isDir())
            return candidate;
        if (fi.isFile())                       // Worktree/Submodul: Zeiger auf das echte gitdir
            return gitDirFromFile(candidate, d);
        if (!d.cdUp())                         // Wurzel erreicht
            break;
    }
    return QString();
}

} // namespace

GitInfo GitInfo::forDirectory(const QString &dir)
{
    GitInfo info;

    const QString gitDir = findGitDir(dir);
    if (gitDir.isEmpty())
        return info;                           // valid bleibt false

    const QString head = firstLineOf(gitDir + QStringLiteral("/HEAD"));
    if (head.isEmpty())
        return info;

    const QLatin1String refPrefix("ref:");
    if (head.startsWith(refPrefix)) {
        QString ref = head.mid(refPrefix.size()).trimmed();
        const QLatin1String heads("refs/heads/");
        if (ref.startsWith(heads))
            ref = ref.mid(heads.size());       // Branchnamen dürfen `/` enthalten → alles nehmen
        // Ein Ref außerhalb von refs/heads/ (etwa refs/remotes/… oder ein Bisect-Ref) wird
        // in voller Länge angezeigt, statt einen Namen zu raten — sichtbar ungewöhnlich.
        if (ref.isEmpty())
            return info;
        info.branch = ref;
        info.valid  = true;
        return info;
    }

    if (isRawSha(head)) {
        info.detached = true;
        info.shortSha = head.left(7);
        info.valid    = true;
    }
    return info;                               // alles andere: unverständliches HEAD → ungültig
}

} // namespace qtmux
