#pragma once

#include "core/CanFrame.hpp"

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace mac_pcan::core {

struct DeviceInfo {
    std::string handle;    // opaque string, e.g. "PCAN_USBBUS1"
    std::string name;      // user-visible name, e.g. "PCAN-USB FD #1"
    bool supportsFd = false;

    // Adapter-stored identifier (PEAK: PCAN_DEVICE_ID, set by the user in
    // PCAN-View). Unlike the handle, it survives replugging — macOS renumbers
    // PCAN_USBBUSn when another adapter is attached. Reported RAW, exactly as
    // the hardware states it: adapters ship unconfigured (PCAN-USB reads 255,
    // PCAN-USB-FD reads 0), so two identical adapters are NOT distinguishable
    // by this alone. Judging that — uniqueness, fallbacks, mapping an id back
    // to a device — is the consumer's policy, deliberately not done here.
    // nullopt means the backend has no such concept (SocketCAN) or the driver
    // would not tell us; that is distinct from a reported 0.
    std::optional<std::uint32_t> deviceId;
};

struct BitrateConfig {
    std::uint32_t nominalBps = 500'000;       // 125k / 250k / 500k / 1M / …
    std::optional<std::uint32_t> dataBps;     // FD only — when set, opens in FD mode
    bool listenOnly = false;
};

// ⚠️ THREADING CONTRACT — read() and write() run on DIFFERENT threads.
// CanService owns a worker thread that sits in read() in a tight loop for as
// long as the session is open, while the host calls write() from its own
// thread (GUI tick, REST handler, test pump). An implementation with mutable
// state therefore has to synchronise it — MockDevice's mutex is not
// decoration, and neither is the one in the test fakes.
// The bill for ignoring this: a std::vector shared unlocked between the two
// sides cost a SEGFAULT in the Linux CI container on 2026-08-12, reproducible
// only under load and invisible on macOS.
class ICanDevice {
public:
    virtual ~ICanDevice() = default;

    virtual std::vector<DeviceInfo> enumerate() = 0;

    virtual bool open(const DeviceInfo& device, const BitrateConfig& config) = 0;

    virtual void close() = 0;

    virtual bool isOpen() const = 0;

    // Blocking read with a timeout. Returns true if a frame was placed in `out`,
    // false on timeout or error. On error, `lastError()` carries the message.
    virtual bool read(CanFrame& out, std::chrono::milliseconds timeout) = 0;

    // Send a frame. Returns true on success. On failure, lastError()
    // carries the message. Implementations decide whether to block or
    // drop on queue-full conditions; the caller treats `false` as a
    // single-frame send error and may retry.
    virtual bool write(const CanFrame& frame) = 0;

    virtual std::string lastError() const = 0;

    // Bus health snapshot. Drivers that can't report it (e.g. SocketCAN
    // via kernel netlink is a follow-up) may return Unknown. The GUI
    // uses this to warn the user when the interface has slipped into
    // an error state — otherwise a bus-off condition just looks like
    // "no frames arriving".
    enum class BusStatus : std::uint8_t {
        Unknown = 0,   // driver can't report / device closed
        Ok,            // bus is healthy
        Warning,       // TX or RX error counter above the warning limit
        Passive,       // error-passive state
        BusOff,        // controller disabled — needs reset to recover
    };
    virtual BusStatus busStatus() const noexcept { return BusStatus::Unknown; }

    // How often this driver has recorded a failure since it was opened.
    // Defaulted like busStatus(), so an implementation that cannot count
    // simply stays at 0 — no existing implementer has to change.
    //
    // ⚠️ WHY A COUNTER AND NOT `lastError()`: a driver-side RECEIVE
    // failure never reached anyone. CanService's worker discards
    // `read() == false` without a word, and the error string is
    // overwritten by the next failure, so neither occurrence nor
    // frequency was observable. On 2026-08-15 an OTA push lost a single
    // WIN_ACK on a demonstrably quiet bus — a second adapter listening on
    // the same wire recorded the frame, this one did not — and every
    // counter reachable from outside read clean. This is the counter that
    // was missing.
    //
    // ⚠️ NOT the same as "frames lost": a driver may drop a frame without
    // reporting anything. A rising count proves the receive path had
    // trouble; a flat count proves only that the driver stayed quiet.
    virtual std::uint64_t errorCount() const noexcept { return 0; }
};

const char* busStatusLabel(ICanDevice::BusStatus s) noexcept;

}  // namespace mac_pcan::core
