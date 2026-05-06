cmake_minimum_required(VERSION 3.22)
project(OpenClickNP
    VERSION 0.1.0
    DESCRIPTION "Clean-room reimplementation of ClickNP (SIGCOMM 2016)"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# Strict warnings
add_compile_options(
    -Wall -Wextra -Wpedantic
    -Wshadow -Wconversion -Wsign-conversion
    -Wold-style-cast -Wnon-virtual-dtor -Woverloaded-virtual
    -Wno-unused-parameter
)

option(OPENCLICKNP_BUILD_TESTS    "Build unit and integration tests" ON)
option(OPENCLICKNP_BUILD_RUNTIME  "Build host runtime library (XRT)"  ON)
option(OPENCLICKNP_REQUIRE_XRT    "Fail if XRT is not found"          OFF)

if(OPENCLICKNP_BUILD_TESTS)
    enable_testing()
endif()

# -------- Subdirectories --------
add_subdirectory(compiler)

if(OPENCLICKNP_BUILD_RUNTIME)
    add_subdirectory(runtime)
endif()

if(OPENCLICKNP_BUILD_TESTS)
    add_subdirectory(tests)
endif()

if(OPENCLICKNP_BUILD_RUNTIME)
    add_subdirectory(examples)
endif()

# Optional: OpenSSL ENGINE integration (built only if OpenSSL >= 3.0
# headers are available). Provides ssl_integration/openclicknp.so so
# stock OpenSSL clients can route RSA modexp through the accelerator
# without source changes.
add_subdirectory(ssl_integration)

# -------- Install --------
install(FILES PLAN.md README.md LICENSE
        DESTINATION share/openclicknp)

# -------- Summary --------
message(STATUS "")
message(STATUS "OpenClickNP configuration:")
message(STATUS "  Build type:       ${CMAKE_BUILD_TYPE}")
message(STATUS "  C++ standard:     ${CMAKE_CXX_STANDARD}")
message(STATUS "  Build tests:      ${OPENCLICKNP_BUILD_TESTS}")
message(STATUS "  Build runtime:    ${OPENCLICKNP_BUILD_RUNTIME}")
message(STATUS "")
