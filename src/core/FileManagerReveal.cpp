#include "FileManagerReveal.h"

#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QSharedPointer>
#include <QUrl>

namespace FileManagerReveal {

Platform currentPlatform() {
#if defined(Q_OS_MACOS)
    return Platform::MacOS;
#elif defined(Q_OS_WIN)
    return Platform::Windows;
#else
    return Platform::Linux;
#endif
}

QString localPathFor(const QString &target) {
    QString path;
    if (target.startsWith(QLatin1String("file:"), Qt::CaseInsensitive)) {
        const QUrl url(target);
        if (!url.isLocalFile()) return {};
        path = url.toLocalFile();
    } else {
        path = target;
    }
    // Nur absolute Pfade: ein relativer hinge am Prozess-CWD von QTmux, nicht an dem der
    // Session — der LinkDetector hat Dateipfade ohnehin schon absolut aufgelöst.
    if (path.isEmpty() || !QDir::isAbsolutePath(path)) return {};
    const QFileInfo fi(path);
    if (!fi.exists()) return {};
    return QDir::cleanPath(fi.absoluteFilePath());
}

QList<Command> commands(const QString &absPath, Platform platform) {
    if (absPath.isEmpty()) return {};
    switch (platform) {
    case Platform::MacOS:
        if (!absPath.startsWith(QLatin1Char('/'))) return {};
        // `open -R` markiert die Datei im Finder; bei einem Ordner den Ordner selbst im
        // Elternordner. Der Pfad beginnt mit `/` — nie als Option lesbar.
        return {{QStringLiteral("open"), {QStringLiteral("-R"), absPath}}};
    case Platform::Windows: {
        // Laufwerk (`C:/`) oder UNC (`//server`); Trenner per Zeichenkette umsetzen, nicht
        // per toNativeSeparators (das wirkt nur AUF Windows — der Test läuft überall).
        const bool drive = absPath.size() >= 3 && absPath.at(1) == QLatin1Char(':')
                           && (absPath.at(2) == QLatin1Char('/') || absPath.at(2) == QLatin1Char('\\'));
        const bool unc = absPath.startsWith(QLatin1String("//"))
                         || absPath.startsWith(QLatin1String("\\\\"));
        if (!drive && !unc) return {};
        // `/select,` als EIGENES Argument (wie Qt Creator): QProcess quotet den Pfad bei
        // Leerzeichen einzeln → `explorer /select, "C:\a b\c.txt"`, das Explorer versteht.
        // Ein zusammengesetztes `"/select,C:\a b"` würde als Ganzes gequotet und öffnet
        // dann nur „Dokumente".
        return {{QStringLiteral("explorer.exe"),
                 {QStringLiteral("/select,"), QString(absPath).replace(QLatin1Char('/'), QLatin1Char('\\'))}}};
    }
    case Platform::Linux: {
        if (!absPath.startsWith(QLatin1Char('/'))) return {};
        // dbus-send trennt array:string:-Elemente an Kommas — darum ALLES außer `/` und
        // den unreservierten Zeichen prozentkodieren (Komma → %2C, Leerzeichen, Umlaute).
        const QString uri = QStringLiteral("file://")
            + QString::fromLatin1(QUrl::toPercentEncoding(absPath, "/"));
        // Elternordner per Zeichenkette (nicht QFileInfo), damit die Ermittlung auf jedem
        // Host dasselbe liefert — der Test prüft alle drei Plattformen überall.
        const int slash = absPath.lastIndexOf(QLatin1Char('/'));
        const QString parent = slash > 0 ? absPath.left(slash) : QStringLiteral("/");
        return {
            {QStringLiteral("dbus-send"),
             {QStringLiteral("--session"), QStringLiteral("--print-reply"),
              QStringLiteral("--dest=org.freedesktop.FileManager1"),
              QStringLiteral("--type=method_call"),
              QStringLiteral("/org/freedesktop/FileManager1"),
              QStringLiteral("org.freedesktop.FileManager1.ShowItems"),
              QStringLiteral("array:string:") + uri,
              QStringLiteral("string:")}},
            {QStringLiteral("xdg-open"), {parent}},
        };
    }
    }
    return {};
}

namespace {

// Startet Befehl `i`; scheitert er (nicht startbar oder Exit ≠ 0), folgt `i+1`.
// Der letzte Befehl läuft losgelöst (startDetached) — dessen Ergebnis interessiert nicht
// mehr. Nur Linux hat überhaupt mehr als einen.
void runFrom(QSharedPointer<const QList<Command>> cmds, int i) {
    if (i >= cmds->size()) return;
    const Command &c = cmds->at(i);
    if (i == cmds->size() - 1) {
        QProcess::startDetached(c.program, c.arguments);
        return;
    }
    auto *p = new QProcess;
    p->setProgram(c.program);
    p->setArguments(c.arguments);
    p->setStandardOutputFile(QProcess::nullDevice());
    p->setStandardErrorFile(QProcess::nullDevice());
    QObject::connect(p, &QProcess::errorOccurred, p, [p, cmds, i](QProcess::ProcessError e) {
        if (e != QProcess::FailedToStart) return;     // Rest meldet `finished`
        p->deleteLater();
        runFrom(cmds, i + 1);
    });
    QObject::connect(p, &QProcess::finished, p,
                     [p, cmds, i](int code, QProcess::ExitStatus st) {
        p->deleteLater();
        if (st != QProcess::NormalExit || code != 0) runFrom(cmds, i + 1);
    });
    p->start();
}

} // namespace

bool reveal(const QString &target) {
    const QString path = localPathFor(target);
    if (path.isEmpty()) return false;
    auto cmds = QSharedPointer<const QList<Command>>::create(commands(path, currentPlatform()));
    if (cmds->isEmpty()) return false;
    runFrom(cmds, 0);
    return true;
}

} // namespace FileManagerReveal
