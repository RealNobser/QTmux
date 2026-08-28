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

    // Snapshot of the underlying driver's bus-health flag. See
    // ICanDevice::BusStatus. Reads the driver directly — no queue mutex.
    // Returns Unknown when the service isn't running.
    ICanDevice::BusStatus busStatus() const noexcept;

    // Numeric bus-error counters straight from the driver — see
    // ICanDevice::BusErrorCounters for which backend reports which field.
    // {} when the service isn't running.
    ICanDevice::BusErrorCounters busErrorCounters() const noexcept;

    // --- Receive-path diagnostics (2026-08-15) -------------------------
    // ⚠️ `lastError()` above is NOT the driver's current error: it only
    // ever holds an open-time or write-time failure. The worker discards
    // read() == false without a word, so a driver-side receive failure —
    // a queue overrun above all — left NO trace anywhere reachable from
    // outside. That is why the OTA run on 2026-08-15 could lose a WIN_ACK
    // on a quiet bus with every counter reading clean.
    //
    // These two report what the RECEIVE path actually sees:
    //   deviceErrors() — how often the DRIVER recorded a failure. Counted
    //                    in the driver, not here: read() also returns
    //                    false on an ordinary idle timeout, so a counter
    //                    in the worker loop would count silence, not
    //                    faults. recordError() fires only on real ones.
    //   deviceError()  — the driver's own last error string, filled at
    //                    the same place. Nothing else surfaces it.
    std::uint64_t deviceErrors() const noexcept;
    std::string   deviceError() const;

    // --- Receive-path supervision (2026-08-16) -------------------------
    // On 2026-08-16 an OTA push drove this adapter's controller into the
    // warning state. The driver reported the failure 2075 times, the
    // worker discarded every one of them, and the session stayed open and
    // permanently DEAF: rx frozen for 35 s while a second adapter on the
    // same wire kept receiving normally. Nothing recovered on its own; a
    // session restart fixed it instantly. The supervisor below performs
    // that restart itself.
    //
    // 🔑 The signal read() cannot give: it returns false for an ordinary
    // idle timeout AND for a driver failure, so the worker alone cannot
    // tell silence from deafness. errorCount() can — errors climbing
    // WHILE no frame arrives is the signature, and an idle bus produces
    // no errors at all.
    //
    // `receiveStuck()` is true from the moment the condition is detected
    // until a recovery succeeds. Callers MUST NOT present device state as
    // measured while it is true — a silent bus and a deaf ear look
    // identical from above, and reporting the second as the first is how
    // "the whole fleet stopped answering" gets diagnosed at the wrong end.
    bool          receiveStuck() const noexcept;
    std::uint64_t recoveries() const noexcept;

    // Thresholds for the supervisor. Public so a test can reason about
    // them instead of guessing, and so the numbers carry their evidence.
    //
    // kStuckSilenceMs — how long NO frame may arrive before silence counts
    // towards the verdict. ⚠️ Silence ALONE never triggers anything: an
    // idle bus is legitimate and must stay legitimate. It is only ever
    // evaluated together with the error burst below.
    static constexpr std::uint64_t kStuckSilenceMs   = 2000;
    // kStuckErrorBurst — how many NEW driver errors within that window
    // turn "quiet" into "deaf". Measured on the 2026-08-16 failure: the
    // driver logged roughly 150-200 errors per 200 ms sample at the peak
    // and 2075 in total, while a healthy 291 s transfer the same night
    // produced exactly ZERO. 32 therefore sits far above any single
    // transient (which increments by one) and far below the real event.
    static constexpr std::uint64_t kStuckErrorBurst  = 32;
    // Do not hammer the driver: wait this long after an attempt before
    // judging again, so one reset is given a chance to work.
    static constexpr std::uint64_t kRecoveryCooldownMs = 5000;

private:
    void workerLoop();
    // Close and reopen the channel — the exact cure that was verified by
    // hand on 2026-08-16 (rx resumed immediately, counters cleared).
    bool attemptRecovery();

    std::unique_ptr<ICanDevice> device_;
    std::thread worker_;
    std::atomic<bool> stopRequested_ = false;
    std::atomic<bool> running_ = false;
    std::atomic<std::uint64_t> totalReceived_ = 0;

    mutable std::mutex queueMtx_;
    std::deque<CanFrame> queue_;

    mutable std::mutex errorMtx_;
    std::string lastError_;

    // What to reopen with. start() used to drop both on the floor, which
    // is why nothing COULD restart the channel from inside.
    DeviceInfo    openDevice_{};
    BitrateConfig openConfig_{};
    // Serialises a supervisor-driven close/open against stop(). Without
    // it a recovery could reopen the channel after stop() closed it, and
    // the handle would stay open for the life of the process.
    std::mutex    recoveryMtx_;
    std::atomic<bool>          receiveStuck_{false};
    std::atomic<std::uint64_t> recoveries_{0};
};

}  // namespace mac_pcan::core
