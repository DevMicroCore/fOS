CXX ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -Wpedantic
BUILD_DIR := build
TOOLCHAIN_SOURCES := ast/AstPrinter.cpp bytecode/Bytecode.cpp compiler/CodeGenerator.cpp compiler/UiSymbols.cpp diagnostics/Diagnostic.cpp format/Disassembler.cpp format/FAppFile.cpp lexer/Lexer.cpp lexer/Token.cpp optimizer/ConstantFolder.cpp parser/Parser.cpp semantic/SemanticAnalyzer.cpp semantic/Type.cpp verifier/BytecodeVerifier.cpp
.PHONY: all test clean lvgl_adapter_check fosrun_smoke_test fosrun_ui_smoke_test fosrun_calculator_test
all: $(BUILD_DIR)/fosc $(BUILD_DIR)/fosrun
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)
$(BUILD_DIR)/fosc: main.cpp Version.h $(TOOLCHAIN_SOURCES) | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -I. main.cpp $(TOOLCHAIN_SOURCES) -o $@
$(BUILD_DIR)/fosrun: runner/FosRun.cpp $(TOOLCHAIN_SOURCES) ../../fOS_Waveshare_P4/src/fapp/FAppLoader.cpp ../../fOS_Waveshare_P4/src/fapp/FAppRuntime.cpp ../../fOS_Waveshare_P4/src/fapp/FAppManifest.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -Itests/arduino_stubs -I. -I../../fOS_Waveshare_P4 runner/FosRun.cpp $(TOOLCHAIN_SOURCES) ../../fOS_Waveshare_P4/src/fapp/FAppLoader.cpp ../../fOS_Waveshare_P4/src/fapp/FAppRuntime.cpp ../../fOS_Waveshare_P4/src/fapp/FAppManifest.cpp -o $@
$(BUILD_DIR)/fosc_lexer_tests: tests/LexerTests.cpp $(TOOLCHAIN_SOURCES) | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -I. tests/LexerTests.cpp $(TOOLCHAIN_SOURCES) -o $@
$(BUILD_DIR)/fosc_parser_tests: tests/ParserTests.cpp $(TOOLCHAIN_SOURCES) | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -I. tests/ParserTests.cpp $(TOOLCHAIN_SOURCES) -o $@
$(BUILD_DIR)/fosc_compiler_tests: tests/CompilerTests.cpp $(TOOLCHAIN_SOURCES) | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -I. tests/CompilerTests.cpp $(TOOLCHAIN_SOURCES) -o $@
$(BUILD_DIR)/fosc_semantic_tests: tests/SemanticTests.cpp $(TOOLCHAIN_SOURCES) | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -I. tests/SemanticTests.cpp $(TOOLCHAIN_SOURCES) -o $@
$(BUILD_DIR)/fosc_optimizer_verifier_tests: tests/OptimizerVerifierTests.cpp $(TOOLCHAIN_SOURCES) | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -I. tests/OptimizerVerifierTests.cpp $(TOOLCHAIN_SOURCES) -o $@
$(BUILD_DIR)/fosc_embedded_loader_tests: tests/EmbeddedLoaderTests.cpp tests/arduino_stubs/FS.h bytecode/Bytecode.cpp format/FAppFile.cpp ../../fOS_Waveshare_P4/src/fapp/FAppLoader.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -Itests/arduino_stubs -I. -I../../fOS_Waveshare_P4 tests/EmbeddedLoaderTests.cpp bytecode/Bytecode.cpp format/FAppFile.cpp ../../fOS_Waveshare_P4/src/fapp/FAppLoader.cpp -o $@
$(BUILD_DIR)/fosc_runtime_manifest_tests: tests/RuntimeManifestTests.cpp tests/arduino_stubs/FS.h $(TOOLCHAIN_SOURCES) ../../fOS_Waveshare_P4/src/fapp/FAppLoader.cpp ../../fOS_Waveshare_P4/src/fapp/FAppRuntime.cpp ../../fOS_Waveshare_P4/src/fapp/FAppManifest.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -Itests/arduino_stubs -I. -I../../fOS_Waveshare_P4 tests/RuntimeManifestTests.cpp $(TOOLCHAIN_SOURCES) ../../fOS_Waveshare_P4/src/fapp/FAppLoader.cpp ../../fOS_Waveshare_P4/src/fapp/FAppRuntime.cpp ../../fOS_Waveshare_P4/src/fapp/FAppManifest.cpp -o $@
$(BUILD_DIR)/fosc_http_api_tests: tests/HttpApiTests.cpp tests/http_stubs/Arduino.h tests/http_stubs/HTTPClient.h ../../fOS_Waveshare_P4/src/fapp/FAppHttpApi.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -Itests/http_stubs -Itests/arduino_stubs -I. -I../../fOS_Waveshare_P4 tests/HttpApiTests.cpp ../../fOS_Waveshare_P4/src/fapp/FAppHttpApi.cpp -o $@
$(BUILD_DIR)/fosc_storage_access_tests: tests/StorageAccessTests.cpp ../../fOS_Waveshare_P4/src/fapp/FAppPath.h ../../fOS_Waveshare_P4/src/fapp/FAppStoragePolicy.h ../../fOS_Waveshare_P4/src/fapp/FAppManifest.cpp ../../fOS_Waveshare_P4/src/fapp/FAppHttpApi.cpp ../../fOS_Waveshare_P4/src/fapp/LvglFAppUiApi.cpp ../../fOS_Waveshare_P4/src/fapp/FAppKeyboardController.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -Itests/http_stubs -Itests/keyboard_lvgl -Itests/lvgl_stubs -I../../fOS_Waveshare_P4 tests/StorageAccessTests.cpp ../../fOS_Waveshare_P4/src/fapp/FAppManifest.cpp ../../fOS_Waveshare_P4/src/fapp/FAppHttpApi.cpp ../../fOS_Waveshare_P4/src/fapp/LvglFAppUiApi.cpp ../../fOS_Waveshare_P4/src/fapp/FAppUiRegistry.cpp ../../fOS_Waveshare_P4/src/fapp/FAppKeyboardController.cpp -o $@
lvgl_adapter_check:
	$(CXX) $(CXXFLAGS) -fsyntax-only -Itests/lvgl_stubs -Itests/arduino_stubs -I. -I../../fOS_Waveshare_P4 ../../fOS_Waveshare_P4/src/fapp/FAppUiRegistry.cpp ../../fOS_Waveshare_P4/src/fapp/LvglFAppUiApi.cpp ../../fOS_Waveshare_P4/src/fapp/FAppEventBridge.cpp ../../fOS_Waveshare_P4/src/fapp/FAppKeyboardController.cpp
fosrun_smoke_test: $(BUILD_DIR)/fosc $(BUILD_DIR)/fosrun
	./$(BUILD_DIR)/fosc build examples/fosrun_demo.fscript --ui examples/fosrun_demo.ui -o $(BUILD_DIR)/fosrun_demo.fapp
	./$(BUILD_DIR)/fosrun $(BUILD_DIR)/fosrun_demo.fapp --ui examples/fosrun_demo.ui --event btn_add.click | grep 'lbl_status .*text="Count: 1"'
fosrun_ui_smoke_test: fosrun_smoke_test
	@set -e; tmp_log=$$(mktemp); ./$(BUILD_DIR)/fosrun $(BUILD_DIR)/fosrun_demo.fapp --ui examples/fosrun_demo.ui --serve --port 9876 >$$tmp_log 2>&1 & pid=$$!; trap 'kill $$pid 2>/dev/null || true' EXIT; sleep 1; curl -fsS http://127.0.0.1:9876/ | grep '<title>fosrun UI</title>'; curl -fsS 'http://127.0.0.1:9876/event?name=btn_add.click' | grep 'Count: 1'; kill $$pid; wait $$pid 2>/dev/null || true; trap - EXIT
fosrun_calculator_test: $(BUILD_DIR)/fosc $(BUILD_DIR)/fosrun
	./$(BUILD_DIR)/fosc build "../../fOS_Waveshare_P4/example app/apps/fscript_calculator/main.fscript" --ui "../../fOS_Waveshare_P4/example app/apps/fscript_calculator/layout.ui" -o $(BUILD_DIR)/fscript_calculator_test.fapp
	./$(BUILD_DIR)/fosrun $(BUILD_DIR)/fscript_calculator_test.fapp --ui "../../fOS_Waveshare_P4/example app/apps/fscript_calculator/layout.ui" --event btn_7.click --event btn_plus.click --event btn_8.click --event btn_equals.click | grep 'txt_display .*text="15"'
test: $(BUILD_DIR)/fosc_storage_access_tests $(BUILD_DIR)/fosc_lexer_tests $(BUILD_DIR)/fosc_parser_tests $(BUILD_DIR)/fosc_semantic_tests $(BUILD_DIR)/fosc_compiler_tests $(BUILD_DIR)/fosc_optimizer_verifier_tests $(BUILD_DIR)/fosc_embedded_loader_tests $(BUILD_DIR)/fosc_runtime_manifest_tests $(BUILD_DIR)/fosc_http_api_tests lvgl_adapter_check fosrun_smoke_test fosrun_ui_smoke_test fosrun_calculator_test
	./$(BUILD_DIR)/fosc_storage_access_tests
	./$(BUILD_DIR)/fosc_lexer_tests
	./$(BUILD_DIR)/fosc_parser_tests
	./$(BUILD_DIR)/fosc_semantic_tests
	./$(BUILD_DIR)/fosc_compiler_tests
	./$(BUILD_DIR)/fosc_optimizer_verifier_tests
	./$(BUILD_DIR)/fosc_embedded_loader_tests
	./$(BUILD_DIR)/fosc_runtime_manifest_tests
	./$(BUILD_DIR)/fosc_http_api_tests
clean:
	$(RM) -r $(BUILD_DIR)

# Rebuild host firmware adapters when the firmware version changes.
$(BUILD_DIR)/fosrun $(BUILD_DIR)/fosc_embedded_loader_tests $(BUILD_DIR)/fosc_runtime_manifest_tests $(BUILD_DIR)/fosc_http_api_tests $(BUILD_DIR)/fosc_storage_access_tests: ../../fOS_Waveshare_P4/src/core/FOSVersion.h

$(BUILD_DIR)/fosrun: ../../fOS_Waveshare_P4/src/fapp/FAppLayoutUnits.h
