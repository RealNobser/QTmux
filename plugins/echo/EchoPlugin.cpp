// Demo-Plugin für das QTmux-Plugin-SDK (QTMUX-8): stellt ein "Echo"-Backend
// bereit, das Eingaben lokal zurückspiegelt. Zweck: die Plugin-Kette
// (QPluginLoader → PluginInterface → AppBackend → Session/Rendering) end-to-end
// beweisen und als Kopiervorlage für echte Plugins (z. B. MacPCAN) dienen.

#include "QTmuxPlugin.h"

#include <QtPlugin>

namespace qtmux {

/// Minimales AppBackend: spiegelt jede Eingabe zurück (CR → CRLF fürs Terminal).
/// Strg+D beendet die Session (Closed → Auto-Remove in der Sidebar).
class EchoBackend : public ITerminalBackend {
    Q_OBJECT
public:
    using ITerminalBackend::ITerminalBackend;

    bool start(int cols, int rows) override {
        Q_UNUSED(cols); Q_UNUSED(rows);
        setState(BackendState::Running);
        emit dataReceived(QByteArrayLiteral(
            "QTmux Echo-Demo-Plugin\r\n"
            "Eingaben werden zurueckgespiegelt; Strg+D beendet.\r\n\r\n> "));
        return true;
    }

    void write(const QByteArray &data) override {
        if (state() != BackendState::Running) return;
        if (data.contains('\x04')) {           // Strg+D = EOF -> Session beenden
            emit dataReceived(QByteArrayLiteral("\r\nEcho beendet.\r\n"));
            terminate();
            return;
        }
        QByteArray out = data;
        out.replace("\r", "\r\n> ");           // Enter: neue Prompt-Zeile
        emit dataReceived(out);
    }

    void resize(int cols, int rows) override { Q_UNUSED(cols); Q_UNUSED(rows); }
    void terminate() override { setState(BackendState::Closed); }
};

/// Plugin-Wurzelobjekt: registriert den Echo-Backend-Typ.
class EchoPlugin : public QObject, public PluginInterface {
    Q_OBJECT
    Q_PLUGIN_METADATA(IID QTmuxPluginInterface_iid)
    Q_INTERFACES(qtmux::PluginInterface)
public:
    QString id() const override { return QStringLiteral("echo-demo"); }
    QString name() const override { return QStringLiteral("Echo-Demo"); }

    QList<PluginBackendType> backendTypes() const override {
        return {{QStringLiteral("echo"), QStringLiteral("Echo (Demo)"),
                 QStringLiteral("Spiegelt Eingaben zurück — SDK-Beispiel")}};
    }

    ITerminalBackend *createBackend(const QString &typeId, const QVariantMap &params,
                                    QObject *parent) override {
        Q_UNUSED(params);
        if (typeId != QLatin1String("echo")) return nullptr;
        return new EchoBackend(parent);
    }
};

} // namespace qtmux

#include "EchoPlugin.moc"
