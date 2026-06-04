#include "SerialBackend.h"

#include <QSerialPort>

namespace qtmux {

SerialBackend::SerialBackend(QObject *parent) : ITerminalBackend(parent) {}

SerialBackend::~SerialBackend() { terminate(); }

bool SerialBackend::start(int, int) {
    setState(BackendState::Starting);

    m_port = new QSerialPort(this);
    m_port->setPortName(m_portName);
    m_port->setBaudRate(m_baud);
    m_port->setDataBits(QSerialPort::Data8);
    m_port->setParity(QSerialPort::NoParity);
    m_port->setStopBits(QSerialPort::OneStop);
    m_port->setFlowControl(QSerialPort::NoFlowControl);

    connect(m_port, &QSerialPort::readyRead, this, [this]() {
        emit dataReceived(m_port->readAll());
    });
    connect(m_port, &QSerialPort::errorOccurred, this, [this](QSerialPort::SerialPortError e) {
        if (e == QSerialPort::NoError) return;
        if (e == QSerialPort::ResourceError) {   // Gerät abgezogen
            setState(BackendState::Closed);
        } else {
            setState(BackendState::Error);
        }
    });

    if (!m_port->open(QIODevice::ReadWrite)) {
        setState(BackendState::Error);
        return false;
    }
    setState(BackendState::Running);
    return true;
}

void SerialBackend::write(const QByteArray &data) {
    if (m_port && m_port->isOpen()) m_port->write(data);
}

void SerialBackend::resize(int, int) {
    // Serielle Verbindungen haben kein Größenkonzept.
}

void SerialBackend::terminate() {
    if (m_port) {
        if (m_port->isOpen()) m_port->close();
        m_port->deleteLater();
        m_port = nullptr;
    }
    setState(BackendState::Closed);
}

} // namespace qtmux
