#include "VtScreen.h"
#include "ColorScheme.h"

#include <vterm.h>
#include <QChar>

namespace qtmux {

namespace {

// VTermColor -> Cell-Farbe. Wandelt indexierte Farben in RGB und erkennt Defaults.
void applyColor(VTermScreen *screen, const VTermColor &in, quint32 &out, bool &isDefault) {
    if (VTERM_COLOR_IS_DEFAULT_FG(&in) || VTERM_COLOR_IS_DEFAULT_BG(&in)) {
        isDefault = true;
        return;
    }
    VTermColor c = in;
    if (!VTERM_COLOR_IS_RGB(&c)) {
        if (!screen) {           // z. B. Scrollback ohne Screen-Kontext
            isDefault = true;
            return;
        }
        vterm_screen_convert_color_to_rgb(screen, &c);
    }
    isDefault = false;
    out = (quint32(c.rgb.red) << 16) | (quint32(c.rgb.green) << 8) | quint32(c.rgb.blue);
}

Cell toCell(VTermScreen *screen, const VTermScreenCell &vc) {
    Cell cell;
    if (vc.chars[0] != 0 && vc.chars[0] != 0xffffffff) {
        cell.text = QString::fromUcs4(reinterpret_cast<const char32_t *>(vc.chars));
        // fromUcs4 erwartet null-terminiert; chars[] ist es (VTERM_MAX_CHARS_PER_CELL).
    }
    cell.width = static_cast<quint8>(vc.width > 0 ? vc.width : 1);
    cell.bold = vc.attrs.bold;
    cell.italic = vc.attrs.italic;
    cell.underline = vc.attrs.underline != 0;
    cell.reverse = vc.attrs.reverse;
    applyColor(screen, vc.fg, cell.fg, cell.fgDefault);
    applyColor(screen, vc.bg, cell.bg, cell.bgDefault);
    if (cell.reverse) {
        std::swap(cell.fg, cell.bg);
        std::swap(cell.fgDefault, cell.bgDefault);
    }
    return cell;
}

// --- C-Callbacks (libvterm ruft mit user = VtScreen*) -----------------------
int cbDamage(VTermRect r, void *user) {
    static_cast<VtScreen *>(user)->cbDamage(r.start_row, r.start_col, r.end_row, r.end_col);
    return 1;
}
int cbMoveCursor(VTermPos pos, VTermPos, int visible, void *user) {
    static_cast<VtScreen *>(user)->cbMoveCursor(pos.row, pos.col, visible != 0);
    return 1;
}
int cbSetTermProp(VTermProp prop, VTermValue *val, void *user) {
    auto *self = static_cast<VtScreen *>(user);
    if (prop == VTERM_PROP_TITLE && val->string.str) {
        self->cbSetTitle(QString::fromUtf8(val->string.str, static_cast<int>(val->string.len)));
    } else if (prop == VTERM_PROP_CURSORVISIBLE) {
        self->cbMoveCursor(self->cursor().y(), self->cursor().x(), val->boolean != 0);
    }
    return 1;
}
int cbBell(void *user) {
    static_cast<VtScreen *>(user)->cbBell();
    return 1;
}
int cbResize(int rows, int cols, void *user) {
    return static_cast<VtScreen *>(user)->cbResize(rows, cols) ? 1 : 0;
}
int cbSbPushline(int cols, const VTermScreenCell *cells, void *user) {
    auto *self = static_cast<VtScreen *>(user);
    std::vector<Cell> line;
    line.reserve(cols);
    // Scrollback-Zellen haben keinen Screen-Kontext für Farbkonvertierung mehr;
    // wir nutzen denselben Screen (Palette bleibt gültig).
    for (int c = 0; c < cols; ++c) line.push_back(toCell(nullptr, cells[c]));
    self->cbPushScrollback(std::move(line));
    return 1;
}
int cbSbPopline(int, VTermScreenCell *, void *) {
    return 0; // (noch) kein Zurückschieben in den Screen
}

void cbOutput(const char *s, size_t len, void *user) {
    static_cast<VtScreen *>(user)->cbOutput(QByteArray(s, static_cast<int>(len)));
}

// Unbekannte OSC-Sequenzen (9 = Notification, 777 = notify, 133 = Prompt-Marker).
int cbOscFallback(int command, VTermStringFragment frag, void *user) {
    static_cast<VtScreen *>(user)->cbOsc(command, frag.str, static_cast<int>(frag.len),
                                         frag.initial, frag.final);
    return 1;
}

const VTermStateFallbacks kStateFallbacks = {
    /* control */ nullptr,
    /* csi     */ nullptr,
    /* osc     */ cbOscFallback,
    /* dcs     */ nullptr,
    /* apc     */ nullptr,
    /* pm      */ nullptr,
    /* sos     */ nullptr,
};

const VTermScreenCallbacks kScreenCallbacks = {
    /* damage      */ cbDamage,
    /* moverect    */ nullptr,
    /* movecursor  */ cbMoveCursor,
    /* settermprop */ cbSetTermProp,
    /* bell        */ cbBell,
    /* resize      */ cbResize,
    /* sb_pushline */ cbSbPushline,
    /* sb_popline  */ cbSbPopline,
    /* sb_clear    */ nullptr,
};

} // namespace

VtScreen::VtScreen(int rows, int cols, QObject *parent)
    : QObject(parent), m_rows(rows), m_cols(cols) {
    m_vt = vterm_new(rows, cols);
    vterm_set_utf8(m_vt, 1);
    vterm_output_set_callback(m_vt, &qtmux::cbOutput, this);

    m_screen = vterm_obtain_screen(m_vt);
    vterm_screen_set_callbacks(m_screen, &kScreenCallbacks, this);
    vterm_screen_set_unrecognised_fallbacks(m_screen, &kStateFallbacks, this);
    vterm_screen_reset(m_screen, 1);
}

VtScreen::~VtScreen() {
    if (m_vt) vterm_free(m_vt);
}

void VtScreen::inputWrite(const QByteArray &data) {
    vterm_input_write(m_vt, data.constData(), static_cast<size_t>(data.size()));
    vterm_screen_flush_damage(m_screen);
}

void VtScreen::setSize(int rows, int cols) {
    if (rows == m_rows && cols == m_cols) return;
    m_rows = rows;
    m_cols = cols;
    vterm_set_size(m_vt, rows, cols);
}

QString VtScreen::screenText() const {
    QStringList lines;
    lines.reserve(m_rows);
    for (int row = 0; row < m_rows; ++row) {
        QString line;
        for (int col = 0; col < m_cols; ++col) {
            const QString t = cell(row, col).text;
            line += t.isEmpty() ? QStringLiteral(" ") : t;
        }
        while (line.endsWith(QChar(' '))) line.chop(1);  // rechte Leerzeichen entfernen
        lines << line;
    }
    while (!lines.isEmpty() && lines.last().isEmpty()) lines.removeLast();
    return lines.join(QChar('\n'));
}

Cell VtScreen::cell(int row, int col) const {
    VTermScreenCell vc;
    VTermPos pos{row, col};
    if (vterm_screen_get_cell(m_screen, pos, &vc)) {
        return toCell(m_screen, vc);
    }
    return Cell{};
}

// --- Handler ----------------------------------------------------------------
void VtScreen::cbDamage(int startRow, int startCol, int endRow, int endCol) {
    emit damaged(QRect(QPoint(startCol, startRow),
                       QPoint(endCol - 1, endRow - 1)));
}

void VtScreen::cbMoveCursor(int row, int col, bool visible) {
    m_cursor = QPoint(col, row);
    m_cursorVisible = visible;
    emit cursorMoved();
}

void VtScreen::cbBell() { emit bell(); }

bool VtScreen::cbResize(int rows, int cols) {
    m_rows = rows;
    m_cols = cols;
    return true;
}

void VtScreen::cbSetTitle(const QString &title) {
    m_title = title;
    emit titleChanged(title);
}

void VtScreen::cbPushScrollback(std::vector<Cell> &&line) {
    m_scrollback.push_back(std::move(line));
    while (static_cast<int>(m_scrollback.size()) > kMaxScrollback) {
        m_scrollback.pop_front();
    }
}

void VtScreen::cbOutput(const QByteArray &data) { emit outputToPty(data); }

void VtScreen::startPaste() { if (m_vt) vterm_keyboard_start_paste(m_vt); }
void VtScreen::endPaste()   { if (m_vt) vterm_keyboard_end_paste(m_vt); }

void VtScreen::applyColorScheme(const ColorScheme &scheme) {
    if (!m_vt) return;
    VTermState *state = vterm_obtain_state(m_vt);
    if (!state) return;
    auto toVt = [](quint32 rgb) {
        VTermColor c;
        vterm_color_rgb(&c, (rgb >> 16) & 0xff, (rgb >> 8) & 0xff, rgb & 0xff);
        return c;
    };
    for (int i = 0; i < 16; ++i) {
        VTermColor c = toVt(scheme.ansi[i]);
        vterm_state_set_palette_color(state, i, &c);
    }
    VTermColor fg = toVt(scheme.fg), bg = toVt(scheme.bg);
    vterm_state_set_default_colors(state, &fg, &bg);
    // Palette-Wechsel erzeugt keine Damage -> vollständige Neuzeichnung anstoßen.
    emit damaged(QRect(0, 0, m_cols, m_rows));
}

void VtScreen::cbOsc(int command, const char *str, int len, bool initial, bool final) {
    if (initial) {
        m_oscBuffer.clear();
        m_oscCommand = command;
    }
    if (str && len > 0) m_oscBuffer.append(str, len);
    if (!final) return;

    const QByteArray data = m_oscBuffer;
    const int cmd = m_oscCommand;
    m_oscBuffer.clear();
    m_oscCommand = -1;

    switch (cmd) {
    case 9: {  // OSC 9 ; …
        // OSC 9 ; 4 ; <state> ; <progress>  (ConEmu/Windows-Terminal-Fortschritt).
        const QList<QByteArray> p = data.split(';');
        if (p.size() >= 2 && p.first() == "4") {
            const int state = p.at(1).toInt();
            const int value = p.size() > 2 ? p.at(2).toInt() : 0;
            emit progress(state, value);
        } else {
            emit notify(QString::fromUtf8(data));  // OSC 9 ; <text> (Notification)
        }
        break;
    }
    case 777: {  // OSC 777 ; notify ; <title> ; <body>
        const QList<QByteArray> parts = data.split(';');
        if (!parts.isEmpty() && parts.first() == "notify") {
            QString text;
            for (int i = 1; i < parts.size(); ++i) {
                if (!text.isEmpty()) text += QStringLiteral(": ");
                text += QString::fromUtf8(parts.at(i));
            }
            emit notify(text);
        }
        break;
    }
    case 133: {  // OSC 133 ; A|B|C|D[;exit]   (Shell-Integration / FinalTerm)
        if (data.isEmpty()) break;
        const char kind = data.at(0);
        int exitCode = -1;
        if (kind == 'D') {
            const int semi = data.indexOf(';');
            if (semi >= 0) exitCode = data.mid(semi + 1).toInt();
        }
        emit promptMarker(kind, exitCode);
        break;
    }
    default:
        break;
    }
}

} // namespace qtmux
