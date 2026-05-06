# Integration tests: end-to-end compiler runs and the SW emulator backend.
add_executable(test_e2e_passtraffic test_e2e_passtraffic.cpp)
target_link_libraries(test_e2e_passtraffic PRIVATE openclicknp_compiler_lib)
add_test(NAME e2e_passtraffic COMMAND test_e2e_passtraffic)

# SystemC end-to-end smoke test (only added if libsystemc is found).
find_path(SYSTEMC_INCLUDE NAMES systemc.h PATHS /usr/include /usr/local/include)
find_library(SYSTEMC_LIB NAMES systemc PATHS /usr/lib /usr/lib/x86_64-linux-gnu)
if(SYSTEMC_INCLUDE AND SYSTEMC_LIB)
    add_executable(test_systemc_smoke test_systemc_smoke.cpp)
    add_test(NAME systemc_smoke
             COMMAND test_systemc_smoke
             WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
    set_tests_properties(systemc_smoke PROPERTIES TIMEOUT 60)
    message(STATUS "SystemC smoke test enabled (${SYSTEMC_INCLUDE})")
endif()

# Verilator end-to-end smoke (only enabled if verilator is present).
find_program(VERILATOR_BIN verilator)
if(VERILATOR_BIN)
    add_executable(test_verilator_smoke test_verilator_smoke.cpp)
    add_test(NAME verilator_smoke
             COMMAND test_verilator_smoke
             WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
    set_tests_properties(verilator_smoke PROPERTIES TIMEOUT 60)
    message(STATUS "Verilator smoke test enabled (${VERILATOR_BIN})")
endif()

# ---------------------------------------------------------------------
# RSA modexp throughput benchmark.
#
# Generates the SW-emu kernels for the three RSA_ModExp_<BITS> elements
# at configure time, links them into a single binary together with the
# benchmark harness. We rename each kernel symbol per element by
# generating with a different topology stub and patching `kernel_u` to
# `kernel_RSA_ModExp_<bits>`.
# ---------------------------------------------------------------------
set(RSA_PERF_GEN ${CMAKE_CURRENT_BINARY_DIR}/_gen_rsa_perf)
file(MAKE_DIRECTORY ${RSA_PERF_GEN})

set(RSA_PERF_KERNEL_SRCS "")
foreach(BITS 1024 2048 4096)
    set(SYN ${RSA_PERF_GEN}/RSA_${BITS}.clnp)
    set(GEN ${RSA_PERF_GEN}/RSA_${BITS})
    set(TOPO ${GEN}/sw_emu/topology.cpp)
    set(PATCHED ${GEN}/sw_emu/topology_${BITS}.cpp)
    file(WRITE ${SYN}
        "import \"crypto/RSA_ModExp_${BITS}.clnp\";\nRSA_ModExp_${BITS} :: u\n")
    add_custom_command(
        OUTPUT ${PATCHED}
        COMMAND $<TARGET_FILE:openclicknp-cc>
                -I ${CMAKE_SOURCE_DIR}/elements
                --no-systemc --no-verilator --no-link --no-host
                -o ${GEN} ${SYN}
        COMMAND ${CMAKE_COMMAND} -E copy ${TOPO} ${PATCHED}
        COMMAND sed -i "s/kernel_u/kernel_RSA_ModExp_${BITS}/g" ${PATCHED}
        COMMAND sed -i "s/namespace u_ns/namespace u_ns_${BITS}/g" ${PATCHED}
        COMMAND sed -i "s/u_ns::State_t/u_ns_${BITS}::State_t/g" ${PATCHED}
        COMMAND sed -i "s/openclicknp_sw_emu/openclicknp_sw_emu_${BITS}/g" ${PATCHED}
        COMMAND sed -i "s/openclicknp_sw_emu_${BITS}_${BITS}/openclicknp_sw_emu_${BITS}/g" ${PATCHED}
        DEPENDS openclicknp-cc ${SYN})
    list(APPEND RSA_PERF_KERNEL_SRCS ${PATCHED})
endforeach()

# Comprehensive RSA accuracy test (cross-checks against OpenSSL).
find_package(OpenSSL 3.0 QUIET)
if(OpenSSL_FOUND)
    add_executable(test_rsa_accuracy test_rsa_accuracy.cpp)
    target_include_directories(test_rsa_accuracy PRIVATE
        ${CMAKE_SOURCE_DIR}/runtime/include)
    target_link_libraries(test_rsa_accuracy PRIVATE OpenSSL::Crypto)
    target_compile_options(test_rsa_accuracy PRIVATE
        -Wno-unused-function -Wno-unused-variable)
    add_test(NAME rsa_accuracy
             COMMAND test_rsa_accuracy)
    set_tests_properties(rsa_accuracy PROPERTIES TIMEOUT 300)
endif()

add_executable(test_rsa_perf
    test_rsa_perf.cpp
    ${RSA_PERF_KERNEL_SRCS})
foreach(SRC ${RSA_PERF_KERNEL_SRCS})
    set_source_files_properties(${SRC} PROPERTIES GENERATED TRUE)
endforeach()
target_include_directories(test_rsa_perf PRIVATE
    ${CMAKE_SOURCE_DIR}/runtime/include)
target_link_libraries(test_rsa_perf PRIVATE pthread)
target_compile_definitions(test_rsa_perf PRIVATE OPENCLICKNP_HAS_XRT=0)
target_compile_options(test_rsa_perf PRIVATE
    -Wno-unused-function -Wno-unused-variable -Wno-unused-but-set-variable
    -Wno-unused-label)
