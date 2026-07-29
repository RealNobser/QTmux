#include "SleepInhibitor.h"
#include "Session.h"

#include <QtGlobal>

#if defined(Q_OS_MACOS)
#  include <IOKit/pwr_mgt/IOPMLib.h>
#  include <CoreFoundation/CoreFoundation.h>
#elif defined(Q_OS_WIN)
#  include <windows.h>
#endif

namespace qtmux {

SleepInhibitor::SleepInhibitor(QObject *parent) : QObject(parent) {}

SleepInhibitor::~SleepInhibitor() {
    // Ohne dieses Freigeben überlebt die Sperre den Prozess (macOS bindet sie an den
    // Prozess, Windows an den Thread) — der Rechner schliefe danach nicht mehr ein.
    setActive(false);
}

bool SleepInhibitor::isSupported() {
#if defined(Q_OS_MACOS) || defined(Q_OS_WIN)
    return true;
#else
    // Linux bräuchte org.freedesktop.login1.Manager.Inhibit über QtDBus. Bewusst noch
    // nicht umgesetzt: Der Code wäre hier nicht lauffähig zu prüfen, und eine ungetestete
    // Sperre, die hängen bleibt, ist schlimmer als gar keine (QTMUX-89 vermerkt das).
    return false;
#endif
}

bool SleepInhibitor::setActive(bool on) {
    if (on == m_active) return true;          // idempotent — keine zweite Assertion

#if defined(Q_OS_MACOS)
    if (on) {
        IOPMAssertionID id = 0;
        // PreventUserIdleSystemSleep: Das System bleibt wach, der BILDSCHIRM darf
        // schlafen. kIOPMAssertionTypeNoDisplaySleep wäre hier falsch.
        const IOReturn r = IOPMAssertionCreateWithName(
            kIOPMAssertionTypePreventUserIdleSystemSleep, kIOPMAssertionLevelOn,
            CFSTR("QTmux: Agent arbeitet"), &id);
        if (r != kIOReturnSuccess) return false;
        m_handle = id;
        m_active = true;
        return true;
    }
    IOPMAssertionRelease(static_cast<IOPMAssertionID>(m_handle));
    m_handle = 0;
    m_active = false;
    return true;

#elif defined(Q_OS_WIN)
    // ⚠️ SetThreadExecutionState wirkt **pro Thread** — Setzen und Aufheben müssen aus
    // demselben Thread kommen. Beides läuft hier im GUI-Thread (SessionModel).
    // ES_SYSTEM_REQUIRED ohne ES_DISPLAY_REQUIRED: System wach, Bildschirm darf aus.
    const EXECUTION_STATE want = on ? (ES_CONTINUOUS | ES_SYSTEM_REQUIRED) : ES_CONTINUOUS;
    if (SetThreadExecutionState(want) == 0) return false;
    m_active = on;
    return true;

#else
    m_active = false;
    return !on;        // „freigeben" gelingt immer, „sperren" kann diese Plattform nicht
#endif
}

bool shouldPreventSleep(bool settingEnabled, const QList<int> &activities) {
    if (!settingEnabled) return false;
    for (int a : activities)
        if (a == static_cast<int>(Session::Activity::Running)) return true;
    return false;
}

} // namespace qtmux
