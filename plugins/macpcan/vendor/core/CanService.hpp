#pragma once

#include "core/ICanDevice.hpp"

#include <atomic>
#include <cstddef>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace mac_pcan::core {

// Owns a worker thread that blocks on ICanDevice::read() and accumulates
// frames in an internal queue. The GUI drains the queue on a timer.
//
// Threading: producer = worker thread, consumer = caller of drain().
// Synchronization is a plain mutex+deque for now; a lock-free SPSC queue
// (moodycamel::ReaderWriterQueue) is the planned upgrade if profiling
// shows contention at high frame rates.
class CanService {
public:
    explicit CanService(std::unique_ptr<ICanDevice> device);
    ~CanService();

    CanService(const CanService&) = delete;
    CanService& operator=(const CanService&) = delete;

    // Opens the device and starts the worker thread.
    bool start(const DeviceInfo& device, const BitrateConfig& config);

    // Signals the worker to stop, joins it, and closes the device.
    void stop();

    bool isRunning() const noexcept;

    // Moves up to `maxFrames` frames from the internal queue into `out`.
    // Returns the number of frames appended.
    std::size_t drain(std::vector<CanFrame>& out,
                      std::size_t maxFrames = static_cast<std::size_t>(-1));

    // Synchronous TX. Forwards to ICanDevice::write(). Returns false if
    // the service isn't running or the device rejected the frame.
    bool send(const CanFrame& frame);

    std::size_t pendingCount() const;
    std::uint64_t totalReceived() const noexcept;
    std::string lastError() const;

private:
    void workerLoop();

    std::unique_ptr<ICanDevice> device_;
    std::thread worker_;
    std::atomic<bool> stopRequested_ = false;
    std::atomic<bool> running_ = false;
    std::atomic<std::uint64_t> totalReceived_ = 0;

    mutable std::mutex queueMtx_;
    std::deque<CanFrame> queue_;

    mutable std::mutex errorMtx_;
    std::string lastError_;
};

}  // namespace mac_pcan::core
