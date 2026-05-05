// SPDX-License-Identifier: Apache-2.0
//
// QDMA slot bridge.
//
// In QDMA mode every ClickNP slot maps to a real QDMA streaming queue
// (one queue per direction per slot). No software multiplexing.

#include "openclicknp/flit.hpp"
#include "openclicknp/platform.hpp"
#include "slot_bridge.hpp"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <vector>

namespace openclicknp::internal {

namespace {

// Same software-side semantics as XDMA — distinct in real hardware via XRT
// queue handles. Keeping a separate class lets us add real QDMA backing
// without touching the XDMA path.
class QdmaBridge final : public SlotBridge {
public:
    QdmaBridge() : queues_(64) {}
    ~QdmaBridge() override { shutdown(); }

    bool send(uint16_t slot, const flit_t& f) override {
        if (slot >= queues_.size()) return false;
        std::lock_guard<std::mutex> lk(queues_[slot].m);
        queues_[slot].q.push_back(f);
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

class StubBridge final : public SlotBridge {
public:
    bool send(uint16_t, const flit_t&) override { return false; }
    bool recv(uint16_t, flit_t&, bool) override { return false; }
    void shutdown() override {}
};

}  // namespace

std::unique_ptr<SlotBridge> makeQdmaBridge() {
    return std::make_unique<QdmaBridge>();
}
std::unique_ptr<SlotBridge> makeStubBridge() {
    return std::make_unique<StubBridge>();
}

}  // namespace openclicknp::internal
