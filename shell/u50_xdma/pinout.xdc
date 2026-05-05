# SPDX-License-Identifier: Apache-2.0
#
# Alveo U50 board-level pin assignments for OpenClickNP boundary I/O.
#
# Pin numbers are taken from the AMD/Xilinx Alveo U50 board files (UG1370).
# CMAC #0 is wired to the front QSFP28 cage A (the "ToR" port in the paper).
# CMAC #1 is wired to the front QSFP28 cage B (the "NIC" port in the paper).
# Reference clock: 161.1328125 MHz from the on-board ZL30171 PLL.

# CMAC #0 reference clock pin (QSFP28-A clock pair).
set_property PACKAGE_PIN N36 [get_ports {tor_qsfp_refclk_p}]
set_property PACKAGE_PIN N37 [get_ports {tor_qsfp_refclk_n}]

# CMAC #1 reference clock pin (QSFP28-B clock pair).
set_property PACKAGE_PIN K38 [get_ports {nic_qsfp_refclk_p}]
set_property PACKAGE_PIN K39 [get_ports {nic_qsfp_refclk_n}]

# Note: GTY transceiver TX/RX pins are auto-located by the Xilinx tools
# based on their associated quad. The locations above are sufficient to
# anchor each CMAC instance to the right QSFP28 cage.

# Voltage/IO standards for the QSFP modules.
set_property IOSTANDARD LVDS [get_ports {tor_qsfp_refclk_p tor_qsfp_refclk_n}]
set_property IOSTANDARD LVDS [get_ports {nic_qsfp_refclk_p nic_qsfp_refclk_n}]
