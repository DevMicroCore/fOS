#include <cstdlib>
#include <iostream>
#include <string>

#include "../compiler/UiSymbols.h"
#include "../lexer/Lexer.h"
#include "../parser/Parser.h"
#include "../semantic/SemanticAnalyzer.h"

namespace {
int failures = 0;

void require(bool condition, const std::string& message)
{
  if (condition) return;
  ++failures;
  std::cerr << "FAILED: " << message << '\n';
}

fosc::SemanticAnalyzerResult analyze(const std::string& source, const std::string& ui = "")
{
  fosc::Lexer lexer(source, "test.fscript");
  fosc::LexerResult lexerResult = lexer.scan();
  require(lexerResult.success(), "semantic test source must pass lexer");
  fosc::Parser parser(std::move(lexerResult.tokens), "test.fscript");
  fosc::ParserResult parserResult = parser.parse();
  require(parserResult.success(), "semantic test source must pass parser");
  fosc::UiSymbolsResult uiResult = fosc::UiSymbols::parse(ui, "layout.ui");
  require(uiResult.success(), "semantic test UI must parse");
  fosc::SemanticAnalyzer analyzer(parserResult.program, uiResult.symbols, "test.fscript");
  return analyzer.analyze();
}

bool hasCode(const fosc::SemanticAnalyzerResult& result, const std::string& code)
{
  for (const fosc::Diagnostic& diagnostic : result.diagnostics) {
    if (diagnostic.code == code) return true;
  }
  return false;
}

void testValidProgram()
{
  const fosc::SemanticAnalyzerResult result = analyze(
    "var counter = 0\n"
    "var name = \"Max\"\n"
    "function add(a, b)\nreturn a + b\nend\n"
    "on btn_ok.click\n"
    "counter = add(counter, 1)\n"
    "if counter > 5 then\nlbl_status.text = \"Hallo \" + name\nend\n"
    "txt_name.clear()\n"
    "end\n",
    "type=button;id=btn_ok\n"
    "type=label;id=lbl_status\n"
    "type=textarea;id=txt_name\n");
  require(result.success(), "complete example should pass semantic analysis");
  require(result.globalCount == 2 && result.functionCount == 1 && result.eventCount == 1,
          "semantic summary counts differ");
}

void testTypeRules()
{
  require(hasCode(analyze("var count = 1\ncount = \"one\"\n"), "FS507"),
          "incompatible assignment should be rejected");
  require(hasCode(analyze("var bad = true + 1\n"), "FS508"),
          "invalid arithmetic types should be rejected");
  require(hasCode(analyze("if 1 then\nvar x = 0\nend\n"), "FS509"),
          "non-boolean if condition should be rejected");
  require(hasCode(analyze(
    "function mixed()\nif true then\nreturn 1\nelse\nreturn \"x\"\nend\nend\n"), "FS510"),
    "incompatible function returns should be rejected");
}

void testControlFlowRules()
{
  require(analyze(
    "var total = 0\n"
    "for var i = 1 to 10 step 2 then\n"
    "if i == 5 then\n"
    "continue\n"
    "elseif i == 9 then\n"
    "break\n"
    "else\n"
    "total = total + i\n"
    "end\n"
    "end\n"
    "while total < 20 then\n"
    "total = total + 1\n"
    "end\n").success(),
    "valid for/while/break/continue should pass semantic analysis");
  require(hasCode(analyze("break\n"), "FS521"),
          "break outside a loop should be rejected");
  require(hasCode(analyze("continue\n"), "FS521"),
          "continue outside a loop should be rejected");
  require(hasCode(analyze("while 1 then\nend\n"), "FS509"),
          "while condition must be boolean");
  require(hasCode(analyze("for var i = \"a\" to 3 then\nend\n"), "FS522"),
          "for start must be numeric");
  require(hasCode(analyze("for var i = 1 to 3 step 0 then\nend\n"), "FS522"),
          "for zero step should be rejected");
}

void testSymbolAndCallRules()
{
  require(hasCode(analyze("missing = 1\n"), "FS504"),
          "unknown variable should be rejected");
  require(hasCode(analyze("unknown()\n"), "FS505"),
          "unknown function should be rejected");
  require(hasCode(analyze("function one(a)\nreturn a\nend\none()\n"), "FS506"),
          "wrong function arity should be rejected");
  require(hasCode(analyze("var same = 1\nvar same = 2\n"), "FS501"),
          "duplicate global should be rejected");
}

void testUiRules()
{
  const std::string ui =
    "type=button;id=btn\n"
    "type=label;id=label\n"
    "type=switch;id=toggle\n";
  require(hasCode(analyze("label.checked = true\n", ui), "FS513"),
          "object-specific property rules should be checked");
  require(hasCode(analyze("toggle.checked = \"yes\"\n", ui), "FS514"),
          "UI property value type should be checked");
  require(hasCode(analyze("label.clear()\n", ui), "FS515"),
          "unsupported UI method should be rejected");
  require(hasCode(analyze("on label.changed\nend\n", ui), "FS517"),
          "unsupported UI event should be rejected");
  require(hasCode(analyze("on btn.click\nreturn 1\nend\n", ui), "FS518"),
          "event return value should be rejected");

  const std::string advancedUi =
    "type=roller;id=forecast\n"
    "type=dropdown;id=places\n"
    "type=keyboard;id=keyboard\n";
  require(analyze(
    "forecast.text = \"Today\\nTomorrow\"\n"
    "forecast.value = 0\n"
    "places.text = \"Current\\nChemnitz\"\n"
    "places.value = 1\n"
    "keyboard.hidden = true\n"
    "on places.changed\nforecast.value = places.value\nend\n"
    "on keyboard.ready\nplaces.value = 0\nend\n"
    "on keyboard.cancel\nkeyboard.hidden = true\nend\n",
    advancedUi).success(), "roller, dropdown and keyboard UI rules should be accepted");
}

void testFosApiRules()
{
  require(analyze(
    "var data = file_read(\"state.txt\")\n"
    "var saved = file_write(\"state.txt\", data)\n"
    "var playing = audio_play(\"sound.mp3\")\n"
    "var online = wifi_status()\n"
    "var started = timer.start(250)\n"
    "Serial.printf(\"online=\" + online)\n"
    "system.restart()\n"
    "var parsed = number_parse(\"12,5\")\n"
    "var label = text(parsed)\n"
    "var rounded = round(parsed)\n"
    "var length = string_length(label)\n"
    "var part = string_slice(label, 0, 2)\n"
    "var comma = string_last_index(label, \",\")\n"
    "var answer = math_eval(\"2+3*4\")\n").success(),
    "step 19 fOS APIs should pass semantic analysis");
  require(hasCode(analyze("timer.start(\"fast\")\n"), "FS520"),
          "timer.start should require an integer interval");
  require(hasCode(analyze("file_write(\"state.txt\", 1)\n"), "FS520"),
          "file_write should require string data");
  require(hasCode(analyze("Serial.printf(1)\n"), "FS520"),
          "Serial.printf should require one preformatted string");
  require(hasCode(analyze("system.restart(1)\n"), "FS519"),
          "system.restart should reject arguments");
  require(hasCode(analyze("string_slice(\"abc\", \"0\", 1)\n"), "FS520"),
          "string_slice should require integer indices");
  require(hasCode(analyze("number_parse(12)\n"), "FS520"),
          "number_parse should require a string");
  require(hasCode(analyze("string_last_index(\"abc\", 1)\n"), "FS520"),
          "string_last_index should require a search string");
}

void testCanvasAndPointerRules()
{
  const std::string ui =
    "type=canvas;id=drawing\n"
    "type=panel;id=touch_area\n";
  require(analyze(
    "on drawing.pointer_down\n"
    "var x = event_x()\n"
    "var y = event_y()\n"
    "drawing.pixel(x, y, 0)\n"
    "drawing.line(x, y, event_screen_x(), event_screen_y(), 255, 2)\n"
    "drawing.rect(1, 2, 30, 40, 255, false)\n"
    "drawing.circle(20, 20, 5, 255, true)\n"
    "drawing.draw_text(2, 2, \"ok\", 0, 20)\n"
    "end\n"
    "on touch_area.pointer_up\nvar pressed = event_pressed()\nend\n",
    ui).success(), "canvas drawing and pointer APIs should pass semantic analysis");
  require(hasCode(analyze("drawing.line(1, 2, 3)\n", ui), "FS516"),
          "canvas line should require six arguments");
  require(hasCode(analyze("drawing.draw_text(1, 2, 3, 4, 20)\n", ui), "FS520"),
          "canvas draw_text should require a string argument");
  require(hasCode(analyze("touch_area.circle(1, 2, 3, 4, true)\n", ui), "FS515"),
          "drawing methods should be canvas-only");
}

}  // namespace

int main()
{
  testValidProgram();
  testTypeRules();
  testControlFlowRules();
  testSymbolAndCallRules();
  testUiRules();
  testFosApiRules();
  testCanvasAndPointerRules();
  if (failures != 0) {
    std::cerr << failures << " semantic test(s) failed.\n";
    return EXIT_FAILURE;
  }
  std::cout << "All semantic analysis tests passed.\n";
  return EXIT_SUCCESS;
}
