#include <QCoreApplication>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QTranslator>
#include <QQmlContext>
#include <QQuickWindow>
#include <QSettings>
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

#include <cstdio>
#include <memory>

#include "qtmux_version.h"   // generiert aus cmake/Version.h.in (PROJECT_VERSION)

#include "AppController.h"
#include "SessionModel.h"
#include "AgentEventHub.h"
#include "ShellIntegration.h"
#include "ColorScheme.h"
#include "ConnectionProfile.h"
#include "HotkeyRegistry.h"
#include "PluginHost.h"
#include "SecretsVault.h"
#include "SettingsIo.h"
#include "UpdateBridge.h"
#include "UpdateQuit.h"
#include "UpdateSelfTest.h"
#include "GlobalHotkey.h"

#include <QtQml/QQmlExtensionPlugin>

#if defined(QTMUX_UPDATE_PICTURES)
#  include "UpdatePicture.h"
#  include "qtmux_buildid.h"
#  include <functional>
#  include <memory>
#endif

// Das QML-Modul „QTmux“ liegt in der statischen Bibliothek qtmux_app (CMakeLists.txt):
// Sein Plugin muss hier hereingeholt werden, sonst findet die Engine das Modul nicht.
Q_IMPORT_QML_PLUGIN(QTmuxPlugin)

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
    // Das Bildziel qtmux-bilder ist ein Konsolenprogramm ohne Sitzungen: Es behält
    // seine Konsole, damit seine Meldungen über stderr ankommen.
#  if !defined(QTMUX_UPDATE_PICTURES)
    if (GetConsoleWindow()) FreeConsole();
#  endif
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
    QString updateSelfTestDir; // --selftest-update <ordner>: Update-Weg am echten Programm
    // Nur im Bildziel qtmux-bilder (QTMUX_UPDATE_PICTURES, src/app/UpdatePicture.h):
    // Szene, Design und Sprache eines Bildes der Update-Oberfläche. In der App bleiben
    // die drei leer, und die Schalter existieren dort nicht.
    QString updatePicture;
    QString pictureTheme;
    QString pictureLanguage;
    int shotSettleMs = 700;    // --settle <ms>: Wartezeit, damit das Layout sich setzt
    // Schalter mit Wert: Fehlt der Wert oder sieht er selbst wie ein Schalter aus
    // („--selftest-update --update-base-url URL“), endet QTmux mit Exit 2, bevor etwas
    // entsteht. Sonst würde „--update-base-url“ still zum Ordnernamen.
    {
        static const char *const kValueOptions[] = {
            "--profile", "--mcp-port", "--screenshot", "--settle", "--selftest-update",
            "--update-base-url", "--update-channel",
#if defined(QTMUX_UPDATE_PICTURES)
            "--update-picture", "--picture-theme", "--picture-language",
#endif
        };
        for (int i = 1; i < argc; ++i) {
            const QByteArray a(argv[i]);
            bool takesValue = false;
            for (const char *o : kValueOptions) takesValue = takesValue || a == o;
            if (!takesValue) continue;
            if (i + 1 >= argc || QByteArray(argv[i + 1]).startsWith("--")) {
                std::fprintf(stderr, "QTmux: %s braucht einen Wert (bekam: %s).\n", argv[i],
                             i + 1 < argc ? argv[i + 1] : "nichts");
                std::fflush(stderr);
                return 2;
            }
            ++i;  // der Wert gehört zu diesem Schalter
        }
    }
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
        else if (a == "--selftest-update")
            updateSelfTestDir = QString::fromLocal8Bit(argv[i + 1]);
#if defined(QTMUX_UPDATE_PICTURES)
        else if (a == "--update-picture")
            updatePicture = QString::fromLocal8Bit(argv[i + 1]);
        else if (a == "--picture-theme")
            pictureTheme = QString::fromLocal8Bit(argv[i + 1]);
        else if (a == "--picture-language")
            pictureLanguage = QString::fromLocal8Bit(argv[i + 1]);
#endif
    }
    // --selftest-system-installer: Installationsweg von Stufe 1 für den Test des
    // Beenden-Wegs (UpdateSelfTest.h). Kein Schalter für Anwender (QTmux hat keine
    // Hilfe-Ausgabe, er steht nur in der Entwicklerdoku). Er gilt AUSSCHLIESSLICH
    // zusammen mit --selftest-update und umgestellter Quelle (dann ist jede
    // Installation ein Trockenlauf); sonst endet QTmux sofort mit Exit 6, bevor
    // irgendetwas entsteht.
    bool updateSelfTestSystemInstaller = false;
    bool updateSourceArgument = false;
    for (int i = 1; i < argc; ++i) {
        const QByteArray a(argv[i]);
        if (a == "--selftest-system-installer")
            updateSelfTestSystemInstaller = true;
        else if (a == "--update-base-url" || a == "--update-channel"
                 || a.startsWith("--update-base-url=") || a.startsWith("--update-channel="))
            updateSourceArgument = true;
    }
    if (updateSelfTestSystemInstaller && (updateSelfTestDir.isEmpty() || !updateSourceArgument)) {
        fprintf(stderr, "QTmux: --selftest-system-installer gilt nur zusammen mit "
                        "--selftest-update und --update-base-url/--update-channel (Exit 6).\n");
        std::fflush(stderr);
        return 6;
    }
#if defined(QTMUX_UPDATE_PICTURES)
    if (updatePicture.isEmpty()) {
        qCritical("qtmux-bilder: --update-picture <szene> fehlt (das Bildziel macht nur Bilder).");
        return 2;
    }
    if (updateSelfTestDir.isEmpty() || shotPath.isEmpty()) {
        qCritical("qtmux-bilder: --update-picture braucht --selftest-update <ordner> und --screenshot <png>.");
        return 2;
    }
#endif
    // Der Update-Selbsttest läuft nie im Profil des Anwenders: Er schreibt
    // Update-Einstellungen und beendet sich über den Update-Weg.
    // Auch ein leer gesetztes QTMUX_PROFILE (oder `--profile ""`) zählt als „kein Profil“.
    if (!updateSelfTestDir.isEmpty() && qEnvironmentVariable("QTMUX_PROFILE").trimmed().isEmpty())
        qputenv("QTMUX_PROFILE", "update-selftest");
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
    // Selbsttest und Bildmodus: Alle Einstellungen als INI-Datei im Selbsttest-Ordner.
    // Sie verschwinden mit ihm; unter macOS bliebe eine Profil-Domain sonst als Datei
    // zurück (cfprefsd schreibt sie nach dem Ende verzögert neu, gemessen).
    if (!updateSelfTestDir.isEmpty()) {
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope,
                           updateSelfTestDir + QStringLiteral("/settings"));
        // Ebenso der Verlauf der Sitzungen (der einzige Datenordner, den ein
        // Selbsttest-Lauf beschreibt; gemessen): unter <ordner>/data, nie unter
        // ~/Library bzw. AppData. Wächter: tst_updatequit
        // aSelfTestLeavesNothingInTheUserDataFolder.
        qtmux::SessionModel::setDataDirOverride(updateSelfTestDir + QStringLiteral("/data"));
    }
#if defined(QTMUX_UPDATE_PICTURES)
    // Bildziel: Design und Sprache ins (eigene) Profil, BEVOR Theme und AppController
    // sie lesen.
    {
        QSettings s;
        if (pictureTheme == QLatin1String("light")) s.setValue(QStringLiteral("ui/themeMode"), 1);
        if (pictureTheme == QLatin1String("dark")) s.setValue(QStringLiteral("ui/themeMode"), 2);
        if (!pictureLanguage.isEmpty()) s.setValue(QStringLiteral("ui/language"), pictureLanguage);
    }
#endif

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
    // Nicht im Selbsttest und nicht im Bildziel (beide laufen mit --selftest-update):
    // Der Wert landete sonst in der App-Domain des Bundles, die ein Profil NICHT trennt,
    // also in der des Benutzers (bzw. als qtmux-bilder.plist), gemessen 2026-11-21. Das
    // native App-Menü folgt in diesen Läufen dann der Systemsprache. Starts mit
    // --screenshot ohne Selbsttest schreiben den Wert weiterhin (alter Befund).
    if (updateSelfTestDir.isEmpty()) {
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
        // Messpunkt (tst_updatequit): Im Selbsttest darf diese Zeile nie erscheinen.
        qInfo("QTmux: AppleLanguages der Bundle-Domain gesetzt (%s).", qPrintable(lang));
    }
#endif

    QGuiApplication app(argc, argv);

    // Lebensdauer des Update-Ablaufs: so lange wie die App (Vorgabe des Hubs), nicht
    // länger. Die Halter stehen ZWISCHEN app und engine: Beim Verlassen von main baut
    // C++ zuerst die engine ab (das QML bindet an „Updates“), dann die Bridge samt
    // Ablauf, Backend, Timern und Einstellungen, dann den Szenen-Baustein des
    // Bildziels (die Bridge zeigt seinen Ablauf), zuletzt app. Vorher war die Bridge
    // ein statisches Objekt und wurde erst nach dem Ende der App abgebaut.
    // Wächter: tst_updatequit (Selbsttest meldet den Abbau bei lebender App).
#if defined(QTMUX_UPDATE_PICTURES)
    std::unique_ptr<qtmux::UpdatePicture> pictureHolder;
#endif
    std::unique_ptr<qtmux::UpdateBridge> updatesHolder;

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

    // Online-Update, Vertrag V2 (K2): der gemeinsame Ablauf des Hubs mit QTmux'
    // Verdrahtung (UpdateBridge). Gleiche Context-Property-Brücke wie oben — bewusst
    // KEIN qmlRegisterSingletonInstance in die URI „QTmux". Angelegt NACH der
    // Sprache (der Ablauf bekommt sie beim Bau) und VOR dem QML (das bindet daran).
    qtmux::UpdateBridge::Wiring updateWiring;
    updateWiring.language = appc ? appc->language() : QStringLiteral("en");
    updateWiring.requestQuit = [&engine]() { return qtmux::quitForUpdate(engine); };
    updateWiring.selfTest = !updateSelfTestDir.isEmpty();
    updateWiring.selfTestDir = updateSelfTestDir;
#if defined(QTMUX_UPDATE_PICTURES)
    // Bildziel: Die Szene des Hubs legt ihren Ablauf an, die Bridge zeigt ihn. Das
    // Bild entsteht, sobald der Ablauf dort steht (unten nach dem QML-Laden).
    static qtmux::UpdatePicture *picture = nullptr;  // gehört pictureHolder (oben)
    static bool pictureReached = false;
    static bool pictureOk = false;
    static std::function<void(bool)> pictureShoot;  // gesetzt, sobald das QML steht
    {
        const QString build = QStringLiteral(QTMUX_BUILD_ID);
        const qsizetype plus = build.indexOf(QLatin1Char('+'));
        pictureHolder = std::make_unique<qtmux::UpdatePicture>(
            updatePicture, updateWiring.language, QStringLiteral(QTMUX_VERSION_STRING),
            plus >= 0 ? build.mid(plus) : QString());
        picture = pictureHolder.get();
        const bool begun = picture->begin([](bool ok) {
            pictureReached = true;
            pictureOk = ok;
            if (pictureShoot) pictureShoot(ok);
        });
        if (!begun) return 2;
        updateWiring.externalFlow = picture->flow();
    }
#endif
    updatesHolder = std::make_unique<qtmux::UpdateBridge>(std::move(updateWiring));
    // Der Name bleibt für die Lambdas unten erreichbar (statisch gebunden, einmal je
    // Prozess); besitzen tut ihn updatesHolder.
    static qtmux::UpdateBridge &updates = *updatesHolder;
    // Im Selbsttest ist die Beenden-Rückfrage immer an: Der Test belegt, dass sie auf
    // dem Update-Weg trotzdem nicht kommt (UpdateQuit.cpp). Das Profil ist eigens.
    if (!updateSelfTestDir.isEmpty())
        QSettings().setValue(QStringLiteral("window/confirmQuit"), true);
    engine.rootContext()->setContextProperty(QStringLiteral("Updates"), &updates);

    // Screenshot-Modus für QML sichtbar machen: dann darf aboutToQuit NICHT persistieren
    // (der stille Screenshot startet mit frischem Zustand und würde sonst beim exit() das
    // Window-Layout des Profils überschreiben, QTMUX-83 Stufe 3).
    engine.rootContext()->setContextProperty(QStringLiteral("qtmuxScreenshotMode"),
                                             !shotPath.isEmpty());

    engine.loadFromModule("QTmux", "Main");
    // Messpunkt für die Start-Bestätigung (tst_updatequit): Bis hier sind
    // Hauptfenster, Sitzungen und PTYs synchron entstanden; der Ablauf startet
    // erst danach und bestätigt einen Lauf später.
    if (!updateSelfTestDir.isEmpty())
        qInfo("QTmux selftest-update: main window loaded (%d root object(s))",
              int(engine.rootObjects().size()));

    // Screenshot-Modus: nach kurzer Settle-Zeit die Root-QQuickWindow-Szene grabben,
    // als PNG speichern und mit dem Ergebnis-Code beenden. Der Timer läuft auf dem
    // GUI-Thread; grabWindow() rendert die (offscreen) Szene und liest sie zurück.
    if (!shotPath.isEmpty() && updatePicture.isEmpty()) {
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

    // Ablauf starten, sobald das Hauptfenster steht: Übernahme alter Schlüssel,
    // Auswertung einer begonnenen Installation, stille Prüfung (der Ablauf wartet
    // selbst einige Sekunden, damit sie nicht in den Aufbau der Sitzungen fällt,
    // und prüft danach alle 6 h — Vertrag V2 §4.2, E1).
    // 🔑 Im Screenshot-Modus NIE — der Lauf soll ein Bild machen und sich beenden,
    // nicht ins Netz gehen. Im Bildziel treibt der Szenen-Baustein seinen Ablauf selbst.
    if (shotPath.isEmpty()) {
        const bool selfTest = !updateSelfTestDir.isEmpty();
        QTimer::singleShot(0, &updates, [&app, selfTest]() {
            updates.start(app.arguments());
            if (updates.flow().sourceOverridden())
                qInfo("QTmux: Update-Quelle umgestellt (Selbsttest), jede Installation ist ein Trockenlauf.");
            else if (selfTest) {
                // `--selftest-update` ohne umgestellte Quelle hiesse: echte Prüfung,
                // echter Download, echte Installation. Das ist nie gemeint.
                qCritical("QTmux: --selftest-update braucht --update-base-url oder "
                          "--update-channel; Abbruch ohne Pruefung.");
                QCoreApplication::exit(2);
            }
        });
        if (!updateSelfTestDir.isEmpty())
            qtmux::driveUpdateSelfTest(updates, updateSelfTestSystemInstaller);
    }

#if defined(QTMUX_UPDATE_PICTURES)
    // Bild, sobald die Szene steht: die Fläche öffnen (Dialog über das Signal des
    // Ablaufs, Seite „Updates“ im Einstellungsfenster; Hinweis und Proxy-Anmeldung
    // erscheinen von selbst), Settle-Zeit, dann greifen. Exit 0 = Bild geschrieben,
    // 1 = Greifen/Speichern scheiterte, 2 = kein Fenster, 4 = Szene nicht erreicht,
    // 5 = Portfeld nicht gefunden.
    pictureShoot = [&engine, shotPath, shotSettleMs](bool ok) {
        // Nie im Aufruf selbst: Eine Szene kann schon in begin() fertig sein.
        QTimer::singleShot(0, qApp, [&engine, shotPath, shotSettleMs, ok]() {
            if (!ok) {
                qCritical("qtmux-bilder: Szene nicht erreicht (Frist des Szenen-Bausteins).");
                QCoreApplication::exit(4);
                return;
            }
            const QList<QObject *> roots = engine.rootObjects();
            auto *main = roots.isEmpty() ? nullptr : qobject_cast<QQuickWindow *>(roots.first());
            if (main && picture->windowWidth() > 0) main->setWidth(picture->windowWidth());
            const QString surface = picture->surface();
            if (surface == QLatin1String("dialog"))
                emit picture->flow()->dialogRequested();
            else if (surface == QLatin1String("settings"))
                emit updates.settingsPageRequested();
            QTimer::singleShot(shotSettleMs, qApp, [main, shotPath, shotSettleMs, surface]() {
                QQuickWindow *win = main;
                if (surface == QLatin1String("settings")) {
                    win = nullptr;
                    for (QWindow *w : QGuiApplication::topLevelWindows()) {
                        auto *q = qobject_cast<QQuickWindow *>(w);
                        if (q && q != main && q->isVisible()) { win = q; break; }
                    }
                }
                if (!win) {
                    qCritical("qtmux-bilder: kein Fenster fuer die Flaeche \"%s\".",
                              qPrintable(surface));
                    QCoreApplication::exit(2);
                    return;
                }
                int settle = 0;
                if (!picture->portText().isEmpty()) {
                    QObject *port = win->findChild<QObject *>(QStringLiteral("updatesProxyPort"));
                    if (!port) {
                        qCritical("qtmux-bilder: Portfeld (updatesProxyPort) nicht gefunden.");
                        QCoreApplication::exit(5);
                        return;
                    }
                    port->setProperty("text", picture->portText());
                    settle = shotSettleMs;
                }
                QTimer::singleShot(settle, qApp, [win, shotPath]() {
                    const QImage img = win->grabWindow();
                    const int rc = (!img.isNull() && img.save(shotPath, "PNG")) ? 0 : 1;
                    qInfo("qtmux-bilder: %s rc=%d", qPrintable(shotPath), rc);
                    QCoreApplication::exit(rc);
                });
            });
        });
    };
    if (pictureReached) pictureShoot(pictureOk);
#endif

    // Laufzeitwechsel über das Sprachmenü neu laden.
    if (appc) {
        QObject::connect(appc, &qtmux::AppController::languageChanged, &app,
                         [&app, &engine](const QString &lang) {
                             // Erst den Hub-Katalog tauschen, dann neu übersetzen:
                             // retranslate() wertet die Bindungen mit dem dann
                             // geladenen Katalog aus.
                             updates.setLanguage(lang);
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
