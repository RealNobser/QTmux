#pragma once

#include "core/ICanDevice.hpp"

#include <condition_variable>
#include <deque>
#include <mutex>

namespace mac_pcan::drivers {

// Synthetic CAN device for tests and headless development.
// Frames are pushed via `enqueue()` and consumed by `read()` in FIFO order.
// When the queue is empty, `read()` blocks until a frame is enqueued, until
// `close()` is called, or until the timeout expires.
class MockDevice final : public core::ICanDevice {
public:
    MockDevice() = default;
    ~MockDevice() override;

    std::vector<core::DeviceInfo> enumerate() override;
    bool open(const core::DeviceInfo& device, const core::BitrateConfig& config) override;
    void close() override;
    bool isOpen() const override;
    bool read(core::CanFrame& out, std::chrono::milliseconds timeout) override;
    bool write(const core::CanFrame& frame) override;
    std::string lastError() const override;

    // Test-side API
    void enqueue(const core::CanFrame& frame);
    void enqueue(core::CanFrame&& frame);
    std::size_t pendingCount() const;

    // Frames captured by write(), for tests to inspect TX behaviour.
    std::vector<core::CanFrame> sentFrames() const;
    std::size_t sentCount() const;
    void clearSent();

    // Test helper: make the next N write() calls return false.
    void failNextWrites(std::size_t count);

private:
    mutable std::mutex mtx_;
    std::condition_variable cv_;
    std::deque<core::CanFrame> pending_;
    std::vector<core::CanFrame> sent_;
    std::size_t failNext_ = 0;
    bool open_ = false;
    std::string lastError_;
    core::BitrateConfig openedConfig_{};
};

}  // namespace mac_pcan::drivers
