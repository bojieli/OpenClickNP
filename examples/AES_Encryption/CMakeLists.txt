add_executable(aes_host host.cpp)
target_link_libraries(aes_host PRIVATE openclicknp_runtime)
