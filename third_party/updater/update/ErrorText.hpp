#pragma once

// appupdate — language-independent names for Qt error values.
//
// CANONICAL: QTmux vendors src/update/ byte-identically. Qt6::Core only.
//
// The raw message under "Details" stays English (update design 2026-11-19).
// QFile::errorString(), QNetworkReply::errorString() and
// QJsonParseError::errorString() are TRANSLATED by Qt's own catalogue (with
// qtbase_de.qm loaded they read "Verbindung beendet") and may carry a URL or a
// local path. So the update core never puts them into an error text; it uses
// the enum value's name instead, which is the same in every language.

#include <QFileDevice>
#include <QJsonParseError>
#include <QString>

namespace appupdate {

[[nodiscard]] inline QString fileErrorName(QFileDevice::FileError e)
{
    switch (e) {
    case QFileDevice::NoError:          return QStringLiteral("NoError");
    case QFileDevice::ReadError:        return QStringLiteral("ReadError");
    case QFileDevice::WriteError:       return QStringLiteral("WriteError");
    case QFileDevice::FatalError:       return QStringLiteral("FatalError");
    case QFileDevice::ResourceError:    return QStringLiteral("ResourceError");
    case QFileDevice::OpenError:        return QStringLiteral("OpenError");
    case QFileDevice::AbortError:       return QStringLiteral("AbortError");
    case QFileDevice::TimeOutError:     return QStringLiteral("TimeOutError");
    case QFileDevice::UnspecifiedError: return QStringLiteral("UnspecifiedError");
    case QFileDevice::RemoveError:      return QStringLiteral("RemoveError");
    case QFileDevice::RenameError:      return QStringLiteral("RenameError");
    case QFileDevice::PositionError:    return QStringLiteral("PositionError");
    case QFileDevice::ResizeError:      return QStringLiteral("ResizeError");
    case QFileDevice::PermissionsError: return QStringLiteral("PermissionsError");
    case QFileDevice::CopyError:        return QStringLiteral("CopyError");
    }
    return QStringLiteral("FileError(%1)").arg(static_cast<int>(e));
}

[[nodiscard]] inline QString jsonErrorText(const QJsonParseError& e)
{
    // QJsonParseError::ParseError has no Q_ENUM; the number plus the offset
    // is enough for "Details" and identical in every language.
    return QStringLiteral("JSON error %1 at offset %2")
        .arg(static_cast<int>(e.error)).arg(e.offset);
}

}  // namespace appupdate
