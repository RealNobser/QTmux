#pragma once

#include <QObject>
#include <QList>

namespace qtmux {

/// Verhindert den **System**-Ruhezustand, solange Agenten arbeiten (QTMUX-89).
/// Plattform-gekapselt wie [GlobalHotkey](GlobalHotkey.h), damit `qtmux_core` Gui-frei
/// bleibt: macOS über IOKit-Power-Management, Windows über `SetThreadExecutionState`,
/// Linux vorerst Stub (s. `isSupported`).
///
/// ⚠️ Bewusst **nur** der Systemschlaf, **nicht** der Bildschirm: Ein wachgehaltenes
/// Display frisst Akku und wirkt wie ein Fehler — der Rechner soll weiterrechnen dürfen,
/// während der Monitor dunkel wird.
class SleepInhibitor : public QObject {
    Q_OBJECT
public:
    explicit SleepInhibitor(QObject *parent = nullptr);
    ~SleepInhibitor() override;

    bool isActive() const { return m_active; }

    /// Sperre setzen oder freigeben. Idempotent — mehrfaches Setzen erzeugt **keine**
    /// zweite Sperre (ein Assertion-Leck hieße: der Rechner schläft nie wieder ein).
    /// @return true, wenn der Zielzustand erreicht wurde. Auf Plattformen ohne
    /// Unterstützung liefert nur `setActive(false)` true.
    bool setActive(bool on);

    /// Kann diese Plattform den Ruhezustand verhindern?
    static bool isSupported();

private:
    bool m_active = false;
    quint64 m_handle = 0;   ///< macOS: IOPMAssertionID; sonst ungenutzt
};

/// Reine Entscheidungsregel, Gui-frei und ohne Systemaufruf — damit sie ohne echte
/// Sessions testbar ist (`tst_session::sleepInhibitRule`).
///
/// Gesperrt wird nur, wenn der Anwender es eingeschaltet hat **und** mindestens eine
/// Session gerade **arbeitet** (`Session::Activity::Running`, von MCP `set_activity`
/// als `busy` gesetzt). `Waiting` zählt bewusst **nicht**: Da wartet der Agent auf einen
/// Menschen, und dann darf der Rechner schlafen — sonst bliebe er wegen einer offenen
/// Rückfrage die ganze Nacht wach.
bool shouldPreventSleep(bool settingEnabled, const QList<int> &activities);

} // namespace qtmux
