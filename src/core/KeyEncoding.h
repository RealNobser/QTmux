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

} // namespace qtmux
