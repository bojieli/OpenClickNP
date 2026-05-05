# Integration tests: end-to-end compiler runs and the SW emulator backend.
add_executable(test_e2e_passtraffic test_e2e_passtraffic.cpp)
target_link_libraries(test_e2e_passtraffic PRIVATE openclicknp_compiler_lib)
add_test(NAME e2e_passtraffic COMMAND test_e2e_passtraffic)
