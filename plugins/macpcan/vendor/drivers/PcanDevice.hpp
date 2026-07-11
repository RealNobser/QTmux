#pragma once

#include "core/ICanDevice.hpp"

#include <cstdint>

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

    // Translate a PCBUSB-internal channel handle name (e.g. "PCAN_USBBUS1")
    // back to the numeric handle used by the C API.
    static std::uint16_t handleFromString(const std::string& s);
    static std::string stringFromHandle(std::uint16_t handle);

private:
    void recordError(unsigned long status, const char* op);
    std::uint16_t channel_ = 0;
    bool open_ = false;
    bool isFd_ = false;  // current session opened via CAN_InitializeFD
    std::string lastError_;
};

}  // namespace mac_pcan::drivers
