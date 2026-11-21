#include "UpdateSelfTest.h"

#include "UpdateBridge.h"

#include "update/InstallBackend.hpp"

#include <QCoreApplication>
#include <QStringList>
#include <QTimer>

#include <memory>

namespace qtmux {

void driveUpdateSelfTest(UpdateBridge &bridge, bool systemInstaller)
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
        } else if (s == Flow::State::ManualInstall) {
            // „Von Hand“: Der Installationsweg hat eine Vorbedingung nicht erfüllt
            // (ein Entwicklerbau ist z. B. nicht familien-signiert). Den Grund nennt
            // der Ablauf im Log („install: manual … detail: …“). Es gibt keinen
            // Knopf mehr zu drücken: eigener Exit-Code, kein Hängen bis zur Frist.
            qInfo("QTmux selftest-update: manual install, ending (exit 3)");
            QCoreApplication::exit(3);
        }
    });
    // Wie der Menüpunkt „Nach Updates suchen …“, kurz nach dem Aufbau. Erst hier steht
    // fest, ob die Quelle umgestellt ist (start() hat die Startoptionen gelesen).
    QTimer::singleShot(500, &bridge, [&bridge, systemInstaller] {
        Flow &f = bridge.flow();
        if (!f.sourceOverridden()) return;
        if (systemInstaller) {
            f.setInstallBackend(appupdate::makeSystemInstallerBackend());
            qInfo("QTmux selftest-update: install way of stage 1 (system installer), dry run");
        }
        bridge.openByHand();
    });
}

} // namespace qtmux
