// SPDX-License-Identifier: Apache-2.0
//
// Minimal libpcap-format reader/writer.
//
// Used by the SW emulator harness, L4 Verilator testbench, and host-side
// loopback tests to feed real packet traces to tor_in/nic_in and capture
// outputs from tor_out/nic_out.
//
// File format reference: pcap-savefile(5).
#pragma once

#include "openclicknp/flit.hpp"

#include <cstdint>
#include <fstream>
#include <string>
#include <vector>

namespace openclicknp {

struct PcapPacket {
    uint32_t ts_sec  = 0;
    uint32_t ts_usec = 0;
    std::vector<uint8_t> bytes;
};

class PcapReader {
public:
    bool open(const std::string& path);
    bool next(PcapPacket& out);
    void close();
    [[nodiscard]] bool isOpen() const noexcept { return f_.is_open(); }

private:
    std::ifstream f_;
    bool swap_ = false;
    bool nano_ = false;
};

class PcapWriter {
public:
    bool open(const std::string& path,
              uint32_t link_type = 1 /* DLT_EN10MB */);
    void write(const PcapPacket& p);
    void close();
    [[nodiscard]] bool isOpen() const noexcept { return f_.is_open(); }

private:
    std::ofstream f_;
};

// Convert a packet to a sequence of 32-byte flits with sop/eop set.
std::vector<flit_t> packetToFlits(const PcapPacket& p);
// Reassemble a flit sequence back to packet bytes.
std::vector<uint8_t> flitsToPacket(const std::vector<flit_t>& fs);

}  // namespace openclicknp
