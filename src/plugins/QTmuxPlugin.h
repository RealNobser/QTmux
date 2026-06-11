#pragma once

// QTmux-Plugin-SDK (QTMUX-8, Phase 5).
//
// Ein Plugin ist eine Qt-Plugin-Bibliothek (QPluginLoader), deren Wurzelobjekt
// `qtmux::PluginInterface` implementiert. v1-Umfang: **Backend-Provider** — ein
// Plugin stellt einen oder mehrere Terminal-Backend-Typen bereit (AppBackends,
// z. B. MacPCAN), die QTmux wie Shell/SSH/Seriell als Session betreibt. Sidebar,
// Rendering, Splits usw. funktionieren damit automatisch (alles läuft über
// `ITerminalBackend`).
//
// Plugin-Seite (Minimalgerüst):
//
//   class MeinPlugin : public QObject, public qtmux::PluginInterface {
//       Q_OBJECT
//       Q_PLUGIN_METADATA(IID QTmuxPluginInterface_iid)
//       Q_INTERFACES(qtmux::PluginInterface)
//   public:
//       QString id() const override { return "mein-plugin"; }
//       ...
//   };
//
// Build: gegen die SDK-Header (dieses Verzeichnis + src/core) kompilieren und
// `qtmux_core` statisch dazulinken (liefert das Meta-Objekt von ITerminalBackend).
// Die `.dylib`/`.so`/`.dll` nach `<App>/plugins`, ins macOS-Bundle `Contents/PlugIns`
// oder in `<AppData>/QTmux/plugins` legen; Entwicklungs-Override: `QTMUX_PLUGIN_DIR`.

#include <QList>
#include <QObject>
#include <QString>
#include <QVariantMap>

#include "ITerminalBackend.h"

namespace qtmux {

/// Beschreibung eines Backend-Typs, den ein Plugin bereitstellt.
struct PluginBackendType {
    QString id;            ///< stabile Typ-ID (persistiert, z. B. "echo")
    QString name;          ///< Anzeigename für Menü/Sidebar (lokalisiert das Plugin selbst)
    QString description;   ///< optionaler Kurztext (Tooltip/Über)
};

/// Wurzel-Schnittstelle eines QTmux-Plugins (reine Schnittstelle, kein QObject —
/// das Wurzelobjekt erbt QObject separat und deklariert Q_INTERFACES).
class PluginInterface {
public:
    virtual ~PluginInterface() = default;

    /// Stabile Plugin-ID (persistiert; Kleinbuchstaben/Bindestriche, z. B. "macpcan").
    virtual QString id() const = 0;
    /// Anzeigename des Plugins.
    virtual QString name() const = 0;
    /// Backend-Typen, die dieses Plugin bereitstellt (mindestens einer).
    virtual QList<PluginBackendType> backendTypes() const = 0;
    /// Erzeugt ein Backend für die Typ-ID. `params` sind typ-spezifische Parameter
    /// (z. B. aus einem späteren Konfigurationsdialog; v1 übergibt eine leere Map).
    /// Rückgabe nullptr bei unbekannter Typ-ID; der Aufrufer übernimmt den Besitz
    /// (Session hält das Backend in einem unique_ptr).
    virtual ITerminalBackend *createBackend(const QString &typeId,
                                            const QVariantMap &params,
                                            QObject *parent) = 0;
};

} // namespace qtmux

/// Interface-ID inkl. Version — bei inkompatiblen SDK-Änderungen hochzählen
/// (QPluginLoader lädt nur exakt passende IIDs).
#define QTmuxPluginInterface_iid "com.qtmux.PluginInterface/1.0"
Q_DECLARE_INTERFACE(qtmux::PluginInterface, QTmuxPluginInterface_iid)
