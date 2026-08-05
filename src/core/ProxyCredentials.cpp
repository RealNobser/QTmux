#include "ProxyCredentials.h"

namespace qtmux {

void ProxyCredentials::set(const QString &user, const QString &password)
{
    m_user = user;
    m_password = password;
    // Neue Anmeldedaten bekommen einen frischen Versuch — sonst wäre eine
    // Korrektur nach einem Tippfehler wirkungslos.
    m_offered = false;
    m_rejected = false;
}

void ProxyCredentials::clear()
{
    m_user.clear();
    m_password.clear();
    m_offered = false;
    m_rejected = false;
}

bool ProxyCredentials::mayAnswer(bool previousAttemptFailed)
{
    if (previousAttemptFailed) {
        // Die Gegenstelle hat abgelehnt, was wir gerade geschickt haben. Ein
        // zweiter Versuch mit demselben Passwort ist zwecklos und in einer
        // AD-Umgebung schädlich (Kontosperre). Passwort verwerfen, Benutzername
        // behalten, aufgeben — der Dialog fragt danach mit dem Hinweis nach.
        m_password.clear();
        m_offered = false;
        m_rejected = true;
        return false;
    }
    if (m_password.isEmpty())
        return false;   // nichts anzubieten → die Lib meldet ProxyAuthRequired
    if (m_offered)
        return false;   // der eine Versuch ist verbraucht
    m_offered = true;
    return true;
}

}  // namespace qtmux
