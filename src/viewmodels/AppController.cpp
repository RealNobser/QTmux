#include "AppController.h"
#include "ProjectCommands.h"
#include "McpAccess.h"   // QTMUX-127: Portprobe auf derselben Adresse wie der Server
#include <QSettings>
#include <QLocale>
#include <QFontDatabase>
#include <QKeySequence>
#include <QGuiApplication>
#include <QClipboard>
#include <QCoreApplication>
#include <QDesktopServices>
#include <QDir>
#include <QUrl>
#include <QProcess>
#include <QTcpServer>
#include <QHostAddress>
#include <QtGlobal>
#include <Qt>

#if defined(Q_OS_MACOS)
#  include <CoreFoundation/CoreFoundation.h>
#elif defined(Q_OS_WIN)
#  include <windows.h>
#endif

namespace {
// System-Einstellung „Bewegung reduzieren" plattformspezifisch (einmalig beim Start).
bool systemReduceMotion() {
#if defined(Q_OS_MACOS)
    // Dieselbe CoreFoundation-C-API wie main.cpp — kein Objective-C/.mm nötig.
    bool result = false;
    CFPropertyListRef v = CFPreferencesCopyValue(
        CFSTR("reduceMotion"), CFSTR("com.apple.universalaccess"),
        kCFPreferencesCurrentUser, kCFPreferencesAnyHost);
    if (v) {
        if (CFGetTypeID(v) == CFBooleanGetTypeID())
            result = CFBooleanGetValue(static_cast<CFBooleanRef>(v));
        CFRelease(v);
    }
    return result;
#elif defined(Q_OS_WIN)
    BOOL animations = TRUE;
    if (SystemParametersInfo(SPI_GETCLIENTAREAANIMATION, 0, &animations, 0))
        return animations == FALSE;
    return false;
#else
    return false;   // Linux: keine einheitliche API — Standard „nicht reduziert".
#endif
}
} // namespace

namespace qtmux {

AppController::AppController(QObject *parent) : QObject(parent) {
    QSettings s;
    // Default: System-Sprache, sofern unterstützt, sonst Englisch.
    const QString sys = QLocale::system().name().left(2);
    const QString fallback = (sys == QLatin1String("de")) ? QStringLiteral("de") : QStringLiteral("en");
    m_language = s.value(QStringLiteral("ui/language"), fallback).toString();

    // „Bewegung reduzieren": System-Einstellung, per QTMUX_REDUCE_MOTION erzwingbar (Tests).
    if (qEnvironmentVariableIsSet("QTMUX_REDUCE_MOTION"))
        m_reduceMotion = qEnvironmentVariableIntValue("QTMUX_REDUCE_MOTION") != 0;
    else
        m_reduceMotion = systemReduceMotion();
}

void AppController::setLanguage(const QString &lang) {
    if (lang == m_language || !languageCodes().contains(lang)) return;
    m_language = lang;
    QSettings().setValue(QStringLiteral("ui/language"), lang);
    emit languageChanged(lang);
}

void AppController::reloadLanguage() {
    QSettings s;
    const QString sys = QLocale::system().name().left(2);
    const QString fallback = (sys == QLatin1String("de")) ? QStringLiteral("de") : QStringLiteral("en");
    const QString lang = s.value(QStringLiteral("ui/language"), fallback).toString();
    if (lang == m_language || !languageCodes().contains(lang)) return;
    m_language = lang;
    emit languageChanged(lang);
}

QString AppController::languageName(const QString &code) const {
    if (code == QLatin1String("de")) return QStringLiteral("Deutsch");
    if (code == QLatin1String("en")) return QStringLiteral("English");
    return code;
}

void AppController::copyToClipboard(const QString &text) const {
    if (auto *cb = QGuiApplication::clipboard()) cb->setText(text);
}

QString AppController::homeDir() const {
    // QDir::homePath() liefert Trennzeichen bereits als '/', auch unter Windows —
    // die Sidebar vergleicht deshalb gegen ein normalisiertes Verzeichnis.
    return QDir::homePath();
}

bool AppController::openLocalPath(const QString &path) const {
    if (path.isEmpty()) return false;
    return QDesktopServices::openUrl(QUrl::fromLocalFile(path));
}

QString AppController::keyChord(int key, int modifiers) const {
    // Reine Modifier-Tasten (oder leeres Event) ergeben keinen Akkord.
    switch (key) {
    case Qt::Key_Control: case Qt::Key_Shift: case Qt::Key_Alt:
    case Qt::Key_Meta:    case Qt::Key_AltGr: case 0:
        return QString();
    default: break;
    }
    // Nur die Modifier behalten, die QKeySequence kennt (Keypad/GroupSwitch raus).
    const int mods = modifiers & (Qt::ControlModifier | Qt::ShiftModifier
                                  | Qt::AltModifier | Qt::MetaModifier);
    return QKeySequence(mods | key).toString(QKeySequence::PortableText);
}

QString AppController::shortcutText(const QVariant &seq) const {
    QKeySequence ks;
    // String-Form (z. B. "Ctrl+Shift+E" aus Hotkeys.bindings).
    if (seq.typeId() == QMetaType::QString) {
        const QString s = seq.toString().trimmed();
        if (s.isEmpty()) return QString();
        ks = QKeySequence(s, QKeySequence::PortableText);
    } else if (seq.canConvert<int>()) {
        // StandardKey-Enum (z. B. ZoomIn/Copy). 0 = leer/kein Shortcut.
        const int v = seq.toInt();
        if (v == 0) return QString();
        ks = QKeySequence(static_cast<QKeySequence::StandardKey>(v));
    }
    if (ks.isEmpty()) return QString();
    // NativeText: plattformüblich (Windows/Linux "Ctrl+…", macOS ⌘/⌥/⇧-Symbole).
    return ks.toString(QKeySequence::NativeText);
}

QVariantList AppController::projectCommands(const QString &workingDir,
                                            const QString &agentId) const {
    QVariantList out;
    if (workingDir.isEmpty())
        return out;
    const auto found = ProjectCommands::filterForAgent(ProjectCommands::scan(workingDir), agentId);
    out.reserve(found.size());
    for (const ProjectCommand &c : found) {
        out.append(QVariantMap{
            {QStringLiteral("name"), c.name},
            {QStringLiteral("description"), c.description},
            {QStringLiteral("source"), c.source},
            {QStringLiteral("agentId"), c.agentId},
            {QStringLiteral("filePath"), c.filePath},
        });
    }
    return out;
}

QStringList AppController::monospaceFonts() const {
    QStringList out;
    const QStringList all = QFontDatabase::families(QFontDatabase::Latin);
    for (const QString &fam : all) {
        if (fam.startsWith(QLatin1Char('.'))) continue;   // versteckte System-Fonts
        if (QFontDatabase::isFixedPitch(fam)) out << fam;
    }
    // Standard-Familie sicher enthalten (manche melden isFixedPitch nicht zuverlässig).
    const QString def = defaultMonospaceFont();
    if (!def.isEmpty() && !out.contains(def)) out.prepend(def);
    out.removeDuplicates();
    return out;
}

QString AppController::defaultMonospaceFont() const {
    return QFontDatabase::systemFont(QFontDatabase::FixedFont).family();
}

int AppController::openNewInstance() const {
    // Freien MCP-Port suchen. Ab 7346 aufwärts: 7345 ist der Standard-Port der
    // produktiven Instanz; ein schon belegter Port (weitere Instanz) wird durch das
    // Probe-listen übersprungen. Der gefundene Port ist beim Start des Kindes zwar
    // theoretisch schon wieder frei (TOCTOU), das ist für eine manuelle Aktion aber
    // unkritisch — schlägt das Binden dennoch fehl, läuft die Instanz nur ohne MCP.
    // 🔑 Probiert wird auf DERSELBEN Adresse, an die der Server danach bindet
    // (QTMUX-127): Ein Port kann auf 127.0.0.1 frei und auf 0.0.0.0 belegt sein
    // (fremder Dienst auf einer LAN-Schnittstelle) — die Probe hätte sonst die
    // falsche Schnittstelle geprüft und einen belegten Port als frei gemeldet.
    const QHostAddress probeAddr =
        mcpaccess::resolveBind(mcpaccess::configuredBindText()).address;
    int port = -1;
    for (int p = 7346; p <= 7399; ++p) {
        QTcpServer probe;
        if (probe.listen(probeAddr, static_cast<quint16>(p))) {
            probe.close();
            port = p;
            break;
        }
    }
    if (port < 0) return -1;

    // Profil aus dem Port ableiten → stabile, getrennte QSettings-Domain je „Slot"
    // (öffnet man denselben Port erneut, findet die Instanz ihre frühere Session-Liste).
    const QString profile = QStringLiteral("i%1").arg(port);
    const QString exe = QCoreApplication::applicationFilePath();
    const QStringList args{
        QStringLiteral("--profile"),  profile,
        QStringLiteral("--mcp-port"), QString::number(port),
    };
    return QProcess::startDetached(exe, args) ? port : -1;
}

} // namespace qtmux
