#include "LinkDetector.h"

#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>
#include <QUrl>

namespace LinkDetector {

bool isAllowedScheme(const QString &scheme) {
    // Eng gehalten: genau die Schemata, deren OS-Handler harmlos-erwartbar sind. Alles
    // andere (javascript:, data:, vscode:, …) bleibt draußen — ein Agent soll über einen
    // Klick keinen beliebigen Protokoll-Handler auslösen können.
    static const QStringList allowed{
        QStringLiteral("http"), QStringLiteral("https"),
        QStringLiteral("ftp"),  QStringLiteral("mailto"),
        QStringLiteral("file"),
    };
    return allowed.contains(scheme.toLower());
}

namespace {

// Am Ende eines Treffers stehen oft Satzzeichen, die nicht mehr zum Ziel gehören
// („(siehe http://x)", „Pfad: /a/b."). Die schneiden wir ab — schließende Klammern
// aber nur, wenn im Treffer keine passende öffnende steht (Wikipedia-URLs …).
QString trimTrailing(const QString &s) {
    QString r = s;
    static const QString punct = QStringLiteral(".,;:!?'\"");
    auto unbalanced = [&r](QChar open, QChar close) {
        return r.count(close) > r.count(open);   // mehr schließende als öffnende
    };
    while (!r.isEmpty()) {
        const QChar c = r.back();
        if (punct.contains(c)) { r.chop(1); continue; }
        // Schließende Klammern nur so weit abschneiden, bis sie ausgeglichen sind —
        // "C_(Sprache)" behält seine Klammer, die umschließende "(…)" fällt weg.
        if (c == ')' && unbalanced('(', ')')) { r.chop(1); continue; }
        if (c == ']' && unbalanced('[', ']')) { r.chop(1); continue; }
        if (c == '}' && unbalanced('{', '}')) { r.chop(1); continue; }
        break;
    }
    return r;
}

// Kandidat wie ein Dateipfad? (enthält Trenner oder Home-Tilde — ein nacktes Wort ohne
// Trenner lassen wir bewusst durch die Existenzprüfung später fallen, wenn es doch eine
// Datei im cwd ist.) Diese Vorprüfung spart nur die QFileInfo-Syscalls für Nicht-Pfade.
bool looksPathish(const QString &tok) {
    return tok.contains('/') || tok.contains('\\')
        || tok.startsWith(QLatin1Char('~'));
}

QString resolvePath(const QString &tok, const QString &cwd) {
    QString p = tok;
    if (p.startsWith(QLatin1String("~/")) || p == QLatin1String("~"))
        p = QDir::homePath() + p.mid(1);
    // Ohne bekanntes Arbeitsverzeichnis zählen nur absolute Pfade. Ein relativer hinge sonst
    // am Arbeitsverzeichnis von QTmux selbst und träfe eine fremde Datei.
    if (!QDir::isAbsolutePath(p) && cwd.isEmpty()) return {};
    const QString candidate = QDir::isAbsolutePath(p) ? p : QDir(cwd).filePath(p);
    // VOR jedem Dateizugriff. Auch das Arbeitsverzeichnis kann ein Netzpfad sein: Der gepollte Wert
    // folgt der Shell, eine OSC-7-Meldung mit Netzpfad verwirft VtScreen dagegen schon vorher.
    if (isNetworkPath(p) || isNetworkPath(candidate)) return {};
    QFileInfo fi(candidate);
    return fi.exists() ? fi.absoluteFilePath() : QString();
}

} // namespace

bool isRemoteFileUrl(const QUrl &url) {
    // Geprüft wird, was beim Öffnen ankommt: `toLocalFile` ist dekodiert und macht aus jedem Host
    // `//host/…`, auch aus `localhost`. Ungültige URLs zählen mit: `fromUserInput` machte aus
    // `file://%2Fserver/x` beim Klick eine http-URL.
    return !url.isValid() || isNetworkPath(url.toLocalFile());
}

bool isNetworkPath(const QString &path) {
    const auto isSeparator = [](QChar c) { return c == QLatin1Char('/') || c == QLatin1Char('\\'); };
    if (path.isEmpty() || !isSeparator(path.at(0))) return false;
    // Von der Wurzel aus laufen und Punkt-Segmente auflösen: `QDir::cleanPath` macht aus `/.//server`
    // und `/x/..//server` ein `//server`. Maßgeblich ist also, was an der Wurzel ankommt: ein weiterer
    // Trenner, oder das NT-Präfix `\??\` (führt unter Windows ebenfalls zu einem anderen Rechner).
    int depth = 0;
    const QStringView view(path);
    for (qsizetype i = 1; i < view.size();) {
        qsizetype end = i;
        while (end < view.size() && !isSeparator(view.at(end))) ++end;
        const QStringView segment = view.sliced(i, end - i);
        if (depth == 0 && end < view.size() && (segment.isEmpty() || segment == u"??")) return true;
        if (segment == u"..") {
            if (depth > 0) --depth;
        } else if (!segment.isEmpty() && segment != u".") {
            ++depth;
        }
        i = end + 1;
    }
    return false;
}

QList<Span> detect(const QString &line, const QString &cwd) {
    QList<Span> spans;

    // 1) URLs mit Schema. Nur freigegebene Schemata; der Rest wird ignoriert. Rahmenzeichen
    //    (U+2500–257F) beenden einen Treffer: Agenten-Oberflächen zeichnen Kästen, und ein
    //    `https://x│` gehört nicht zum Ziel.
    static const QRegularExpression urlRe(
        QStringLiteral(R"((\w[\w+.-]*)://[^\s"'<>`\x{2500}-\x{257F}]+|mailto:[^\s"'<>`\x{2500}-\x{257F}]+)"));
    QList<QPair<int,int>> taken;  // belegte Bereiche, um Pfad-Doppel zu vermeiden
    auto it = urlRe.globalMatch(line);
    while (it.hasNext()) {
        const auto m = it.next();
        QString hit = trimTrailing(m.captured(0));
        if (hit.isEmpty()) continue;
        const QString scheme = hit.startsWith(QLatin1String("mailto:"))
            ? QStringLiteral("mailto")
            : hit.left(hit.indexOf(QLatin1String("://")));
        if (!isAllowedScheme(scheme)) continue;
        if (scheme.compare(QLatin1String("file"), Qt::CaseInsensitive) == 0
            && isRemoteFileUrl(QUrl(hit)))
            continue;
        const int hs = static_cast<int>(m.capturedStart(0));
        const int hl = static_cast<int>(hit.length());
        spans.append({hs, hl, hit, Span::Url});
        taken.append({hs, hl});
    }

    // 2) Dateipfade: die Zeile in „whitespace-getrennte Token" zerlegen und jedes auf
    //    Existenz prüfen. Die Existenzprüfung ist der Fehlalarm-Filter — nur was wirklich
    //    auf der Platte liegt, wird klickbar.
    static const QRegularExpression tokRe(QStringLiteral(R"([^\s"'<>`|\x{2500}-\x{257F}]+)"));
    auto jt = tokRe.globalMatch(line);
    while (jt.hasNext()) {
        const auto m = jt.next();
        const int start = m.capturedStart(0);
        const int len = m.capturedLength(0);
        bool overlaps = false;
        for (const auto &t : taken)
            if (start < t.first + t.second && start + len > t.first) { overlaps = true; break; }
        if (overlaps) continue;

        QString tok = trimTrailing(m.captured(0));
        if (tok.isEmpty() || !looksPathish(tok)) continue;
        const QString abs = resolvePath(tok, cwd);
        if (abs.isEmpty()) continue;
        spans.append({start, static_cast<int>(tok.length()), abs, Span::FilePath});
    }

    std::sort(spans.begin(), spans.end(),
              [](const Span &a, const Span &b) { return a.start < b.start; });
    return spans;
}

} // namespace LinkDetector
