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

namespace {

// Ctrl+<Zeichen> → Steuerbyte (ASCII-Caret-Konvention). -1 = nicht bildbar.
int ctrlByteFor(QChar c) {
    const QChar u = c.toUpper();
    if (u >= QLatin1Char('A') && u <= QLatin1Char('Z')) return u.unicode() - '@'; // C-a = 0x01
    switch (u.unicode()) {
    case '@': case ' ': return 0x00;  // C-@ / C-Space = NUL
    case '[': return 0x1b;
    case '\\': return 0x1c;
    case ']': return 0x1d;
    case '^': return 0x1e;
    case '_': return 0x1f;
    case '?': return 0x7f;            // C-? = DEL
    default:  return -1;
    }
}

} // namespace

QByteArray encodeNamedKey(const QString &spec) {
    // Modifier-Präfixe im tmux-Stil abschälen (mehrfach erlaubt: "C-M-p").
    QString rest = spec;
    bool ctrl = false, alt = false, shift = false;
    while (rest.size() >= 3 && rest.at(1) == QLatin1Char('-')) {
        const QChar m = rest.at(0).toUpper();
        if (m == QLatin1Char('C'))      ctrl = true;
        else if (m == QLatin1Char('M') || m == QLatin1Char('A')) alt = true;
        else if (m == QLatin1Char('S')) shift = true;
        else break;
        rest = rest.mid(2);
    }
    if (rest.isEmpty())
        return {};

    // xterm-Modifier-Parameter: 1 + Shift(1) + Alt(2) + Ctrl(4).
    const int mod = 1 + (shift ? 1 : 0) + (alt ? 2 : 0) + (ctrl ? 4 : 0);
    // Cursor-Klasse (ESC[A …): mit Modifier ESC[1;<mod><Buchstabe>.
    auto csiKey = [mod](char letter) {
        if (mod == 1) return QByteArray("\x1b[") + letter;
        return QByteArray("\x1b[1;") + QByteArray::number(mod) + letter;
    };
    // Tilde-Klasse (ESC[5~ …): mit Modifier ESC[<n>;<mod>~.
    auto tildeKey = [mod](int n) {
        QByteArray b = QByteArray("\x1b[") + QByteArray::number(n);
        if (mod != 1) b += ';' + QByteArray::number(mod);
        return b + '~';
    };

    // Einzelzeichen (nur als Chord sinnvoll; ohne Modifier ist es ohnehin Literaltext).
    if (rest.size() == 1) {
        const QChar c = rest.at(0);
        QByteArray out;
        if (ctrl) {
            const int b = ctrlByteFor(c);
            if (b < 0) return {};
            out = QByteArray(1, static_cast<char>(b));
        } else {
            out = QString(shift ? c.toUpper() : c).toUtf8();
        }
        if (alt) out.prepend('\x1b');   // Meta-Kodierung wie encodeMetaSequence
        return out;
    }

    const QString name = rest.toLower();
    if (name == QLatin1String("enter") || name == QLatin1String("return"))
        return (shift || alt) ? QByteArray("\x1b\r") : QByteArray("\r");  // QTMUX-43
    if (name == QLatin1String("escape") || name == QLatin1String("esc"))
        return ctrl ? QByteArray() : QByteArray(alt ? "\x1b\x1b" : "\x1b");
    if (name == QLatin1String("tab")) {
        if (ctrl) return {};             // kein xterm-Standard — Fehler statt Raterei
        QByteArray out = shift ? QByteArray("\x1b[Z") : QByteArray("\t");
        if (alt) out.prepend('\x1b');
        return out;
    }
    if (name == QLatin1String("btab"))
        return (ctrl || alt) ? QByteArray() : QByteArray("\x1b[Z");
    if (name == QLatin1String("space")) {
        QByteArray out = ctrl ? QByteArray(1, '\0') : QByteArray(" ");
        if (alt) out.prepend('\x1b');
        return out;
    }
    if (name == QLatin1String("backspace") || name == QLatin1String("bspace")) {
        // Ctrl+Backspace sendet BS (0x08) — so unterscheiden Zeilen-Editoren
        // Wort-Löschen vom Zeichen-Löschen (DEL, 0x7f).
        QByteArray out = ctrl ? QByteArray("\x08") : QByteArray("\x7f");
        if (alt) out.prepend('\x1b');
        return out;
    }
    if (name == QLatin1String("up"))    return csiKey('A');
    if (name == QLatin1String("down"))  return csiKey('B');
    if (name == QLatin1String("right")) return csiKey('C');
    if (name == QLatin1String("left"))  return csiKey('D');
    if (name == QLatin1String("home"))  return csiKey('H');
    if (name == QLatin1String("end"))   return csiKey('F');
    if (name == QLatin1String("insert")   || name == QLatin1String("ic"))   return tildeKey(2);
    if (name == QLatin1String("delete")   || name == QLatin1String("dc"))   return tildeKey(3);
    if (name == QLatin1String("pageup")   || name == QLatin1String("pgup")) return tildeKey(5);
    if (name == QLatin1String("pagedown") || name == QLatin1String("pgdn")) return tildeKey(6);

    // F1–F12: ohne Modifier F1–F4 als SS3 (ESC O P…), sonst xterm-CSI-Form.
    if (name.size() >= 2 && name.at(0) == QLatin1Char('f')) {
        bool ok = false;
        const int fn = name.mid(1).toInt(&ok);
        if (ok && fn >= 1 && fn <= 12) {
            static const char ss3[] = {'P', 'Q', 'R', 'S'};
            static const int  tld[] = {15, 17, 18, 19, 20, 21, 23, 24};
            if (fn <= 4)
                return mod == 1 ? QByteArray("\x1bO") + ss3[fn - 1]
                                : QByteArray("\x1b[1;") + QByteArray::number(mod) + ss3[fn - 1];
            return tildeKey(tld[fn - 5]);
        }
    }

    return {};
}

} // namespace qtmux
