#pragma once

#include "core/ICanDevice.hpp"

#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>

namespace mac_pcan::drivers {

// Real-hardware ICanDevice for PEAK PCAN-USB / PCAN-USB-FD interfaces via
// the PCBUSB library (mac-can.com).
//
// Scope (M2): classic-CAN read only. CAN-FD initialization, error-frame
// surfacing and TX are added in later milestones.
class PcanDevice final : public core::ICanDevice {
public:
    PcanDevice() = default;
    ~PcanDevice() override;

    PcanDevice(const PcanDevice&) = delete;
    PcanDevice& operator=(const PcanDevice&) = delete;

    std::vector<core::DeviceInfo> enumerate() override;
    bool open(const core::DeviceInfo& device, const core::BitrateConfig& config) override;
    void close() override;
    bool isOpen() const override;
    bool read(core::CanFrame& out, std::chrono::milliseconds timeout) override;
    bool write(const core::CanFrame& frame) override;
    std::string lastError() const override;
    core::ICanDevice::BusStatus busStatus() const noexcept override;

    // Translate a PCBUSB-internal channel handle name (e.g. "PCAN_USBBUS1")
    // back to the numeric handle used by the C API.
    static std::uint16_t handleFromString(const std::string& s);
    static std::string stringFromHandle(std::uint16_t handle);

private:
    void recordError(unsigned long status, const char* op);
    // THE only way to touch lastError_. read() runs on the CanService worker
    // thread while write() runs on the caller's (since the precise replay
    // driver, that is its own thread) and lastError() is polled from the GUI
    // — three threads on one std::string. Unsynchronised, two concurrent
    // assignments shred the heap buffer and abort in free(). Invisible on the
    // mock, which never takes the error path at all.
    void setError(std::string text);
    // Atomic, not mutex-guarded: read() and write() touch these on EVERY
    // frame while close() may run from another thread (GUI, or the precise
    // replay driver). A mutex in the TX hot path would cost more than it
    // buys; these are PODs, so atomics remove the data race outright. The
    // remaining logical race (close() landing between the check and the
    // PCBUSB call) is PCBUSB's to reject — it returns an error status for an
    // uninitialised channel, which is defined behaviour, unlike a torn read.
    std::atomic<std::uint16_t> channel_{0};
    std::atomic<bool> open_{false};
    std::atomic<bool> isFd_{false};  // current session opened via CAN_InitializeFD
    mutable std::mutex errorMtx_;
    std::string lastError_;
};

}  // namespace mac_pcan::drivers
