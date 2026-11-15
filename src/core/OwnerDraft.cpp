#include "OwnerDraft.h"

namespace qtmux {

namespace {

using Kind = OwnerDraftState::Kind;

// Was eine einzelne Taste für die Eingabezeile bedeuten KANN. Die Einteilung ist die
// ganze Regel; jede Klasse ist im Header begründet.
enum class KeyClass {
    Submit,      // Enter: Zeile abgeschickt
    Cancel,      // Strg-C: Zeile verworfen
    KillLine,    // Strg-U: löscht die Zeile bis zum Anfang
    EraseOne,    // Backspace: nimmt ein Zeichen weg
    Countable,   // ein einfaches Zeichen, das genau ein Backspace wieder entfernt
    Content,     // Inhalt, den QTmux nicht mitzählen kann
    MayCreate,   // kann aus einer LEEREN Zeile eine gefüllte machen (Verlauf, Vorschlag)
    Edit,        // bewegt den Cursor oder ändert vorhandenen Text
    Neutral,     // berührt die Eingabezeile nicht
};

OwnerDraftState apply(OwnerDraftState s, KeyClass c) {
    switch (c) {
    case KeyClass::Submit:
    case KeyClass::Cancel:
        return {};
    case KeyClass::KillLine:
        // Nur die selbst gezählte Einzelzeile ist danach sicher leer.
        return s.kind == Kind::Counted ? OwnerDraftState{} : s;
    case KeyClass::EraseOne:
        if (s.kind != Kind::Counted) return s;
        return s.count <= 1 ? OwnerDraftState{} : OwnerDraftState{Kind::Counted, s.count - 1};
    case KeyClass::Countable:
        if (s.kind == Kind::Open) return s;
        return {Kind::Counted, s.kind == Kind::Counted ? s.count + 1 : 1};
    case KeyClass::Content:
    case KeyClass::MayCreate:
        return {Kind::Open, 0};
    case KeyClass::Edit:
        // In einer leeren Zeile gibt es nichts zu bewegen; in einer gezählten stimmt
        // danach die Annahme „Cursor am Ende“ nicht mehr.
        return s.kind == Kind::Counted ? OwnerDraftState{Kind::Open, 0} : s;
    case KeyClass::Neutral:
        return s;
    }
    return s;
}

// Länge einer UTF-8-Folge aus dem Startbyte; 0 = kein gültiges Startbyte.
int utf8Length(unsigned char lead) {
    if (lead < 0x80) return 1;
    if ((lead & 0xE0) == 0xC0) return 2;
    if ((lead & 0xF0) == 0xE0) return 3;
    if ((lead & 0xF8) == 0xF0) return 4;
    return 0;
}

// Liest EINE Taste ab `pos` und rückt `pos` dahinter. Unvollständige oder unbekannte
// Folgen zählen als `Content` — der Merker steht dann, was die sichere Seite ist.
KeyClass readKey(const QByteArray &b, qsizetype &pos) {
    const auto at = [&b](qsizetype i) { return static_cast<unsigned char>(b.at(i)); };
    const unsigned char c = at(pos);

    if (c == 0x1b) {
        if (pos + 1 >= b.size()) { ++pos; return KeyClass::Neutral; }   // Escape allein
        const unsigned char n = at(pos + 1);
        if (n == '[' || n == 'O') {
            // CSI bzw. SS3: Parameter/Zwischenbytes überspringen, Schlussbyte lesen.
            qsizetype i = pos + 2;
            QByteArray params;
            while (i < b.size() && at(i) >= 0x20 && at(i) < 0x40) params += b.at(i++);
            if (i >= b.size()) { pos = b.size(); return KeyClass::Content; }
            const unsigned char fin = at(i);
            pos = i + 1;
            // Hoch/Runter holen den Verlauf, Rechts übernimmt einen Vorschlag: Aus einer
            // leeren Zeile kann so ein Entwurf werden, ohne dass ein Zeichen getippt wurde.
            if (fin == 'A' || fin == 'B' || fin == 'C') return KeyClass::MayCreate;
            if (fin == 'Z') return KeyClass::Neutral;                      // Umschalt+Tab
            if (fin == '~' && (params == "5" || params == "6")) return KeyClass::Neutral;  // Bild auf/ab
            if (fin == 'I' && n == '[') return KeyClass::Neutral;          // Fokus-Meldung
            if (fin == 'O' && n == '[') return KeyClass::Neutral;
            return KeyClass::Edit;   // Links, Pos1, Ende, Entf, Funktionstasten …
        }
        pos += 2;
        if (n == '\r' || n == '\n') return KeyClass::Content;   // Umschalt/Alt+Enter: Umbruch IM Entwurf
        if (n == 0x7f || n == 0x08) return KeyClass::Edit;      // Alt+Backspace: Wort löschen
        if (n < 0x20) return KeyClass::Edit;
        // Alt+Zeichen: meist Bewegung, in Shells aber auch Einfügen (Alt+. = letztes Argument).
        return KeyClass::MayCreate;
    }

    if (c == '\r' || c == '\n') { ++pos; return KeyClass::Submit; }
    if (c == 0x03) { ++pos; return KeyClass::Cancel; }
    if (c == 0x15) { ++pos; return KeyClass::KillLine; }
    if (c == 0x7f || c == 0x08) { ++pos; return KeyClass::EraseOne; }
    if (c < 0x20) {
        ++pos;
        switch (c) {
        case '\t':   // Vervollständigung bzw. Vorschlag übernehmen
        case 0x10:   // Strg-P: vorheriger Verlaufseintrag
        case 0x0e:   // Strg-N: nächster Verlaufseintrag
        case 0x12:   // Strg-R: Verlaufssuche
        case 0x19:   // Strg-Y: zuletzt Gelöschtes einfügen
            return KeyClass::MayCreate;
        default:
            return KeyClass::Edit;
        }
    }

    // Druckbares Zeichen: als UTF-8 lesen. Mitgezählt wird nur, was ein einzelnes
    // Backspace sicher wieder entfernt — Codepunkte unter U+0300 (ASCII, Umlaute, ß).
    // Darüber beginnen kombinierende Zeichen, und dahinter liegen Emoji, bei denen ein
    // Programm je nach Bauart einen halben oder gleich mehrere Codepunkte löscht.
    const int len = utf8Length(c);
    if (len == 0 || pos + len > b.size()) { pos = b.size(); return KeyClass::Content; }
    quint32 cp = len == 1 ? c : (c & (0xFF >> (len + 1)));
    for (int k = 1; k < len; ++k) {
        const unsigned char cont = at(pos + k);
        if ((cont & 0xC0) != 0x80) { pos = b.size(); return KeyClass::Content; }
        cp = (cp << 6) | (cont & 0x3F);
    }
    pos += len;
    return cp < 0x300 ? KeyClass::Countable : KeyClass::Content;
}

} // namespace

OwnerDraftState nextOwnerDraft(OwnerDraftState state, const QByteArray &bytes,
                               HumanInputKind kind) {
    if (bytes.isEmpty()) return state;
    // Eine Einfügung ist Inhalt, den niemand mitzählen kann: Viele Programme zeigen sie
    // als Platzhalter, den ein einziges Backspace entfernt, andere fügen jede Zeile ein.
    // Auch ein Zeilenende darin ist kein Abschicken — im Paste-Rahmen ist es Text.
    if (kind == HumanInputKind::Paste) return {Kind::Open, 0};
    qsizetype pos = 0;
    while (pos < bytes.size()) state = apply(state, readKey(bytes, pos));
    return state;
}

bool endsOwnerDraft(const QByteArray &bytes, HumanInputKind kind) {
    if (kind != HumanInputKind::Key || bytes.isEmpty()) return false;
    qsizetype pos = 0;
    KeyClass last = KeyClass::Neutral;
    while (pos < bytes.size()) last = readKey(bytes, pos);
    return last == KeyClass::Submit || last == KeyClass::Cancel;
}

} // namespace qtmux
