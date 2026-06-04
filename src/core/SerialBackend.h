#pragma once

#include "ITerminalBackend.h"
#include <QString>

class QSerialPort;

namespace qtmux {

/// Backend für serielle Verbindungen (UART/USB-Seriell) über QtSerialPort.
/// Für Hardware-Konsolen wie ESP/MacPCAN. resize() ist ein No-Op (kein Größenkonzept).
class SerialBackend : public ITerminalBackend {
    Q_OBJECT
public:
    explicit SerialBackend(QObject *parent = nullptr);
    ~SerialBackend() override;

    void setPortName(const QString &name) { m_portName = name; }
    void setBaudRate(int baud) { m_baud = baud; }

    bool start(int cols, int rows) override;
    void write(const QByteArray &data) override;
    void resize(int cols, int rows) override;   // No-Op
    void terminate() override;

    QString portName() const { return m_portName; }

private:
    QSerialPort *m_port = nullptr;
    QString m_portName;
    int m_baud = 115200;
};

} // namespace qtmux
