#include <QCoreApplication>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QTranslator>
#include <QQmlContext>

#include "AppController.h"
#include "ColorScheme.h"

namespace {

// Lädt die .qm-Übersetzung für `lang`, tauscht den installierten Translator aus
// und stößt die Neuübersetzung aller QML-Bindings an.
void applyLanguage(QGuiApplication &app, QQmlApplicationEngine &engine,
                   QTranslator *&active, const QString &lang) {
    auto *next = new QTranslator(&app);
    if (next->load(QStringLiteral("qtmux_%1").arg(lang), QStringLiteral(":/i18n"))) {
        if (active) { app.removeTranslator(active); active->deleteLater(); }
        app.installTranslator(next);
        active = next;
        engine.retranslate();
    } else {
        // Keine .qm (z. B. Quellsprache Deutsch) -> vorhandene Übersetzung entfernen.
        next->deleteLater();
        if (active) { app.removeTranslator(active); active->deleteLater(); active = nullptr; }
        engine.retranslate();
    }
}

} // namespace

int main(int argc, char *argv[])
{
    // macOS blendet Icons in (nativen) Menüs sonst aus -> explizit erlauben,
    // damit unsere Phosphor-Icons auch in der nativen Menüleiste erscheinen.
    QCoreApplication::setAttribute(Qt::AA_DontShowIconsInMenus, false);

    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName("QTmux");
    QGuiApplication::setOrganizationName("QTmux");
    QGuiApplication::setApplicationVersion("0.1.0");

    QQuickStyle::setStyle("Basic");

    QQmlApplicationEngine engine;
    // Farbschema-Registry als globale Context-Property verfügbar machen (dieselbe
    // Instanz, die Core/Session nutzen) → QML wählt/importiert, Core wendet an.
    // Bewusst KEIN qmlRegisterSingletonInstance in die URI „QTmux": das kollidiert
    // mit der auto-generierten Modul-Typregistrierung (Symptom: „TerminalItem is not a type").
    engine.rootContext()->setContextProperty(
        QStringLiteral("ColorSchemes"), qtmux::ColorSchemeRegistry::instance());

    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreationFailed,
        &app, []() { QCoreApplication::exit(-1); },
        Qt::QueuedConnection);

    engine.loadFromModule("QTmux", "Main");

    // Sprache initial setzen und bei Wechsel (über das Sprachmenü) neu laden.
    if (auto *appc = engine.singletonInstance<qtmux::AppController *>("QTmux", "App")) {
        static QTranslator *active = nullptr;
        applyLanguage(app, engine, active, appc->language());
        QObject::connect(appc, &qtmux::AppController::languageChanged, &app,
                         [&app, &engine](const QString &lang) {
                             applyLanguage(app, engine, active, lang);
                         });
    }

    return app.exec();
}
