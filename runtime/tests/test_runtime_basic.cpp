// SPDX-License-Identifier: Apache-2.0
#include "openclicknp/flit.hpp"
#include "openclicknp/sw_runtime.hpp"

#include <cassert>
#include <cstdio>

int main() {
    using namespace openclicknp;

    // flit_t basic flag handling
    flit_t f{};
    f.set_sop(true);
    f.set_eop(false);
    assert(f.sop() && !f.eop());
    f.set(0, 0xDEADBEEF'CAFEBABEull);
    assert(f.get(0) == 0xDEADBEEF'CAFEBABEull);
    f.set_slot_id(42);
    assert(f.slot_id() == 42);

    // SwStream FIFO
    SwStream s(2);
    flit_t a{}; a.set(0, 1);
    flit_t b{}; b.set(0, 2);
    flit_t c{}; c.set(0, 3);
    assert(s.write_nb(a));
    assert(s.write_nb(b));
    assert(!s.write_nb(c));   // full
    flit_t out{};
    assert(s.read_nb(out) && out.get(0) == 1);
    assert(s.write_nb(c));     // now has space
    assert(s.read_nb(out) && out.get(0) == 2);
    assert(s.read_nb(out) && out.get(0) == 3);
    assert(!s.read_nb(out));   // empty

    // Port masks
    assert(PORT_BIT(1) == 2u);
    assert(PORT_BIT(3) == 8u);
    assert(PORT_NULL == 0u);
    assert(PORT_ALL == 0x7FFFFFFFu);

    std::printf("runtime_basic: OK\n");
    return 0;
}
