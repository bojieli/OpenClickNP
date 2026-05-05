# OpenClickNP — Evaluation summary

Run on 2026-05-05T04:38:10Z — host ubuntu

## Per-element FPGA resource estimates

| Element | LUT | FF | DSP | BRAM | Latency (cyc) | II |
|---|---|---|---|---|---|---|
| AddLatency | synth_failed |  |  |  |  |  |
| AES_CTR | 2162 | 1910 | 0 | 0 | 651.89 | 1 |
| AES_Dispatch | 24054 | 4755 | 0 | 0 | 661.38 | 1 |
| AES_ECB | 1797 | 1158 | 0 | 0 | 682.13 | 1 |
| AES_LastFlitMux | 3815 | 2095 | 0 | 0 | 405.35 | 1 |
| AES_Merge | 37881 | 3417 | 0 | 0 | 265.25 | 1 |
| AES_Merge | synth_failed |  |  |  |  |  |
| AllocResultAdapter | 3380 | 2598 | 0 | 0 | 654.02 | 1 |
| AppendHMAC | 1159 | 1004 | 0 | 0 | 682.13 | 1 |
| Capture | synth_failed |  |  |  |  |  |
| ChannelShaping | 1437 | 1119 | 0 | 0 | 499.25 | 1 |
| CheckSopEop | 2441 | 2249 | 0 | 0 | 289.44 | 1 |
| Clock | 14 | 2 | 0 | 0 | 0.00 | 1 |
| Counter | 37 | 6 | 0 | 0 | 12820.51 | 1 |
| CuckooHash | synth_failed |  |  |  |  |  |
| Demux | 10923 | 2626 | 0 | 0 | 661.38 | 1 |
| Demux2 | 10928 | 2624 | 0 | 0 | 661.38 | 1 |
| DropElem | 37 | 6 | 0 | 0 | 12820.51 | 1 |
| DropEnforcer | 1629 | 1133 | 0 | 0 | 282.34 | 1 |
| Dump | 37 | 6 | 0 | 0 | 12820.51 | 1 |
| element | LUT | FF | DSP | BRAM | Fmax_MHz | II |
| Firewall_5tuple | 37 | 6 | 0 | 0 | 12820.51 | 1 |
| FixSopEop | 2453 | 2101 | 0 | 0 | 682.13 | 1 |
| FlitCount | 1519 | 1744 | 0 | 0 | 465.77 | 1 |
| FlitDemux | 10823 | 2620 | 0 | 0 | 661.38 | 1 |
| FlitDemuxByMeta | 24036 | 4752 | 0 | 0 | 564.97 | 1 |
| FlitMetaBuffer | synth_failed |  |  |  |  |  |
| FlitMetaMux | 3815 | 2095 | 0 | 0 | 405.35 | 1 |
| FlitMux2RR | 4239 | 2100 | 0 | 0 | 376.69 | 1 |
| FlitMux | 3815 | 2095 | 0 | 0 | 405.35 | 1 |
| FlowCache | synth_failed |  |  |  |  |  |
| FlowCounter | synth_failed |  |  |  |  |  |
| FlowTupleParser | 3370 | 2859 | 0 | 0 | 675.11 | 1 |
| Fork | 3374 | 2598 | 0 | 0 | 654.02 | 1 |
| Forwarder | 4063 | 3630 | 0 | 0 | 590.67 | 1 |
| GmemFlowLookup | synth_failed |  |  |  |  |  |
| GmemTableRead | synth_failed |  |  |  |  |  |
| GmemTableWrite | synth_failed |  |  |  |  |  |
| HashDemux | 24036 | 4752 | 0 | 0 | 564.97 | 1 |
| HashTableCache | synth_failed |  |  |  |  |  |
| HashTable | synth_failed |  |  |  |  |  |
| HashTCAM | synth_failed |  |  |  |  |  |
| Idle | 14 | 2 | 0 | 0 | 0.00 | 1 |
| InfiniteSource | 125 | 583 | 0 | 0 | 651.89 | 1 |
| IPChecksum | 2765 | 1296 | 0 | 0 | 569.52 | 1 |
| IP_Parser | 1243 | 1578 | 0 | 0 | 682.13 | 1 |
| L4LoadBalancer | synth_failed |  |  |  |  |  |
| LB_NexthopTable | synth_failed |  |  |  |  |  |
| LPM_Tree | synth_failed |  |  |  |  |  |
| Mem_to_channel | synth_failed |  |  |  |  |  |

117 of 153 elements synthesized successfully

## Per-application throughput (SW emulator)

| Application | Pkt size (B) | Mpps | Gbps |
|---|---|---|---|
| AES_Encryption | 64 | 40.00 | 20.48 |
| AES_Encryption | 128 | 20.00 | 20.48 |
| AES_Encryption | 256 | 10.00 | 20.48 |
| AES_Encryption | 512 | 5.00 | 20.48 |
| AES_Encryption | 1024 | 2.50 | 20.48 |
| AES_Encryption | 1518 | 1.67 | 20.28 |
| DDoSDetect | 64 | 40.00 | 20.48 |
| DDoSDetect | 128 | 20.00 | 20.48 |
| DDoSDetect | 256 | 10.00 | 20.48 |
| DDoSDetect | 512 | 5.00 | 20.48 |
| DDoSDetect | 1024 | 2.50 | 20.48 |
| DDoSDetect | 1518 | 1.67 | 20.28 |
| Firewall | 64 | 40.00 | 20.48 |
| Firewall | 128 | 20.00 | 20.48 |
| Firewall | 256 | 10.00 | 20.48 |
| Firewall | 512 | 5.00 | 20.48 |
| Firewall | 1024 | 2.50 | 20.48 |
| Firewall | 1518 | 1.67 | 20.28 |
| FlowMonitor | 64 | 40.00 | 20.48 |
| FlowMonitor | 128 | 20.00 | 20.48 |
| FlowMonitor | 256 | 10.00 | 20.48 |
| FlowMonitor | 512 | 5.00 | 20.48 |
| FlowMonitor | 1024 | 2.50 | 20.48 |
| FlowMonitor | 1518 | 1.67 | 20.28 |
| IP_Forwarding | 64 | 40.00 | 20.48 |
| IP_Forwarding | 128 | 20.00 | 20.48 |
| IP_Forwarding | 256 | 10.00 | 20.48 |
| IP_Forwarding | 512 | 5.00 | 20.48 |
| IP_Forwarding | 1024 | 2.50 | 20.48 |
| IP_Forwarding | 1518 | 1.67 | 20.28 |
| L4LoadBalancer | 64 | 40.00 | 20.48 |
| L4LoadBalancer | 128 | 20.00 | 20.48 |
| L4LoadBalancer | 256 | 10.00 | 20.48 |
| L4LoadBalancer | 512 | 5.00 | 20.48 |
| L4LoadBalancer | 1024 | 2.50 | 20.48 |
| L4LoadBalancer | 1518 | 1.67 | 20.28 |
| NVGRE_Decap | 64 | 40.00 | 20.48 |
| NVGRE_Decap | 128 | 20.00 | 20.48 |
| NVGRE_Decap | 256 | 10.00 | 20.48 |
| NVGRE_Decap | 512 | 5.00 | 20.48 |
| NVGRE_Decap | 1024 | 2.50 | 20.48 |
| NVGRE_Decap | 1518 | 1.67 | 20.28 |
| NVGRE_Encap | 64 | 40.00 | 20.48 |
| NVGRE_Encap | 128 | 20.00 | 20.48 |
| NVGRE_Encap | 256 | 10.00 | 20.48 |
| NVGRE_Encap | 512 | 5.00 | 20.48 |
| NVGRE_Encap | 1024 | 2.50 | 20.48 |
| NVGRE_Encap | 1518 | 1.67 | 20.28 |
| PacketCapture | 64 | 40.00 | 20.48 |
| PacketCapture | 128 | 20.00 | 20.48 |
| PacketCapture | 256 | 10.00 | 20.48 |
| PacketCapture | 512 | 5.00 | 20.48 |
| PacketCapture | 1024 | 2.50 | 20.48 |
| PacketCapture | 1518 | 1.67 | 20.28 |
| PacketLogger | 64 | 40.00 | 20.48 |
| PacketLogger | 128 | 20.00 | 20.48 |
| PacketLogger | 256 | 10.00 | 20.48 |
| PacketLogger | 512 | 5.00 | 20.48 |
| PacketLogger | 1024 | 2.50 | 20.48 |
| PacketLogger | 1518 | 1.67 | 20.28 |
| PassTraffic | 64 | 40.00 | 20.48 |
| PassTraffic | 128 | 20.00 | 20.48 |
| PassTraffic | 256 | 10.00 | 20.48 |
| PassTraffic | 512 | 5.00 | 20.48 |
| PassTraffic | 1024 | 2.50 | 20.48 |
| PassTraffic | 1518 | 1.67 | 20.28 |
| PFabric | 64 | 40.00 | 20.48 |
| PFabric | 128 | 20.00 | 20.48 |
| PFabric | 256 | 10.00 | 20.48 |
| PFabric | 512 | 5.00 | 20.48 |
| PFabric | 1024 | 2.50 | 20.48 |
| PFabric | 1518 | 1.67 | 20.28 |
| PortScanDetect | 64 | 40.00 | 20.48 |
| PortScanDetect | 128 | 20.00 | 20.48 |
| PortScanDetect | 256 | 10.00 | 20.48 |
| PortScanDetect | 512 | 5.00 | 20.48 |
| PortScanDetect | 1024 | 2.50 | 20.48 |
| PortScanDetect | 1518 | 1.67 | 20.28 |
| RateLimiter | 64 | 40.00 | 20.48 |
| RateLimiter | 128 | 20.00 | 20.48 |
| RateLimiter | 256 | 10.00 | 20.48 |
| RateLimiter | 512 | 5.00 | 20.48 |
| RateLimiter | 1024 | 2.50 | 20.48 |
| RateLimiter | 1518 | 1.67 | 20.28 |
| RoCE_Gateway | 64 | 40.00 | 20.48 |
| RoCE_Gateway | 128 | 20.00 | 20.48 |
| RoCE_Gateway | 256 | 10.00 | 20.48 |
| RoCE_Gateway | 512 | 5.00 | 20.48 |
| RoCE_Gateway | 1024 | 2.50 | 20.48 |
| RoCE_Gateway | 1518 | 1.67 | 20.28 |
| VLAN_Bridge | 64 | 40.00 | 20.48 |
| VLAN_Bridge | 128 | 20.00 | 20.48 |
| VLAN_Bridge | 256 | 10.00 | 20.48 |
| VLAN_Bridge | 512 | 5.00 | 20.48 |
| VLAN_Bridge | 1024 | 2.50 | 20.48 |
| VLAN_Bridge | 1518 | 1.67 | 20.28 |

## Per-application latency (graph-depth-based estimate at 322 MHz)

| Application | # Elements | Graph depth | Latency (µs) |
|---|---|---|---|
| AES_Encryption | 2 | 2 | 0.019 |
| DDoSDetect | 5 | 2 | 0.019 |
| Firewall | 8 | 2 | 0.019 |
| FlowMonitor | 3 | 3 | 0.028 |
| IP_Forwarding | 3 | 3 | 0.028 |
| L4LoadBalancer | 5 | 5 | 0.047 |
| NVGRE_Decap | 2 | 2 | 0.019 |
| NVGRE_Encap | 5 | 5 | 0.047 |
| PacketCapture | 2 | 2 | 0.019 |
| PacketLogger | 2 | 2 | 0.019 |
| PassTraffic | 2 | 2 | 0.019 |
| PFabric | 2 | 2 | 0.019 |
| PortScanDetect | 3 | 3 | 0.028 |
| RateLimiter | 2 | 2 | 0.019 |
| RoCE_Gateway | 3 | 3 | 0.028 |
| VLAN_Bridge | 3 | 3 | 0.028 |

## CDC analysis (representative design)

```
Copyright 1986-2022 Xilinx, Inc. All Rights Reserved. Copyright 2022-2025 Advanced Micro Devices, Inc. All Rights Reserved.
--------------------------------------------------------------------------------------------------------------------------------------------------
| Tool Version      : Vivado v.2025.2 (lin64) Build 6299465 Fri Nov 14 12:34:56 MST 2025
| Date              : Tue May  5 03:55:52 2026
| Host              : ubuntu running 64-bit Ubuntu 22.04.5 LTS
| Command           : report_cdc -severity Info -file /home/ubuntu/OpenClickNP/eval/reports/cdc_status_block.rpt
| Design            : openclicknp_status
| Device            : xcu50-fsvh2104
| Speed File        : -2  PRODUCTION 1.30 05-01-2022
| Design State      : Synthesized
| Temperature Grade : E
--------------------------------------------------------------------------------------------------------------------------------------------------

CDC Report

```

