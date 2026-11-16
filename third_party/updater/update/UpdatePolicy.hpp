#pragma once

// appupdate::policy — pure decision functions for the update flow. No I/O,
// no QSettings (persistence is each app's business: the app feeds stored
// strings in and writes results back). Everything here is trivially
// unit-testable.

#include "update/UpdateManifest.hpp"

#include <QDateTime>
#include <QString>
#include <QVersionNumber>

#include <cstdint>
#include <optional>

namespace appupdate::policy {

// ---- Contract V1 (2026-11-16, docs/SHARED.md „Update-Härtung“) ----------
// Both rules run AFTER the Ed25519 check, in this order: product binding
// first, replay second. UpdateChecker applies them; they are free functions
// so every consumer (DeviceUpdateService, tests, a vendored copy) decides
// the same way from the same lines.

// The channel a client follows unless it says otherwise.
inline constexpr const char* kDefaultChannel = "stable";

// Rule A2.1: `product` and `channel` must equal what this client expects.
// Empty result = bound to us; otherwise the reason, in English, for the log
// and the callback's error text. A missing field is a mismatch, never a
// wildcard — "no channel" must not read as "any channel".
[[nodiscard]] inline QString productMismatch(const UpdateManifest& m,
                                             const QString& expectedProduct,
                                             const QString& expectedChannel)
{
    if (m.product != expectedProduct) {
        return QStringLiteral("manifest is for product '%1', expected '%2'")
            .arg(m.product, expectedProduct);
    }
    if (m.channel.isEmpty()) {
        return QStringLiteral("manifest names no channel, expected '%1'")
            .arg(expectedChannel);
    }
    if (m.channel != expectedChannel) {
        return QStringLiteral("manifest is for channel '%1', expected '%2'")
            .arg(m.channel, expectedChannel);
    }
    return QString();
}

// Rule A2.2: replay protection by a monotonic serial. `lastSerial` is the
// highest serial this client ever accepted for the product (0 = never).
enum class SerialVerdict : std::uint8_t {
    Accept,
    ReplayWithoutSerial,  // lastSerial > 0, manifest carries none
    ReplayOlderSerial,    // serial < lastSerial
};

[[nodiscard]] inline SerialVerdict checkSerial(quint32 lastSerial,
                                               std::optional<quint32> serial)
{
    if (lastSerial == 0) return SerialVerdict::Accept;  // the transition
    if (!serial) return SerialVerdict::ReplayWithoutSerial;
    if (*serial < lastSerial) return SerialVerdict::ReplayOlderSerial;
    return SerialVerdict::Accept;  // equal = the same manifest again: fine
}

// What to store after an ACCEPTED manifest: never moves backwards.
[[nodiscard]] inline quint32 nextLastSerial(quint32 lastSerial,
                                            std::optional<quint32> serial)
{
    return serial && *serial > lastSerial ? *serial : lastSerial;
}

// The one settings key every app writes its lastSerial under, so RAFTNG,
// QTmux, EmbyStudio and MacPCAN cannot drift apart. The hub never touches
// QSettings itself; this is only the spelling. Value: the number (uint32).
[[nodiscard]] inline QString lastSerialSettingsKey(const QString& productSlug)
{
    return QStringLiteral("update/last_serial/") + productSlug;
}

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
