// MacPCAN-Plugin (QTMUX-9, Phase 5): erstes echtes QTmux-Plugin. Stellt ein
// „Terminal-artiges" CAN-Bus-Backend bereit — empfangene Frames erscheinen als
// scrollende Zeilen, getippte Zeilen „<hexid> b0 b1 …" senden einen Frame.
//
// Zwei Backend-Typen (bewusst getrennt, kein stiller Fallback):
//   • "pcan"      → echte PEAK-PCAN-USB-Hardware über die PCBUSB-Bibliothek
//   • "pcan-mock" → synthetischer Demo-CAN (ohne Hardware nutz- und vorführbar)
//
// Die CAN-Zugriffsschicht (CanFrame/ICanDevice/CanService/PcanDevice/MockDevice)
// ist aus dem MacPCAN-Projekt (github.com/…/MacPCAN) vendoriert und Qt-frei; die
// einzige PCBUSB-Abhängigkeit steckt in PcanDevice.cpp. PCBUSB (UV Software) ist
// proprietär, aber redistributierbar — wird NICHT ins Repo gelegt, sondern zur
// Build-Zeit gefunden (s. CMakeLists.txt), analog zur Clink-Erkennung.

#include "QTmuxPlugin.h"

#include "CanText.h"
#include "core/CanService.hpp"
#include "drivers/MockDevice.hpp"
#include "drivers/PcanDevice.hpp"

#include <QByteArray>
#include <QElapsedTimer>
#include <QTimer>
#include <QtPlugin>

#include <cmath>
#include <memory>
#include <optional>
#include <vector>

namespace qtmux {

/// AppBackend, das eine CAN-Verbindung als Terminal betreibt.
class CanBackend : public ITerminalBackend {
    Q_OBJECT
public:
    enum class Kind { Pcan, Mock };

    CanBackend(Kind kind, QObject *parent = nullptr)
        : ITerminalBackend(parent), m_kind(kind) {}
    ~CanBackend() override { terminate(); }

    bool start(int cols, int rows) override {
        Q_UNUSED(cols);
        Q_UNUSED(rows);
        setState(BackendState::Starting);
        emit dataReceived(banner());
        m_clock.start();
        connectBus(m_bitrate);
        if (state() == BackendState::Running)
            emit dataReceived(prompt());
        return true;
    }

    void write(const QByteArray &data) override {
        // Im Fehler-/Nicht-Verbunden-Zustand nur Strg+D (schließen) beachten.
        if (!m_service || state() != BackendState::Running) {
            if (data.contains('\x04')) terminate();
            return;
        }
        for (char c : data) {
            switch (c) {
            case '\r':
            case '\n':
                emit dataReceived(QByteArrayLiteral("\r\n"));
                processLine();
                m_line.clear();
                if (state() == BackendState::Running)
                    emit dataReceived(prompt());
                break;
            case '\x7f': // DEL
            case '\b':   // Backspace
                if (!m_line.isEmpty()) {
                    m_line.chop(1);
                    emit dataReceived(QByteArrayLiteral("\b \b"));
                }
                break;
            case '\x03': // Strg+C: aktuelle Zeile verwerfen
                emit dataReceived(QByteArrayLiteral("^C\r\n") + prompt());
                m_line.clear();
                break;
            case '\x04': // Strg+D: Session beenden
                emit dataReceived(QByteArrayLiteral("\r\n"));
                terminate();
                return;
            default:
                if (static_cast<unsigned char>(c) >= 0x20) {
                    m_line.append(c);
                    emit dataReceived(QByteArray(1, c)); // lokales Echo
                }
                break;
            }
        }
    }

    void resize(int, int) override {} // CAN hat kein Größenkonzept
    void terminate() override {
        disconnectBus();
        setState(BackendState::Closed);
    }

private:
    // Öffnet Gerät + Service mit der gegebenen Bitrate und startet die RX-Pump.
    // Setzt bei Erfolg Running, sonst Error (und lässt die Ansicht offen, damit die
    // Meldung sichtbar bleibt — Closed würde die Session automatisch entfernen).
    void connectBus(std::uint32_t bps) {
        std::unique_ptr<mac_pcan::core::ICanDevice> dev;
        if (m_kind == Kind::Mock) {
            auto mock = std::make_unique<mac_pcan::drivers::MockDevice>();
            m_mock = mock.get(); // nicht besitzend; Service übernimmt den Besitz
            dev = std::move(mock);
        } else {
            dev = std::make_unique<mac_pcan::drivers::PcanDevice>();
        }

        const auto devices = dev->enumerate();
        if (devices.empty()) {
            emit dataReceived(QByteArrayLiteral(
                "\r\n\x1b[31mKeine PCAN-USB-Hardware gefunden.\x1b[0m\r\n"
                "Schließen Sie ein PEAK-PCAN-USB-Interface an — oder öffnen Sie\r\n"
                "eine „CAN-Bus (Demo)\"-Session (synthetische Frames, ohne Hardware).\r\n"
                "\r\nStrg+D schließt diese Ansicht.\r\n"));
            setState(BackendState::Error);
            return;
        }

        const auto &info = devices.front();
        mac_pcan::core::BitrateConfig cfg;
        cfg.nominalBps = bps;

        m_service = std::make_unique<mac_pcan::core::CanService>(std::move(dev));
        if (!m_service->start(info, cfg)) {
            emit dataReceived(QByteArrayLiteral("\r\n\x1b[31mVerbindung fehlgeschlagen: \x1b[0m")
                              + QByteArray::fromStdString(m_service->lastError())
                              + QByteArrayLiteral("\r\nStrg+D schließt diese Ansicht.\r\n"));
            m_service.reset();
            m_mock = nullptr;
            setState(BackendState::Error);
            return;
        }
        m_bitrate = bps;

        emit dataReceived(QByteArrayLiteral("\r\nVerbunden mit ")
                          + QByteArray::fromStdString(info.name)
                          + QByteArrayLiteral(" @ ") + fmtRate(bps)
                          + QByteArrayLiteral(".\r\n") + helpLine());
        setState(BackendState::Running);

        // RX-Pump (einmalig anlegen, danach nur starten/stoppen).
        if (!m_drainTimer) {
            m_drainTimer = new QTimer(this);
            connect(m_drainTimer, &QTimer::timeout, this, &CanBackend::pump);
        }
        m_drainTimer->start(50);

        // Demo-Generator: dem MockDevice periodisch synthetische Frames einspeisen,
        // sodass sie den echten Weg (Worker-Thread → Queue → drain) durchlaufen.
        if (m_kind == Kind::Mock && m_mock) {
            if (!m_genTimer) {
                m_genTimer = new QTimer(this);
                connect(m_genTimer, &QTimer::timeout, this, &CanBackend::generate);
            }
            m_genTimer->start(600);
        }
    }

    // Stoppt Pump/Generator und schließt das Gerät (Backend bleibt am Leben).
    void disconnectBus() {
        if (m_drainTimer) m_drainTimer->stop();
        if (m_genTimer) m_genTimer->stop();
        if (m_service) { m_service->stop(); m_service.reset(); }
        m_mock = nullptr;
    }

    QByteArray banner() const {
        return QByteArrayLiteral(
            "\x1b[1mQTmux CAN-Bus\x1b[0m — MacPCAN-Plugin\r\n");
    }
    QByteArray helpLine() const {
        return QByteArrayLiteral(
            "Senden: \x1b[36m<hexid> b0 b1 …\x1b[0m (hex, bis 8 Byte)   ·   "
            "\x1b[36mbaud <rate>\x1b[0m (z. B. 125k)   ·   "
            "\x1b[36mhelp\x1b[0m · \x1b[36mclear\x1b[0m · \x1b[36mquit\x1b[0m · Strg+D\r\n");
    }
    QByteArray prompt() const { return QByteArrayLiteral("> "); }

    // 125000 → "125 kBit/s", 1000000 → "1 MBit/s".
    static QByteArray fmtRate(std::uint32_t bps) {
        if (bps >= 1000000 && bps % 1000000 == 0)
            return QByteArray::number(bps / 1000000) + " MBit/s";
        return QByteArray::number(bps / 1000) + " kBit/s";
    }

    // Parst „125000", „125k", „1M" → Bit/s. nullopt bei Fehler.
    static std::optional<std::uint32_t> parseBitrate(const QString &in) {
        QString t = in.trimmed().toLower();
        if (t.isEmpty()) return std::nullopt;
        std::uint32_t mult = 1;
        if (t.endsWith(QLatin1Char('k'))) { mult = 1000; t.chop(1); }
        else if (t.endsWith(QLatin1Char('m'))) { mult = 1000000; t.chop(1); }
        bool ok = false;
        const double v = t.toDouble(&ok);
        if (!ok || v <= 0.0) return std::nullopt;
        const auto bps = static_cast<std::uint32_t>(std::llround(v * mult));
        return bps > 0 ? std::optional<std::uint32_t>(bps) : std::nullopt;
    }

    // Verwirft die aktuelle Prompt-Zeile, gibt die RX-Zeilen darüber aus und stellt
    // Prompt + bereits Getipptes wieder her — so bleibt die Eingabe beim asynchronen
    // Zustrom von Frames erhalten.
    void pump() {
        std::vector<mac_pcan::core::CanFrame> frames;
        if (m_service) m_service->drain(frames, 200);
        if (frames.empty()) return;

        QByteArray out = QByteArrayLiteral("\r\x1b[K"); // Prompt-Zeile löschen
        for (const auto &f : frames)
            out += mac_pcan::text::formatFrame(f) + QByteArrayLiteral("\r\n");
        out += prompt() + m_line; // Prompt + Eingabe wiederherstellen
        emit dataReceived(out);
    }

    void processLine() {
        const QString line = QString::fromUtf8(m_line).trimmed();
        if (line.isEmpty()) return;
        if (line == QLatin1String("help")) {
            emit dataReceived(helpLine());
            return;
        }
        if (line == QLatin1String("clear")) {
            emit dataReceived(QByteArrayLiteral("\x1b[2J\x1b[H"));
            return;
        }
        if (line == QLatin1String("quit") || line == QLatin1String("exit")) {
            terminate();
            return;
        }
        if (line == QLatin1String("baud") || line.startsWith(QLatin1String("baud "))) {
            const auto bps = parseBitrate(line.mid(4));
            if (!bps) {
                emit dataReceived(QByteArrayLiteral(
                    "\x1b[31mUngültige Bitrate. Beispiele: baud 125k · baud 250000 · baud 1M\x1b[0m\r\n"));
                return;
            }
            disconnectBus();
            emit dataReceived(QByteArrayLiteral("\x1b[33mWechsle auf ") + fmtRate(*bps)
                              + QByteArrayLiteral(" …\x1b[0m"));
            connectBus(*bps);
            return;
        }
        QString err;
        auto frame = mac_pcan::text::parseSendCommand(line, &err);
        if (!frame) {
            emit dataReceived(QByteArrayLiteral("\x1b[31mFehler: ") + err.toUtf8()
                              + QByteArrayLiteral("\x1b[0m\r\n"));
            return;
        }
        if (m_service && m_service->send(*frame)) {
            emit dataReceived(QByteArrayLiteral("\x1b[36mTX\x1b[0m")
                              + mac_pcan::text::formatFrame(*frame)
                              + QByteArrayLiteral("\r\n"));
        } else {
            emit dataReceived(QByteArrayLiteral("\x1b[31mSenden fehlgeschlagen: ")
                              + QByteArray::fromStdString(m_service ? m_service->lastError() : "kein Gerät")
                              + QByteArrayLiteral("\x1b[0m\r\n"));
        }
    }

    // Synthetische Demo-Frames: rotierende IDs + Zähler-Nutzdaten.
    void generate() {
        if (!m_mock) return;
        static const std::uint32_t ids[] = {0x100, 0x123, 0x2A0, 0x7DF};
        mac_pcan::core::CanFrame f;
        f.id = ids[m_genSeq % 4];
        f.dlc = 8;
        f.timestamp_us = static_cast<std::uint64_t>(m_clock.nsecsElapsed() / 1000);
        for (int i = 0; i < 8; ++i)
            f.data[i] = static_cast<std::uint8_t>((m_genSeq + i) & 0xFF);
        ++m_genSeq;
        m_mock->enqueue(f);
    }

    Kind m_kind;
    std::uint32_t m_bitrate = 500'000; // Default 500 kBit/s; live per „baud" änderbar
    std::unique_ptr<mac_pcan::core::CanService> m_service;
    mac_pcan::drivers::MockDevice *m_mock = nullptr; // nicht besitzend (Service besitzt)
    QTimer *m_drainTimer = nullptr;
    QTimer *m_genTimer = nullptr;
    QElapsedTimer m_clock;
    QByteArray m_line;
    std::uint32_t m_genSeq = 0;
};

/// Plugin-Wurzelobjekt: registriert die beiden CAN-Backend-Typen.
class MacPcanPlugin : public QObject, public PluginInterface {
    Q_OBJECT
    Q_PLUGIN_METADATA(IID QTmuxPluginInterface_iid)
    Q_INTERFACES(qtmux::PluginInterface)
public:
    QString id() const override { return QStringLiteral("macpcan"); }
    QString name() const override { return QStringLiteral("MacPCAN"); }

    QList<PluginBackendType> backendTypes() const override {
        return {
            {QStringLiteral("pcan"), QStringLiteral("CAN-Bus (PCAN-USB)"),
             QStringLiteral("PEAK PCAN-USB-Interface über die PCBUSB-Bibliothek")},
            {QStringLiteral("pcan-mock"), QStringLiteral("CAN-Bus (Demo)"),
             QStringLiteral("Synthetischer CAN-Bus ohne Hardware — zum Ausprobieren")},
        };
    }

    ITerminalBackend *createBackend(const QString &typeId, const QVariantMap &params,
                                    QObject *parent) override {
        Q_UNUSED(params);
        if (typeId == QLatin1String("pcan"))
            return new CanBackend(CanBackend::Kind::Pcan, parent);
        if (typeId == QLatin1String("pcan-mock"))
            return new CanBackend(CanBackend::Kind::Mock, parent);
        return nullptr;
    }
};

} // namespace qtmux

#include "MacPcanPlugin.moc"
