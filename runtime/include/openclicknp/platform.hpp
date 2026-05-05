// SPDX-License-Identifier: Apache-2.0
//
// openclicknp::Platform — top-level host-side handle to a programmed FPGA.
//
// Loads an .xclbin, instantiates per-kernel handles, sets up the slot
// bridge (XDMA or QDMA), exposes signal RPC and counters.
#pragma once

#include "openclicknp/flit.hpp"

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace openclicknp {

class Platform;
class Element;

struct NetworkCounters {
    uint64_t tor_tx = 0, tor_rx = 0;
    uint64_t nic_tx = 0, nic_rx = 0;
    bool tor_link_up = false;
    bool nic_link_up = false;
};

struct SignalRequest {
    uint16_t cmd = 0;
    uint32_t sparam = 0;
    uint64_t lparam[7] = {};
};

struct SignalResponse {
    uint16_t cmd = 0;
    uint32_t sparam = 0;
    uint64_t lparam[7] = {};
};

struct HostMessage {
    flit_t   flit{};
    uint16_t slot_id = 0;
};

// ---- Generated-code registration API ----
//
// The compiler-emitted host/kernel_table.cpp calls registerGenerated() at
// static-initialization time. The runtime then knows which kernels exist,
// which have signal handlers, etc. This avoids global weak-symbol hacks.
struct KernelInfo {
    const char* name;
    const char* element_type;
    bool        has_signal;
    bool        is_autorun;
    int         axilite_base;
};
struct SignalEntry {
    int         gid;
    const char* kernel;
    int         axilite_base;
};
struct HostStreamEntry {
    const char* kernel;
    int         port;
    bool        kernel_to_host;
    int         slot_id;
};

void registerGenerated(const KernelInfo*       kernels,       size_t n_kernels,
                       const SignalEntry*      signals,       size_t n_signals,
                       const HostStreamEntry*  host_streams,  size_t n_streams);

class Platform {
public:
    enum class TransportKind { XDMA, QDMA, Stub };

    Platform();
    ~Platform();

    // Open a programmed FPGA. `bdf` is the PCIe BDF (e.g. "0000:01:00.1");
    // pass empty string to auto-select the first U50.
    bool open(const std::string& xclbin_path,
              const std::string& bdf,
              TransportKind transport);

    void close();

    // Launch every non-autorun kernel (no-op if everything is autorun).
    void launchAll();

    // Get a handle to a kernel by instance name.
    Element* element(const std::string& name);

    // Counters
    [[nodiscard]] uint64_t cycleCounter() const;
    [[nodiscard]] NetworkCounters networkCounters() const;

    // Slot bridge access (used by Element classes; not typically called
    // directly by user code).
    bool sendSlot(uint16_t slot_id, const flit_t& f);
    bool recvSlot(uint16_t slot_id, flit_t& out, bool blocking);

    // Signal RPC dispatch (used by Element::signal()).
    bool dispatchSignal(int gid, const SignalRequest& req,
                        SignalResponse& rsp,
                        int timeout_ms = 1000);

    [[nodiscard]] bool isOpen() const noexcept { return is_open_; }
    [[nodiscard]] TransportKind transport() const noexcept { return transport_; }

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    bool          is_open_  = false;
    TransportKind transport_ = TransportKind::Stub;
};

}  // namespace openclicknp
