#include "GlobalHotkey.h"

#ifdef Q_OS_MACOS
#include <Carbon/Carbon.h>
#endif

namespace qtmux {

#ifdef Q_OS_MACOS

namespace {
EventHotKeyRef g_hotKeyRef = nullptr;
EventHandlerRef g_handlerRef = nullptr;
GlobalHotkey *g_self = nullptr;

OSStatus hotKeyHandler(EventHandlerCallRef, EventRef, void *) {
    // Aus dem Carbon-Callback in den GUI-Thread (queued) zustellen.
    if (g_self) QMetaObject::invokeMethod(g_self, "activated", Qt::QueuedConnection);
    return noErr;
}
} // namespace

GlobalHotkey::GlobalHotkey(QObject *parent) : QObject(parent) {}

GlobalHotkey::~GlobalHotkey() { setEnabled(false); }

bool GlobalHotkey::setEnabled(bool on) {
    if (on == m_enabled) return true;
    if (on) {
        g_self = this;
        if (!g_handlerRef) {
            EventTypeSpec evt{kEventClassKeyboard, kEventHotKeyPressed};
            InstallApplicationEventHandler(&hotKeyHandler, 1, &evt, nullptr, &g_handlerRef);
        }
        EventHotKeyID id;
        id.signature = 'qtmx';
        id.id = 1;
        // Ctrl + `  (Backtick = kVK_ANSI_Grave 0x32; controlKey)
        const OSStatus st = RegisterEventHotKey(0x32, controlKey, id,
                                                GetApplicationEventTarget(), 0, &g_hotKeyRef);
        m_enabled = (st == noErr);
        return m_enabled;
    }
    if (g_hotKeyRef) { UnregisterEventHotKey(g_hotKeyRef); g_hotKeyRef = nullptr; }
    m_enabled = false;
    return true;
}

#else  // Windows/Linux: vorerst nicht implementiert (Feature dort deaktiviert).

GlobalHotkey::GlobalHotkey(QObject *parent) : QObject(parent) {}
GlobalHotkey::~GlobalHotkey() = default;

bool GlobalHotkey::setEnabled(bool on) {
    m_enabled = false;          // nicht unterstützt
    return !on;                 // Ausschalten "gelingt", Einschalten nicht
}

#endif

} // namespace qtmux
