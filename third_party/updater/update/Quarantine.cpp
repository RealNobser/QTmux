#include "update/Quarantine.hpp"

#include <QFile>

#include <utility>

#if defined(Q_OS_MACOS)
#include <cerrno>
#include <cstring>
#include <sys/xattr.h>
#endif

namespace appupdate {

namespace {

std::function<QuarantineState(const QString&)>& probeOverride()
{
    static std::function<QuarantineState(const QString&)> fn;
    return fn;
}

}  // namespace

void setQuarantineProbeForTests(
    std::function<QuarantineState(const QString&)> probe)
{
    probeOverride() = std::move(probe);
}

QuarantineState quarantineState(const QString& path, QString* errorOut)
{
    if (errorOut) errorOut->clear();
    if (probeOverride()) return probeOverride()(path);
#if defined(Q_OS_MACOS)
    if (path.isEmpty()) {
        if (errorOut) *errorOut = QStringLiteral("empty path");
        return QuarantineState::Unreadable;
    }
    const QByteArray native = QFile::encodeName(path);
    // Size query only (value buffer nullptr): existence is the whole answer.
    const ssize_t n = ::getxattr(native.constData(), "com.apple.quarantine",
                                 nullptr, 0, 0, 0);
    if (n >= 0) return QuarantineState::Present;
    const int err = errno;
    if (err == ENOATTR) return QuarantineState::Absent;
    if (errorOut) {
        *errorOut = QStringLiteral("cannot read the attributes of %1: %2")
                        .arg(path, QString::fromLocal8Bit(std::strerror(err)));
    }
    return QuarantineState::Unreadable;
#else
    Q_UNUSED(path);
    return QuarantineState::NotApplicable;
#endif
}

QString quarantineStateName(QuarantineState state)
{
    switch (state) {
    case QuarantineState::Absent:        return QStringLiteral("absent");
    case QuarantineState::Present:       return QStringLiteral("present");
    case QuarantineState::Unreadable:    return QStringLiteral("unreadable");
    case QuarantineState::NotApplicable: return QStringLiteral("not-applicable");
    }
    return QStringLiteral("unreadable");
}

}  // namespace appupdate
