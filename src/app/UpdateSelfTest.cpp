#include "UpdateSelfTest.h"

#include "UpdateBridge.h"

#include <QStringList>
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

bool driveUpdatePicture(UpdateBridge &bridge, const QString &scene,
                        std::function<void(bool prefsWindow)> ready)
{
    using Flow = appupdate::UpdateFlow;
    static const QStringList kScenes{QStringLiteral("3"), QStringLiteral("5"),
                                     QStringLiteral("hint-avail"),
                                     QStringLiteral("hint-avail-narrow"),
                                     QStringLiteral("settings")};
    if (!kScenes.contains(scene)) {
        qCritical("QTmux update-picture: unbekannte Szene \"%s\"; heute erreichbar: %s",
                  qPrintable(scene), qPrintable(kScenes.join(QStringLiteral(", "))));
        return false;
    }
    auto done = std::make_shared<bool>(false);
    auto finish = [ready, done](bool prefsWindow) {
        if (*done) return;
        *done = true;
        ready(prefsWindow);
    };
    if (scene == QLatin1String("settings")) {
        QTimer::singleShot(500, &bridge, [&bridge, finish] {
            emit bridge.settingsPageRequested();
            finish(true);
        });
        return true;
    }
    Flow &flow = bridge.flow();
    const bool hint = scene.startsWith(QLatin1String("hint-"));
    QObject::connect(&flow, &Flow::stateChanged, &bridge, [&bridge, &flow, scene, hint, finish] {
        if (!flow.sourceOverridden()) return;
        const auto s = flow.state();
        qInfo("QTmux update-picture: state %d", static_cast<int>(s));
        if (s == Flow::State::Available) {
            if (scene == QLatin1String("3")) finish(false);
            else if (scene == QLatin1String("5"))
                bridge.perform(static_cast<int>(Flow::Action::Download));
            else if (hint)   // Dialog wie mit Esc schließen: Der Hinweis bleibt.
                bridge.perform(flow.viewMap().value(QStringLiteral("escapeAction")).toInt());
        } else if (s == Flow::State::Ready && scene == QLatin1String("5")) {
            finish(false);
        }
    });
    if (hint) {
        QObject::connect(&flow, &Flow::hintChanged, &bridge, [&flow, finish] {
            if (flow.hintMap().value(QStringLiteral("visible")).toBool()) finish(false);
        });
    }
    QTimer::singleShot(500, &bridge, [&bridge] {
        if (bridge.flow().sourceOverridden()) bridge.openByHand();
    });
    return true;
}

} // namespace qtmux
