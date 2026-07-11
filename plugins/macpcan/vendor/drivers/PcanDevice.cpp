#include "drivers/PcanDevice.hpp"

#include <PCBUSB.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstring>
#include <sstream>
#include <thread>

namespace mac_pcan::drivers {

namespace {

// PCBUSB on macOS defines PCAN_USBBUS1..8 only (USBBUS9..16 are gated by
// `#ifndef __APPLE__` in PCBUSB.h v0.13).
constexpr std::array<std::uint16_t, 8> kUsbChannels{
    PCAN_USBBUS1, PCAN_USBBUS2, PCAN_USBBUS3, PCAN_USBBUS4,
    PCAN_USBBUS5, PCAN_USBBUS6, PCAN_USBBUS7, PCAN_USBBUS8,
};

constexpr std::uint16_t baudCodeFor(std::uint32_t bps) {
    switch (bps) {
        case 1'000'000: return PCAN_BAUD_1M;
        case 800'000:   return PCAN_BAUD_800K;
        case 500'000:   return PCAN_BAUD_500K;
        case 250'000:   return PCAN_BAUD_250K;
        case 125'000:   return PCAN_BAUD_125K;
        case 100'000:   return PCAN_BAUD_100K;
        case 50'000:    return PCAN_BAUD_50K;
        case 20'000:    return PCAN_BAUD_20K;
        case 10'000:    return PCAN_BAUD_10K;
        case 5'000:     return PCAN_BAUD_5K;
        default:        return 0;
    }
}

// CAN-FD bitrate strings for f_clock = 80 MHz (PCAN-USB-FD default).
// Format expected by CAN_InitializeFD; segment values follow Peak's
// reference matrix. Add more pairs as needed by the bus you're testing.
const char* fdBitrateStringFor(std::uint32_t nom, std::uint32_t data) {
    if (nom == 500'000 && data == 1'000'000)
        return "f_clock_mhz=80,nom_brp=10,nom_tseg1=12,nom_tseg2=3,nom_sjw=3,"
               "data_brp=8,data_tseg1=7,data_tseg2=2,data_sjw=2";
    if (nom == 500'000 && data == 2'000'000)
        return "f_clock_mhz=80,nom_brp=10,nom_tseg1=12,nom_tseg2=3,nom_sjw=3,"
               "data_brp=4,data_tseg1=7,data_tseg2=2,data_sjw=2";
    if (nom == 500'000 && data == 4'000'000)
        return "f_clock_mhz=80,nom_brp=10,nom_tseg1=12,nom_tseg2=3,nom_sjw=3,"
               "data_brp=2,data_tseg1=7,data_tseg2=2,data_sjw=2";
    if (nom == 500'000 && data == 5'000'000)
        return "f_clock_mhz=80,nom_brp=10,nom_tseg1=12,nom_tseg2=3,nom_sjw=3,"
               "data_brp=2,data_tseg1=5,data_tseg2=2,data_sjw=2";
    if (nom == 1'000'000 && data == 2'000'000)
        return "f_clock_mhz=80,nom_brp=5,nom_tseg1=12,nom_tseg2=3,nom_sjw=3,"
               "data_brp=4,data_tseg1=7,data_tseg2=2,data_sjw=2";
    if (nom == 1'000'000 && data == 4'000'000)
        return "f_clock_mhz=80,nom_brp=5,nom_tseg1=12,nom_tseg2=3,nom_sjw=3,"
               "data_brp=2,data_tseg1=7,data_tseg2=2,data_sjw=2";
    if (nom == 1'000'000 && data == 5'000'000)
        return "f_clock_mhz=80,nom_brp=5,nom_tseg1=12,nom_tseg2=3,nom_sjw=3,"
               "data_brp=2,data_tseg1=5,data_tseg2=2,data_sjw=2";
    return nullptr;
}

std::uint64_t timestampToMicroseconds(const TPCANTimestamp& ts) {
    const std::uint64_t millis =
        (static_cast<std::uint64_t>(ts.millis_overflow) << 32) | ts.millis;
    return millis * 1000ULL + ts.micros;
}

std::string errorText(unsigned long status) {
    char buf[256] = {};
    if (CAN_GetErrorText(static_cast<TPCANStatus>(status), 0,
                         reinterpret_cast<LPSTR>(buf)) == PCAN_ERROR_OK) {
        return std::string(buf);
    }
    return "PCBUSB error";
}

}  // namespace

PcanDevice::~PcanDevice() {
    close();
}

std::uint16_t PcanDevice::handleFromString(const std::string& s) {
    for (auto h : kUsbChannels) {
        if (s == stringFromHandle(h)) {
            return h;
        }
    }
    return 0;
}

std::string PcanDevice::stringFromHandle(std::uint16_t handle) {
    for (std::size_t i = 0; i < kUsbChannels.size(); ++i) {
        if (kUsbChannels[i] == handle) {
            std::ostringstream os;
            os << "PCAN_USBBUS" << (i + 1);
            return os.str();
        }
    }
    return "PCAN_NONEBUS";
}

std::vector<core::DeviceInfo> PcanDevice::enumerate() {
    std::vector<core::DeviceInfo> result;
    for (std::uint16_t channel : kUsbChannels) {
        DWORD condition = 0;
        if (CAN_GetValue(channel, PCAN_CHANNEL_CONDITION,
                         &condition, sizeof(condition)) != PCAN_ERROR_OK) {
            continue;
        }
        const bool available = (condition & PCAN_CHANNEL_AVAILABLE) != 0;
        if (!available) {
            continue;
        }

        core::DeviceInfo info;
        info.handle = stringFromHandle(channel);

        char name[256] = {};
        if (CAN_GetValue(channel, PCAN_HARDWARE_NAME, name, sizeof(name)) == PCAN_ERROR_OK
            && name[0] != '\0') {
            info.name = std::string(name);
        } else {
            info.name = info.handle;
        }

        DWORD features = 0;
        if (CAN_GetValue(channel, PCAN_CHANNEL_FEATURES,
                         &features, sizeof(features)) == PCAN_ERROR_OK) {
            info.supportsFd = (features & FEATURE_FD_CAPABLE) != 0;
        }

        result.push_back(std::move(info));
    }
    return result;
}

bool PcanDevice::open(const core::DeviceInfo& device, const core::BitrateConfig& config) {
    if (open_) {
        close();
    }

    const std::uint16_t handle = handleFromString(device.handle);
    if (handle == 0) {
        lastError_ = "unknown channel handle: " + device.handle;
        return false;
    }

    const bool wantFd = config.dataBps.has_value();
    TPCANStatus init = PCAN_ERROR_OK;

    if (wantFd) {
        const char* bitrateStr = fdBitrateStringFor(config.nominalBps, *config.dataBps);
        if (!bitrateStr) {
            lastError_ = "unsupported CAN-FD bitrate pair: " +
                         std::to_string(config.nominalBps) + "/" +
                         std::to_string(*config.dataBps);
            return false;
        }
        // PCBUSB takes a non-const LPSTR; the string is read-only by the
        // library but the signature requires the cast.
        init = CAN_InitializeFD(handle, const_cast<char*>(bitrateStr));
    } else {
        const std::uint16_t baud = baudCodeFor(config.nominalBps);
        if (baud == 0) {
            lastError_ = "unsupported nominal bitrate: " + std::to_string(config.nominalBps);
            return false;
        }
        init = CAN_Initialize(handle, baud, 0, 0, 0);
    }

    if (init != PCAN_ERROR_OK) {
        recordError(init, wantFd ? "CAN_InitializeFD" : "CAN_Initialize");
        return false;
    }

    if (config.listenOnly) {
        DWORD value = PCAN_PARAMETER_ON;
        const TPCANStatus s = CAN_SetValue(handle, PCAN_LISTEN_ONLY, &value, sizeof(value));
        if (s != PCAN_ERROR_OK) {
            recordError(s, "CAN_SetValue(LISTEN_ONLY)");
            CAN_Uninitialize(handle);
            return false;
        }
    }

    channel_ = handle;
    open_ = true;
    isFd_ = wantFd;
    lastError_.clear();
    return true;
}

void PcanDevice::close() {
    if (!open_) {
        return;
    }
    CAN_Uninitialize(channel_);
    open_ = false;
    isFd_ = false;
    channel_ = 0;
}

bool PcanDevice::isOpen() const {
    return open_;
}

bool PcanDevice::read(core::CanFrame& out, std::chrono::milliseconds timeout) {
    if (!open_) {
        lastError_ = "device not open";
        return false;
    }

    // Polled read with sub-millisecond sleeps until timeout. Selects the
    // classic or FD path depending on how the channel was initialized.
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    TPCANMsg msg{};
    TPCANTimestamp ts{};
    TPCANMsgFD msgFd{};
    TPCANTimestampFD tsFd{};

    for (;;) {
        TPCANStatus status;
        if (isFd_) {
            status = CAN_ReadFD(channel_, &msgFd, &tsFd);
        } else {
            status = CAN_Read(channel_, &msg, &ts);
        }
        if (status == PCAN_ERROR_OK) {
            out = core::CanFrame{};
            if (isFd_) {
                out.id  = msgFd.ID;
                out.dlc = msgFd.DLC;
                const std::uint32_t mtype = msgFd.MSGTYPE;
                if (mtype & PCAN_MESSAGE_EXTENDED) out.flags |= core::CanFrame::Extended;
                if (mtype & PCAN_MESSAGE_RTR)      out.flags |= core::CanFrame::Rtr;
                if (mtype & PCAN_MESSAGE_FD)       out.flags |= core::CanFrame::Fd;
                if (mtype & PCAN_MESSAGE_BRS)      out.flags |= core::CanFrame::Brs;
                if (mtype & PCAN_MESSAGE_ESI)      out.flags |= core::CanFrame::Esi;
                const std::size_t n = std::min<std::size_t>(out.payloadBytes(), out.data.size());
                std::memcpy(out.data.data(), msgFd.DATA, n);
                out.timestamp_us = tsFd;  // already µs
            } else {
                out.id  = msg.ID;
                out.dlc = msg.LEN;
                const std::uint32_t mtype = msg.MSGTYPE;
                if (mtype & PCAN_MESSAGE_EXTENDED) out.flags |= core::CanFrame::Extended;
                if (mtype & PCAN_MESSAGE_RTR)      out.flags |= core::CanFrame::Rtr;
                const std::size_t n = std::min<std::size_t>(msg.LEN, 8);
                std::memcpy(out.data.data(), msg.DATA, n);
                out.timestamp_us = timestampToMicroseconds(ts);
            }
            return true;
        }
        if (status != PCAN_ERROR_QRCVEMPTY) {
            // Bus error or other non-empty failure. Surface but don't
            // tear the channel down — the worker decides what to do.
            recordError(status, isFd_ ? "CAN_ReadFD" : "CAN_Read");
            return false;
        }
        if (std::chrono::steady_clock::now() >= deadline) {
            return false;
        }
        std::this_thread::sleep_for(std::chrono::microseconds(500));
    }
}

bool PcanDevice::write(const core::CanFrame& frame) {
    if (!open_) {
        lastError_ = "device not open";
        return false;
    }

    if (isFd_) {
        TPCANMsgFD msg{};
        msg.ID = frame.id;
        msg.DLC = frame.dlc;
        std::uint32_t mtype = PCAN_MESSAGE_STANDARD;
        if (frame.isExtended())                 mtype |= PCAN_MESSAGE_EXTENDED;
        if (frame.isFd())                       mtype |= PCAN_MESSAGE_FD;
        if (frame.flags & core::CanFrame::Brs)  mtype |= PCAN_MESSAGE_BRS;
        if (frame.flags & core::CanFrame::Esi)  mtype |= PCAN_MESSAGE_ESI;
        if (frame.isRtr())                      mtype |= PCAN_MESSAGE_RTR;
        msg.MSGTYPE = static_cast<TPCANMessageType>(mtype);
        const std::size_t n = std::min<std::size_t>(frame.payloadBytes(), sizeof(msg.DATA));
        std::memcpy(msg.DATA, frame.data.data(), n);

        const TPCANStatus status = CAN_WriteFD(channel_, &msg);
        if (status != PCAN_ERROR_OK) {
            recordError(status, "CAN_WriteFD");
            return false;
        }
    } else {
        TPCANMsg msg{};
        msg.ID = frame.id;
        msg.LEN = std::min<std::uint8_t>(frame.dlc, 8);
        std::uint32_t mtype = PCAN_MESSAGE_STANDARD;
        if (frame.isExtended()) mtype |= PCAN_MESSAGE_EXTENDED;
        if (frame.isRtr())      mtype |= PCAN_MESSAGE_RTR;
        msg.MSGTYPE = static_cast<TPCANMessageType>(mtype);
        std::memcpy(msg.DATA, frame.data.data(), msg.LEN);

        const TPCANStatus status = CAN_Write(channel_, &msg);
        if (status != PCAN_ERROR_OK) {
            recordError(status, "CAN_Write");
            return false;
        }
    }
    return true;
}

std::string PcanDevice::lastError() const {
    return lastError_;
}

void PcanDevice::recordError(unsigned long status, const char* op) {
    std::ostringstream os;
    os << op << " failed: 0x" << std::hex << status << " (" << errorText(status) << ")";
    lastError_ = os.str();
}

}  // namespace mac_pcan::drivers
