// SPDX-License-Identifier: Apache-2.0
#include "openclicknp/pcap.hpp"

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <vector>

int main() {
    using namespace openclicknp;
    const char* path = std::getenv("TMPDIR")
        ? (std::string(std::getenv("TMPDIR")) + "/openclicknp_test.pcap").c_str()
        : "/tmp/openclicknp_test.pcap";

    {
        PcapWriter w;
        assert(w.open(path));
        for (int i = 0; i < 5; ++i) {
            PcapPacket p;
            p.ts_sec  = 1000 + i;
            p.ts_usec = i;
            p.bytes.resize(64 + i * 30);
            for (size_t j = 0; j < p.bytes.size(); ++j)
                p.bytes[j] = static_cast<uint8_t>((i + j) & 0xFFu);
            w.write(p);
        }
        w.close();
    }
    {
        PcapReader r;
        assert(r.open(path));
        for (int i = 0; i < 5; ++i) {
            PcapPacket p;
            assert(r.next(p));
            assert(p.ts_sec  == static_cast<uint32_t>(1000 + i));
            assert(p.ts_usec == static_cast<uint32_t>(i));
            assert(p.bytes.size() == static_cast<size_t>(64 + i * 30));
            for (size_t j = 0; j < p.bytes.size(); ++j)
                assert(p.bytes[j] == static_cast<uint8_t>((i + j) & 0xFFu));
        }
        PcapPacket eof;
        assert(!r.next(eof));
    }
    // Round-trip flit conversion.
    {
        PcapPacket p;
        p.bytes.resize(150);
        for (size_t j = 0; j < p.bytes.size(); ++j)
            p.bytes[j] = static_cast<uint8_t>(j);
        auto fs = packetToFlits(p);
        // 150 bytes / 32 = 5 flits (last one with padbytes=10).
        assert(fs.size() == 5);
        assert(fs.front().sop() && !fs.front().eop());
        assert(fs.back().eop());
        assert(fs.back().padbytes() == 10);
        auto bytes_back = flitsToPacket(fs);
        assert(bytes_back == p.bytes);
    }
    std::printf("pcap_roundtrip: OK\n");
    return 0;
}
