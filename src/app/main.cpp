#include <QCoreApplication>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QTranslator>
#include <QQmlContext>

#if defined(Q_OS_WIN)
#  include <windows.h>
#endif

#if defined(Q_OS_MACOS)
#  include <QSettings>
#  include <QLocale>
#  include <CoreFoundation/CoreFoundation.h>
#endif

#include "AppController.h"
#include "AgentEventHub.h"
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

    // App-Identität bereits VOR der QGuiApplication setzen (statisch erlaubt), damit
    // QSettings unten dieselbe (Bundle-abgeleitete) Domain trifft wie der AppController.
    QGuiApplication::setApplicationName("QTmux");
    QGuiApplication::setOrganizationName("QTmux");
    QGuiApplication::setApplicationVersion("1.1.1");

#if defined(Q_OS_MACOS)
    // Die nativen App-Menü-Standarditems (Über/Einstellungen/Dienste/Ausblenden/
    // Beenden) lokalisiert macOS über die AppleLanguages-Preference und folgt sonst
    // der System-UI-Sprache — unabhängig von unserer App-Sprachwahl und vom
    // QTranslator. Damit auch dieses Menü der eingestellten Sprache folgt, setzen
    // wir AppleLanguages aus den QSettings VOR dem QGuiApplication-Ctor (der AppKit/
    // NSApplication initialisiert und die Menü-Strings lokalisiert). Der Wert landet
    // in der App-Preference-Domain (com.qtmux.app.plist) und wird von cfprefsd beim
    // Prozessende persistiert — das ist unkritisch, da wir ihn bei JEDEM Start neu
    // aus ui/language ableiten und überschreiben. Greift beim Start; ein Laufzeit-
    // Sprachwechsel erfordert für DIESE Items einen Neustart. (Ein eingespeistes
    // -AppleLanguages-argv wirkt NICHT: NSUserDefaults liest das echte
    // OS-Prozess-argv, nicht Qts argv.)
    {
        QSettings s;   // Default-Ctor: gleiche Domain wie der AppController
        const QString sys = QLocale::system().name().left(2);
        const QString fallback = (sys == QLatin1String("de")) ? QStringLiteral("de")
                                                              : QStringLiteral("en");
        const QString lang = s.value(QStringLiteral("ui/language"), fallback).toString();
        CFStringRef cfLang = lang.toCFString();
        const void *items[] = { cfLang };
        CFArrayRef langs = CFArrayCreate(nullptr, items, 1, &kCFTypeArrayCallBacks);
        CFPreferencesSetAppValue(CFSTR("AppleLanguages"), langs,
                                 kCFPreferencesCurrentApplication);
        CFRelease(langs);
        CFRelease(cfLang);
    }
#endif

    QGuiApplication app(argc, argv);

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

    // Inter-Agenten-Benachrichtigung: zentraler Ereignis-Bus. QML-Dialog liest/setzt
    // Abos; Session speist OSC-Ereignisse ein, der McpServer liefert sie per Long-Poll.
    engine.rootContext()->setContextProperty(
        QStringLiteral("AgentEvents"), qtmux::AgentEventHub::instance());

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

    // Sprache initial setzen, BEVOR das QML (und damit die native macOS-Menüleiste)
    // gebaut wird. Sonst verschiebt das Cocoa-Plugin „Über/Einstellungen/Beenden"
    // per MenuRole sofort mit dem Quelltext (Deutsch) ins native App-Menü, und ein
    // späteres engine.retranslate() aktualisiert gerade diese promoteten App-Menü-
    // Einträge nicht mehr (die regulären File/Edit/View-Menüs schon). singletonInstance
    // erzeugt den AppController-Singleton bereits vor dem Laden von Main.qml.
    static QTranslator *active = nullptr;
    auto *appc = engine.singletonInstance<qtmux::AppController *>("QTmux", "App");
    if (appc)
        applyLanguage(app, engine, active, appc->language());

    engine.loadFromModule("QTmux", "Main");

    // Laufzeitwechsel über das Sprachmenü neu laden.
    if (appc) {
        QObject::connect(appc, &qtmux::AppController::languageChanged, &app,
                         [&app, &engine](const QString &lang) {
                             applyLanguage(app, engine, active, lang);
                         });
    }

    return app.exec();
}
