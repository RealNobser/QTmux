#include <QCoreApplication>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QTranslator>
#include <QQmlContext>
#include <QQuickWindow>
#include <QTimer>
#include <QImage>

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

    // Startargumente --profile / --mcp-port GANZ FRÜH in die entsprechenden Env-Vars
    // übersetzen, damit der restliche (env-basierte) Code unverändert bleibt: die
    // QSettings-Domain unten liest QTMUX_PROFILE, der McpServer liest QTMUX_MCP_PORT.
    // So kann „Neues Fenster" (AppController::openNewInstance) eine unabhängige Instanz
    // mit eigenem Profil + freiem Port starten, ohne dass Umgebungsvererbung nötig wäre.
    // Bereits gesetzte Env-Vars haben Vorrang (explizit gestartete Testinstanz).
    QString shotPath;          // --screenshot <png>: stiller Selbst-Screenshot (s. u.)
    int shotSettleMs = 700;    // --settle <ms>: Wartezeit, damit das Layout sich setzt
    for (int i = 1; i + 1 < argc; ++i) {
        const QByteArray a(argv[i]);
        if (a == "--profile" && !qEnvironmentVariableIsSet("QTMUX_PROFILE"))
            qputenv("QTMUX_PROFILE", argv[i + 1]);
        else if (a == "--mcp-port" && !qEnvironmentVariableIsSet("QTMUX_MCP_PORT"))
            qputenv("QTMUX_MCP_PORT", argv[i + 1]);
        else if (a == "--screenshot")
            shotPath = QString::fromLocal8Bit(argv[i + 1]);
        else if (a == "--settle")
            shotSettleMs = qMax(50, QByteArray(argv[i + 1]).toInt());
    }
    // Stiller Selbst-Screenshot (Vorbild: RAFTNG --screenshot): die App rendert sich
    // OFFSCREEN und schreibt die Root-Fenster-Szene per grabWindow() in ein PNG — ohne
    // OS-Compositor und ohne Bildschirmaufnahme-Berechtigung (TCC). Erzwingt zusätzlich den
    // QPainter-Renderpfad (QTMUX_NO_GPU), damit der Grab auch das benutzerdefinierte
    // Glyph-Material zuverlässig enthält.
    if (!shotPath.isEmpty()) {
        if (!qEnvironmentVariableIsSet("QT_QPA_PLATFORM")) qputenv("QT_QPA_PLATFORM", "offscreen");
        qputenv("QTMUX_NO_GPU", "1");
    }

    // App-Identität bereits VOR der QGuiApplication setzen (statisch erlaubt), damit
    // QSettings unten dieselbe (Bundle-abgeleitete) Domain trifft wie der AppController.
    // Instanz-Profil (QTMUX_PROFILE): hängt einen Suffix an den App-Namen und trennt
    // damit die gesamte QSettings-Domain — Session-Liste, Profile, Hotkeys, Vault.
    // Zweck: eine ZWEITE Instanz zum Testen starten, ohne den gespeicherten Zustand
    // der produktiven zu überschreiben (die beim Beenden ihre Session-Liste sichert).
    // Zusammen mit QTMUX_MCP_PORT ist das der saubere Weg, die MCP-Schicht zu prüfen.
    const QString profile = qEnvironmentVariable("QTMUX_PROFILE").trimmed();
    QGuiApplication::setApplicationName(profile.isEmpty()
                                            ? QStringLiteral("QTmux")
                                            : QStringLiteral("QTmux-%1").arg(profile));
    QGuiApplication::setOrganizationName("QTmux");
    QGuiApplication::setApplicationVersion("1.6.1");

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

    // Screenshot-Modus: nach kurzer Settle-Zeit die Root-QQuickWindow-Szene grabben,
    // als PNG speichern und mit dem Ergebnis-Code beenden. Der Timer läuft auf dem
    // GUI-Thread; grabWindow() rendert die (offscreen) Szene und liest sie zurück.
    if (!shotPath.isEmpty()) {
        QTimer::singleShot(shotSettleMs, &app, [&engine, shotPath]() {
            const QList<QObject *> roots = engine.rootObjects();
            auto *win = roots.isEmpty() ? nullptr : qobject_cast<QQuickWindow *>(roots.first());
            int rc = 2;
            if (win) {
                const QImage img = win->grabWindow();
                rc = (!img.isNull() && img.save(shotPath, "PNG")) ? 0 : 1;
            }
            QCoreApplication::exit(rc);
        });
    }

    // Laufzeitwechsel über das Sprachmenü neu laden.
    if (appc) {
        QObject::connect(appc, &qtmux::AppController::languageChanged, &app,
                         [&app, &engine](const QString &lang) {
                             applyLanguage(app, engine, active, lang);
                         });
    }

    return app.exec();
}
