// SPDX-License-Identifier: Apache-2.0
#include "openclicknp/pcap.hpp"

#include <cstring>

namespace openclicknp {

namespace {
constexpr uint32_t kMagicLE     = 0xa1b2c3d4u;
constexpr uint32_t kMagicBE     = 0xd4c3b2a1u;
constexpr uint32_t kMagicLENano = 0xa1b23c4du;
constexpr uint32_t kMagicBENano = 0x4d3cb2a1u;

uint16_t bswap16(uint16_t v) { return static_cast<uint16_t>((v >> 8) | (v << 8)); }
uint32_t bswap32(uint32_t v) {
    return ((v & 0x000000FFu) << 24) | ((v & 0x0000FF00u) << 8)
         | ((v & 0x00FF0000u) >> 8)  | ((v & 0xFF000000u) >> 24);
}
}  // namespace

bool PcapReader::open(const std::string& path) {
    f_.open(path, std::ios::binary);
    if (!f_) return false;
    uint32_t magic = 0;
    f_.read(reinterpret_cast<char*>(&magic), 4);
    if (magic == kMagicLE)         { swap_ = false; nano_ = false; }
    else if (magic == kMagicBE)    { swap_ = true;  nano_ = false; }
    else if (magic == kMagicLENano){ swap_ = false; nano_ = true;  }
    else if (magic == kMagicBENano){ swap_ = true;  nano_ = true;  }
    else return false;
    char header_rest[20];
    f_.read(header_rest, 20);
    return static_cast<bool>(f_);
}

bool PcapReader::next(PcapPacket& out) {
    uint32_t hdr[4];
    f_.read(reinterpret_cast<char*>(hdr), 16);
    if (f_.gcount() != 16) return false;
    if (swap_) for (auto& w : hdr) w = bswap32(w);
    out.ts_sec  = hdr[0];
    out.ts_usec = hdr[1];
    uint32_t incl_len = hdr[2];
    if (incl_len > 65535) return false;
    out.bytes.resize(incl_len);
    f_.read(reinterpret_cast<char*>(out.bytes.data()), incl_len);
    return f_.gcount() == static_cast<std::streamsize>(incl_len);
}

void PcapReader::close() { f_.close(); }

bool PcapWriter::open(const std::string& path, uint32_t link_type) {
    f_.open(path, std::ios::binary);
    if (!f_) return false;
    uint32_t hdr[6] = {
        kMagicLE,
        (2u << 16) | 4u,    // version 2.4
        0u,                  // thiszone
        0u,                  // sigfigs
        65535u,              // snaplen
        link_type,
    };
    f_.write(reinterpret_cast<const char*>(hdr), sizeof(hdr));
    return static_cast<bool>(f_);
}

void PcapWriter::write(const PcapPacket& p) {
    uint32_t hdr[4] = {
        p.ts_sec, p.ts_usec,
        static_cast<uint32_t>(p.bytes.size()),
        static_cast<uint32_t>(p.bytes.size()),
    };
    f_.write(reinterpret_cast<const char*>(hdr), sizeof(hdr));
    f_.write(reinterpret_cast<const char*>(p.bytes.data()),
             static_cast<std::streamsize>(p.bytes.size()));
}

void PcapWriter::close() { f_.close(); }

std::vector<flit_t> packetToFlits(const PcapPacket& p) {
    std::vector<flit_t> out;
    if (p.bytes.empty()) return out;
    size_t off = 0;
    while (off < p.bytes.size()) {
        flit_t f{};
        size_t n = std::min<size_t>(OPENCLICKNP_PAYLOAD_BYTES,
                                    p.bytes.size() - off);
        f.set_data(p.bytes.data() + off, static_cast<int>(n));
        f.set_sop(off == 0);
        bool last = (off + n == p.bytes.size());
        f.set_eop(last);
        if (last && n < OPENCLICKNP_PAYLOAD_BYTES) {
            f.set_padbytes(static_cast<uint8_t>(OPENCLICKNP_PAYLOAD_BYTES - n));
        }
        out.push_back(f);
        off += n;
    }
    return out;
}

std::vector<uint8_t> flitsToPacket(const std::vector<flit_t>& fs) {
    std::vector<uint8_t> out;
    if (fs.empty()) return out;
    out.reserve(fs.size() * OPENCLICKNP_PAYLOAD_BYTES);
    for (size_t i = 0; i < fs.size(); ++i) {
        const auto& f = fs[i];
        int valid = OPENCLICKNP_PAYLOAD_BYTES;
        if (i + 1 == fs.size() && f.eop()) {
            valid -= f.padbytes();
            if (valid < 0) valid = 0;
        }
        size_t pos = out.size();
        out.resize(pos + static_cast<size_t>(valid));
        f.get_data(out.data() + pos, valid);
    }
    return out;
}

}  // namespace openclicknp
