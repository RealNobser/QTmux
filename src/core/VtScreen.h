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

namespace qtmux {

/// Eine gerenderte Terminalzelle in Qt-freundlicher Form.
struct Cell {
    QString text;                 // ein Graphem (meist 1 Codepoint)
    quint32 fg = 0;               // 0xRRGGBB, gueltig nur wenn !fgDefault
    quint32 bg = 0;               // 0xRRGGBB, gueltig nur wenn !bgDefault
    bool fgDefault = true;        // Theme-Vordergrund verwenden
    bool bgDefault = true;        // Theme-Hintergrund verwenden
    bool bold = false;
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

    QPoint cursor() const { return m_cursor; }
    bool cursorVisible() const { return m_cursorVisible; }
    QString title() const { return m_title; }

    int scrollbackCount() const { return static_cast<int>(m_scrollback.size()); }
    /// Scrollback-Zeile (0 = älteste).
    const std::vector<Cell> &scrollbackLine(int index) const { return m_scrollback[index]; }

    /// Bracketed-Paste-Markierungen (ESC[200~ / ESC[201~) ausgeben — libvterm sendet
    /// sie über die Output-Callback NUR, wenn die Anwendung DECSET 2004 aktiviert hat.
    /// Eine Einfügung wird zwischen startPaste()/endPaste() geklammert.
    void startPaste();
    void endPaste();

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
    /// Shell-Integrations-Prompt-Marker (OSC 133): kind 'A'/'B'/'C'/'D'.
    /// exitCode gilt nur für 'D' (sonst -1).
    void promptMarker(char kind, int exitCode);

public:
    // Interne Handler — von den C-Callbacks in VtScreen.cpp aufgerufen.
    // (public, da die freien libvterm-Callbacks darauf zugreifen; nutze sie nicht direkt.)
    void cbDamage(int startRow, int startCol, int endRow, int endCol);
    void cbMoveCursor(int row, int col, bool visible);
    void cbBell();
    bool cbResize(int rows, int cols);
    void cbSetTitle(const QString &title);
    void cbPushScrollback(std::vector<Cell> &&line);
    void cbOutput(const QByteArray &data);
    /// Sammelt OSC-Fragmente (libvterm liefert sie ggf. stückweise) und parst sie.
    void cbOsc(int command, const char *str, int len, bool initial, bool final);

private:
    VTerm *m_vt = nullptr;
    VTermScreen *m_screen = nullptr;
    int m_rows = 0;
    int m_cols = 0;
    QPoint m_cursor{0, 0};
    bool m_cursorVisible = true;
    QString m_title;

    std::deque<std::vector<Cell>> m_scrollback;
    static constexpr int kMaxScrollback = 10000;

    int m_oscCommand = -1;     // aktuell gesammelte OSC-Nummer
    QByteArray m_oscBuffer;    // akkumulierte OSC-Fragmente bis 'final'
};

} // namespace qtmux
