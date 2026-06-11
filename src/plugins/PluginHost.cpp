#include "PluginHost.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QLibrary>
#include <QPluginLoader>
#include <QStandardPaths>

namespace qtmux {

PluginHost::PluginHost(QObject *parent) : QObject(parent) {}
PluginHost::~PluginHost() = default;   // Loader bleiben geladen bis Prozessende

PluginHost &PluginHost::instance() {
    static PluginHost host;
    return host;
}

QStringList PluginHost::searchDirs() const {
    QStringList dirs;
    // 1) Entwicklungs-/Test-Override (mehrere Pfade per Listentrenner).
    const QString env = qEnvironmentVariable("QTMUX_PLUGIN_DIR");
    if (!env.isEmpty())
        dirs += env.split(QDir::listSeparator(), Qt::SkipEmptyParts);
    // 2) Neben der Anwendung.
    const QString appDir = QCoreApplication::applicationDirPath();
    if (!appDir.isEmpty()) {
        dirs += appDir + QStringLiteral("/plugins");
#if defined(Q_OS_MACOS)
        // 3) macOS-Bundle-Konvention: qtmux.app/Contents/PlugIns.
        dirs += QDir::cleanPath(appDir + QStringLiteral("/../PlugIns"));
#endif
    }
    // 4) Vom Anwender installierte Plugins.
    const QString appData = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (!appData.isEmpty()) dirs += appData + QStringLiteral("/plugins");
    dirs.removeDuplicates();
    return dirs;
}

int PluginHost::loadAll() {
    int loaded = 0;
    const QStringList dirs = searchDirs();
    for (const QString &dirPath : dirs) {
        QDir dir(dirPath);
        if (!dir.exists()) continue;
        const QFileInfoList files = dir.entryInfoList(QDir::Files | QDir::Readable);
        for (const QFileInfo &fi : files) {
            if (!QLibrary::isLibrary(fi.absoluteFilePath())) continue;
            if (!loadFile(fi.absoluteFilePath()).isEmpty()) ++loaded;
        }
    }
    if (loaded > 0) emit changed();
    return loaded;
}

QString PluginHost::loadFile(const QString &filePath) {
    // Bereits geladen? (gleiche Datei, kanonisch verglichen)
    const QString canonical = QFileInfo(filePath).canonicalFilePath();
    for (const Loaded &p : m_plugins)
        if (p.file == canonical) return {};

    auto loader = std::make_unique<QPluginLoader>(filePath);
    QObject *root = loader->instance();
    if (!root) {
        // Kein Qt-Plugin / falsche IID / Linkfehler — Grund festhalten, weiter.
        m_lastError = loader->errorString();
        return {};
    }
    auto *iface = qobject_cast<PluginInterface *>(root);
    if (!iface) {
        m_lastError = QStringLiteral("%1: Wurzelobjekt implementiert PluginInterface nicht")
                          .arg(QFileInfo(filePath).fileName());
        loader->unload();
        return {};
    }
    const QString id = iface->id();
    if (id.isEmpty() || find(id)) {
        // Doppelte Plugin-ID (z. B. dieselbe Bibliothek in zwei Suchpfaden) — überspringen.
        m_lastError = QStringLiteral("%1: Plugin-ID leer oder bereits geladen (\"%2\")")
                          .arg(QFileInfo(filePath).fileName(), id);
        loader->unload();
        return {};
    }

    Loaded entry;
    entry.loader = std::move(loader);
    entry.iface = iface;
    entry.file = canonical;
    m_plugins.push_back(std::move(entry));
    return id;
}

const PluginHost::Loaded *PluginHost::find(const QString &pluginId) const {
    for (const Loaded &p : m_plugins)
        if (p.iface->id() == pluginId) return &p;
    return nullptr;
}

QVariantList PluginHost::backendTypesVariant() const {
    QVariantList out;
    for (const Loaded &p : m_plugins) {
        const QList<PluginBackendType> types = p.iface->backendTypes();
        for (const PluginBackendType &t : types) {
            QVariantMap m;
            m[QStringLiteral("pluginId")] = p.iface->id();
            m[QStringLiteral("typeId")] = t.id;
            m[QStringLiteral("name")] = t.name;
            m[QStringLiteral("description")] = t.description;
            m[QStringLiteral("pluginName")] = p.iface->name();
            out.append(m);
        }
    }
    return out;
}

QVariantList PluginHost::pluginsVariant() const {
    QVariantList out;
    for (const Loaded &p : m_plugins) {
        QVariantMap m;
        m[QStringLiteral("id")] = p.iface->id();
        m[QStringLiteral("name")] = p.iface->name();
        m[QStringLiteral("file")] = p.file;
        out.append(m);
    }
    return out;
}

QString PluginHost::backendTypeName(const QString &pluginId, const QString &typeId) const {
    const Loaded *p = find(pluginId);
    if (!p) return {};
    const QList<PluginBackendType> types = p->iface->backendTypes();
    for (const PluginBackendType &t : types)
        if (t.id == typeId) return t.name;
    return {};
}

ITerminalBackend *PluginHost::createBackend(const QString &pluginId, const QString &typeId,
                                            const QVariantMap &params, QObject *parent) const {
    const Loaded *p = find(pluginId);
    return p ? p->iface->createBackend(typeId, params, parent) : nullptr;
}

} // namespace qtmux
