#include "SshBackend.h"

namespace qtmux {

SshBackend::SshBackend(QObject *parent) : PtyBackend(parent) {}

bool SshBackend::start(int cols, int rows) {
    QStringList args;
    if (m_port != 22 && m_port > 0) args << QStringLiteral("-p") << QString::number(m_port);
    if (!m_identity.isEmpty()) args << QStringLiteral("-i") << m_identity;
    // Pseudo-Terminal erzwingen (wir laufen ohnehin in einem PTY) für interaktive Shells.
    args << QStringLiteral("-t") << target();

    setProgram(QStringLiteral("ssh"));
    setArguments(args);
    return PtyBackend::start(cols, rows);
}

} // namespace qtmux
