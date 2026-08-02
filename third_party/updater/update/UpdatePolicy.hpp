#pragma once

// appupdate::policy — pure decision functions for the update flow. No I/O,
// no QSettings (persistence is each app's business: the app feeds stored
// strings in and writes results back). Everything here is trivially
// unit-testable.

#include <QDateTime>
#include <QString>
#include <QVersionNumber>

namespace appupdate::policy {

// Strictly newer — equal versions are not an update.
[[nodiscard]] inline bool isNewer(const QVersionNumber& remote,
                                  const QVersionNumber& current)
{
    return QVersionNumber::compare(remote, current) > 0;
}

// Downgrades are allowed (owner decision 2026-08-02) but the dialog must
// warn loudly — this is the predicate for that warning.
[[nodiscard]] inline bool isDowngrade(const QVersionNumber& remote,
                                      const QVersionNumber& current)
{
    return QVersionNumber::compare(remote, current) < 0;
}

// True when the user pressed "Skip this version" for exactly this remote
// version. A later version clears the skip implicitly (it compares unequal).
// Manual checks ignore this predicate by simply not calling it.
[[nodiscard]] inline bool isSkipped(const QVersionNumber& remote,
                                    const QString& skippedVersion)
{
    const auto skipped = QVersionNumber::fromString(skippedVersion);
    return !skipped.isNull() && QVersionNumber::compare(remote, skipped) == 0;
}

// The silent startup check runs at most once per calendar day (local time —
// this is a user-facing "daily", not a 24 h sliding window). True when the
// stored ISO timestamp is empty, unparsable, or from an earlier day.
// The auto-check on/off switch is the app's own setting and gates the call
// site, not this predicate.
[[nodiscard]] inline bool autoCheckDue(const QString& lastCheckIso,
                                       const QDateTime& now)
{
    const QDateTime last = QDateTime::fromString(lastCheckIso, Qt::ISODate);
    if (!last.isValid()) {
        return true;
    }
    return last.toLocalTime().date() < now.toLocalTime().date();
}

}  // namespace appupdate::policy
