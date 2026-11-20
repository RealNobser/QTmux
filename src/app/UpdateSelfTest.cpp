#include "UpdateSelfTest.h"

#include "UpdateBridge.h"

#include <QTimer>

#include <memory>

namespace qtmux {

void driveUpdateSelfTest(UpdateBridge &bridge)
{
    using Flow = appupdate::UpdateFlow;
    Flow &flow = bridge.flow();
    // Der Treiber handelt NUR bei umgestellter Quelle: Nur dann ist jede Installation
    // ein Trockenlauf. Ohne sie bricht main.cpp den Selbsttest ab, bevor er etwas tut.
    auto quitPressed = std::make_shared<bool>(false);
    QObject::connect(&flow, &Flow::stateChanged, &bridge, [&bridge, &flow, quitPressed] {
        const auto s = flow.state();
        qInfo("QTmux selftest-update: state %d", static_cast<int>(s));
        if (!flow.sourceOverridden()) return;
        if (s == Flow::State::Available) {
            bridge.perform(static_cast<int>(Flow::Action::Download));
        } else if (s == Flow::State::Ready && !*quitPressed) {
            // Genau einmal: Lehnt QTmux das Beenden ab, kehrt der Ablauf nach
            // „Bereit“ zurück, und ein zweiter Druck belegte nichts mehr.
            *quitPressed = true;
            qInfo("QTmux selftest-update: quit-and-install with %d running session(s)",
                  bridge.runningSessions());
            bridge.perform(static_cast<int>(Flow::Action::QuitAndInstall));
        }
    });
    // Wie der Menüpunkt „Nach Updates suchen …“, kurz nach dem Aufbau.
    QTimer::singleShot(500, &bridge, [&bridge] {
        if (bridge.flow().sourceOverridden()) bridge.openByHand();
    });
}

} // namespace qtmux
