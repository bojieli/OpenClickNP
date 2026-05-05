add_library(openclicknp_compiler_lib STATIC
    src/lex/lexer.cpp
    src/parse/parser.cpp
    src/ir/resolver.cpp
    src/ir/eg_builder.cpp
    src/ir/lower.cpp
    src/ir/platform.cpp
    src/analyses/port_arity.cpp
    src/analyses/cycles.cpp
    src/analyses/bandwidth.cpp
    src/analyses/autorun.cpp
    src/backends/body_rewrite.cpp
    src/backends/hls_cpp/emit.cpp
    src/backends/sw_emu/emit.cpp
    src/backends/systemc/emit.cpp
    src/backends/verilator_sim/emit.cpp
    src/backends/vpp_link/emit.cpp
    src/backends/xrt_host/emit.cpp
    src/diagnostic.cpp
    src/source.cpp
    src/driver.cpp
)

target_include_directories(openclicknp_compiler_lib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(openclicknp_compiler_lib PUBLIC cxx_std_17)

# Main binary
add_executable(openclicknp-cc src/main.cpp)
target_link_libraries(openclicknp-cc PRIVATE openclicknp_compiler_lib)

install(TARGETS openclicknp-cc DESTINATION bin)
install(TARGETS openclicknp_compiler_lib
        ARCHIVE DESTINATION lib)
install(DIRECTORY include/ DESTINATION include)
