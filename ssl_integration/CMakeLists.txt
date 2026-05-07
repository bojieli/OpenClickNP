# SPDX-License-Identifier: Apache-2.0
# Build the OpenSSL ENGINE that exposes openclicknp::bigint::modexp.
#
# Result: ssl_integration/openclicknp.so — drop in OpenSSL's engines/
# directory or load explicitly with `openssl ... -engine ./openclicknp.so`.

find_package(OpenSSL 3.0)
if(NOT OpenSSL_FOUND)
    message(STATUS "OpenSSL >= 3.0 not found — ssl_integration disabled")
    return()
endif()

enable_language(C)

add_library(openclicknp_engine SHARED
    openclicknp_engine.c
    openclicknp_modexp.cpp
    openclicknp_aes_shim.cpp)
set_target_properties(openclicknp_engine PROPERTIES
    OUTPUT_NAME openclicknp
    PREFIX "")
target_include_directories(openclicknp_engine PRIVATE
    ${CMAKE_SOURCE_DIR}/runtime/include)
target_link_libraries(openclicknp_engine PRIVATE
    OpenSSL::Crypto)
target_compile_definitions(openclicknp_engine PRIVATE
    OPENSSL_API_COMPAT=0x10101000L)
target_compile_options(openclicknp_engine PRIVATE
    -Wno-deprecated-declarations)  # ENGINE API is deprecated in 3.x

# Standalone demo: encrypts + verifies a sample plaintext via the engine.
add_executable(test_openssl_engine test_openssl_engine.c)
target_include_directories(test_openssl_engine PRIVATE
    ${CMAKE_SOURCE_DIR}/runtime/include)
target_link_libraries(test_openssl_engine PRIVATE
    OpenSSL::Crypto)
target_compile_definitions(test_openssl_engine PRIVATE
    ENGINE_PATH="${CMAKE_CURRENT_BINARY_DIR}/openclicknp.so")
target_compile_options(test_openssl_engine PRIVATE
    -Wno-deprecated-declarations)
add_test(NAME openssl_engine_smoke
         COMMAND test_openssl_engine)
set_tests_properties(openssl_engine_smoke PROPERTIES
    DEPENDS openclicknp_engine
    TIMEOUT 60)
