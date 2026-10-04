cmake_minimum_required(VERSION 3.16)
project(fosc VERSION 0.12.1 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
add_library(fosc_frontend
  ast/AstPrinter.cpp
  bytecode/Bytecode.cpp
  compiler/CodeGenerator.cpp
  compiler/UiSymbols.cpp
  diagnostics/Diagnostic.cpp
  format/Disassembler.cpp
  format/FAppFile.cpp
  lexer/Lexer.cpp
  lexer/Token.cpp
  optimizer/ConstantFolder.cpp
  parser/Parser.cpp
  semantic/SemanticAnalyzer.cpp
  semantic/Type.cpp
  verifier/BytecodeVerifier.cpp
)
target_include_directories(fosc_frontend PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
if(MSVC)
  target_compile_options(fosc_frontend PRIVATE /W4)
else()
  target_compile_options(fosc_frontend PRIVATE -Wall -Wextra -Wpedantic)
endif()
add_executable(fosc main.cpp)
target_link_libraries(fosc PRIVATE fosc_frontend)
add_executable(fosrun
  runner/FosRun.cpp
  ../../fOS4.0/src/fapp/FAppLoader.cpp
  ../../fOS4.0/src/fapp/FAppRuntime.cpp
  ../../fOS4.0/src/fapp/FAppManifest.cpp
)
target_include_directories(fosrun BEFORE PRIVATE
  tests/arduino_stubs
  ../../fOS4.0
)
target_link_libraries(fosrun PRIVATE fosc_frontend)
enable_testing()
add_executable(fosc_lexer_tests tests/LexerTests.cpp)
target_link_libraries(fosc_lexer_tests PRIVATE fosc_frontend)
add_test(NAME lexer COMMAND fosc_lexer_tests)
add_executable(fosc_parser_tests tests/ParserTests.cpp)
target_link_libraries(fosc_parser_tests PRIVATE fosc_frontend)
add_test(NAME parser COMMAND fosc_parser_tests)
add_executable(fosc_compiler_tests tests/CompilerTests.cpp)
target_link_libraries(fosc_compiler_tests PRIVATE fosc_frontend)
add_test(NAME compiler COMMAND fosc_compiler_tests)
add_executable(fosc_semantic_tests tests/SemanticTests.cpp)
target_link_libraries(fosc_semantic_tests PRIVATE fosc_frontend)
add_test(NAME semantic COMMAND fosc_semantic_tests)
add_executable(fosc_optimizer_verifier_tests tests/OptimizerVerifierTests.cpp)
target_link_libraries(fosc_optimizer_verifier_tests PRIVATE fosc_frontend)
add_test(NAME optimizer_verifier COMMAND fosc_optimizer_verifier_tests)
add_executable(fosc_embedded_loader_tests
  tests/EmbeddedLoaderTests.cpp
  ../../fOS4.0/src/fapp/FAppLoader.cpp
)
target_include_directories(fosc_embedded_loader_tests BEFORE PRIVATE
  tests/arduino_stubs
  ../../fOS4.0
)
target_link_libraries(fosc_embedded_loader_tests PRIVATE fosc_frontend)
add_test(NAME embedded_loader COMMAND fosc_embedded_loader_tests)
add_executable(fosc_runtime_manifest_tests
  tests/RuntimeManifestTests.cpp
  ../../fOS4.0/src/fapp/FAppLoader.cpp
  ../../fOS4.0/src/fapp/FAppRuntime.cpp
  ../../fOS4.0/src/fapp/FAppManifest.cpp
)
target_include_directories(fosc_runtime_manifest_tests BEFORE PRIVATE
  tests/arduino_stubs
  ../../fOS4.0
)
target_link_libraries(fosc_runtime_manifest_tests PRIVATE fosc_frontend)
add_test(NAME runtime_manifest COMMAND fosc_runtime_manifest_tests)
add_executable(fosc_http_api_tests
  tests/HttpApiTests.cpp
  ../../fOS4.0/src/fapp/FAppHttpApi.cpp
)
target_include_directories(fosc_http_api_tests BEFORE PRIVATE
  tests/http_stubs
  tests/arduino_stubs
  ../../fOS4.0
)
add_test(NAME http_api COMMAND fosc_http_api_tests)
