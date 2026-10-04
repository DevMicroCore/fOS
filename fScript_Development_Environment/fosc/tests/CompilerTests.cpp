#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <string>

#include "../bytecode/Bytecode.h"
#include "../compiler/CodeGenerator.h"
#include "../compiler/UiSymbols.h"
#include "../format/Disassembler.h"
#include "../format/FAppFile.h"
#include "../lexer/Lexer.h"
#include "../parser/Parser.h"
#include "../verifier/BytecodeVerifier.h"

namespace {
int failures = 0;

void require(bool condition, const std::string& message)
{
  if (condition) return;
  ++failures;
  std::cerr << "FAILED: " << message << '\n';
}

struct CompileResult {
  fosc::CodeGeneratorResult generated;
  fosc::UiSymbolsResult ui;
};

CompileResult compile(const std::string& source, const std::string& ui)
{
  fosc::Lexer lexer(source, "test.fscript");
  fosc::LexerResult lexerResult = lexer.scan();
  require(lexerResult.success(), "compiler test source must pass lexer");
  fosc::Parser parser(std::move(lexerResult.tokens), "test.fscript");
  fosc::ParserResult parserResult = parser.parse();
  require(parserResult.success(), "compiler test source must pass parser");
  fosc::UiSymbolsResult uiResult = fosc::UiSymbols::parse(ui, "layout.ui");
  fosc::CodeGenerator generator(
    parserResult.program,
    uiResult.symbols,
    "test.fscript",
    source);
  return {generator.generate(), std::move(uiResult)};
}

void testUiSymbolOrder()
{
  const fosc::UiSymbolsResult result = fosc::UiSymbols::parse(
    "type=button;id=btn_ok\n"
    "type=unknown;id=ignored\n"
    "type=label;id=lbl_status\n"
    "type=roller;id=forecast\n"
    "type=dropdown;id=places\n"
    "type=keyboard;id=keyboard\n",
    "layout.ui");
  require(result.success(), "valid UI symbols should parse");
  require(result.symbols.at("btn_ok").id == 1, "first named UI object should be #1");
  require(result.symbols.at("lbl_status").id == 2, "unsupported objects must not consume IDs");
  require(result.symbols.at("forecast").id == 3, "roller should receive a stable UI ID");
  require(result.symbols.at("places").id == 4, "dropdown should receive a stable UI ID");
  require(result.symbols.at("keyboard").id == 5, "keyboard should receive a stable UI ID");

  const fosc::UiSymbolsResult duplicate = fosc::UiSymbols::parse(
    "type=button;id=same\ntype=label;id=same\n",
    "layout.ui");
  require(!duplicate.success() && duplicate.diagnostics[0].code == "FS302",
          "duplicate UI IDs should fail deterministically");
}

void testCompleteCompilationAndRoundTrip()
{
  const std::string source =
    "var counter = 0\n"
    "function add(a, b)\nreturn a + b\nend\n"
    "on btn_ok.click\n"
    "counter = add(counter, 1)\n"
    "if counter > 5 then\n"
    "lbl_status.text = \"Hallo\"\n"
    "else\nlbl_status.text = \"Warten\"\nend\n"
    "txt_name.clear()\n"
    "end\n";
  const std::string ui =
    "type=button;id=btn_ok\n"
    "type=label;id=lbl_status\n"
    "type=textarea;id=txt_name\n";
  CompileResult result = compile(source, ui);
  require(result.ui.success(), "compiler UI should parse");
  require(result.generated.success(), "complete program should generate bytecode");
  require(result.generated.module.globalCount == 1, "global count differs");
  require(result.generated.module.functions.size() == 3, "init, function and event expected");
  require(result.generated.module.events.size() == 1, "one event binding expected");
  require(result.generated.module.events[0].objectId == 1, "event must use numeric UI ID #1");
  require(result.generated.module.events[0].functionIndex == 2, "event function index differs");

  fosc::FAppWriteResult written = fosc::FAppFile::write(result.generated.module);
  require(written.success(), "valid bytecode module should serialize");
  require(written.image.size() > fosc::fapp::kHeaderSize, "fAPP should contain sections");

  fosc::FAppReadResult read = fosc::FAppFile::read(written.image);
  require(read.success(), "serialized fAPP should pass validation");
  require(read.file.header.formatMajor == 1 && read.file.header.bytecodeMajor == 1,
          "format versions differ");
  require(read.file.header.globalCount == 1, "round-trip global count differs");
  require(read.file.events.size() == 1 && read.file.events[0].objectId == 1,
          "round-trip event differs");

  const std::string disassembly = fosc::Disassembler::disassemble(read.file);
  require(disassembly.find("Function #0 <init>") != std::string::npos,
          "disassembly should name init function");
  require(disassembly.find("SET_UI_PROPERTY object #2.text") != std::string::npos,
          "disassembly should contain numeric UI property access");
  require(disassembly.find("CALL_UI_METHOD object #3.clear") != std::string::npos,
          "disassembly should contain numeric UI method call");

  const std::string imageText(written.image.begin(), written.image.end());
  require(imageText.find("btn_ok") == std::string::npos,
          "UI source names must not be stored in the fAPP image");
  require(imageText.find("lbl_status") == std::string::npos,
          "UI source names must not be stored in the fAPP image");
}

void testCrcRejectsModification()
{
  CompileResult result = compile("var answer = 42\n", "");
  require(result.generated.success(), "minimal program should compile");
  fosc::FAppWriteResult written = fosc::FAppFile::write(result.generated.module);
  require(written.success(), "minimal program should serialize");
  written.image.back() ^= 0x01u;
  fosc::FAppReadResult read = fosc::FAppFile::read(std::move(written.image));
  require(!read.success() && read.error.find("FS414") != std::string::npos,
          "modified fAPP must fail CRC validation");
}

void testUnknownUiObjectDiagnostic()
{
  CompileResult result = compile(
    "on missing.click\nmissing.text = \"x\"\nend\n",
    "type=button;id=present\n");
  require(!result.generated.success(), "unknown UI object should fail compilation");
  bool found = false;
  for (const fosc::Diagnostic& diagnostic : result.generated.diagnostics) {
    if (diagnostic.code == "FS309" || diagnostic.code == "FS320") found = true;
  }
  require(found, "unknown UI object diagnostic should identify the problem");
}

void testControlFlowCompilation()
{
  CompileResult result = compile(
    "var total = 0\n"
    "for var i = 1 to 5 then\n"
    "if i == 2 then\n"
    "continue\n"
    "elseif i == 4 then\n"
    "break\n"
    "else\n"
    "total = total + i\n"
    "end\n"
    "end\n"
    "while total < 10 then\n"
    "total = total + 1\n"
    "end\n",
    "");
  require(result.generated.success(), "control flow should generate bytecode");
  fosc::FAppWriteResult written = fosc::FAppFile::write(result.generated.module);
  require(written.success(), "control flow bytecode should serialize");
  fosc::FAppReadResult read = fosc::FAppFile::read(std::move(written.image));
  require(read.success(), "control flow bytecode should round-trip");
  require(fosc::BytecodeVerifier::verify(read.file).success(),
          "control flow bytecode should verify");
  const std::string disassembly = fosc::Disassembler::disassemble(read.file);
  require(disassembly.find("JUMP_IF_FALSE") != std::string::npos,
          "control flow should contain conditional jumps");
  require(disassembly.find("JUMP") != std::string::npos,
          "control flow should contain jumps");
}

void testNativeHttpCompilation()
{
  CompileResult result = compile(
    "var ok = http_get(\"https://example.com/data.json\")\n"
    "var status = http_status()\n"
    "var temperature = http_json(\"current.temperature_2m\")\n"
    "var chunk = http_text(0)\n"
    "var query = url_encode(\"Nova Gorica\")\n"
    "var weekday = date_weekday(\"2026-08-04\")\n",
    "");
  require(result.generated.success(), "native HTTP functions should compile");
  fosc::FAppWriteResult written = fosc::FAppFile::write(result.generated.module);
  require(written.success(), "native HTTP bytecode should serialize");
  fosc::FAppReadResult read = fosc::FAppFile::read(std::move(written.image));
  require(read.success(), "native HTTP bytecode should round-trip");
  require(fosc::BytecodeVerifier::verify(read.file).success(), "native HTTP bytecode should verify");
  const std::string disassembly = fosc::Disassembler::disassemble(read.file);
  require(disassembly.find("CALL_NATIVE http_get argc=1") != std::string::npos,
          "disassembly should expose http_get");
  require(disassembly.find("CALL_NATIVE http_json argc=1") != std::string::npos,
          "disassembly should expose http_json");
  require(disassembly.find("CALL_NATIVE date_weekday argc=1") != std::string::npos,
          "disassembly should expose date_weekday");
}

void testFosApiCompilation()
{
  CompileResult result = compile(
    "var data = file_read(\"state.txt\")\n"
    "var saved = file_write(\"state.txt\", \"ready\")\n"
    "var playing = audio_play(\"sound.mp3\")\n"
    "var online = wifi_status()\n"
    "var timer_ok = timer.start(1000)\n"
    "Serial.printf(\"online=\" + online + \"\\n\")\n"
    "system.restart()\n"
    "var parsed = number_parse(\"12,5\")\n"
    "var label = text(parsed)\n"
    "var rounded = round(parsed)\n"
    "var length = string_length(label)\n"
    "var part = string_slice(label, 0, 2)\n"
    "var comma = string_last_index(label, \",\")\n"
    "var answer = math_eval(\"2+3*4\")\n",
    "");
  require(result.generated.success(), "step 19 fOS APIs should compile");
  const fosc::FAppWriteResult written = fosc::FAppFile::write(result.generated.module);
  require(written.success(), "step 19 native bytecode should serialize");
  const fosc::FAppReadResult read = fosc::FAppFile::read(written.image);
  require(read.success(), "step 19 native bytecode should round-trip");
  const std::string disassembly = fosc::Disassembler::disassemble(read.file);
  require(disassembly.find("CALL_NATIVE file_read argc=1") != std::string::npos,
          "disassembly should expose file_read");
  require(disassembly.find("CALL_NATIVE system.restart argc=0") != std::string::npos,
          "disassembly should expose system.restart");
  require(disassembly.find("CALL_NATIVE timer.start argc=1") != std::string::npos,
          "disassembly should expose timer.start");
  require(disassembly.find("CALL_NATIVE Serial.printf argc=1") != std::string::npos,
          "disassembly should expose Serial.printf");
  require(disassembly.find("CALL_NATIVE text argc=1") != std::string::npos,
          "disassembly should expose text");
  require(disassembly.find("CALL_NATIVE number_parse argc=1") != std::string::npos,
          "disassembly should expose number_parse");
  require(disassembly.find("CALL_NATIVE string_slice argc=3") != std::string::npos,
          "disassembly should expose string_slice");
  require(disassembly.find("CALL_NATIVE math_eval argc=1") != std::string::npos,
          "disassembly should expose math_eval");
  require(disassembly.find("CALL_NATIVE string_last_index argc=2") != std::string::npos,
          "disassembly should expose string_last_index");
}

}  // namespace

int main()
{
  testUiSymbolOrder();
  testCompleteCompilationAndRoundTrip();
  testCrcRejectsModification();
  testUnknownUiObjectDiagnostic();
  testControlFlowCompilation();
  testNativeHttpCompilation();
  testFosApiCompilation();
  if (failures != 0) {
    std::cerr << failures << " compiler test(s) failed.\n";
    return EXIT_FAILURE;
  }
  std::cout << "All compiler and fAPP format tests passed.\n";
  return EXIT_SUCCESS;
}
