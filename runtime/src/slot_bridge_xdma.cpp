// SPDX-License-Identifier: Apache-2.0
//
// XDMA slot bridge.
//
// Multiplexes up to 31 virtual ClickNP "slots" onto a small number of XDMA
// streaming AXIS H2C/C2H channels. The 16-bit slot_id is encoded in bytes
// 48..49 of every flit. FPGA-side slot_demux/mux IP fans the streams in/out
// by slot_id.
//
// Without XRT installed, we provide a software-only loopback so unit tests
// and emulation runs still exercise the same API.

#include "openclicknp/flit.hpp"
#include "openclicknp/platform.hpp"
#include "slot_bridge.hpp"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <thread>
#include <vector>

#if OPENCLICKNP_HAS_XRT
#  include <xrt/xrt_device.h>
#  include <xrt/xrt_kernel.h>
#  include <xrt/xrt_bo.h>
#endif

namespace openclicknp::internal {

namespace {

// Software loopback. Per-slot SPSC queue.
class XdmaBridge final : public SlotBridge {
public:
    XdmaBridge() : queues_(64) {}
    ~XdmaBridge() override { shutdown(); }

    bool send(uint16_t slot, const flit_t& f) override {
        if (slot >= queues_.size()) return false;
        flit_t copy = f;
        copy.set_slot_id(slot);
        std::lock_guard<std::mutex> lk(queues_[slot].m);
        queues_[slot].q.push_back(copy);
        queues_[slot].cv.notify_all();
        return true;
    }
    bool recv(uint16_t slot, flit_t& out, bool blocking) override {
        if (slot >= queues_.size()) return false;
        std::unique_lock<std::mutex> lk(queues_[slot].m);
        if (blocking) {
            queues_[slot].cv.wait_for(lk,
                std::chrono::milliseconds(50),
                [&]{ return !queues_[slot].q.empty() || stopped_; });
        }
        if (queues_[slot].q.empty()) return false;
        out = queues_[slot].q.front();
        queues_[slot].q.pop_front();
        return true;
    }
    void shutdown() override {
        stopped_ = true;
        for (auto& s : queues_) s.cv.notify_all();
    }
private:
    struct Slot {
        std::mutex m;
        std::condition_variable cv;
        std::deque<flit_t> q;
    };
    std::vector<Slot> queues_;
    std::atomic<bool> stopped_{false};
};

}  // namespace

std::unique_ptr<SlotBridge> makeXdmaBridge() {
    return std::make_unique<XdmaBridge>();
}

}  // namespace openclicknp::internal
