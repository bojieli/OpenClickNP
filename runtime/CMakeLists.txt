# Runtime: libopenclicknp_runtime.so
#
# XRT is optional at build time. If found, the XRT-backed Platform is built
# with full functionality. If not, the runtime falls back to a "stub" backend
# that lets the rest of the project compile and lets host code link, but
# refuses to actually open an FPGA at run time.

add_library(openclicknp_runtime SHARED
    src/platform.cpp
    src/element.cpp
    src/slot_bridge_xdma.cpp
    src/slot_bridge_qdma.cpp
    src/signal_rpc.cpp
    src/counters.cpp
    src/pcap.cpp
    src/lookups.cpp
)

target_include_directories(openclicknp_runtime PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(openclicknp_runtime PUBLIC cxx_std_17)

# Try to find XRT
find_path(XRT_INCLUDE_DIR
    NAMES xrt/xrt_kernel.h
    HINTS /opt/xilinx/xrt/include $ENV{XILINX_XRT}/include
)
find_library(XRT_CORE_LIB NAMES xrt_core
    HINTS /opt/xilinx/xrt/lib $ENV{XILINX_XRT}/lib)
find_library(XRT_COREUTIL_LIB NAMES xrt_coreutil
    HINTS /opt/xilinx/xrt/lib $ENV{XILINX_XRT}/lib)

if (XRT_INCLUDE_DIR AND XRT_CORE_LIB)
    target_compile_definitions(openclicknp_runtime PUBLIC OPENCLICKNP_HAS_XRT=1)
    target_include_directories(openclicknp_runtime PUBLIC ${XRT_INCLUDE_DIR})
    target_link_libraries(openclicknp_runtime PUBLIC
        ${XRT_CORE_LIB} ${XRT_COREUTIL_LIB} pthread)
    message(STATUS "  Runtime backend: XRT (${XRT_INCLUDE_DIR})")
else()
    target_compile_definitions(openclicknp_runtime PUBLIC OPENCLICKNP_HAS_XRT=0)
    target_link_libraries(openclicknp_runtime PUBLIC pthread)
    if (OPENCLICKNP_REQUIRE_XRT)
        message(FATAL_ERROR "XRT not found and OPENCLICKNP_REQUIRE_XRT=ON")
    else()
        message(STATUS "  Runtime backend: stub (XRT not found)")
    endif()
endif()

install(TARGETS openclicknp_runtime LIBRARY DESTINATION lib)
install(DIRECTORY include/ DESTINATION include)

if (OPENCLICKNP_BUILD_TESTS)
    add_subdirectory(tests)
endif()
