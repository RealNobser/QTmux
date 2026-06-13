#include <QCoreApplication>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QTranslator>
#include <QQmlContext>

#if defined(Q_OS_WIN)
#  include <windows.h>
#endif

#include "AppController.h"
#include "ColorScheme.h"
#include "ConnectionProfile.h"
#include "HotkeyRegistry.h"
#include "PluginHost.h"
#include "SecretsVault.h"
#include "GlobalHotkey.h"

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
#if defined(Q_OS_WIN)
    // Windows/ConPTY: QTmux ist eine GUI-App und sollte KEINE Konsole besitzen.
    // Manche Starter könnten dem Prozess dennoch eine anhängen; die per ConPTY
    // gestarteten Kindshells würden sich dann an diese geerbte Konsole binden statt
    // an die Pseudo-Konsole. Vor dem Start jeglicher PTYs lösen wir sie daher.
    // (Hinweis: Der VS-Code-Debugger braucht zusätzlich console=externalTerminal in
    // launch.json, sonst stört die Handle-Umleitung von "internalConsole" die
    // ConPTY-Datenflüsse — siehe dortigen Kommentar.)
    if (GetConsoleWindow()) FreeConsole();
#endif

    // macOS blendet Icons in (nativen) Menüs sonst aus -> explizit erlauben,
    // damit unsere Phosphor-Icons auch in der nativen Menüleiste erscheinen.
    QCoreApplication::setAttribute(Qt::AA_DontShowIconsInMenus, false);

    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName("QTmux");
    QGuiApplication::setOrganizationName("QTmux");
    QGuiApplication::setApplicationVersion("1.0.2");

    QQuickStyle::setStyle("Basic");

    QQmlApplicationEngine engine;
    // Farbschema-Registry als globale Context-Property verfügbar machen (dieselbe
    // Instanz, die Core/Session nutzen) → QML wählt/importiert, Core wendet an.
    // Bewusst KEIN qmlRegisterSingletonInstance in die URI „QTmux": das kollidiert
    // mit der auto-generierten Modul-Typregistrierung (Symptom: „TerminalItem is not a type").
    engine.rootContext()->setContextProperty(
        QStringLiteral("ColorSchemes"), qtmux::ColorSchemeRegistry::instance());

    // Connection-Manager: gespeicherte Verbindungsprofile (Shell/SSH/Seriell).
    // Wie oben als Context-Property; QML liest/bearbeitet, beim Verbinden ruft es
    // die passende SessionModel::create…Session-Methode (QTMUX-7).
    engine.rootContext()->setContextProperty(
        QStringLiteral("Profiles"), qtmux::ConnectionProfileRegistry::instance());

    // Konfigurierbare Tastenkürzel (QTMUX-15): QML bindet Action.shortcut an
    // Hotkeys.bindings[id]; Neubelegung wirkt sofort. Gleiche Brücke wie oben.
    engine.rootContext()->setContextProperty(
        QStringLiteral("Hotkeys"), qtmux::HotkeyRegistry::instance());

    // Secrets-Vault (QTMUX-22): verschlüsselter Geheimnis-Speicher (Master-Passwort).
    engine.rootContext()->setContextProperty(
        QStringLiteral("Vault"), qtmux::SecretsVault::instance());

    // Globaler Quake-Hotkey (Ctrl+`) als Context-Property; QML schaltet ihn je nach
    // Einstellung und reagiert auf `activated` (Fenster ein-/ausblenden).
    qtmux::GlobalHotkey quakeHotkey;
    engine.rootContext()->setContextProperty(QStringLiteral("QuakeHotkey"), &quakeHotkey);

    // Plugin-System (QTMUX-8): Plugins VOR dem QML-Laden einsammeln, damit das
    // „+"-Menü die Backend-Typen sofort kennt und restoreState() Plugin-Sessions
    // wiederherstellen kann. Gleiche Context-Property-Brücke wie oben.
    qtmux::PluginHost::instance().loadAll();
    engine.rootContext()->setContextProperty(
        QStringLiteral("Plugins"), &qtmux::PluginHost::instance());

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
