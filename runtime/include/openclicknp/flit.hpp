// SPDX-License-Identifier: Apache-2.0
//
// flit_t — the 64-byte (512-bit) channel message described in the paper.
//
// Layout (little-endian, all fields packed):
//   bytes  0..31  : packet payload data (32 B per the paper)
//   byte    32    : flags (sop, eop, error, fcs_error, fcs_valid)
//   byte    33    : padbytes count (0..32)
//   bytes 34..47  : reserved / metadata-mode user fields
//   bytes 48..49  : slot_id (used by the host slot-bridge)
//   bytes 50..63  : reserved
//
// In metadata-mode usage, the same 64 B carries a user-defined struct whose
// shape is determined by the element's .state declarations. The compiler
// does not enforce metadata layout — it is shared by convention between
// communicating elements.
#pragma once

#include <array>
#include <cstdint>
#include <cstring>

namespace openclicknp {

constexpr int OPENCLICKNP_FLIT_BYTES = 64;
constexpr int OPENCLICKNP_PAYLOAD_BYTES = 32;

struct flit_t {
    std::array<uint8_t, OPENCLICKNP_FLIT_BYTES> raw{};

    // ---- payload accessors ----
    void set_data(const void* src, int len) {
        if (len > OPENCLICKNP_PAYLOAD_BYTES) len = OPENCLICKNP_PAYLOAD_BYTES;
        std::memcpy(raw.data(), src, static_cast<size_t>(len));
    }
    void get_data(void* dst, int len) const {
        if (len > OPENCLICKNP_PAYLOAD_BYTES) len = OPENCLICKNP_PAYLOAD_BYTES;
        std::memcpy(dst, raw.data(), static_cast<size_t>(len));
    }

    // Convenience: per-uint64 lane access.
    void set(int lane, uint64_t v) {
        if (lane < 0 || lane >= 4) return;
        std::memcpy(raw.data() + lane * 8, &v, 8);
    }
    uint64_t get(int lane) const {
        uint64_t v = 0;
        if (lane < 0 || lane >= 4) return 0;
        std::memcpy(&v, raw.data() + lane * 8, 8);
        return v;
    }

    // ---- flags (byte 32) ----
    bool sop()       const { return raw[32] & 0x01; }
    bool eop()       const { return raw[32] & 0x02; }
    bool error()     const { return raw[32] & 0x04; }
    bool fcs_error() const { return raw[32] & 0x08; }
    bool fcs_valid() const { return raw[32] & 0x10; }

    void set_sop(bool b)       { raw[32] = (raw[32] & ~0x01) | (b ? 0x01 : 0); }
    void set_eop(bool b)       { raw[32] = (raw[32] & ~0x02) | (b ? 0x02 : 0); }
    void set_error(bool b)     { raw[32] = (raw[32] & ~0x04) | (b ? 0x04 : 0); }
    void set_fcs_error(bool b) { raw[32] = (raw[32] & ~0x08) | (b ? 0x08 : 0); }
    void set_fcs_valid(bool b) { raw[32] = (raw[32] & ~0x10) | (b ? 0x10 : 0); }

    // ---- padbytes (byte 33) ----
    uint8_t padbytes() const          { return raw[33]; }
    void    set_padbytes(uint8_t pb)  { raw[33] = pb; }

    // ---- slot id (bytes 48..49) ----
    uint16_t slot_id() const {
        return static_cast<uint16_t>(raw[48]) |
               (static_cast<uint16_t>(raw[49]) << 8);
    }
    void set_slot_id(uint16_t s) {
        raw[48] = static_cast<uint8_t>(s & 0xFF);
        raw[49] = static_cast<uint8_t>((s >> 8) & 0xFF);
    }
};

// Port masks: 1 bit per port number, 1-based (PORT_BIT(n) = 1<<n).
using port_mask_t = uint32_t;
constexpr port_mask_t PORT_NULL = 0;
constexpr port_mask_t PORT_ALL  = 0x7FFF'FFFFu;
constexpr port_mask_t PROC_BREAK = 0x8000'0000u;
constexpr port_mask_t SIGNAL_DISABLE = 0x4000'0000u;

constexpr port_mask_t PORT_BIT(int n) {
    return static_cast<port_mask_t>(1u) << static_cast<unsigned>(n);
}
#define PORT_1  ::openclicknp::PORT_BIT(1)
#define PORT_2  ::openclicknp::PORT_BIT(2)
#define PORT_3  ::openclicknp::PORT_BIT(3)
#define PORT_4  ::openclicknp::PORT_BIT(4)
#define PORT_5  ::openclicknp::PORT_BIT(5)
#define PORT_6  ::openclicknp::PORT_BIT(6)
#define PORT_7  ::openclicknp::PORT_BIT(7)
#define PORT_8  ::openclicknp::PORT_BIT(8)

// Built-ins available to user element bodies (for both HLS and SW emu).
#define OPENCLICKNP_MAX_PORTS 16

#define input_ready(port_mask)         ((_input_port & (port_mask)) != 0)
#define test_input_port(port_mask)     input_ready(port_mask)
#define get_input_port()               (_input_port)
#define clear_input_ready(port_mask)   (_input_port &= ~(port_mask))
#define clear_input_port(port_mask)    clear_input_ready(port_mask)
#define get_input_data(idx)            (_input_data[idx])
#define peek_input_port(idx)           (_input_data[idx])
#define read_input_port(port_mask)                                        \
    (clear_input_ready(port_mask),                                        \
     _input_data[__builtin_ctz(static_cast<unsigned>(port_mask))])

#define set_output_ready(port_mask)    (_output_port |= (port_mask))
#define clear_output_ready(port_mask)  (_output_port &= ~(port_mask))
#define set_output_port(idx, value)                                       \
    do { _output_port |= ::openclicknp::PORT_BIT(idx);                    \
         _output_data[idx] = (value); } while (0)
#define set_port_output(idx, value)    set_output_port(idx, value)

#define last_output_failed(port_mask)  ((_output_failed & (port_mask)) != 0)
#define last_output_success(port_mask) ((_output_success & (port_mask)) != 0)

// ---- ClSignal (the host-controllable RPC payload, 64 bytes) ----
struct ClSignal {
    uint16_t kid;
    uint16_t cmd;
    uint32_t sparam;
    uint64_t lparam[7];
};
static_assert(sizeof(ClSignal) == 64, "ClSignal must be 64 bytes");

}  // namespace openclicknp
