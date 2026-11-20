#pragma once

// appupdate::ReleaseNotes — the release notes of ONE version, parsed from the
// manifest's Markdown into a small model both surfaces draw the same way
// (update design 2026-11-19).
//
// CANONICAL: QTmux vendors src/update/ byte-identically. Qt6::Core only.
//
// The notes are INPUT from the update server: signed, but still text that
// ends up in a rich-text label. So the parser is a whitelist, not a
// Markdown renderer:
//   * every character of the source is HTML-escaped; the only markup that
//     comes out is what the parser itself writes: <b>, <code>, <a href>;
//   * a link becomes an anchor only for https: — javascript:, file:, http:,
//     data: … are shown as their plain link text;
//   * HTML in the source is shown as text, never interpreted;
//   * hard limits on input size, line length and item count, each one
//     reported through `truncated`, never a silent cut.
//
// Source shape (what publish.py's notes look like):
//
//   **Headline:** first paragraph before the first heading  → highlight
//   ## Neu | New | Nytt                → group New
//   ## Verbessert | Improved | Förbättrat → group Improved
//   ## Behoben | Fixed | Åtgärdat       → group Fixed
//   ## anything else                    → group Other, with its title
//   ### sub                             → subheading
//   - item / * item / + item / 1. item  → item (deeper levels drawn flat)

#include <QList>
#include <QString>
#include <QVariantList>

#include <cstdint>

namespace appupdate {

enum class NotesGroupKind : std::uint8_t { New, Improved, Fixed, Other };

struct NotesBlock {
    enum class Kind : std::uint8_t { Group, Subheading, Item };
    Kind kind = Kind::Item;
    NotesGroupKind group = NotesGroupKind::Other;  // Group only
    // Group: the source title as plain text (empty for the implicit group of
    // items that come before any heading). Known kinds show the catalog
    // label instead (UpdateTexts::notesGroupLabel).
    QString title;
    // Subheading and Item: escaped HTML subset (<b>, <code>, <a href="https:…">).
    QString html;
};

struct ReleaseNotes {
    QString highlightHtml;   // empty = no "Das Wichtigste" line
    QList<NotesBlock> blocks;
    bool truncated = false;  // a limit cut something off

    [[nodiscard]] bool isEmpty() const { return highlightHtml.isEmpty() && blocks.isEmpty(); }
};

// Limits. Generous for real notes (today's longest: ~40 lines), tight enough
// that a hostile or broken manifest cannot make a dialog unusable.
inline constexpr int kNotesMaxInputChars = 64 * 1024;
inline constexpr int kNotesMaxBlocks     = 400;
inline constexpr int kNotesMaxLineChars  = 2000;

[[nodiscard]] ReleaseNotes parseReleaseNotes(const QString& markdown);

// One line of inline Markdown → the escaped HTML subset. Exposed for tests
// and for single-line texts the surfaces want to format the same way.
[[nodiscard]] QString notesInlineToHtml(const QString& text);

// For QML: a list of maps {kind: "group"|"subheading"|"item",
// group: "new"|"improved"|"fixed"|"other", title, html}.
[[nodiscard]] QVariantList notesToVariantList(const ReleaseNotes& notes);

[[nodiscard]] QString notesGroupKindName(NotesGroupKind k);

}  // namespace appupdate
