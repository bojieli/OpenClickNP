# Per-element behavioral unit tests.
#
# For each element we test, we:
#   1. Run openclicknp-cc on a synthetic single-element topology.
#      The compiler emits generated/sw_emu/topology.cpp with the kernel
#      function as `extern "C" void kernel_<elem>(...)`.
#   2. Compile the generated topology into a static archive linked
#      against the openclicknp_runtime.
#   3. Compile our hand-written test_<elem>.cpp that calls the kernel
#      and asserts on its behavior.

set(ELEMENT_TEST_GEN_DIR ${CMAKE_CURRENT_BINARY_DIR}/_gen)
file(MAKE_DIRECTORY ${ELEMENT_TEST_GEN_DIR})

# CMake function: openclicknp_element_test(<element_name> <relpath_in_elements>)
#   • generates SW-emu code at configure time
#   • adds an executable that links the generated topology + per-element TB
function(openclicknp_element_test ELEM SUBDIR)
    set(SYN ${ELEMENT_TEST_GEN_DIR}/${ELEM}.clnp)
    set(GEN ${ELEMENT_TEST_GEN_DIR}/${ELEM})
    set(TOPO ${GEN}/sw_emu/topology.cpp)

    # Synthetic 1-instance topology importing only this element. The
    # instance is driven by host_in / host_out so the kernel function
    # is callable (its signature is a flat list of streams).
    file(WRITE ${SYN}
         "import \"${SUBDIR}/${ELEM}.clnp\";\n${ELEM} :: u\n")

    # Generate at configure time so CMake sees the resulting .cpp file
    # exists when it builds this target.
    execute_process(
        COMMAND $<TARGET_FILE:openclicknp-cc>
                -I ${CMAKE_SOURCE_DIR}/elements
                --no-systemc --no-verilator --no-link --no-host
                -o ${GEN} ${SYN}
        OUTPUT_QUIET
        ERROR_QUIET)

    # If generation failed (compiler not yet built at configure time), use
    # add_custom_command to defer to build time.
    add_custom_command(
        OUTPUT ${TOPO}
        COMMAND $<TARGET_FILE:openclicknp-cc>
                -I ${CMAKE_SOURCE_DIR}/elements
                --no-systemc --no-verilator --no-link --no-host
                -o ${GEN} ${SYN}
        DEPENDS openclicknp-cc ${SYN}
        COMMENT "[element_test] generating ${ELEM}")
    add_custom_target(gen_elem_${ELEM} DEPENDS ${TOPO})

    add_executable(test_elem_${ELEM}
        test_${ELEM}.cpp
        ${TOPO})
    set_source_files_properties(${TOPO} PROPERTIES GENERATED TRUE)
    add_dependencies(test_elem_${ELEM} gen_elem_${ELEM})
    target_include_directories(test_elem_${ELEM} PRIVATE
        ${CMAKE_SOURCE_DIR}/runtime/include
        ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(test_elem_${ELEM} PRIVATE pthread)
    target_compile_definitions(test_elem_${ELEM} PRIVATE OPENCLICKNP_HAS_XRT=0)
    # Generated SW-emu code uses `int openclicknp_sw_emu_main(int, char**)`
    # but we don't want to link it; we link only the kernel symbols.
    target_compile_options(test_elem_${ELEM} PRIVATE
        -Wno-unused-function -Wno-unused-variable -Wno-unused-but-set-variable
        -Wno-unused-label)

    add_test(NAME element_${ELEM} COMMAND test_elem_${ELEM})
endfunction()

# ----------------------------------------------------------------------
# Pick a representative subset spanning all 9 categories.
# ----------------------------------------------------------------------

# Core dataflow plumbing
openclicknp_element_test(Pass        core)
openclicknp_element_test(Tee         core)
openclicknp_element_test(Counter     core)
openclicknp_element_test(Idle        core)
openclicknp_element_test(DropElem    core)
openclicknp_element_test(Fork        core)
openclicknp_element_test(PassThrough core)
openclicknp_element_test(StagingReg  core)
openclicknp_element_test(FlitMux     core)
openclicknp_element_test(FlitDemux   core)
openclicknp_element_test(ZeroSource  core)
openclicknp_element_test(InfiniteSource core)
openclicknp_element_test(MetaGen     core)
openclicknp_element_test(FlitCount   core)
openclicknp_element_test(AddLatency  core)
openclicknp_element_test(p2s         core)
openclicknp_element_test(s2p         core)
openclicknp_element_test(CheckSopEop core)

# Lookups
openclicknp_element_test(HashTable          lookups)
openclicknp_element_test(CuckooHash         lookups)
openclicknp_element_test(LPM_Tree           lookups)
openclicknp_element_test(FlowCache          lookups)
openclicknp_element_test(FlowCounter        lookups)
openclicknp_element_test(RegTCAM            lookups)
openclicknp_element_test(SRAM_TCAM          lookups)
openclicknp_element_test(HashTCAM           lookups)
openclicknp_element_test(WildcardSeqLookup  lookups)
openclicknp_element_test(HashTableCache     lookups)
openclicknp_element_test(HashDemux          lookups)
openclicknp_element_test(GmemFlowLookup     lookups)
openclicknp_element_test(GmemTableRead      lookups)

# Parsers
openclicknp_element_test(FlowTupleParser parsers)
openclicknp_element_test(IP_Parser       parsers)
openclicknp_element_test(TCP_Parser      parsers)
openclicknp_element_test(UDP_Parser      parsers)
openclicknp_element_test(VLAN_Parser     parsers)
openclicknp_element_test(RoCE_Classifier parsers)
openclicknp_element_test(NVGRE_Parser    parsers)

# Actions
openclicknp_element_test(IPChecksum    actions)
openclicknp_element_test(TCPChecksum   actions)
openclicknp_element_test(NVGRE_Encap   actions)
openclicknp_element_test(NVGRE_Decap   actions)
openclicknp_element_test(PacketSetDIP  actions)
openclicknp_element_test(PacketModifier actions)
openclicknp_element_test(AppendHMAC    actions)
openclicknp_element_test(DropEnforcer  actions)
openclicknp_element_test(AllocResultAdapter actions)

# Crypto
openclicknp_element_test(AES_CTR       crypto)
openclicknp_element_test(AES_ECB       crypto)
openclicknp_element_test(AES_Dispatch  crypto)
openclicknp_element_test(SHA1          crypto)
openclicknp_element_test(Mult1024      crypto)
openclicknp_element_test(Add2048       crypto)
openclicknp_element_test(Subtract1024  crypto)
openclicknp_element_test(Montgomery    crypto)

# Queues
openclicknp_element_test(MinHeap          queues)
openclicknp_element_test(PriorityQueue    queues)
openclicknp_element_test(PriorityQueueSystolic queues)
openclicknp_element_test(RateLimit        queues)
openclicknp_element_test(ChannelShaping   queues)
openclicknp_element_test(FlitScheduler    queues)
openclicknp_element_test(StopAndWaitScheduler queues)

# Traffic
openclicknp_element_test(TrafficGen        traffic)
openclicknp_element_test(PacketGen         traffic)
openclicknp_element_test(RateLimiter       traffic)
openclicknp_element_test(HWTimeStampPktGen traffic)

# Networking
openclicknp_element_test(L4LoadBalancer  networking)
openclicknp_element_test(NexthopAlloc    networking)
openclicknp_element_test(LB_NexthopTable networking)
openclicknp_element_test(Firewall_5tuple networking)
openclicknp_element_test(PortScanDetect  networking)
openclicknp_element_test(Regex           networking)
openclicknp_element_test(PktLogger       networking)

# Testing
openclicknp_element_test(LatencyTest       testing)
openclicknp_element_test(DelayTester       testing)
openclicknp_element_test(NoEchoLatencyTest testing)
openclicknp_element_test(XorInputs         testing)

# ----- Remaining 46 elements (liveness tests) -----
openclicknp_element_test(AES_LastFlitMux                crypto)
openclicknp_element_test(AES_Merge                      crypto)
openclicknp_element_test(Capture                        core)
openclicknp_element_test(Clock                          core)
openclicknp_element_test(Credit_PktGen                  traffic)
openclicknp_element_test(Demux                          core)
openclicknp_element_test(Demux2                         core)
openclicknp_element_test(Dump                           core)
openclicknp_element_test(FixSopEop                      core)
openclicknp_element_test(FlitDemuxByMeta                core)
openclicknp_element_test(FlitMetaBuffer                 core)
openclicknp_element_test(FlitMetaMux                    core)
openclicknp_element_test(FlitMux2RR                     core)
openclicknp_element_test(FlowTupleTest                  testing)
openclicknp_element_test(Forwarder                      networking)
openclicknp_element_test(GmemTableWrite                 lookups)
openclicknp_element_test(Mem_to_channel                 core)
openclicknp_element_test(MultiFIFO                      core)
openclicknp_element_test(Mux                            core)
openclicknp_element_test(NVGRE_Gen                      traffic)
openclicknp_element_test(NVGRE_Match_Post               actions)
openclicknp_element_test(NVGRE_Match_Pre                actions)
openclicknp_element_test(PacketBuffer                   core)
openclicknp_element_test(PriorityParser                 parsers)
openclicknp_element_test(Rake                           queues)
openclicknp_element_test(Rand                           core)
openclicknp_element_test(RandomAccessGmemTest           testing)
openclicknp_element_test(RandomSource                   core)
openclicknp_element_test(RateLimitBlocking              queues)
openclicknp_element_test(RateLimitOneRate               queues)
openclicknp_element_test(Receiver                       traffic)
openclicknp_element_test(RegexDispatcher                networking)
openclicknp_element_test(RevParser                      parsers)
openclicknp_element_test(RoCE_Gen                       traffic)
openclicknp_element_test(RSA_modexp                     crypto)
openclicknp_element_test(RSA_ModExp_1024                crypto)
openclicknp_element_test(RSA_ModExp_2048                crypto)
openclicknp_element_test(RSA_ModExp_4096                crypto)
openclicknp_element_test(Sender_CreditPktGen            traffic)
openclicknp_element_test(SHA1_BlockGen                  crypto)
openclicknp_element_test(Sync                           core)
openclicknp_element_test(TestDumper                     core)
openclicknp_element_test(TestOnSoftware                 testing)
openclicknp_element_test(TestRandom                     testing)
openclicknp_element_test(TimerDriftPktGen               traffic)
openclicknp_element_test(TimeStamp_Parser               parsers)
openclicknp_element_test(TimeStamp_PktGen               parsers)
openclicknp_element_test(TreeReduce                     core)
openclicknp_element_test(TripleQueue                    queues)
