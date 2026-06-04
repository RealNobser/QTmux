#include "Session.h"
#include "VtScreen.h"

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
    if (m_backend) m_backend->write(data);
}

void Session::resize(int cols, int rows) {
    if (cols == m_cols && rows == m_rows) return;
    m_cols = cols;
    m_rows = rows;
    if (m_screen) m_screen->setSize(rows, cols);
    if (m_backend) m_backend->resize(cols, rows);
}

} // namespace qtmux
