# SPDX-License-Identifier: Apache-2.0
#
# Vivado platform-build script for the U50 QDMA backing.
# In QDMA mode the slot_mux/slot_demux is omitted: each ClickNP slot maps
# directly to a real QDMA streaming queue.

set repo_root [lindex $argv 0]
set out_dcp   [lindex $argv 1]
set proj_dir  [file dirname $out_dcp]

create_project openclicknp_u50_qdma $proj_dir/proj -part xcu50-fsvh2104-2-e -force
set_property ip_repo_paths /opt/xilinx/platforms/xilinx_u50_gen3x16_qdma_5_202210_1/hw \
    [current_project]
update_ip_catalog

add_files -fileset sources_1 \
    $repo_root/shell/u50_qdma/openclicknp_top.v \
    $repo_root/shell/common/openclicknp_status.v

add_files -fileset constrs_1 \
    $repo_root/shell/u50_qdma/clocks.xdc \
    $repo_root/shell/u50_qdma/cdc_waivers.xdc \
    $repo_root/shell/u50_qdma/pinout.xdc

set_property top openclicknp_top [current_fileset]
update_compile_order -fileset sources_1
synth_design -top openclicknp_top -part xcu50-fsvh2104-2-e -mode out_of_context
write_checkpoint -force $out_dcp
puts "OpenClickNP U50/QDMA platform layer DCP written: $out_dcp"
