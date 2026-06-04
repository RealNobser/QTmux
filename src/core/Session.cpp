#include "Session.h"
#include "VtScreen.h"
#include "AgentRegistry.h"

namespace qtmux {

Session::Session(QObject *parent) : QObject(parent) {}
Session::~Session() = default;

void Session::attachBackend(ITerminalBackend *backend, Type type, int cols, int rows) {
    m_type = type;
    m_cols = cols;
    m_rows = rows;
    // Besitz liegt beim unique_ptr — KEIN setParent(this) (würde Double-Free verursachen).
    m_backend.reset(backend);

    m_screen = std::make_unique<VtScreen>(rows, cols);

    connect(m_backend.get(), &ITerminalBackend::dataReceived,
            m_screen.get(), &VtScreen::inputWrite);
    connect(m_screen.get(), &VtScreen::outputToPty,
            m_backend.get(), &ITerminalBackend::write);
    connect(m_screen.get(), &VtScreen::titleChanged, this, &Session::setTitle);
    connect(m_screen.get(), &VtScreen::bell, this, &Session::onBell);
    connect(m_screen.get(), &VtScreen::notify, this, &Session::onNotify);
    connect(m_screen.get(), &VtScreen::promptMarker, this, &Session::onPromptMarker);
    // Zustand aus dem Signal-Argument nehmen (NICHT m_backend dereferenzieren — das
    // Signal kann während der Zerstörung des Backends feuern).
    connect(m_backend.get(), &ITerminalBackend::stateChanged, this,
            [this](BackendState st) {
                emit stateChanged();
                if (st == BackendState::Closed) setActivity(Activity::Closed);
            });
}

void Session::setActive(bool active) {
    m_active = active;
    if (active && m_needsAttention) {
        m_needsAttention = false;
        emit attentionChanged();
    }
}

void Session::raiseAttention() {
    if (!m_active && !m_needsAttention) {
        m_needsAttention = true;
        emit attentionChanged();
    }
}

void Session::setActivity(Activity a) {
    if (a == m_activity) return;
    m_activity = a;
    emit activityChanged();
}

void Session::onBell() {
    emit bell();
    raiseAttention();   // Bell einer nicht-fokussierten Session = Aufmerksamkeit
}

void Session::onNotify(const QString &text) {
    // OSC 9/777: Notification-Text merken (Sidebar) und Aufmerksamkeit anfordern.
    m_lastNotification = text;
    emit notificationChanged();
    raiseAttention();
}

void Session::onPromptMarker(char kind, int exitCode) {
    // OSC 133 (FinalTerm/Shell-Integration): Befehls-Lebenszyklus verfolgen.
    switch (kind) {
    case 'C':                       // Befehl beginnt Ausgabe
        m_commandRunning = true;
        setActivity(Activity::Running);
        break;
    case 'D':                       // Befehl beendet (exitCode)
        setActivity(exitCode > 0 ? Activity::Error : Activity::Running);
        if (m_commandRunning) {     // echtes Kommando lief -> bei Inaktivität melden
            m_commandRunning = false;
            raiseAttention();
        }
        break;
    case 'A':                       // neue Prompt
    case 'B':                       // Prompt bereit für Eingabe
        if (m_activity == Activity::Error) setActivity(Activity::Running);
        break;
    default:
        break;
    }
}

void Session::setTitle(const QString &t) {
    if (t.isEmpty() || t == m_title) return;
    m_title = t;
    emit titleChanged(t);
}

void Session::start(int cols, int rows) {
    if (!m_backend) return;
    m_cols = cols;
    m_rows = rows;
    m_screen->setSize(rows, cols);
    m_backend->start(cols, rows);
}

void Session::write(const QByteArray &data) {
    observeInput(data);
    if (m_backend) m_backend->write(data);
}

void Session::observeInput(const QByteArray &data) {
    // Rekonstruiert die getippte Zeile zeichenweise und erkennt beim Enter
    // ein bekanntes Agenten-Kommando (z. B. "agy" -> AntiGravity).
    for (char ch : data) {
        if (ch == '\r' || ch == '\n') {
            if (const AgentInfo *agent = AgentRegistry::detect(m_inputLine)) {
                if (m_agentId != agent->id) {
                    m_agentId = agent->id;
                    emit agentChanged();
                }
                m_titleFromAgent = true;
                setTitle(agent->displayName);
            }
            m_inputLine.clear();
        } else if (ch == '\x7f' || ch == '\b') {  // Backspace
            m_inputLine.chop(1);
        } else if (static_cast<unsigned char>(ch) >= 0x20) {
            m_inputLine.append(QChar::fromLatin1(ch));
        } else {
            // Steuerzeichen (Ctrl-C, Pfeiltasten-Escapes …) verwerfen die Zeilenannahme.
            m_inputLine.clear();
        }
    }
}

void Session::resize(int cols, int rows) {
    if (cols == m_cols && rows == m_rows) return;
    m_cols = cols;
    m_rows = rows;
    if (m_screen) m_screen->setSize(rows, cols);
    if (m_backend) m_backend->resize(cols, rows);
}

} // namespace qtmux
