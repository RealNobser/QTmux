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
};

struct BitrateConfig {
    std::uint32_t nominalBps = 500'000;       // 125k / 250k / 500k / 1M / …
    std::optional<std::uint32_t> dataBps;     // FD only — when set, opens in FD mode
    bool listenOnly = false;
};

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
};

}  // namespace mac_pcan::core
