# SPDX-License-Identifier: Apache-2.0
#
# OpenClickNP user clock @ 322.265625 MHz
# (3.10625 ns period; matches CMAC user clock at 100 GbE).

create_clock -period 3.10625 -name openclicknp_user_clk \
    [get_pins -hierarchical "*openclicknp_top*ap_clk*"]

# Mark async crossings between vendor IP boundaries (CMAC, HBM, XDMA) and
# the user clock as asynchronous. These are handled inside the IP blocks
# via well-tested CDC structures.
set_clock_groups -asynchronous \
    -group [get_clocks openclicknp_user_clk] \
    -group [get_clocks -of_objects [get_pins -hierarchical "*cmac*tx_clk*"]] \
    -group [get_clocks -of_objects [get_pins -hierarchical "*cmac*rx_clk*"]]

set_clock_groups -asynchronous \
    -group [get_clocks openclicknp_user_clk] \
    -group [get_clocks -of_objects [get_pins -hierarchical "*hbm*ref_clk*"]]

set_clock_groups -asynchronous \
    -group [get_clocks openclicknp_user_clk] \
    -group [get_clocks -of_objects [get_pins -hierarchical "*xdma*axi_aclk*"]]
