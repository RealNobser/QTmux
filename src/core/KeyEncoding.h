#pragma once

#include <QByteArray>
#include <QString>
#include <Qt>

namespace qtmux {

/// Übersetzt eine Taste in die Byte-Sequenz fürs PTY (xterm/VT220-konform).
/// Gui-freie Logik (nur QtCore-Typen), damit sie ohne Quick-Aufbau testbar ist —
/// TerminalItem::encodeKey delegiert hierher. `text` ist QKeyEvent::text()
/// (Fallback für druckbare Zeichen und Ctrl-Steuercodes).
///
/// QTMUX-43: Shift+Enter und Alt+Enter senden ESC CR statt CR. Agenten-TUIs
/// (Claude Code u. a.) verstehen ESC CR als »Zeilenumbruch einfügen« statt
/// »absenden« — dieselbe Sequenz, die Claude Codes /terminal-setup in anderen
/// Terminals auf Shift+Enter legt. Bewusste Einschränkung: klassische Shells
/// binden ESC CR nicht (readline ignoriert es; unter ConPTY kann das ESC je nach
/// Zeilen-Editor die Eingabe verwerfen) — wer in einer Shell Enter meint, drückt
/// Enter ohne Modifier, das bleibt unverändert CR.
QByteArray encodeKeyBytes(int key, Qt::KeyboardModifiers mods, const QString &text);

/// QTMUX-84: xterm-Meta-Kodierung für Alt+<druckbares Zeichen> → `ESC` + Zeichen.
/// Liefert eine leere Sequenz, wenn die Kombination NICHT meta-kodiert werden darf
/// (kein Alt, AltGr, Steuercode, nicht-ASCII). Bewusst immer verfügbar — auch auf
/// macOS, wo `encodeKeyBytes` sie nicht benutzt —, damit die Logik plattformunabhängig
/// unit-testbar bleibt.
QByteArray encodeMetaSequence(int key, Qt::KeyboardModifiers mods, const QString &text);

/// true auf Windows/Linux, false auf macOS. Dort erzeugt die Wahltaste Sonderzeichen
/// (Option+v = „√"), und physisches Ctrl ist bereits Meta — eine Meta-Kodierung würde
/// die Zeicheneingabe kaputtmachen statt etwas hinzuzufügen.
bool metaPrefixEnabled();

/// MCP `send_keys`: Übersetzt einen benannten Tasten-Ausdruck im
/// tmux-send-keys-Stil in die PTY-Byte-Sequenz. Erkannt werden Chords mit den
/// Präfixen `C-` (Ctrl), `M-`/`A-` (Alt/Meta) und `S-` (Shift) — einzeln oder
/// kombiniert (`C-M-p`) — vor einem Einzelzeichen oder einer Sondertaste, sowie
/// die Sondertasten-Namen selbst (Groß-/Kleinschreibung egal): Enter/Return,
/// Escape/Esc, Tab, BTab, Backspace/BSpace, Space, Up/Down/Left/Right,
/// Home/End, PageUp/PgUp, PageDown/PgDn, Delete/DC, Insert/IC, F1–F12.
/// Modifier auf Cursor-/Funktionstasten kodieren xterm-konform (`C-Right` →
/// `ESC[1;5C`). Enter folgt QTMUX-43: `S-Enter`/`M-Enter` → ESC CR (Umbruch
/// einfügen), sonst CR. Liefert eine LEERE Sequenz für alles Unerkannte —
/// der Aufrufer entscheidet über den Literal-Fallback (tmux-Verhalten:
/// unerkannte Ausdrücke sind Text).
QByteArray encodeNamedKey(const QString &spec);

} // namespace qtmux
