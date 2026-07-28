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

} // namespace qtmux
