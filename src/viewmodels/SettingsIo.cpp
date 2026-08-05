#include "SettingsIo.h"

#include "ColorScheme.h"
#include "ConnectionProfile.h"
#include "HotkeyRegistry.h"

#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QSaveFile>
#include <QSettings>

#include <limits>

namespace qtmux {

namespace {

// Kennung im Dateikopf: verhindert, dass eine beliebige JSON-Datei als Einstellungen
// eingelesen wird (und benennt beim Import gleich die Herkunftsversion).
constexpr auto kMarker = "qtmux-settings";
constexpr int kFormat = 1;

QString pathOf(const QUrl &url) {
    return url.isLocalFile() ? url.toLocalFile() : url.toString();
}

// Vergleich zweier Werte über ihre Textform. QSettings liefert je Backend
// unterschiedliche Typen für denselben Wert (INI-Format gibt Zahlen als QString
// zurück, die macOS-plist und die Windows-Registry als int) — ein `==` auf QVariant
// würde damit Änderungen melden, wo keine sind.
bool sameValue(const QVariant &a, const QVariant &b) {
    if (!a.isValid() || !b.isValid()) return a.isValid() == b.isValid();
    if (a.typeId() == QMetaType::QStringList || b.typeId() == QMetaType::QStringList
        || a.typeId() == QMetaType::QVariantList || b.typeId() == QMetaType::QVariantList)
        return a.toStringList() == b.toStringList();
    return a.toString() == b.toString();
}

// JSON kennt nur Zahl/Text/Wahrheitswert/Liste. Ganzzahlige Zahlen wieder als int
// ablegen, damit ein exportierter und reimportierter Wert denselben Typ hat wie der
// ursprünglich geschriebene (sonst stünde nach dem Import 13.0 in den Einstellungen).
QVariant fromJson(const QJsonValue &v) {
    switch (v.type()) {
    case QJsonValue::Bool:
        return v.toBool();
    case QJsonValue::Double: {
        const double d = v.toDouble();
        const qlonglong l = static_cast<qlonglong>(d);
        if (static_cast<double>(l) == d
            && l >= std::numeric_limits<int>::min() && l <= std::numeric_limits<int>::max())
            return static_cast<int>(l);
        return d;
    }
    case QJsonValue::String:
        return v.toString();
    case QJsonValue::Array: {
        const QJsonArray arr = v.toArray();
        QStringList out;
        for (const QJsonValue &e : arr) out << e.toString();
        return out;
    }
    default:
        return {};
    }
}

// Anzeigetext eines Werts für die Import-Vorschau. `QVariant::toStringList()` liefert
// für einen int eine LEERE Liste (nicht konvertierbar) — ein blindes join() zeigte
// darum bei jeder Zahl nichts an.
QString displayValue(const QVariant &v) {
    if (!v.isValid()) return {};
    if (v.typeId() == QMetaType::QStringList || v.typeId() == QMetaType::QVariantList)
        return v.toStringList().join(QStringLiteral(", "));
    return v.toString();
}

} // namespace

SettingsIo::SettingsIo(QObject *parent) : QObject(parent) {}

// --- Schlüsselmodell --------------------------------------------------------
QStringList SettingsIo::categories() {
    return { QStringLiteral("allgemein"),   QStringLiteral("erscheinungsbild"),
             QStringLiteral("terminal"),    QStringLiteral("eingabe"),
             QStringLiteral("agenten"),     QStringLiteral("hotkeys"),
             QStringLiteral("verbindungen"),QStringLiteral("vault"),
             QStringLiteral("erweiterungen") };
}

QStringList SettingsIo::patternsFor(const QString &category) {
    if (category == QLatin1String("allgemein"))
        return { QStringLiteral("ui/language"),
                 QStringLiteral("ui/themeMode"),
                 QStringLiteral("window/confirmQuit"),
                 QStringLiteral("window/restoreSessionMode"),
                 QStringLiteral("window/quakeMode"),
                 QStringLiteral("window/preventSleep"),
                 // Online-Update (QTMUX-125). `update/lastCheck` ist bewusst NICHT
                 // dabei: das ist Laufzeitzustand (wann zuletzt geprüft wurde), keine
                 // Einstellung — exportiert und woanders importiert würde er die
                 // Tagesdrosselung einer fremden Maschine erben.
                 QStringLiteral("update/autoCheck"),
                 QStringLiteral("update/skippedVersion"),
                 QStringLiteral("update/baseUrl"),
                 // Proxy (QTMUX-129). Die Felder decken sich mit
                 // `appupdate::ProxyConfig` — Modus, Typ, Host, Port.
                 QStringLiteral("update/proxyMode"),
                 QStringLiteral("update/proxyType"),
                 QStringLiteral("update/proxyHost"),
                 QStringLiteral("update/proxyPort") };
                 // 🔑 `update/proxyUser` steht BEWUSST NICHT hier. Er wird zwar
                 // in QSettings gehalten (Komfort: `DOMÄNE\name` nicht bei jedem
                 // Start neu tippen), gehört aber nicht in eine Exportdatei —
                 // die ist zum Weitergeben gedacht, und ein Domänen-Benutzername
                 // ist Personenbezug. Ein PASSWORT existiert als Schlüssel gar
                 // nicht: es lebt nur im Sitzungsspeicher (ProxyCredentials.h).
    if (category == QLatin1String("erscheinungsbild"))
        return { QStringLiteral("colorSchemes/") };
    if (category == QLatin1String("terminal"))
        return { QStringLiteral("window/terminalFontFamily"),
                 QStringLiteral("window/terminalFontSize"),
                 QStringLiteral("window/terminalLigatures"),
                 QStringLiteral("window/terminalGpuRendering"),
                 QStringLiteral("window/defaultShellProgram") };
    if (category == QLatin1String("eingabe"))
        return { QStringLiteral("window/copyOnSelect"),
                 QStringLiteral("window/rightClickPaste"),
                 QStringLiteral("window/pasteWarnMultiline"),
                 QStringLiteral("window/altScrollMode") };
    if (category == QLatin1String("agenten"))
        return { QStringLiteral("window/restoreAgents"),
                 QStringLiteral("window/resumeAgentMode"),
                 QStringLiteral("mcp/port"),
                 // QTMUX-127: Die Bind-Adresse ist eine Einstellung und darf mit —
                 // `mcp/token` NICHT: Ein Export ist eine Datei zum Weitergeben, und
                 // ein Reset dieser Seite darf ein Geheimnis nicht stillschweigend
                 // entfernen (der Server bliebe sonst beim nächsten Start aus).
                 QStringLiteral("mcp/bindAddress") };
    if (category == QLatin1String("hotkeys"))
        return { QStringLiteral("hotkeys/") };
    if (category == QLatin1String("verbindungen"))
        return { QStringLiteral("profiles/") };
    // `vault`: liegt als verschlüsselte Datei außerhalb von QSettings. Ein
    // „Zurücksetzen" würde hier Geheimnisse löschen — das ist kein Zurücksetzen einer
    // Einstellung, sondern Datenverlust, und wird darum nicht angeboten.
    // `erweiterungen`: Plugins werden gefunden, nicht konfiguriert.
    return {};
}

bool SettingsIo::matchesCategory(const QString &key, const QString &category) {
    const QStringList pats = patternsFor(category);
    for (const QString &p : pats) {
        if (p.endsWith(QLatin1Char('/'))) {
            if (key.startsWith(p)) return true;
        } else if (key == p) {
            return true;
        }
    }
    return false;
}

QString SettingsIo::categoryOf(const QString &key) {
    const QStringList cats = categories();
    for (const QString &c : cats)
        if (matchesCategory(key, c)) return c;
    return {};
}

// --- Zugriff ----------------------------------------------------------------
QStringList SettingsIo::exportableKeys() const {
    QSettings s;
    QStringList out;
    const QStringList all = s.allKeys();
    for (const QString &k : all)
        if (isExportable(k)) out << k;
    out.sort();
    return out;
}

QStringList SettingsIo::keysForCategory(const QString &category) const {
    QSettings s;
    QStringList out;
    const QStringList all = s.allKeys();
    for (const QString &k : all)
        if (matchesCategory(k, category)) out << k;
    out.sort();
    return out;
}

QVariant SettingsIo::valueFor(const QString &key) const {
    if (!isExportable(key)) return {};
    return QSettings().value(key);
}

// --- Export -----------------------------------------------------------------
QByteArray SettingsIo::exportJson() const {
    QSettings s;
    QJsonObject keys;
    const QStringList ks = exportableKeys();
    for (const QString &k : ks)
        keys.insert(k, QJsonValue::fromVariant(s.value(k)));

    QJsonObject root;
    root.insert(QStringLiteral("format"), QString::fromLatin1(kMarker));
    root.insert(QStringLiteral("formatVersion"), kFormat);
    root.insert(QStringLiteral("appVersion"), QCoreApplication::applicationVersion());
    root.insert(QStringLiteral("keys"), keys);
    return QJsonDocument(root).toJson(QJsonDocument::Indented);
}

bool SettingsIo::exportToFile(const QUrl &url) {
    QString path = pathOf(url);
    if (path.isEmpty()) { setError(tr("Kein Zielpfad angegeben.")); return false; }
    // Ohne Endung .json anhängen: der FileDialog liefert auf allen Plattformen genau
    // das, was getippt wurde.
    if (QFileInfo(path).suffix().isEmpty()) path += QStringLiteral(".json");

    QSaveFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        setError(tr("Datei kann nicht geschrieben werden: %1").arg(f.errorString()));
        return false;
    }
    f.write(exportJson());
    if (!f.commit()) {
        setError(tr("Datei kann nicht geschrieben werden: %1").arg(f.errorString()));
        return false;
    }
    setError(QString());
    return true;
}

// --- Import -----------------------------------------------------------------
QVariantMap SettingsIo::readFile(const QUrl &url) {
    const QString path = pathOf(url);
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        setError(tr("Datei kann nicht gelesen werden: %1").arg(f.errorString()));
        return {};
    }
    QJsonParseError err{};
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        setError(tr("Keine gültige JSON-Datei: %1").arg(err.errorString()));
        return {};
    }
    const QJsonObject root = doc.object();
    if (root.value(QStringLiteral("format")).toString() != QString::fromLatin1(kMarker)) {
        setError(tr("Keine QTmux-Einstellungsdatei."));
        return {};
    }
    const QJsonObject keys = root.value(QStringLiteral("keys")).toObject();
    QVariantMap out;
    for (auto it = keys.constBegin(); it != keys.constEnd(); ++it)
        out.insert(it.key(), fromJson(it.value()));
    setError(QString());
    return out;
}

QVariantList SettingsIo::importPreview(const QUrl &url) {
    const QVariantMap in = readFile(url);
    if (in.isEmpty()) return {};

    QSettings s;
    QVariantList out;
    for (auto it = in.constBegin(); it != in.constEnd(); ++it) {
        const bool skipped = !isExportable(it.key());
        const QVariant now = skipped ? QVariant() : s.value(it.key());
        if (!skipped && sameValue(now, it.value())) continue;   // ändert nichts
        QVariantMap e;
        e.insert(QStringLiteral("key"), it.key());
        e.insert(QStringLiteral("from"), displayValue(now));
        e.insert(QStringLiteral("to"), displayValue(it.value()));
        e.insert(QStringLiteral("skipped"), skipped);
        out << e;
    }
    return out;
}

QStringList SettingsIo::importFile(const QUrl &url) {
    const QVariantMap in = readFile(url);
    if (in.isEmpty()) return {};

    QSettings s;
    QStringList changed;
    for (auto it = in.constBegin(); it != in.constEnd(); ++it) {
        if (!isExportable(it.key())) continue;   // fremde/neuere Schlüssel nie schreiben
        if (sameValue(s.value(it.key()), it.value())) continue;
        s.setValue(it.key(), it.value());
        changed << it.key();
    }
    if (!changed.isEmpty()) afterWrite(changed);
    return changed;
}

// --- Zurücksetzen -----------------------------------------------------------
QStringList SettingsIo::resetCategory(const QString &category) {
    const QStringList keys = keysForCategory(category);
    if (keys.isEmpty()) return {};
    QSettings s;
    for (const QString &k : keys) s.remove(k);
    afterWrite(keys);
    return keys;
}

QStringList SettingsIo::resetAll() {
    const QStringList keys = exportableKeys();
    if (keys.isEmpty()) return {};
    QSettings s;
    for (const QString &k : keys) s.remove(k);
    afterWrite(keys);
    return keys;
}

// --- intern -----------------------------------------------------------------
void SettingsIo::afterWrite(const QStringList &keys) {
    QSettings().sync();
    // Nur die betroffenen Registries neu einlesen — sie halten ihren Stand im
    // Speicher und würden sonst beim nächsten Schreiben den alten zurückschreiben.
    bool schemes = false, hotkeys = false, profiles = false;
    for (const QString &k : keys) {
        if (k.startsWith(QLatin1String("colorSchemes/"))) schemes = true;
        else if (k.startsWith(QLatin1String("hotkeys/")))  hotkeys = true;
        else if (k.startsWith(QLatin1String("profiles/"))) profiles = true;
    }
    if (schemes)  ColorSchemeRegistry::instance()->reload();
    if (hotkeys)  HotkeyRegistry::instance()->reload();
    if (profiles) ConnectionProfileRegistry::instance()->reload();
    emit changed(keys);
}

void SettingsIo::setError(const QString &e) {
    if (m_lastError == e) return;
    m_lastError = e;
    emit lastErrorChanged();
}

} // namespace qtmux
