#include "update/UpdateTexts.hpp"

#include <QCoreApplication>
#include <QDateTime>
#include <QPointer>
#include <QTranslator>

#include <cmath>
#include <cstring>

namespace appupdate {

namespace {

struct Entry {
    const char* source;   // English, as in the design
    const char* key;      // design key, used as disambiguation
};

// The whole catalogue. QT_TRANSLATE_NOOP3 makes lupdate extract each source
// with its key; translate() below asks with exactly these strings.
constexpr Entry kTable[] = {
    // Allgemein
    QT_TRANSLATE_NOOP3("UpdateTexts", "Software Update – {product}", "win.title"),
    QT_TRANSLATE_NOOP3("UpdateTexts", "Close", "btn.close"),
    QT_TRANSLATE_NOOP3("UpdateTexts", "Later", "btn.later"),
    QT_TRANSLATE_NOOP3("UpdateTexts", "Details", "details"),
    QT_TRANSLATE_NOOP3("UpdateTexts", "Copy", "details.copy"),
    QT_TRANSLATE_NOOP3("UpdateTexts", "Copied", "details.copied"),
    QT_TRANSLATE_NOOP3("UpdateTexts", "What’s new in {neu}", "cl.head"),
    QT_TRANSLATE_NOOP3("UpdateTexts", "{datum} · {groesse}", "cl.meta"),
    QT_TRANSLATE_NOOP3("UpdateTexts", "New", "cl.new"),
    QT_TRANSLATE_NOOP3("UpdateTexts", "Improved", "cl.improved"),
    QT_TRANSLATE_NOOP3("UpdateTexts", "Fixed", "cl.fixed"),
    QT_TRANSLATE_NOOP3("UpdateTexts", "There are no release notes for this version.", "cl.empty"),
    // 1 · Prüfen
    QT_TRANSLATE_NOOP3("UpdateTexts", "Checking for updates…", "s1.title"),
    QT_TRANSLATE_NOOP3("UpdateTexts", "{product} is asking the update server for a new version.", "s1.lead"),
    QT_TRANSLATE_NOOP3("UpdateTexts", "This can take a little longer through a proxy.", "s1.slow"),
    QT_TRANSLATE_NOOP3("UpdateTexts", "Cancel", "s1.cancel"),
    // 2 · Aktuell
    QT_TRANSLATE_NOOP3("UpdateTexts", "{product} is up to date", "s2.title"),
    QT_TRANSLATE_NOOP3("UpdateTexts", "You’re using the latest version, {alt}.", "s2.lead"),
    QT_TRANSLATE_NOOP3("UpdateTexts", "What’s new in {alt}", "s2.notes"),
    // 3 · Update verfügbar
    QT_TRANSLATE_NOOP3("UpdateTexts", "Update available", "s3.title"),
    QT_TRANSLATE_NOOP3("UpdateTexts", "{product} {neu} is available. You have {alt}.", "s3.lead"),
    QT_TRANSLATE_NOOP3("UpdateTexts", "Skip this version", "s3.skip"),
    QT_TRANSLATE_NOOP3("UpdateTexts", "Download", "s3.download"),
    // 4 · Download läuft
    QT_TRANSLATE_NOOP3("UpdateTexts", "Downloading update", "s4.title"),
    QT_TRANSLATE_NOOP3("UpdateTexts", "{geladen} of {groesse}", "s4.amount"),
    QT_TRANSLATE_NOOP3("UpdateTexts", "{prozent}%", "s4.percent"),
    QT_TRANSLATE_NOOP3("UpdateTexts", "Verifying…", "s4.verify"),
    QT_TRANSLATE_NOOP3("UpdateTexts", "You can close this window. The download continues, and the notice in the main window shows its progress.", "s4.hint"),
    QT_TRANSLATE_NOOP3("UpdateTexts", "Cancel download", "s4.cancel"),
    // 5 · Bereit
    QT_TRANSLATE_NOOP3("UpdateTexts", "Ready to install", "s5.title"),
    QT_TRANSLATE_NOOP3("UpdateTexts", "{product} {neu} has been downloaded and verified.", "s5.lead"),
    QT_TRANSLATE_NOOP3("UpdateTexts", "{product} will quit, install {neu} and restart.", "s5.mac"),
    QT_TRANSLATE_NOOP3("UpdateTexts", "{product} will quit and start the installer. Windows will ask for administrator permission.", "s5.win"),
    QT_TRANSLATE_NOOP3("UpdateTexts", "{n} sessions are running. They will be closed when {product} quits.", "s5.qtmux.n"),
    QT_TRANSLATE_NOOP3("UpdateTexts", "1 session is running. It will be closed when {product} quits.", "s5.qtmux.1"),
    QT_TRANSLATE_NOOP3("UpdateTexts", "If you have unsaved work, {product} will ask before quitting.", "s5.work"),
    QT_TRANSLATE_NOOP3("UpdateTexts", "The bus connection will be closed.", "s5.bus"),
    QT_TRANSLATE_NOOP3("UpdateTexts", "Quit and update", "s5.go"),
    // 6 · Nach dem Neustart
    QT_TRANSLATE_NOOP3("UpdateTexts", "Updated to {neu}", "s6.title"),
    QT_TRANSLATE_NOOP3("UpdateTexts", "{product} has been updated. Previously installed: {alt}.", "s6.lead"),
    // Hinweis im Hauptfenster
    QT_TRANSLATE_NOOP3("UpdateTexts", "Update {neu} available", "n.avail"),
    QT_TRANSLATE_NOOP3("UpdateTexts", "Downloading update · {prozent}%", "n.loading"),
    QT_TRANSLATE_NOOP3("UpdateTexts", "Update {neu} ready", "n.ready"),
    QT_TRANSLATE_NOOP3("UpdateTexts", "Updated to {neu}", "n.done"),
    QT_TRANSLATE_NOOP3("UpdateTexts", "Update failed to install", "n.failed"),
    QT_TRANSLATE_NOOP3("UpdateTexts", "Dismiss", "n.dismiss"),
    QT_TRANSLATE_NOOP3("UpdateTexts", "View update", "n.tip.avail"),
    // 7 · Ältere Version angeboten
    QT_TRANSLATE_NOOP3("UpdateTexts", "Older version offered", "s7.title"),
    QT_TRANSLATE_NOOP3("UpdateTexts", "The update server is offering {product} {neu}. You have the newer version {alt}.", "s7.lead"),
    QT_TRANSLATE_NOOP3("UpdateTexts", "Released {datum} · {groesse}", "s7.meta"),
    QT_TRANSLATE_NOOP3("UpdateTexts", "This takes you back to an older version. {neu} may not be able to read files and settings saved by {alt}. Only download it if you mean to go back.", "s7.warn"),
    QT_TRANSLATE_NOOP3("UpdateTexts", "Download anyway", "s7.anyway"),
    QT_TRANSLATE_NOOP3("UpdateTexts", "Older version ready", "s7.ready"),
    QT_TRANSLATE_NOOP3("UpdateTexts", "Quit and downgrade", "s7.go"),
    // 8 · Kein Paket
    QT_TRANSLATE_NOOP3("UpdateTexts", "No package for this system", "s8.title"),
    QT_TRANSLATE_NOOP3("UpdateTexts", "{product} {neu} has been released, but not yet for {system}. You’ll keep using {alt}.", "s8.lead"),
    QT_TRANSLATE_NOOP3("UpdateTexts", "As soon as a package is available, {product} will let you know at startup.", "s8.next"),
    // 9 · Noch nichts veröffentlicht
    QT_TRANSLATE_NOOP3("UpdateTexts", "No release published yet", "s9.title"),
    QT_TRANSLATE_NOOP3("UpdateTexts", "There’s no release of {product} on the update server yet. This isn’t an error. You’re using {alt}.", "s9.lead"),
    // 10 · Prüfung fehlgeschlagen
    QT_TRANSLATE_NOOP3("UpdateTexts", "Couldn’t check for updates", "s10.title"),
    QT_TRANSLATE_NOOP3("UpdateTexts", "{product} isn’t connected to a network. Check your connection and try again.", "s10.offline"),
    QT_TRANSLATE_NOOP3("UpdateTexts", "The update server can’t be reached. Check your network connection and proxy settings, then try again.", "s10.unreachable"),
    QT_TRANSLATE_NOOP3("UpdateTexts", "The update server’s response couldn’t be read. This is usually caused by the server or a proxy in between. Try again later.", "s10.garbled"),
    QT_TRANSLATE_NOOP3("UpdateTexts", "Proxy sign-in was cancelled. Without signing in, {product} can’t reach the update server.", "s10.auth"),
    QT_TRANSLATE_NOOP3("UpdateTexts", "Proxy settings", "s10.settings"),
    QT_TRANSLATE_NOOP3("UpdateTexts", "Check again", "s10.retry"),
    // 11 · Update abgelehnt
    QT_TRANSLATE_NOOP3("UpdateTexts", "Update rejected for security reasons", "s11.title"),
    QT_TRANSLATE_NOOP3("UpdateTexts", "{product} received an update offer that failed its security check and didn’t use it. Your installation is unchanged.", "s11.lead"),
    QT_TRANSLATE_NOOP3("UpdateTexts", "The offer’s signature is invalid.", "s11.sig"),
    QT_TRANSLATE_NOOP3("UpdateTexts", "The offer was meant for a different product.", "s11.product"),
    QT_TRANSLATE_NOOP3("UpdateTexts", "The offer is older than one {product} has already received.", "s11.replay"),
    QT_TRANSLATE_NOOP3("UpdateTexts", "The file was larger than announced. The download was stopped and deleted.", "s11.size"),
    QT_TRANSLATE_NOOP3("UpdateTexts", "You don’t need to do anything. Try again later. If this keeps happening, contact support.", "s11.calm"),
    QT_TRANSLATE_NOOP3("UpdateTexts", "Check again", "s11.retry"),
    // 12 · Download fehlgeschlagen
    QT_TRANSLATE_NOOP3("UpdateTexts", "Download failed", "s12.title"),
    QT_TRANSLATE_NOOP3("UpdateTexts", "The connection dropped at {geladen} of {groesse}. Check your network connection and download again.", "s12.conn"),
    QT_TRANSLATE_NOOP3("UpdateTexts", "The downloaded file is damaged and has been deleted. Download it again.", "s12.corrupt"),
    QT_TRANSLATE_NOOP3("UpdateTexts", "Download again", "s12.retry"),
    // 13 · Installation fehlgeschlagen
    QT_TRANSLATE_NOOP3("UpdateTexts", "Installation failed", "s13.title"),
    QT_TRANSLATE_NOOP3("UpdateTexts", "{product} couldn’t replace itself with {neu} and is still running version {alt}. Your data isn’t affected.", "s13.lead"),
    QT_TRANSLATE_NOOP3("UpdateTexts", "Try again. If it fails again, install the update manually.", "s13.next"),
    QT_TRANSLATE_NOOP3("UpdateTexts", "{product} will quit again to do this.", "s13.foot"),
    QT_TRANSLATE_NOOP3("UpdateTexts", "Install manually", "s13.manual"),
    QT_TRANSLATE_NOOP3("UpdateTexts", "Quit and try again", "s13.go"),
    // 14 · Von Hand installieren
    QT_TRANSLATE_NOOP3("UpdateTexts", "Please install manually", "s14.title"),
    QT_TRANSLATE_NOOP3("UpdateTexts", "{product} isn’t allowed to change its own program folder. The verified file for {neu} is ready.", "s14.lead"),
    QT_TRANSLATE_NOOP3("UpdateTexts", "Quit {product}.", "s14.step1"),
    QT_TRANSLATE_NOOP3("UpdateTexts", "Open the file and drag {product} to the Applications folder, replacing the old version.", "s14.mac2"),
    QT_TRANSLATE_NOOP3("UpdateTexts", "Open the file. Windows will ask for administrator permission.", "s14.win2"),
    QT_TRANSLATE_NOOP3("UpdateTexts", "Replace the current program file with the new one.", "s14.lin2"),
    QT_TRANSLATE_NOOP3("UpdateTexts", "Show in folder", "s14.show"),
    // 15 · Proxy-Anmeldung
    QT_TRANSLATE_NOOP3("UpdateTexts", "Proxy sign-in", "s15.title"),
    QT_TRANSLATE_NOOP3("UpdateTexts", "The proxy {proxy} requires you to sign in so {product} can check for updates.", "s15.lead"),
    QT_TRANSLATE_NOOP3("UpdateTexts", "User name", "s15.user"),
    QT_TRANSLATE_NOOP3("UpdateTexts", "Password", "s15.pass"),
    QT_TRANSLATE_NOOP3("UpdateTexts", "Remember for this session", "s15.remember"),
    QT_TRANSLATE_NOOP3("UpdateTexts", "The password is never saved. At most, it’s remembered until {product} quits.", "s15.never"),
    QT_TRANSLATE_NOOP3("UpdateTexts", "Sign-in was rejected. Check your user name and password. {product} sends each entry only once so your account doesn’t get locked.", "s15.rejected"),
    QT_TRANSLATE_NOOP3("UpdateTexts", "Sign in", "s15.ok"),
    QT_TRANSLATE_NOOP3("UpdateTexts", "Cancel", "s15.cancel"),
    // Einstellungen › Updates
    QT_TRANSLATE_NOOP3("UpdateTexts", "Updates", "set.section"),
    QT_TRANSLATE_NOOP3("UpdateTexts", "Check for updates automatically at startup", "set.auto"),
    QT_TRANSLATE_NOOP3("UpdateTexts", "The check runs quietly. If {product} finds an update, a notice appears in the main window; no window opens.", "set.auto.desc"),
    QT_TRANSLATE_NOOP3("UpdateTexts", "Installed version", "set.version"),
    QT_TRANSLATE_NOOP3("UpdateTexts", "Check now", "set.check"),
    QT_TRANSLATE_NOOP3("UpdateTexts", "Skipped version", "set.skipped"),
    QT_TRANSLATE_NOOP3("UpdateTexts", "{neu} will no longer be offered.", "set.skipped.desc"),
    QT_TRANSLATE_NOOP3("UpdateTexts", "Reset", "set.reset"),
    QT_TRANSLATE_NOOP3("UpdateTexts", "Network proxy", "set.proxy.group"),
    QT_TRANSLATE_NOOP3("UpdateTexts", "Proxy", "set.proxy"),
    QT_TRANSLATE_NOOP3("UpdateTexts", "System", "set.proxy.system"),
    QT_TRANSLATE_NOOP3("UpdateTexts", "Direct", "set.proxy.direct"),
    QT_TRANSLATE_NOOP3("UpdateTexts", "Manual", "set.proxy.manual"),
    QT_TRANSLATE_NOOP3("UpdateTexts", "Uses the operating system’s setting. This is usually right on a company network.", "set.proxy.system.desc"),
    QT_TRANSLATE_NOOP3("UpdateTexts", "Connects without a proxy, even if the operating system specifies one.", "set.proxy.direct.desc"),
    QT_TRANSLATE_NOOP3("UpdateTexts", "Uses the details below.", "set.proxy.manual.desc"),
    QT_TRANSLATE_NOOP3("UpdateTexts", "Type", "set.kind"),
    QT_TRANSLATE_NOOP3("UpdateTexts", "Server and port", "set.server"),
    QT_TRANSLATE_NOOP3("UpdateTexts", "User name", "set.user"),
    QT_TRANSLATE_NOOP3("UpdateTexts", "Optional. {product} asks for the password when needed.", "set.user.desc"),
    QT_TRANSLATE_NOOP3("UpdateTexts", "Discard credentials for this session", "set.forget"),
    QT_TRANSLATE_NOOP3("UpdateTexts", "A password is remembered until {product} quits. It is never saved.", "set.forget.some"),
    QT_TRANSLATE_NOOP3("UpdateTexts", "No password is remembered right now. It is never saved.", "set.forget.none"),
    QT_TRANSLATE_NOOP3("UpdateTexts", "Discard", "set.forget.btn"),

    // ---- NOT FROM THE DESIGN (working state, review pending) -----
    // Every "x." key below is written in the style of the design's
    // catalogue and still awaits review.
    // macOS label of the primary button in 14 (the design note on s14.show).
    QT_TRANSLATE_NOOP3("UpdateTexts", "Show in Finder", "x.s14.show.mac"),
    // 14 for the reasons the design does not draw (it draws only "folder not
    // writable", s14.lead).
    QT_TRANSLATE_NOOP3("UpdateTexts", "{product} is running straight from the disk image and can’t replace itself there. The verified file for {neu} is ready.", "x.s14.dmg"),
    QT_TRANSLATE_NOOP3("UpdateTexts", "macOS has moved {product} to a protected location, so it can’t replace itself. The verified file for {neu} is ready.", "x.s14.translocated"),
    QT_TRANSLATE_NOOP3("UpdateTexts", "This copy of {product} doesn’t carry the expected signature, so it can’t check the new version and won’t replace itself. The verified file for {neu} is ready.", "x.s14.unsigned"),
    QT_TRANSLATE_NOOP3("UpdateTexts", "{product} isn’t running from an AppImage, so it can’t replace itself. The verified file for {neu} is ready.", "x.s14.noappimage"),
    QT_TRANSLATE_NOOP3("UpdateTexts", "{product} can’t install this update by itself. The verified file for {neu} is ready.", "x.s14.other"),
    // 14 for the further reasons of the install seam.
    QT_TRANSLATE_NOOP3("UpdateTexts", "macOS has marked the downloaded file as coming from the internet, and {product} never removes that mark itself. The verified file for {neu} is ready.", "x.s14.quarantined"),
    QT_TRANSLATE_NOOP3("UpdateTexts", "This copy of {product} wasn’t set up with the installer, so it can’t update itself. The verified file for {neu} is ready.", "x.s14.notinstalled"),
    QT_TRANSLATE_NOOP3("UpdateTexts", "Another copy of {product} is running from the same folder, so it can’t replace itself now. The verified file for {neu} is ready.", "x.s14.otherinstance"),
    QT_TRANSLATE_NOOP3("UpdateTexts", "The update helper of {product} can’t be started on this computer. The verified file for {neu} is ready.", "x.s14.helper"),
    QT_TRANSLATE_NOOP3("UpdateTexts", "macOS didn’t allow {product} to replace itself. You can allow it under System Settings › Privacy & Security › App Management, or install the update manually. The verified file for {neu} is ready.", "x.s14.appmanagement"),
    // 14 for the reasons added later: shared location, file system, and the
    // plain "ready" lead where by hand is simply the normal way.
    QT_TRANSLATE_NOOP3("UpdateTexts", "{product} is in a folder that other users can also write to. Move {product} to the Applications folder, then it can update itself in future. The verified file for {neu} is ready.", "x.s14.shared"),
    QT_TRANSLATE_NOOP3("UpdateTexts", "The drive {product} is on doesn’t support replacing it while it runs. The verified file for {neu} is ready.", "x.s14.filesystem"),
    QT_TRANSLATE_NOOP3("UpdateTexts", "The verified file for {neu} is ready.", "x.s14.ready"),
    // 10 when the manual proxy has no host or no port: no connection is tried.
    QT_TRANSLATE_NOOP3("UpdateTexts", "The manual proxy setting is incomplete: a host and a port are needed. Complete it in the proxy settings, then try again.", "x.s10.proxy"),
    // 5 while prepare() runs (asynchronous since seam revision 2).
    QT_TRANSLATE_NOOP3("UpdateTexts", "Preparing…", "x.s5.preparing"),
    // 13 when the user declined the installation (result=cancelled).
    QT_TRANSLATE_NOOP3("UpdateTexts", "Installation cancelled", "x.s13c.title"),
    QT_TRANSLATE_NOOP3("UpdateTexts", "The installation of {neu} was cancelled, so {product} is still running version {alt}. Nothing has been changed.", "x.s13c.lead"),
    // macOS remedy, only for a file measured as quarantined (Quarantine.hpp rule).
    QT_TRANSLATE_NOOP3("UpdateTexts", "If “{product}” can’t be opened, click “Open Anyway” under System Settings › Privacy & Security.", "x.s14.quarantine"),
    // Footnote line 1 in 5 while the system installer does the work (stage 1,
    // before self-replacement): the design's s5.mac promises a restart that
    // only stage 2 delivers.
    QT_TRANSLATE_NOOP3("UpdateTexts", "{product} will quit and open the disk image. Then drag {product} to the Applications folder.", "x.s5.mac.dmg"),
    QT_TRANSLATE_NOOP3("UpdateTexts", "{product} will quit and replace its program file. Start {product} again afterwards.", "x.s5.lin.appimage"),
    // 12 for causes the design does not draw.
    QT_TRANSLATE_NOOP3("UpdateTexts", "The file couldn’t be saved. Check the free disk space and download again.", "x.s12.file"),
    QT_TRANSLATE_NOOP3("UpdateTexts", "The update server didn’t deliver the file. Try again later.", "x.s12.server"),
    // Loud notice for an unreadable stored serial.
    QT_TRANSLATE_NOOP3("UpdateTexts", "The stored update serial number can’t be read. {product} keeps checking offers but can’t reliably recognise an older one.", "x.serial"),
    // Systems in words (the design, s8.lead).
    QT_TRANSLATE_NOOP3("UpdateTexts", "macOS", "x.sys.macos"),
    QT_TRANSLATE_NOOP3("UpdateTexts", "Windows (x86, 64 Bit)", "x.sys.win.x64"),
    QT_TRANSLATE_NOOP3("UpdateTexts", "Windows on ARM", "x.sys.win.arm"),
    QT_TRANSLATE_NOOP3("UpdateTexts", "Linux (x86, 64 Bit)", "x.sys.linux.x64"),
    QT_TRANSLATE_NOOP3("UpdateTexts", "Linux on ARM", "x.sys.linux.arm"),
    QT_TRANSLATE_NOOP3("UpdateTexts", "this system", "x.sys.other"),
};

const Entry* find(const char* key)
{
    for (const Entry& e : kTable) {
        if (std::strcmp(e.key, key) == 0) return &e;
    }
    return nullptr;
}

QString fill(QString text, const QHash<QString, QString>& args)
{
    for (auto it = args.constBegin(); it != args.constEnd(); ++it) {
        text.replace(QLatin1Char('{') + it.key() + QLatin1Char('}'), it.value());
    }
    return text;
}

QPointer<QTranslator> g_hubTranslator;
QString g_hubLanguage;

}  // namespace

bool installUpdateTranslator(const QString& language)
{
    QCoreApplication* app = QCoreApplication::instance();
    const QString lang = language.left(2).toLower();
    if (g_hubTranslator && g_hubLanguage == lang) return true;
    if (g_hubTranslator) {
        if (app) QCoreApplication::removeTranslator(g_hubTranslator);
        delete g_hubTranslator;
    }
    g_hubLanguage.clear();
    if (lang.isEmpty() || lang == QLatin1String("en")) {
        g_hubLanguage = lang;
        return true;
    }
    auto* t = new QTranslator(app);
    if (!t->load(QStringLiteral(":/appupdate/i18n/appupdate_") + lang + QStringLiteral(".qm"))) {
        delete t;
        return false;
    }
    QCoreApplication::installTranslator(t);
    g_hubTranslator = t;
    g_hubLanguage = lang;
    return true;
}

UpdateTexts::UpdateTexts(QString productName, QString language)
    : m_product(std::move(productName)),
      m_language(language.left(2).toLower()),
      m_locale(m_language.isEmpty() ? QLocale(QLocale::English) : QLocale(m_language))
{
}

bool UpdateTexts::hasKey(const char* key) { return find(key) != nullptr; }

QStringList UpdateTexts::keys()
{
    QStringList out;
    for (const Entry& e : kTable) out << QString::fromLatin1(e.key);
    return out;
}

QString UpdateTexts::get(const char* key, const QHash<QString, QString>& args) const
{
    const Entry* e = find(key);
    if (!e) return QLatin1Char('[') + QString::fromLatin1(key) + QLatin1Char(']');
    QString text = QCoreApplication::translate("UpdateTexts", e->source, e->key);
    text.replace(QLatin1String("{product}"), m_product);
    return fill(std::move(text), args);
}

QString UpdateTexts::size(qint64 bytes) const
{
    return m_locale.formattedDataSize(bytes, 1, QLocale::DataSizeSIFormat);
}

QString UpdateTexts::amountInUnitOf(qint64 bytes, qint64 totalBytes) const
{
    // The unit formattedDataSize picks for the total: 1000^k with k the
    // largest power below the total (SI).
    double unit = 1.0;
    int steps = 0;
    while (totalBytes >= unit * 1000.0 && steps < 6) {
        unit *= 1000.0;
        ++steps;
    }
    const double v = static_cast<double>(bytes) / unit;
    return steps == 0 ? m_locale.toString(bytes) : m_locale.toString(v, 'f', 1);
}

QString UpdateTexts::longDate(const QDate& date) const
{
    if (!date.isValid()) return QString();
    if (m_language == QLatin1String("de")) return m_locale.toString(date, QStringLiteral("d. MMMM yyyy"));
    if (m_language == QLatin1String("sv")) return m_locale.toString(date, QStringLiteral("d MMMM yyyy"));
    return m_locale.toString(date, QStringLiteral("MMMM d, yyyy"));
}

QString UpdateTexts::longDateFromIso(const QString& iso) const
{
    QDate d = QDate::fromString(iso.left(10), Qt::ISODate);
    if (!d.isValid()) d = QDateTime::fromString(iso, Qt::ISODate).date();
    return longDate(d);
}

QString UpdateTexts::percent(int value) const
{
    return get("s4.percent", {{QStringLiteral("prozent"), m_locale.toString(value)}});
}

QString UpdateTexts::systemName(const QString& artifactKey) const
{
    const QString k = artifactKey.toLower();
    if (k.startsWith(QLatin1String("macos"))) return get("x.sys.macos");
    if (k.startsWith(QLatin1String("win"))) {
        return get(k.contains(QLatin1String("arm")) ? "x.sys.win.arm" : "x.sys.win.x64");
    }
    if (k.startsWith(QLatin1String("linux"))) {
        return get(k.contains(QLatin1String("arm")) || k.contains(QLatin1String("aarch"))
                       ? "x.sys.linux.arm" : "x.sys.linux.x64");
    }
    return get("x.sys.other");
}

}  // namespace appupdate
