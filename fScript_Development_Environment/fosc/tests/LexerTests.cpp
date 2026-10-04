#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

#include "../lexer/Lexer.h"

namespace {
int failures = 0;

void require(bool condition, const std::string& message)
{
  if (condition) return;
  ++failures;
  std::cerr << "FAILED: " << message << '\n';
}

std::vector<fosc::TokenType> typesOf(const fosc::LexerResult& result)
{
  std::vector<fosc::TokenType> types;
  for (const fosc::Token& token : result.tokens) types.push_back(token.type);
  return types;
}

void testVariablesAndLiterals()
{
  fosc::Lexer lexer(
    "var counter = 42\nvar value = 3.5\nvar scientific = 1.25e-3\n"
    "var name = \"Max\"\nvar active = true\nvar missing = nil\n", "variables.fscript");
  const fosc::LexerResult result = lexer.scan();
  require(result.success(), "variables: expected successful lexing");
  require(result.tokens.size() == 25, "variables: unexpected token count");
  require(result.tokens[3].type == fosc::TokenType::IntegerLiteral, "variables: integer missing");
  require(result.tokens[7].type == fosc::TokenType::FloatLiteral, "variables: float missing");
  require(result.tokens[15].literal == "Max", "variables: string value differs");
}

void testFunctionAndUiEvent()
{
  fosc::Lexer lexer(
    "function add(a, b)\nreturn a + b\nend\n"
    "on btn_ok.click\nlbl_status.text = \"Hallo\"\nend\n", "program.fscript");
  const fosc::LexerResult result = lexer.scan();
  require(result.success(), "program: expected successful lexing");
  const auto types = typesOf(result);
  require(types.front() == fosc::TokenType::Function, "program: FUNCTION missing");
  require(types[12] == fosc::TokenType::On, "program: ON missing");
  require(types.back() == fosc::TokenType::EndOfFile, "program: EOF missing");
}

void testCommentsOperatorsAndLocations()
{
  fosc::Lexer lexer(
    "-- Lua comment\r\nvar a = 1 // JS comment\r\n/* block\ncomment */\n"
    "  a = a != 0 && a <= 10 || !false\n", "comments.fscript");
  const fosc::LexerResult result = lexer.scan();
  require(result.success(), "comments: expected successful lexing");
  require(result.tokens[0].location.line == 2, "comments: wrong first token line");
  require(result.tokens[4].location.line == 5, "comments: wrong expression line");
  require(result.tokens[4].location.column == 3, "comments: wrong expression column");
}

void testStringEscapes()
{
  fosc::Lexer lexer("var text = \"A\\nB\\t\\\"C\"\n", "strings.fscript");
  const fosc::LexerResult result = lexer.scan();
  require(result.success(), "strings: expected successful lexing");
  require(result.tokens[3].literal == "A\nB\t\"C", "strings: decoded value differs");
}

void testControlFlowKeywords()
{
  fosc::Lexer lexer(
    "elseif while for to step break continue\n",
    "control.fscript");
  const fosc::LexerResult result = lexer.scan();
  require(result.success(), "control keywords: expected successful lexing");
  require(result.tokens.size() == 8, "control keywords: unexpected token count");
  require(result.tokens[0].type == fosc::TokenType::ElseIf, "control keywords: ELSEIF missing");
  require(result.tokens[1].type == fosc::TokenType::While, "control keywords: WHILE missing");
  require(result.tokens[2].type == fosc::TokenType::For, "control keywords: FOR missing");
  require(result.tokens[3].type == fosc::TokenType::To, "control keywords: TO missing");
  require(result.tokens[4].type == fosc::TokenType::Step, "control keywords: STEP missing");
  require(result.tokens[5].type == fosc::TokenType::Break, "control keywords: BREAK missing");
  require(result.tokens[6].type == fosc::TokenType::Continue, "control keywords: CONTINUE missing");
}

void requireDiagnostic(const std::string& source, const std::string& code)
{
  fosc::Lexer lexer(source, "error.fscript");
  const fosc::LexerResult result = lexer.scan();
  require(!result.success(), code + ": expected diagnostic");
  require(!result.diagnostics.empty() && result.diagnostics[0].code == code,
          code + ": wrong diagnostic");
}

void testDiagnostics()
{
  requireDiagnostic("var value = @\n", "FS001");
  requireDiagnostic("var value = \"open\n", "FS002");
  requireDiagnostic("var value = \"bad\\q\"\n", "FS003");
  requireDiagnostic("/* open", "FS004");
  requireDiagnostic("var value = 1e+\n", "FS005");
}
}  // namespace

int main()
{
  testVariablesAndLiterals();
  testFunctionAndUiEvent();
  testCommentsOperatorsAndLocations();
  testStringEscapes();
  testControlFlowKeywords();
  testDiagnostics();
  if (failures != 0) {
    std::cerr << failures << " lexer test(s) failed.\n";
    return EXIT_FAILURE;
  }
  std::cout << "All lexer tests passed.\n";
  return EXIT_SUCCESS;
}
