#pragma once

#include <array>
#include <cstdint>

namespace mac_pcan::core {

struct CanFrame {
    enum Flag : std::uint32_t {
        Extended = 1u << 0,  // 29-bit identifier
        Rtr      = 1u << 1,  // Remote-Transmission-Request
        Fd       = 1u << 2,  // CAN-FD frame
        Brs      = 1u << 3,  // Bit-Rate-Switch (FD only)
        Esi      = 1u << 4,  // Error-State-Indicator (FD only)
        ErrorFrame = 1u << 5,
    };

    std::uint32_t id = 0;          // 11-bit (Std) or 29-bit (Ext) identifier
    std::uint8_t  dlc = 0;         // data length code (0..15, FD: 0..64 bytes)
    std::uint32_t flags = 0;       // bitmask of Flag values
    std::uint64_t timestamp_us = 0; // hardware timestamp, microseconds
    std::uint8_t  bus_index = 0;   // index of the physical PCAN channel (multi-bus)
    std::array<std::uint8_t, 64> data{};

    bool isExtended() const noexcept { return (flags & Extended) != 0; }
    bool isFd() const noexcept       { return (flags & Fd) != 0; }
    bool isRtr() const noexcept      { return (flags & Rtr) != 0; }

    // Composite key for HashMap<CAN-ID -> Aggregator-Entry>. Std and Ext
    // identifiers share the numeric range 0..0x7FF, so we must not collide.
    std::uint32_t aggregatorKey() const noexcept {
        return (id << 1) | (isExtended() ? 1u : 0u);
    }

    // Number of bytes the DLC actually addresses. Classic CAN: DLC == bytes;
    // CAN-FD: DLC is a coded length where 9..15 map to 12/16/20/24/32/48/64.
    std::uint8_t payloadBytes() const noexcept {
        if (!isFd() || dlc <= 8) {
            return dlc;
        }
        constexpr std::uint8_t fdLengths[] = {12, 16, 20, 24, 32, 48, 64};
        const std::uint8_t idx = static_cast<std::uint8_t>(dlc - 9);
        return idx < sizeof(fdLengths) ? fdLengths[idx] : 0;
    }
};

}  // namespace mac_pcan::core
