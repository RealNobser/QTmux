#include "drivers/MockDevice.hpp"

namespace mac_pcan::drivers {

MockDevice::~MockDevice() {
    close();
}

std::vector<core::DeviceInfo> MockDevice::enumerate() {
    return {core::DeviceInfo{"MOCK_BUS1", "Mock CAN (synthetic)", true}};
}

bool MockDevice::open(const core::DeviceInfo& /*device*/, const core::BitrateConfig& config) {
    std::lock_guard lock(mtx_);
    open_ = true;
    openedConfig_ = config;
    lastError_.clear();
    return true;
}

void MockDevice::close() {
    {
        std::lock_guard lock(mtx_);
        open_ = false;
    }
    cv_.notify_all();
}

bool MockDevice::isOpen() const {
    std::lock_guard lock(mtx_);
    return open_;
}

bool MockDevice::read(core::CanFrame& out, std::chrono::milliseconds timeout) {
    std::unique_lock lock(mtx_);
    if (!open_) {
        lastError_ = "device not open";
        return false;
    }
    if (!cv_.wait_for(lock, timeout, [this] { return !pending_.empty() || !open_; })) {
        return false;  // timeout, no data
    }
    if (!open_) {
        return false;  // closed while waiting
    }
    out = pending_.front();
    pending_.pop_front();
    return true;
}

bool MockDevice::write(const core::CanFrame& frame) {
    std::lock_guard lock(mtx_);
    if (!open_) {
        lastError_ = "device not open";
        return false;
    }
    if (failNext_ > 0) {
        --failNext_;
        lastError_ = "synthetic write failure";
        return false;
    }
    sent_.push_back(frame);
    lastError_.clear();
    return true;
}

std::string MockDevice::lastError() const {
    std::lock_guard lock(mtx_);
    return lastError_;
}

std::vector<core::CanFrame> MockDevice::sentFrames() const {
    std::lock_guard lock(mtx_);
    return sent_;
}

std::size_t MockDevice::sentCount() const {
    std::lock_guard lock(mtx_);
    return sent_.size();
}

void MockDevice::clearSent() {
    std::lock_guard lock(mtx_);
    sent_.clear();
}

void MockDevice::failNextWrites(std::size_t count) {
    std::lock_guard lock(mtx_);
    failNext_ = count;
}

void MockDevice::enqueue(const core::CanFrame& frame) {
    {
        std::lock_guard lock(mtx_);
        pending_.push_back(frame);
    }
    cv_.notify_one();
}

void MockDevice::enqueue(core::CanFrame&& frame) {
    {
        std::lock_guard lock(mtx_);
        pending_.push_back(std::move(frame));
    }
    cv_.notify_one();
}

std::size_t MockDevice::pendingCount() const {
    std::lock_guard lock(mtx_);
    return pending_.size();
}

}  // namespace mac_pcan::drivers
