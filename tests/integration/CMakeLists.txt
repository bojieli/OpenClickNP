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
