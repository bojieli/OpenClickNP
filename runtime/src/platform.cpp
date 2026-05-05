// SPDX-License-Identifier: Apache-2.0
#include "openclicknp/platform.hpp"
#include "openclicknp/element.hpp"
#include "slot_bridge.hpp"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <map>
#include <mutex>
#include <thread>
#include <vector>

#if OPENCLICKNP_HAS_XRT
#  include <xrt/xrt_device.h>
#  include <xrt/xrt_kernel.h>
#  include <xrt/xrt_bo.h>
#  include <experimental/xrt_ip.h>
#endif

namespace openclicknp {

// Global generated-code registry, populated at static-init time.
namespace {
struct Registry {
    std::vector<KernelInfo>      kernels;
    std::vector<SignalEntry>     signals;
    std::vector<HostStreamEntry> streams;
};
Registry& reg() { static Registry r; return r; }
}  // namespace

namespace internal { void recordSignals(const SignalEntry*, size_t); }

void registerGenerated(const KernelInfo* k, size_t nk,
                       const SignalEntry* s, size_t ns,
                       const HostStreamEntry* h, size_t nh) {
    auto& r = reg();
    r.kernels .assign(k, k + nk);
    r.signals .assign(s, s + ns);
    r.streams .assign(h, h + nh);
    internal::recordSignals(s, ns);
}

struct Platform::Impl {
    std::string xclbin_path;
    std::string bdf;
    std::map<std::string, std::unique_ptr<Element>> elements;
    std::unique_ptr<internal::SlotBridge>           bridge;
    std::mutex                                      lock;
    std::atomic<uint64_t>                           cycles{0};
#if OPENCLICKNP_HAS_XRT
    xrt::device  device;
    xrt::uuid    uuid;
    std::map<std::string, xrt::kernel> kernels;
    std::map<int, xrt::ip>             axilite_blocks;
#endif
};

Platform::Platform() : impl_(std::make_unique<Impl>()) {}
Platform::~Platform() { close(); }

bool Platform::open(const std::string& xclbin_path,
                    const std::string& bdf,
                    TransportKind transport) {
    std::lock_guard<std::mutex> lk(impl_->lock);
    impl_->xclbin_path = xclbin_path;
    impl_->bdf         = bdf;
    transport_         = transport;

#if OPENCLICKNP_HAS_XRT
    if (!xclbin_path.empty()) {
        try {
            impl_->device = bdf.empty() ? xrt::device(0) : xrt::device(bdf);
            impl_->uuid   = impl_->device.load_xclbin(xclbin_path);
            for (const auto& ki : reg().kernels) {
                impl_->kernels.emplace(
                    ki.name, xrt::kernel(impl_->device, impl_->uuid, ki.name));
            }
            for (const auto& se : reg().signals) {
                impl_->axilite_blocks.emplace(
                    se.gid, xrt::ip(impl_->device, impl_->uuid, se.kernel));
            }
        } catch (const std::exception& e) {
            std::fprintf(stderr, "openclicknp: open() failed: %s\n", e.what());
            return false;
        }
    }
#else
    (void)bdf;
#endif

    switch (transport) {
        case TransportKind::XDMA: impl_->bridge = internal::makeXdmaBridge(); break;
        case TransportKind::QDMA: impl_->bridge = internal::makeQdmaBridge(); break;
        case TransportKind::Stub: impl_->bridge = internal::makeStubBridge(); break;
    }
    if (!impl_->bridge) impl_->bridge = internal::makeStubBridge();

    is_open_ = true;
    return true;
}

void Platform::close() {
    std::lock_guard<std::mutex> lk(impl_->lock);
    if (impl_->bridge) impl_->bridge->shutdown();
    impl_->elements.clear();
    is_open_ = false;
}

void Platform::launchAll() {}

Element* Platform::element(const std::string& name) {
    std::lock_guard<std::mutex> lk(impl_->lock);
    auto it = impl_->elements.find(name);
    if (it != impl_->elements.end()) return it->second.get();
    auto e = std::make_unique<Element>(*this, name);
    auto* p = e.get();
    impl_->elements.emplace(name, std::move(e));
    return p;
}

uint64_t Platform::cycleCounter() const {
    return impl_->cycles.load();
}

NetworkCounters Platform::networkCounters() const { return {}; }

bool Platform::sendSlot(uint16_t slot, const flit_t& f) {
    return impl_->bridge ? impl_->bridge->send(slot, f) : false;
}
bool Platform::recvSlot(uint16_t slot, flit_t& out, bool blocking) {
    return impl_->bridge ? impl_->bridge->recv(slot, out, blocking) : false;
}

bool Platform::dispatchSignal(int gid, const SignalRequest& req,
                              SignalResponse& rsp, int /*timeout_ms*/) {
#if OPENCLICKNP_HAS_XRT
    auto it = impl_->axilite_blocks.find(gid);
    if (it == impl_->axilite_blocks.end()) return false;
    auto& ip = it->second;
    ip.write_register(0x04, req.cmd);
    ip.write_register(0x08, req.sparam);
    for (int i = 0; i < 7; ++i) {
        uint64_t v = req.lparam[i];
        ip.write_register(0x10 + i*8 + 0, static_cast<uint32_t>(v & 0xFFFFFFFFu));
        ip.write_register(0x10 + i*8 + 4, static_cast<uint32_t>(v >> 32));
    }
    ip.write_register(0x00, 0x1);
    for (int i = 0; i < 10000; ++i) {
        uint32_t st = ip.read_register(0x00);
        if (st & 0x2) break;
        std::this_thread::sleep_for(std::chrono::microseconds(50));
    }
    rsp.cmd    = static_cast<uint16_t>(ip.read_register(0x44));
    rsp.sparam = ip.read_register(0x48);
    for (int i = 0; i < 7; ++i) {
        uint32_t lo = ip.read_register(0x50 + i*8 + 0);
        uint32_t hi = ip.read_register(0x50 + i*8 + 4);
        rsp.lparam[i] = (static_cast<uint64_t>(hi) << 32) | lo;
    }
    return true;
#else
    (void)gid; (void)req;
    // SW-only fallback: respond with zeros so host code keeps running.
    rsp = {};
    return true;
#endif
}

}  // namespace openclicknp
