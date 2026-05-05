# SPDX-License-Identifier: Apache-2.0
#
# Vivado platform-build script for the U50 XDMA backing.
#
# This script is invoked by `scripts/build/implement.sh` to add OpenClickNP-
# specific constraints (clocks, CDC waivers, and a small openclicknp_status
# AXI-Lite block exposing the cycle counter and CMAC link counters) on top
# of the stock xilinx_u50_gen3x16_xdma platform.

set repo_root [lindex $argv 0]
set out_dcp   [lindex $argv 1]
set proj_dir  [file dirname $out_dcp]

create_project openclicknp_u50_xdma $proj_dir/proj -part xcu50-fsvh2104-2-e -force

# Stock platform IP repo.
set_property ip_repo_paths /opt/xilinx/platforms/xilinx_u50_gen3x16_xdma_5_202210_1/hw \
    [current_project]
update_ip_catalog

# Add the OpenClickNP top-level wrapper.
add_files -fileset sources_1 \
    $repo_root/shell/u50_xdma/openclicknp_top.v \
    $repo_root/shell/common/openclicknp_status.v \
    $repo_root/shell/common/slot_demux.v \
    $repo_root/shell/common/slot_mux.v

# Constraints
add_files -fileset constrs_1 \
    $repo_root/shell/u50_xdma/clocks.xdc \
    $repo_root/shell/u50_xdma/cdc_waivers.xdc \
    $repo_root/shell/u50_xdma/pinout.xdc

set_property top openclicknp_top [current_fileset]
update_compile_order -fileset sources_1

# Synthesize and write a checkpoint that v++ can pick up via --vivado.preBuild.
synth_design -top openclicknp_top -part xcu50-fsvh2104-2-e -mode out_of_context
write_checkpoint -force $out_dcp
puts "OpenClickNP U50/XDMA platform layer DCP written: $out_dcp"
