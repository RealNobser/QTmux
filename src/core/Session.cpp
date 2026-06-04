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
    m_backend.reset(backend);
    m_backend->setParent(this);

    m_screen = std::make_unique<VtScreen>(rows, cols);

    connect(m_backend.get(), &ITerminalBackend::dataReceived,
            m_screen.get(), &VtScreen::inputWrite);
    connect(m_screen.get(), &VtScreen::outputToPty,
            m_backend.get(), &ITerminalBackend::write);
    connect(m_screen.get(), &VtScreen::titleChanged, this, &Session::setTitle);
    connect(m_screen.get(), &VtScreen::bell, this, &Session::bell);
    connect(m_backend.get(), &ITerminalBackend::stateChanged,
            this, &Session::stateChanged);
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
