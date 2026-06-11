#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>
#include <memory>
#include <vector>

#include "QTmuxPlugin.h"

QT_BEGIN_NAMESPACE
class QPluginLoader;
QT_END_NAMESPACE

namespace qtmux {

/// Lädt und verwaltet QTmux-Plugins (QTMUX-8). Gui-frei (nur Qt Core) —
/// prozessweiter Singleton via instance(), als Context-Property `Plugins` in QML.
///
/// Suchpfade (in dieser Reihenfolge, Duplikate per Datei-ID übersprungen):
///   1. `QTMUX_PLUGIN_DIR` (Env; mehrere Pfade per QDir::listSeparator) — Dev/Tests
///   2. `<App-Verzeichnis>/plugins`
///   3. macOS-Bundle: `<App-Verzeichnis>/../PlugIns`
///   4. `<AppData>/QTmux/plugins` (vom Anwender installierte Plugins)
class PluginHost : public QObject {
    Q_OBJECT
    /// Backend-Typen aller geladenen Plugins als Liste von Maps
    /// {pluginId, typeId, name, description, pluginName} — fürs „+"-Menü.
    Q_PROPERTY(QVariantList backendTypes READ backendTypesVariant NOTIFY changed)
    /// Geladene Plugins als Liste von Maps {id, name, file} (Über/Diagnose).
    Q_PROPERTY(QVariantList plugins READ pluginsVariant NOTIFY changed)
public:
    static PluginHost &instance();

    /// Durchsucht die Suchpfade und lädt alle (noch nicht geladenen) Plugins.
    /// Idempotent; gibt die Zahl der NEU geladenen Plugins zurück.
    int loadAll();

    /// Lädt eine einzelne Plugin-Datei (für Tests/Diagnose). Gibt die Plugin-ID
    /// zurück oder leer bei Fehler (Grund in lastError()).
    QString loadFile(const QString &filePath);

    int pluginCount() const { return static_cast<int>(m_plugins.size()); }
    QVariantList backendTypesVariant() const;
    QVariantList pluginsVariant() const;
    QString lastError() const { return m_lastError; }

    /// Anzeigename eines Backend-Typs (leer, wenn unbekannt).
    QString backendTypeName(const QString &pluginId, const QString &typeId) const;
    /// Erzeugt ein Backend über das Plugin. nullptr, wenn Plugin/Typ unbekannt.
    /// Der Aufrufer übernimmt den Besitz.
    ITerminalBackend *createBackend(const QString &pluginId, const QString &typeId,
                                    const QVariantMap &params = {},
                                    QObject *parent = nullptr) const;

signals:
    void changed();

private:
    explicit PluginHost(QObject *parent = nullptr);
    ~PluginHost() override;

    struct Loaded {
        std::unique_ptr<QPluginLoader> loader;   // hält die Bibliothek geladen
        PluginInterface *iface = nullptr;        // Wurzelobjekt (gehört dem Loader)
        QString file;
    };

    QStringList searchDirs() const;
    const Loaded *find(const QString &pluginId) const;

    std::vector<Loaded> m_plugins;
    QString m_lastError;
};

} // namespace qtmux
