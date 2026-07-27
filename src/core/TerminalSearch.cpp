#include "TerminalSearch.h"

namespace TerminalSearch {

QList<Match> find(const QStringList &lines, const QString &needle, bool caseSensitive) {
    QList<Match> out;
    if (needle.isEmpty()) return out;
    const Qt::CaseSensitivity cs = caseSensitive ? Qt::CaseSensitive : Qt::CaseInsensitive;
    const int nlen = needle.length();
    for (int i = 0; i < lines.size(); ++i) {
        const QString &line = lines.at(i);
        int from = 0;
        while (true) {
            const int idx = line.indexOf(needle, from, cs);
            if (idx < 0) break;
            out.append({i, idx, nlen});
            from = idx + nlen;   // nicht-ueberlappend weitersuchen
        }
    }
    return out;
}

} // namespace TerminalSearch
