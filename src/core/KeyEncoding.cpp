#include "KeyEncoding.h"

namespace qtmux {

QByteArray encodeKeyBytes(int key, Qt::KeyboardModifiers mods, const QString &text) {
    switch (key) {
    case Qt::Key_Return:
    case Qt::Key_Enter:
        // QTMUX-43: Shift/Alt+Enter → ESC CR (Umbruch einfügen in Agenten-TUIs,
        // siehe Header). Ctrl+Enter bleibt bewusst normales CR.
        if (mods & (Qt::ShiftModifier | Qt::AltModifier))
            return "\x1b\r";
        return "\r";
    case Qt::Key_Backspace:return "\x7f";
    case Qt::Key_Tab:
        return (mods & Qt::ShiftModifier) ? QByteArray("\x1b[Z")
                                          : QByteArray("\t");
    case Qt::Key_Backtab:  return "\x1b[Z";
    case Qt::Key_Escape:   return "\x1b";
    case Qt::Key_Up:       return "\x1b[A";
    case Qt::Key_Down:     return "\x1b[B";
    case Qt::Key_Right:    return "\x1b[C";
    case Qt::Key_Left:     return "\x1b[D";
    case Qt::Key_Home:     return "\x1b[H";
    case Qt::Key_End:      return "\x1b[F";
    case Qt::Key_PageUp:   return "\x1b[5~";
    case Qt::Key_PageDown: return "\x1b[6~";
    case Qt::Key_Delete:   return "\x1b[3~";
    // Funktionstasten F1–F12 (xterm/VT220-Sequenzen). ConPTY übersetzt eingehende
    // VT-Sequenzen in Konsolen-Tastenereignisse → cmd/Clink (F7-History etc.) reagieren.
    case Qt::Key_F1:       return "\x1bOP";
    case Qt::Key_F2:       return "\x1bOQ";
    case Qt::Key_F3:       return "\x1bOR";
    case Qt::Key_F4:       return "\x1bOS";
    case Qt::Key_F5:       return "\x1b[15~";
    case Qt::Key_F6:       return "\x1b[17~";
    case Qt::Key_F7:       return "\x1b[18~";
    case Qt::Key_F8:       return "\x1b[19~";
    case Qt::Key_F9:       return "\x1b[20~";
    case Qt::Key_F10:      return "\x1b[21~";
    case Qt::Key_F11:      return "\x1b[23~";
    case Qt::Key_F12:      return "\x1b[24~";
    default:
        // QTMUX-84: Alt+<Zeichen> als Meta-Sequenz, bevor der text()-Fallback greift.
        if (metaPrefixEnabled()) {
            const QByteArray meta = encodeMetaSequence(key, mods, text);
            if (!meta.isEmpty())
                return meta;
        }
        return text.toUtf8();
    }
}

QByteArray encodeMetaSequence(int key, Qt::KeyboardModifiers mods, const QString &text) {
    if (!(mods & Qt::AltModifier))
        return {};
    // ⚠️ AltGr meldet Windows als Ctrl+Alt. Würden wir das meta-kodieren, gingen auf
    // deutschen Tastaturen @ (AltGr+q), € (AltGr+e), \ ~ | [ ] { } verloren — der Fix
    // wäre schlimmer als der Fehler. Ctrl+Alt bleibt darum beim text()-Fallback.
    if (mods & Qt::ControlModifier)
        return {};

    QString ch = text;
    if (ch.isEmpty()) {
        // Windows liefert bei Alt+Buchstabe ein LEERES text() (genau der Grund, warum
        // Alt-Kürzel bisher spurlos verpufften) → Zeichen aus dem Key-Code bilden.
        // Qt::Key_V == 'V'; ohne Shift der Kleinbuchstabe, wie xterm es sendet.
        if (key < 0x20 || key > 0x7e)
            return {};
        ch = QChar(key);
        if (!(mods & Qt::ShiftModifier))
            ch = ch.toLower();
    } else {
        const QChar c = ch.at(0);
        // Bereits meta-kodiert (manche X11-Layouts liefern ESC selbst) → nicht doppeln.
        if (c == QChar(0x1b))
            return {};
        // Steuercodes (Ctrl-Kombinationen, Backspace) bleiben unangetastet.
        if (c.unicode() < 0x20 || c == QChar(0x7f))
            return {};
    }
    return QByteArray("\x1b") + ch.toUtf8();
}

bool metaPrefixEnabled() {
#if defined(Q_OS_MACOS)
    return false;
#else
    return true;
#endif
}

} // namespace qtmux
