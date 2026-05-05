# Unit tests for the compiler frontend (lex/parse/IR).
add_executable(test_compiler_smoke test_compiler_smoke.cpp)
target_link_libraries(test_compiler_smoke PRIVATE openclicknp_compiler_lib)
add_test(NAME compiler_smoke COMMAND test_compiler_smoke)

add_executable(test_lexer test_lexer.cpp)
target_link_libraries(test_lexer PRIVATE openclicknp_compiler_lib)
add_test(NAME lexer COMMAND test_lexer)

add_executable(test_parser test_parser.cpp)
target_link_libraries(test_parser PRIVATE openclicknp_compiler_lib)
add_test(NAME parser COMMAND test_parser)

add_executable(test_resolver test_resolver.cpp)
target_link_libraries(test_resolver PRIVATE openclicknp_compiler_lib)
add_test(NAME resolver COMMAND test_resolver)

add_executable(test_analyses test_analyses.cpp)
target_link_libraries(test_analyses PRIVATE openclicknp_compiler_lib)
add_test(NAME analyses COMMAND test_analyses)
