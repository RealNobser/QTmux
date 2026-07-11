#pragma once

// Textumsetzung zwischen CAN-Frames und Terminal-Zeilen für das MacPCAN-Plugin.
// Bewusst Gui-frei und ohne PCBUSB-Abhängigkeit (nur Qt Core + der vendorierte
// CanFrame), damit Format und Parser isoliert (tst_macpcan) getestet werden können.

#include "core/CanFrame.hpp"

#include <QByteArray>
#include <QChar>
#include <QRegularExpression>
#include <QString>
#include <QStringList>

#include <optional>

namespace mac_pcan::text {

/// Formatiert einen empfangenen CAN-Frame als eine Terminal-Zeile (candump-nah):
///   "  12.345678  #0  123  [8]  DE AD BE EF 00 11 22 33  E"
/// Zeitstempel = Sekunden.Mikrosekunden, ID 3-stellig (Standard) bzw. 8-stellig
/// (Extended) hex, dann Länge in [] und die Datenbytes; angehängt optionale Flags
/// (E=Extended, R=RTR, F=CAN-FD, B=BRS, !=Error-Frame).
inline QByteArray formatFrame(const core::CanFrame &f) {
    const quint64 us = f.timestamp_us;
    const QString ts = QStringLiteral("%1.%2")
                           .arg(us / 1000000ULL)
                           .arg(us % 1000000ULL, 6, 10, QChar('0'));

    const bool ext = f.isExtended();
    const QString id = QStringLiteral("%1")
                           .arg(f.id, ext ? 8 : 3, 16, QChar('0'))
                           .toUpper();

    const int n = f.payloadBytes();
    QStringList bytes;
    for (int i = 0; i < n && i < 64; ++i)
        bytes << QStringLiteral("%1").arg(f.data[i], 2, 16, QChar('0')).toUpper();

    QString flags;
    if (ext) flags += QLatin1Char('E');
    if (f.isRtr()) flags += QLatin1Char('R');
    if (f.isFd()) flags += QLatin1Char('F');
    if (f.flags & core::CanFrame::Brs) flags += QLatin1Char('B');
    if (f.flags & core::CanFrame::ErrorFrame) flags += QLatin1Char('!');

    QString line = QStringLiteral("  %1  #%2  %3  [%4]  %5")
                       .arg(ts)
                       .arg(f.bus_index)
                       .arg(id)
                       .arg(n)
                       .arg(bytes.join(QLatin1Char(' ')));
    if (!flags.isEmpty())
        line += QStringLiteral("  ") + flags;
    return line.toUtf8();
}

/// Parst eine getippte Sende-Zeile „<hexid> [b0 b1 …]" (alles hex, bis 8 Datenbytes
/// für Classic-CAN). Ein Präfix `x`/`X` vor der ID **oder** eine ID > 0x7FF erzwingt
/// einen Extended-Frame. Bei Fehler wird `nullopt` zurückgegeben und (falls gesetzt)
/// `*errMsg` mit einer Kurzbeschreibung gefüllt.
inline std::optional<core::CanFrame> parseSendCommand(const QString &lineIn,
                                                      QString *errMsg = nullptr) {
    static const QRegularExpression ws(QStringLiteral("\\s+"));
    const QStringList toks =
        lineIn.trimmed().split(ws, Qt::SkipEmptyParts);
    if (toks.isEmpty()) {
        if (errMsg) *errMsg = QStringLiteral("leere Zeile");
        return std::nullopt;
    }

    core::CanFrame f;
    QString idtok = toks.first();
    bool forceExt = false;
    if (idtok.startsWith(QLatin1Char('x')) || idtok.startsWith(QLatin1Char('X'))) {
        forceExt = true;
        idtok = idtok.mid(1);
    }
    bool ok = false;
    const quint32 id = idtok.toUInt(&ok, 16);
    if (!ok || id > 0x1FFFFFFFu) {
        if (errMsg) *errMsg = QStringLiteral("ungültige ID: %1").arg(toks.first());
        return std::nullopt;
    }

    const int nd = toks.size() - 1;
    if (nd > 8) {
        if (errMsg) *errMsg = QStringLiteral("max. 8 Datenbytes (Classic-CAN)");
        return std::nullopt;
    }
    for (int i = 0; i < nd; ++i) {
        const quint32 b = toks.at(i + 1).toUInt(&ok, 16);
        if (!ok || b > 0xFF) {
            if (errMsg) *errMsg = QStringLiteral("ungültiges Byte: %1").arg(toks.at(i + 1));
            return std::nullopt;
        }
        f.data[i] = static_cast<std::uint8_t>(b);
    }
    f.id = id;
    f.dlc = static_cast<std::uint8_t>(nd);
    if (forceExt || id > 0x7FF)
        f.flags |= core::CanFrame::Extended;
    return f;
}

} // namespace mac_pcan::text
