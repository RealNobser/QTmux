#include "update/ReleaseNotes.hpp"

#include <QVariantMap>

namespace appupdate {

namespace {

void escapeInto(QString& out, QStringView s)
{
    for (const QChar c : s) {
        switch (c.unicode()) {
        case '&':  out += QLatin1String("&amp;");  break;
        case '<':  out += QLatin1String("&lt;");   break;
        case '>':  out += QLatin1String("&gt;");   break;
        case '"':  out += QLatin1String("&quot;"); break;
        case '\'': out += QLatin1String("&#39;");  break;
        default:
            // Control characters (except tab) carry no meaning in a label and
            // can confuse a renderer; drop them.
            if (c.unicode() < 0x20 && c != QLatin1Char('\t')) break;
            out += c;
        }
    }
}

QString escaped(QStringView s)
{
    QString out;
    escapeInto(out, s);
    return out;
}

bool isSafeHttpsUrl(QStringView url)
{
    if (!url.startsWith(QLatin1String("https://"), Qt::CaseInsensitive)) return false;
    if (url.size() <= 8 || url.size() > 2048) return false;
    for (const QChar c : url) {
        if (c.isSpace() || c == QLatin1Char('"') || c == QLatin1Char('\'')
            || c == QLatin1Char('<') || c == QLatin1Char('>')
            || c == QLatin1Char('\\') || c.unicode() < 0x20) {
            return false;
        }
    }
    // No user info ("https://a@b/", "https://a:b@b/"): the text before the @
    // reads like the host, the link goes elsewhere.
    const QStringView rest = url.mid(8);
    qsizetype end = rest.size();
    for (const QChar stop : {QLatin1Char('/'), QLatin1Char('?'), QLatin1Char('#')}) {
        const qsizetype at = rest.indexOf(stop);
        if (at >= 0 && at < end) end = at;
    }
    if (rest.first(end).contains(QLatin1Char('@'))) return false;
    return true;
}

QString cutLine(const QString& line, bool* truncated)
{
    if (line.size() <= kNotesMaxLineChars) return line;
    *truncated = true;
    return line.left(kNotesMaxLineChars) + QChar(0x2026);
}

NotesGroupKind groupKindFor(const QString& title)
{
    const QString t = title.trimmed().toCaseFolded();
    for (const char* w : {"neu", "new", "nytt", "neu in dieser version"}) {
        if (t == QLatin1String(w)) return NotesGroupKind::New;
    }
    for (const char* w : {"verbessert", "improved", "förbättrat", "förbättringar"}) {
        if (t == QString::fromUtf8(w)) return NotesGroupKind::Improved;
    }
    for (const char* w : {"behoben", "fixed", "åtgärdat", "rättat"}) {
        if (t == QString::fromUtf8(w)) return NotesGroupKind::Fixed;
    }
    return NotesGroupKind::Other;
}

// "- x", "* x", "+ x", "1. x", "1) x" with any indentation → x; else null.
QString listItemText(const QString& line)
{
    int i = 0;
    while (i < line.size() && (line[i] == QLatin1Char(' ') || line[i] == QLatin1Char('\t'))) ++i;
    if (i >= line.size()) return QString();
    const QChar c = line[i];
    if ((c == QLatin1Char('-') || c == QLatin1Char('*') || c == QLatin1Char('+'))
        && i + 1 < line.size() && line[i + 1] == QLatin1Char(' ')) {
        return line.mid(i + 2).trimmed();
    }
    int j = i;
    while (j < line.size() && line[j].isDigit() && j - i < 4) ++j;
    if (j > i && j + 1 < line.size()
        && (line[j] == QLatin1Char('.') || line[j] == QLatin1Char(')'))
        && line[j + 1] == QLatin1Char(' ')) {
        return line.mid(j + 2).trimmed();
    }
    return QString();
}

// "## x" → level 2, text x. Returns 0 for no heading.
int headingLevel(const QString& line, QString* text)
{
    int n = 0;
    while (n < line.size() && line[n] == QLatin1Char('#') && n < 7) ++n;
    if (n == 0 || n > 6 || n >= line.size() || line[n] != QLatin1Char(' ')) return 0;
    QString t = line.mid(n + 1).trimmed();
    while (t.endsWith(QLatin1Char('#'))) t.chop(1);  // closing hashes
    *text = t.trimmed();
    return n;
}

}  // namespace

QString notesGroupKindName(NotesGroupKind k)
{
    switch (k) {
    case NotesGroupKind::New:      return QStringLiteral("new");
    case NotesGroupKind::Improved: return QStringLiteral("improved");
    case NotesGroupKind::Fixed:    return QStringLiteral("fixed");
    case NotesGroupKind::Other:    return QStringLiteral("other");
    }
    return QStringLiteral("other");
}

QString notesInlineToHtml(const QString& text)
{
    QString out;
    out.reserve(text.size() + 16);
    const qsizetype n = text.size();
    qsizetype i = 0;
    bool bold = false;
    while (i < n) {
        const QChar c = text[i];
        // `code` — nothing inside is formatted.
        if (c == QLatin1Char('`')) {
            const qsizetype end = text.indexOf(QLatin1Char('`'), i + 1);
            if (end > i + 1) {
                out += QLatin1String("<code>");
                escapeInto(out, QStringView(text).mid(i + 1, end - i - 1));
                out += QLatin1String("</code>");
                i = end + 1;
                continue;
            }
        }
        // **bold** — toggles; an unmatched opener is closed at the end.
        if (c == QLatin1Char('*') && i + 1 < n && text[i + 1] == QLatin1Char('*')) {
            if (!bold && text.indexOf(QLatin1String("**"), i + 2) < 0) {
                out += QLatin1String("**");  // no partner: literal
                i += 2;
                continue;
            }
            out += bold ? QLatin1String("</b>") : QLatin1String("<b>");
            bold = !bold;
            i += 2;
            continue;
        }
        // [label](url)
        if (c == QLatin1Char('[')) {
            const qsizetype close = text.indexOf(QLatin1String("]("), i + 1);
            const qsizetype nl = text.indexOf(QLatin1Char('['), i + 1);
            if (close > i && (nl < 0 || nl > close)) {
                const qsizetype paren = text.indexOf(QLatin1Char(')'), close + 2);
                if (paren > close + 2) {
                    const QStringView label = QStringView(text).mid(i + 1, close - i - 1);
                    const QStringView url = QStringView(text).mid(close + 2, paren - close - 2).trimmed();
                    if (isSafeHttpsUrl(url)) {
                        out += QLatin1String("<a href=\"");
                        escapeInto(out, url);
                        out += QLatin1String("\">");
                        escapeInto(out, label);
                        out += QLatin1String("</a>");
                    } else {
                        // Not https: the label as plain text, no anchor.
                        escapeInto(out, label);
                    }
                    i = paren + 1;
                    continue;
                }
            }
        }
        escapeInto(out, QStringView(&c, 1));
        ++i;
    }
    if (bold) out += QLatin1String("</b>");
    return out;
}

ReleaseNotes parseReleaseNotes(const QString& markdownIn)
{
    ReleaseNotes notes;
    QString markdown = markdownIn;
    if (markdown.size() > kNotesMaxInputChars) {
        markdown.truncate(kNotesMaxInputChars);
        notes.truncated = true;
    }
    const QStringList lines = markdown.split(QLatin1Char('\n'));

    bool seenHeading = false;
    bool groupOpen = false;
    QString highlight;          // first paragraph, before any heading
    bool highlightDone = false;
    QString pendingItem;        // item text being collected (with continuations)
    bool haveItem = false;

    const auto full = [&notes]() {
        if (notes.blocks.size() >= kNotesMaxBlocks) {
            notes.truncated = true;
            return true;
        }
        return false;
    };
    const auto ensureGroup = [&]() {
        if (groupOpen || full()) return;
        NotesBlock g;
        g.kind = NotesBlock::Kind::Group;
        g.group = NotesGroupKind::Other;
        notes.blocks.append(g);
        groupOpen = true;
    };
    const auto flushItem = [&]() {
        if (!haveItem) return;
        haveItem = false;
        ensureGroup();
        if (full()) return;
        NotesBlock b;
        b.kind = NotesBlock::Kind::Item;
        b.html = notesInlineToHtml(cutLine(pendingItem, &notes.truncated));
        notes.blocks.append(b);
        pendingItem.clear();
    };
    const auto flushHighlight = [&]() {
        if (!highlight.isEmpty() && !highlightDone) {
            notes.highlightHtml = notesInlineToHtml(cutLine(highlight, &notes.truncated));
        }
        highlightDone = true;
    };

    for (QString line : lines) {
        if (line.endsWith(QLatin1Char('\r'))) line.chop(1);
        if (full()) break;

        QString htext;
        const int level = headingLevel(line.trimmed(), &htext);
        if (level > 0) {
            flushItem();
            flushHighlight();
            seenHeading = true;
            if (level <= 2) {
                NotesBlock g;
                g.kind = NotesBlock::Kind::Group;
                g.group = groupKindFor(htext);
                g.title = cutLine(htext, &notes.truncated);
                notes.blocks.append(g);
                groupOpen = true;
            } else {
                ensureGroup();
                if (full()) break;
                NotesBlock s;
                s.kind = NotesBlock::Kind::Subheading;
                s.html = notesInlineToHtml(cutLine(htext, &notes.truncated));
                notes.blocks.append(s);
            }
            continue;
        }

        const QString item = listItemText(line);
        if (!item.isNull()) {
            flushItem();
            if (!seenHeading) flushHighlight();
            pendingItem = item;
            haveItem = true;
            continue;
        }

        const QString t = line.trimmed();
        if (t.isEmpty()) {
            flushItem();
            if (!seenHeading && !highlight.isEmpty()) flushHighlight();
            continue;
        }
        // Plain text line.
        if (haveItem && (line.startsWith(QLatin1Char(' ')) || line.startsWith(QLatin1Char('\t')))) {
            pendingItem += QLatin1Char(' ') + t;   // continuation of an item
            continue;
        }
        if (!seenHeading && !highlightDone && !haveItem) {
            highlight += highlight.isEmpty() ? t : QLatin1Char(' ') + t;
            continue;
        }
        // A paragraph somewhere else: shown as an item, so nothing is lost.
        flushItem();
        pendingItem = t;
        haveItem = true;
    }
    flushItem();
    flushHighlight();

    // An empty explicit group (heading without items) stays — the source
    // asked for it; an empty implicit group cannot arise (made on demand).
    return notes;
}

QVariantList notesToVariantList(const ReleaseNotes& notes)
{
    QVariantList list;
    list.reserve(notes.blocks.size());
    for (const NotesBlock& b : notes.blocks) {
        QVariantMap m;
        switch (b.kind) {
        case NotesBlock::Kind::Group:      m.insert(QStringLiteral("kind"), QStringLiteral("group")); break;
        case NotesBlock::Kind::Subheading: m.insert(QStringLiteral("kind"), QStringLiteral("subheading")); break;
        case NotesBlock::Kind::Item:       m.insert(QStringLiteral("kind"), QStringLiteral("item")); break;
        }
        m.insert(QStringLiteral("group"), notesGroupKindName(b.group));
        m.insert(QStringLiteral("title"), escaped(b.title));
        m.insert(QStringLiteral("html"), b.html);
        list.append(m);
    }
    return list;
}

}  // namespace appupdate
