#include "core/CanService.hpp"

#include <algorithm>
#include <chrono>
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
    openDevice_ = deviceInfo;   // remembered so the supervisor can reopen
    openConfig_ = config;
    receiveStuck_.store(false);
    stopRequested_.store(false);
    running_.store(true);
    worker_ = std::thread(&CanService::workerLoop, this);
    return true;
}

void CanService::stop() {
    if (!running_.exchange(false)) {
        return;
    }
    {
        // Under the same lock the supervisor takes: otherwise a recovery
        // already in flight could reopen the channel right after this
        // close() and leave the handle open for good.
        std::lock_guard lk(recoveryMtx_);
        stopRequested_.store(true);
        if (device_) {
            device_->close();  // unblocks read() inside the worker
        }
    }
    if (worker_.joinable()) {
        worker_.join();
    }
}

bool CanService::isRunning() const noexcept {
    return running_.load();
}

namespace {
std::uint64_t steadyMs() {
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count());
}
}  // namespace

void CanService::workerLoop() {
    using namespace std::chrono_literals;
    CanFrame frame;

    // Supervisor state. See the contract on receiveStuck() for the
    // incident this exists for.
    std::uint64_t windowStartMs  = steadyMs();
    std::uint64_t windowErrors   = device_ ? device_->errorCount() : 0;
    std::uint64_t lastAttemptMs  = 0;

    while (!stopRequested_.load()) {
        if (device_->read(frame, 100ms)) {
            {
                std::lock_guard lock(queueMtx_);
                queue_.push_back(frame);
            }
            totalReceived_.fetch_add(1);
            // A frame got through, so whatever happened before, the ear
            // works. Re-arm the window and drop any stuck verdict.
            windowStartMs = steadyMs();
            windowErrors  = device_->errorCount();
            receiveStuck_.store(false);
            continue;
        }

        // read() said false. That is an ordinary idle timeout OR a driver
        // failure, and it CANNOT tell us which — the whole reason this
        // supervision exists. The error counter can: on a quiet bus it
        // does not move, on a deaf one it runs away.
        const std::uint64_t now    = steadyMs();
        const std::uint64_t errors = device_->errorCount();
        const bool silentLongEnough = (now - windowStartMs) >= kStuckSilenceMs;
        const bool errorsBursting   = (errors - windowErrors) >= kStuckErrorBurst;

        if (!(silentLongEnough && errorsBursting)) {
            continue;   // quiet bus, or a handful of transients: leave it alone
        }

        receiveStuck_.store(true);
        if (lastAttemptMs != 0 && (now - lastAttemptMs) < kRecoveryCooldownMs) {
            continue;   // give the previous reset its chance
        }
        lastAttemptMs = now;
        if (attemptRecovery()) {
            windowStartMs = steadyMs();
            windowErrors  = device_->errorCount();
        }
    }
}

bool CanService::attemptRecovery() {
    std::lock_guard lk(recoveryMtx_);
    if (stopRequested_.load()) return false;   // stop() owns the channel now
    device_->close();
    if (!device_->open(openDevice_, openConfig_)) {
        std::lock_guard elk(errorMtx_);
        lastError_ = "receive-path recovery failed to reopen: "
                   + device_->lastError();
        return false;
    }
    recoveries_.fetch_add(1);
    // NOT cleared here. Only a frame that actually arrives proves the ear
    // works again — declaring health because a reopen returned true would
    // be the same unbacked promise this whole mechanism exists to end.
    return true;
}

bool CanService::receiveStuck() const noexcept {
    return receiveStuck_.load();
}

std::uint64_t CanService::recoveries() const noexcept {
    return recoveries_.load();
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

ICanDevice::BusStatus CanService::busStatus() const noexcept {
    // CAN_GetStatus / equivalent is a read-only driver query and safe
    // to fire while another thread is inside read()/write(). We take
    // no queue lock to keep the GUI-tick path snappy.
    if (!device_ || !running_.load()) return ICanDevice::BusStatus::Unknown;
    return device_->busStatus();
}

ICanDevice::BusErrorCounters CanService::busErrorCounters() const noexcept {
    // Same contract as busStatus(): a read-only driver query, no queue
    // lock, and {} (all fields nullopt) when nothing is running — callers
    // omit missing fields instead of inventing a zero.
    if (!device_ || !running_.load()) return {};
    return device_->busErrorCounters();
}

std::uint64_t CanService::deviceErrors() const noexcept {
    // Deliberately NOT gated on running_: a session that has just been
    // closed still holds the count of what went wrong while it was open,
    // and that is exactly when someone reads it.
    return device_ ? device_->errorCount() : 0;
}

std::string CanService::deviceError() const {
    return device_ ? device_->lastError() : std::string{};
}

const char* busStatusLabel(ICanDevice::BusStatus s) noexcept {
    switch (s) {
        case ICanDevice::BusStatus::Ok:      return "OK";
        case ICanDevice::BusStatus::Warning: return "Warning";
        case ICanDevice::BusStatus::Passive: return "Error-Passive";
        case ICanDevice::BusStatus::BusOff:  return "Bus-Off";
        case ICanDevice::BusStatus::Unknown: return "Unknown";
    }
    return "Unknown";
}

}  // namespace mac_pcan::core
