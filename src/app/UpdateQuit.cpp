#include "UpdateQuit.h"

#include <QCoreApplication>
#include <QQmlApplicationEngine>
#include <QQuickWindow>
#include <QTimer>

namespace qtmux {

bool quitForUpdate(QQmlApplicationEngine &engine)
{
    const QList<QObject *> roots = engine.rootObjects();
    auto *win = roots.isEmpty() ? nullptr : qobject_cast<QQuickWindow *>(roots.first());
    if (!win) {
        qWarning("QTmux: Beenden fuer das Update ohne Hauptfenster abgelehnt.");
        return false;
    }
    // Für den Selbsttest: belegt, dass die Rückfrage eingeschaltet war und trotzdem
    // nicht kam.
    qInfo("QTmux: Beenden fuer das Update (confirmQuit=%d).",
          win->property("confirmQuit").toBool() ? 1 : 0);
    const QVariant before = win->property("quitConfirmed");
    win->setProperty("quitConfirmed", true);
    // Synchron: Main.qml `onClosing` läuft jetzt — sichert und beendet die
    // Sitzungen. Lehnt es ab, steht QTmux unverändert da.
    if (!win->close()) {
        win->setProperty("quitConfirmed", before);
        qInfo("QTmux: Das Hauptfenster hat das Schliessen fuer das Update abgelehnt.");
        return false;
    }
    // Übrige Fenster (Einstellungen) haben keine eigene Rückfrage. Das Beenden
    // läuft über die Ereignisschleife, damit aboutToQuit — wo der Ablauf erst die
    // Installation startet — nach dem vollständigen Schließen kommt.
    QTimer::singleShot(0, qApp, &QCoreApplication::quit);
    return true;
}

} // namespace qtmux
