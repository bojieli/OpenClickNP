# SPDX-License-Identifier: Apache-2.0
#
# CDC waivers for OpenClickNP on Alveo U50 (XDMA platform).
#
# Each entry below documents a vendor-IP CDC crossing that the OpenClickNP
# user logic does NOT need to verify because it is internal to a Xilinx
# IP block already validated by AMD's design tools.
#
# Per the project CDC policy in PLAN.md §8.1, NO user-logic crossings are
# waived; all such crossings live exclusively inside `set_clock_groups
# -asynchronous` boundaries declared in clocks.xdc.

# CMAC TX/RX clock domain crossings (handled inside the CMAC IP).
create_waiver -id "CDC-1" \
    -from [get_pins -of [get_cells -hierarchical "*cmac_subsystem*"]] \
    -to   [get_pins -of [get_cells -hierarchical "*cmac_subsystem*"]] \
    -description "CMAC IP internal CDC; vendor-validated"

# HBM reference / clock-domain crossings inside the HBM controller.
create_waiver -id "CDC-2" \
    -from [get_pins -of [get_cells -hierarchical "*hbm_*"]] \
    -to   [get_pins -of [get_cells -hierarchical "*hbm_*"]] \
    -description "HBM IP internal CDC; vendor-validated"

# XDMA streaming/MM clock crossings inside the XDMA IP.
create_waiver -id "CDC-3" \
    -from [get_pins -of [get_cells -hierarchical "*xdma_*"]] \
    -to   [get_pins -of [get_cells -hierarchical "*xdma_*"]] \
    -description "XDMA IP internal CDC; vendor-validated"
