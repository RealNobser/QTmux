#pragma once

#include <QObject>
#include <QByteArray>
#include <QString>
#include <QRect>
#include <QPoint>
#include <deque>
#include <vector>

struct VTerm;
struct VTermScreen;
struct VTermState;

namespace qtmux {

struct ColorScheme;

/// Eine gerenderte Terminalzelle in Qt-freundlicher Form.
struct Cell {
    QString text;                 // ein Graphem (meist 1 Codepoint)
    quint32 fg = 0;               // 0xRRGGBB, gueltig nur wenn !fgDefault
    quint32 bg = 0;               // 0xRRGGBB, gueltig nur wenn !bgDefault
    bool fgDefault = true;        // Theme-Vordergrund verwenden
    bool bgDefault = true;        // Theme-Hintergrund verwenden
    bool bold = false;
    bool faint = false;           // SGR 2 (dim) — Vordergrund abgedunkelt rendern
    bool italic = false;
    bool underline = false;
    bool reverse = false;
    quint8 width = 1;             // 1 = normal, 2 = doppelt breit (CJK)
};

/// Wrapper um libvterm: parst VT-/ANSI-Bytes, hält den Screen-State + Scrollback
/// und meldet Damage-Regionen, damit das TerminalItem nur Geändertes neu rendert.
class VtScreen : public QObject {
    Q_OBJECT
public:
    explicit VtScreen(int rows, int cols, QObject *parent = nullptr);
    ~VtScreen() override;

    /// Bytes vom Backend (PTY/SSH/…) in den Parser geben.
    void inputWrite(const QByteArray &data);

    void setSize(int rows, int cols);
    int rows() const { return m_rows; }
    int cols() const { return m_cols; }

    /// Zelle des sichtbaren Bildschirms (0-basiert).
    Cell cell(int row, int col) const;

    /// Sichtbarer Bildschirm als Klartext (Zeilen mit \n, rechte Leerzeichen entfernt).
    QString screenText() const;
    /// Gesamte Scrollback-Historie als Klartext (älteste Zeile zuerst), ohne den
    /// sichtbaren Bildschirm. Leer, wenn kein Scrollback vorhanden.
    QString scrollbackText() const;

    QPoint cursor() const { return m_cursor; }
    bool cursorVisible() const { return m_cursorVisible; }
    QString title() const { return m_title; }

    int scrollbackCount() const { return static_cast<int>(m_scrollback.size()); }
    /// Scrollback-Zeile (0 = älteste).
    const std::vector<Cell> &scrollbackLine(int index) const { return m_scrollback[index].cells; }
    /// Ist die Scrollback-Zeile ein weicher Umbruch (Flow-Fortsetzung) der vorigen?
    /// (Für Copy: an solchen Grenzen KEIN \n einfügen — eine logische Zeile.)
    bool scrollbackContinuation(int index) const { return m_scrollback[index].continuation; }
    /// Ist die sichtbare Zeile `row` eine Flow-Fortsetzung der vorigen Zeile?
    bool lineContinuation(int row) const;

    /// Bracketed-Paste-Markierungen (ESC[200~ / ESC[201~) ausgeben — libvterm sendet
    /// sie über die Output-Callback NUR, wenn die Anwendung DECSET 2004 aktiviert hat.
    /// Eine Einfügung wird zwischen startPaste()/endPaste() geklammert.
    void startPaste();
    void endPaste();

    /// Maus-Tracking-Modus der Anwendung: 0=aus, 1=Klick, 2=Drag, 3=Move (aus
    /// DECSET 1000/1002/1003). Ist er ungleich 0, sollen Maus-/Scrollrad-Events an
    /// die Anwendung gemeldet werden (statt lokal zu scrollen/selektieren).
    int mouseTracking() const { return m_mouseTracking; }
    /// Mausbewegung an die Anwendung melden (0-basierte Zelle). No-op ohne Tracking;
    /// libvterm sendet nur, wenn der aktive Modus die Bewegung verlangt.
    void mouseMove(int row, int col, Qt::KeyboardModifiers mods);
    /// Maustaste an die Anwendung melden. button: 1=links, 2=mitte, 3=rechts,
    /// 4=Rad hoch, 5=Rad runter. Setzt intern erst die Position, dann das Ereignis.
    void mouseButton(int button, bool pressed, int row, int col,
                     Qt::KeyboardModifiers mods);

    /// Setzt die libvterm-Palette (16 ANSI-Farben + Default-fg/bg) aus einem Schema
    /// und stößt eine vollständige Neuzeichnung an. Default-fg/bg rendert das
    /// TerminalItem über das Theme; hier v. a. für korrekte ANSI-/Reverse-Farben.
    void applyColorScheme(const ColorScheme &scheme);

signals:
    /// Geänderter Bereich in Zellkoordinaten (col/row).
    void damaged(const QRect &cells);
    void cursorMoved();
    void bell();
    void titleChanged(const QString &title);
    /// Antworten des Terminals (z. B. Device-Status), die zurück ins Backend müssen.
    void outputToPty(const QByteArray &data);
    /// Desktop-/App-Notification (OSC 9 bzw. OSC 777).
    void notify(const QString &text);
    /// Fortschritt (OSC 9;4): state 0=aus, 1=normal, 2=Fehler, 3=unbestimmt,
    /// 4=pausiert/Warnung; value 0..100 (bei 3 ohne Bedeutung).
    void progress(int state, int value);
    /// Shell-Integrations-Prompt-Marker (OSC 133): kind 'A'/'B'/'C'/'D'.
    /// exitCode gilt nur für 'D' (sonst -1).
    void promptMarker(char kind, int exitCode);
    /// Strukturiertes Agenten-Ereignis (OSC 777 ; qtmux-event ; <kind> ; <text>):
    /// kind = done/question/error/info. Für die Inter-Agenten-Benachrichtigung.
    void agentEvent(const QString &kind, const QString &text);

public:
    // Interne Handler — von den C-Callbacks in VtScreen.cpp aufgerufen.
    // (public, da die freien libvterm-Callbacks darauf zugreifen; nutze sie nicht direkt.)
    void cbDamage(int startRow, int startCol, int endRow, int endCol);
    void cbMoveCursor(int row, int col, bool visible);
    void cbBell();
    bool cbResize(int rows, int cols);
    void cbSetTitle(const QString &title);
    void cbPushScrollback(std::vector<Cell> &&line, bool continuation);
    void cbOutput(const QByteArray &data);
    void cbSetMouse(int mode);
    /// Sammelt OSC-Fragmente (libvterm liefert sie ggf. stückweise) und parst sie.
    void cbOsc(int command, const char *str, int len, bool initial, bool final);

private:
    VTerm *m_vt = nullptr;
    VTermScreen *m_screen = nullptr;
    VTermState *m_state = nullptr;   // für vterm_state_get_lineinfo (Flow-Continuation)
    int m_rows = 0;
    int m_cols = 0;
    QPoint m_cursor{0, 0};
    bool m_cursorVisible = true;
    QString m_title;
    int m_mouseTracking = 0;   // VTERM_PROP_MOUSE: 0=aus,1=Klick,2=Drag,3=Move

    // Scrollback-Zeile + ob sie ein weicher Umbruch der vorigen ist (für Copy).
    struct SbLine {
        std::vector<Cell> cells;
        bool continuation = false;
    };
    std::deque<SbLine> m_scrollback;
    static constexpr int kMaxScrollback = 10000;

    int m_oscCommand = -1;     // aktuell gesammelte OSC-Nummer
    QByteArray m_oscBuffer;    // akkumulierte OSC-Fragmente bis 'final'
};

} // namespace qtmux
