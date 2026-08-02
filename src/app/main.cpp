#include <QCoreApplication>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QTranslator>
#include <QQmlContext>
#include <QQuickWindow>
#include <QTimer>
#include <QImage>
#include <QDir>
#include <QFileInfo>
#include <QLibraryInfo>

#if defined(Q_OS_WIN)
#  include <windows.h>
#else
#  include <QSocketNotifier>
#  include <csignal>
#  include <unistd.h>
#endif

#if defined(Q_OS_MACOS)
#  include <QSettings>
#  include <QLocale>
#  include <CoreFoundation/CoreFoundation.h>
#endif

#include <QTextStream>

#include "qtmux_version.h"   // generiert aus cmake/Version.h.in (PROJECT_VERSION)

#include "AppController.h"
#include "AgentEventHub.h"
#include "ShellIntegration.h"
#include "ColorScheme.h"
#include "ConnectionProfile.h"
#include "HotkeyRegistry.h"
#include "PluginHost.h"
#include "SecretsVault.h"
#include "SettingsIo.h"
#include "GlobalHotkey.h"

namespace {

// Tauscht EINEN installierten Translator gegen die .qm `<basis>_<lang>` aus :/i18n.
// Liegt keine vor (z. B. Quellsprache), wird der bisherige entfernt statt beibehalten —
// sonst bliebe beim Wechsel de->en die deutsche Übersetzung aktiv.
void swapTranslator(QGuiApplication &app, QTranslator *&active,
                    const QString &base, const QString &lang) {
    auto *next = new QTranslator(&app);
    if (next->load(QStringLiteral("%1_%2").arg(base, lang), QStringLiteral(":/i18n"))) {
        if (active) { app.removeTranslator(active); active->deleteLater(); }
        app.installTranslator(next);
        active = next;
        return;
    }
    next->deleteLater();
    if (active) { app.removeTranslator(active); active->deleteLater(); active = nullptr; }
}

// Setzt die Sprache und stößt die Neuübersetzung aller QML-Bindings an.
// 🔑 ZWEI Translator (QTMUX-117): `qtmux_<lang>` trägt unsere eigenen Texte,
// `qtbase_<lang>` die von Qt selbst gestellten — vor allem die Beschriftungen der
// Standardknöpfe (Kontext QPlatformTheme: "OK", "Cancel", "Save"…). Ohne den zweiten
// heißt der Abbrechen-Knopf jedes AppDialog auch auf Deutsch "Cancel", egal wie
// vollständig unsere .ts sind. Die Datei liegt eingebettet vor (s. CMakeLists),
// damit sie in der gepackten App genauso da ist wie im Entwickler-Build.
void applyLanguage(QGuiApplication &app, QQmlApplicationEngine &engine,
                   QTranslator *&active, QTranslator *&activeQt, const QString &lang) {
    swapTranslator(app, active, QStringLiteral("qtmux"), lang);
    swapTranslator(app, activeQt, QStringLiteral("qtbase"), lang);
    engine.retranslate();
}

// `--install-shell-integration [ZIEL]` (QTMUX-38): schreibt die mitgelieferten Helfer an
// einen stabilen Ort und nennt die Zeile, die in einen Stop-Hook gehört.
//
// 🔑 Läuft VOR der QGuiApplication und beendet den Prozess selbst — sonst blitzte auf macOS
// kurz ein Dock-Icon auf und Qt würde eine GUI-Umgebung hochfahren, die hier niemand braucht.
// Rückgabe: Exit-Code für main().
int runInstallShellIntegration(int argc, char *argv[], const QString &target) {
#if defined(Q_OS_WIN)
    // 🔑 Der Grund, warum dieser Befehl unter Windows sonst „kaputt" wirkt: qtmux MUSS
    // WIN32_EXECUTABLE sein (eine Konsolen-App vererbt ihre Konsole an die ConPTY-
    // Kindshells → Terminal stumm). Eine GUI-App bekommt aber von sich aus keine Konsole,
    // die Ausgabe ginge also an niemanden.
    //
    // ⚠️ Nur anhängen, wenn stdout **gar kein** Handle hat. Wer den Aufruf umleitet
    // (`> datei.txt`) oder durch eine Pipe schickt (PowerShell `| Out-String`), hat ein
    // gültiges, geerbtes Handle — ein `freopen("CONOUT$")` würde es überschreiben und die
    // Ausgabe am Ziel des Aufrufers **vorbei** ins Konsolenfenster schreiben. Genau so
    // gemessen: Dateien korrekt geschrieben, Exit 0, Ausgabe spurlos verschwunden.
    const HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    const bool haveStdout = (hOut != nullptr && hOut != INVALID_HANDLE_VALUE);
    if (!haveStdout && AttachConsole(ATTACH_PARENT_PROCESS)) {
        FILE *dummy = nullptr;
        freopen_s(&dummy, "CONOUT$", "w", stdout);
        freopen_s(&dummy, "CONOUT$", "w", stderr);
    }
    if (haveStdout || GetConsoleWindow())
        SetConsoleOutputCP(CP_UTF8);   // sonst Mojibake bei Umlauten
#endif
    QCoreApplication app(argc, argv);
    QTextStream out(stdout);

    const qtmux::ShellIntegrationResult r = qtmux::ShellIntegration::install(target);
    if (!r.ok) {
        QTextStream(stderr) << QStringLiteral("QTmux: %1\n").arg(r.error);
        return 1;
    }

    out << QStringLiteral("Shell-Helfer installiert in:\n  %1\n\n").arg(
        QDir::toNativeSeparators(r.targetDir));
    if (!r.written.isEmpty())
        out << QStringLiteral("  geschrieben:  %1\n").arg(r.written.join(QLatin1String(", ")));
    if (!r.unchanged.isEmpty())
        out << QStringLiteral("  unverändert:  %1\n").arg(r.unchanged.join(QLatin1String(", ")));

    // Eine Zeile zum Kopieren statt einer Pfadsuche — das ist der eigentliche Zweck.
    out << QStringLiteral(
        "\nStop-Hook eines Agenten (z. B. in ~/.claude/settings.json), Feld \"command\":\n"
        "  %1\n"
        "\nShell-Integration (OSC 133) laden — Zeile in die Startdatei der Shell:\n"
        "  bash:       source \"%2/qtmux.bash\"\n"
        "  zsh:        source \"%2/qtmux.zsh\"\n"
        "  PowerShell: . \"%2/qtmux.ps1\"\n"
        "\nEinzelheiten: %2/README.md\n")
               .arg(qtmux::ShellIntegration::hookCommandExample(r.targetDir),
                    QDir::toNativeSeparators(r.targetDir));
    out.flush();
    return 0;
}

#if !defined(Q_OS_WIN)
// Ist das Offscreen-Plattform-Plugin vorhanden? Die Prüfung MUSS vor der QGuiApplication
// laufen: Steht QT_QPA_PLATFORM auf „offscreen" und fehlt das Plugin, beendet Qt den Prozess
// mit qFatal — unter Windows als **Absturz** (Release 0xC0000409, Debug 0x80000003), ohne
// dass ein GUI-Prozess die Meldung irgendwo zeigen könnte. Und das ist auf Windows der
// Normalfall: `windeployqt` legt nur `qwindows.dll` neben die .exe (siehe die Kopier-Regel
// in CMakeLists.txt, die das Plugin seither mitnimmt). Ein `qtmux --screenshot` aus einer
// älteren Installation darf trotzdem nicht abstürzen, darum dieser Wächter.
bool offscreenPluginAvailable(const QString &exePath) {
    const QStringList names {
#if defined(Q_OS_WIN)
        QStringLiteral("qoffscreen.dll"), QStringLiteral("qoffscreend.dll")
#elif defined(Q_OS_MACOS)
        QStringLiteral("libqoffscreen.dylib")
#else
        QStringLiteral("libqoffscreen.so")
#endif
    };
    const QDir exeDir = QFileInfo(exePath).absoluteDir();
    const QStringList dirs {
        exeDir.filePath(QStringLiteral("platforms")),                 // Windows/Linux-Deploy
        exeDir.filePath(QStringLiteral("../PlugIns/platforms")),      // macOS-App-Bundle
        QDir(QLibraryInfo::path(QLibraryInfo::PluginsPath))           // Qt-Installation
            .filePath(QStringLiteral("platforms")),
    };
    for (const QString &d : dirs)
        for (const QString &n : names)
            if (QFileInfo::exists(QDir(d).filePath(n))) return true;
    return false;
}
#endif   // !Q_OS_WIN — Windows greift immer am sichtbaren Fenster (s. main)

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
    // `--install-shell-integration [ZIEL]` (QTMUX-38) wird VOR allem anderen behandelt und
    // beendet den Prozess: kein QML, kein Fenster, keine QSettings-Domain. Eigene Schleife,
    // weil das Zielverzeichnis OPTIONAL ist — die Paar-Schleife unten sieht nur `--x <wert>`.
    for (int i = 1; i < argc; ++i) {
        if (QByteArray(argv[i]) != "--install-shell-integration") continue;
        const bool hasTarget = (i + 1 < argc) && argv[i + 1][0] != '-';
        return runInstallShellIntegration(
            argc, argv, hasTarget ? QString::fromLocal8Bit(argv[i + 1]) : QString());
    }

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
    // Grund, falls NICHT offscreen gegriffen wird (leer = offscreen). Wird nach dem Start
    // als Warnung ausgegeben, damit im Log steht, welcher Weg gelaufen ist.
    QString shotOnVisibleWindow;
    if (!shotPath.isEmpty()) {
        if (!qEnvironmentVariableIsSet("QT_QPA_PLATFORM")) {
#if defined(Q_OS_WIN)
            // Windows: BEWUSST nicht offscreen. Die Offscreen-Plattform bringt dort keine
            // Fonts mit („QFontDatabase: Cannot find font directory …; Qt no longer ships
            // fonts") und zeichnet JEDE Glyphe als leeres Kästchen — ein Bild, das genau
            // die Textprüfung unmöglich macht, für die der Screenshot gemacht wird
            // (am Bild verglichen: offscreen 11 kB Kästchen, sichtbares Fenster 40 kB
            // lesbar). Der TCC-Grund für offscreen ist ein macOS-Begriff und gilt hier nicht.
            shotOnVisibleWindow = QStringLiteral(
                "Windows: Screenshot am sichtbaren Fenster (die Offscreen-Plattform hat dort "
                "keine Fonts und zeichnet jede Glyphe als Kaestchen).");
#else
            if (offscreenPluginAvailable(QString::fromLocal8Bit(argv[0])))
                qputenv("QT_QPA_PLATFORM", "offscreen");
            else
                shotOnVisibleWindow = QStringLiteral(
                    "Offscreen-Plattform-Plugin nicht gefunden: Screenshot am sichtbaren "
                    "Fenster (kein Absturz, nur Ausweichweg).");
#endif
        }
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
    // Version kommt generiert aus `project(QTmux VERSION …)` (cmake/Version.h.in) —
    // nicht mehr als Literal, das beim Bump vergessen werden kann.
    QGuiApplication::setApplicationVersion(QStringLiteral(QTMUX_VERSION_STRING));

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

    // Einstellungen zurücksetzen / exportieren / importieren (Design 1a, Stufe 6).
    // Gleiche Brücke wie oben; die Kopfzeile des Einstellungsfensters treibt es.
    static qtmux::SettingsIo settingsIo;
    engine.rootContext()->setContextProperty(QStringLiteral("SettingsIo"), &settingsIo);

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
    static QTranslator *active = nullptr;    // eigene Texte (qtmux_<lang>)
    static QTranslator *activeQt = nullptr;  // Qt-Standardtexte (qtbase_<lang>)
    auto *appc = engine.singletonInstance<qtmux::AppController *>("QTmux", "App");
    if (appc)
        applyLanguage(app, engine, active, activeQt, appc->language());

    // Screenshot-Modus für QML sichtbar machen: dann darf aboutToQuit NICHT persistieren
    // (der stille Screenshot startet mit frischem Zustand und würde sonst beim exit() das
    // Window-Layout des Profils überschreiben, QTMUX-83 Stufe 3).
    engine.rootContext()->setContextProperty(QStringLiteral("qtmuxScreenshotMode"),
                                             !shotPath.isEmpty());

    engine.loadFromModule("QTmux", "Main");

    // Screenshot-Modus: nach kurzer Settle-Zeit die Root-QQuickWindow-Szene grabben,
    // als PNG speichern und mit dem Ergebnis-Code beenden. Der Timer läuft auf dem
    // GUI-Thread; grabWindow() rendert die (offscreen) Szene und liest sie zurück.
    if (!shotPath.isEmpty()) {
        if (!shotOnVisibleWindow.isEmpty())
            qWarning("QTmux: %s", qPrintable(shotOnVisibleWindow));
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
                             applyLanguage(app, engine, active, activeQt, lang);
                         });
    }

#if !defined(Q_OS_WIN)
    // Sauberer Quit auf SIGTERM/SIGINT (Logout, Shutdown, `kill`): async-signal-sicher über
    // ein Self-Pipe an den Event-Loop reichen → quitConfirmed setzen (die „Beenden
    // bestätigen"-Rückfrage übergehen, sonst lehnt onClosing ab) → QCoreApplication::quit()
    // → onClosing persistiert Windows/Layout/Scrollback (QTMUX-83, Stufe 3). Ohne das
    // beendete ein SIGTERM den Prozess hart und der Zustand ginge verloren.
    static int s_sigPipe[2] = {-1, -1};
    if (::pipe(s_sigPipe) == 0) {
        std::signal(SIGTERM, [](int) { char c = 1; ssize_t n = ::write(s_sigPipe[1], &c, 1); (void)n; });
        std::signal(SIGINT,  [](int) { char c = 1; ssize_t n = ::write(s_sigPipe[1], &c, 1); (void)n; });
        auto *sn = new QSocketNotifier(s_sigPipe[0], QSocketNotifier::Read, &app);
        QObject::connect(sn, &QSocketNotifier::activated, &app, [sn, &engine]() {
            char c; ssize_t n = ::read(s_sigPipe[0], &c, 1); (void)n;
            sn->setEnabled(false);
            const auto roots = engine.rootObjects();
            if (!roots.isEmpty()) roots.first()->setProperty("quitConfirmed", true);
            QCoreApplication::quit();
        });
    }
#endif

    return app.exec();
}
