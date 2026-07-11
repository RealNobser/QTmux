#include "core/CanService.hpp"

#include <algorithm>
#include <utility>

namespace mac_pcan::core {

CanService::CanService(std::unique_ptr<ICanDevice> device) : device_(std::move(device)) {}

CanService::~CanService() {
    stop();
}

bool CanService::start(const DeviceInfo& deviceInfo, const BitrateConfig& config) {
    if (running_.load()) {
        return false;
    }
    if (!device_->open(deviceInfo, config)) {
        std::lock_guard lock(errorMtx_);
        lastError_ = "open() failed: " + device_->lastError();
        return false;
    }
    stopRequested_.store(false);
    running_.store(true);
    worker_ = std::thread(&CanService::workerLoop, this);
    return true;
}

void CanService::stop() {
    if (!running_.exchange(false)) {
        return;
    }
    stopRequested_.store(true);
    if (device_) {
        device_->close();  // unblocks read() inside the worker
    }
    if (worker_.joinable()) {
        worker_.join();
    }
}

bool CanService::isRunning() const noexcept {
    return running_.load();
}

void CanService::workerLoop() {
    using namespace std::chrono_literals;
    CanFrame frame;
    while (!stopRequested_.load()) {
        if (device_->read(frame, 100ms)) {
            {
                std::lock_guard lock(queueMtx_);
                queue_.push_back(frame);
            }
            totalReceived_.fetch_add(1);
        }
    }
}

std::size_t CanService::drain(std::vector<CanFrame>& out, std::size_t maxFrames) {
    std::lock_guard lock(queueMtx_);
    const std::size_t n = std::min(maxFrames, queue_.size());
    out.reserve(out.size() + n);
    for (std::size_t i = 0; i < n; ++i) {
        out.push_back(std::move(queue_.front()));
        queue_.pop_front();
    }
    return n;
}

std::size_t CanService::pendingCount() const {
    std::lock_guard lock(queueMtx_);
    return queue_.size();
}

std::uint64_t CanService::totalReceived() const noexcept {
    return totalReceived_.load();
}

bool CanService::send(const CanFrame& frame) {
    if (!device_) {
        std::lock_guard lock(errorMtx_);
        lastError_ = "no device";
        return false;
    }
    if (!device_->write(frame)) {
        std::lock_guard lock(errorMtx_);
        lastError_ = "write failed: " + device_->lastError();
        return false;
    }
    return true;
}

std::string CanService::lastError() const {
    std::lock_guard lock(errorMtx_);
    return lastError_;
}

}  // namespace mac_pcan::core
