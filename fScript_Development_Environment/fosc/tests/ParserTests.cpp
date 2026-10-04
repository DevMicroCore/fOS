#include <cstdlib>
#include <iostream>
#include <string>

#include "../ast/Ast.h"
#include "../lexer/Lexer.h"
#include "../parser/Parser.h"

namespace {
int failures = 0;

void require(bool condition, const std::string& message)
{
  if (condition) return;
  ++failures;
  std::cerr << "FAILED: " << message << '\n';
}

fosc::ParserResult parseSource(const std::string& source)
{
  fosc::Lexer lexer(source, "test.fscript");
  fosc::LexerResult lexerResult = lexer.scan();
  require(lexerResult.success(), "test source must pass lexer");
  fosc::Parser parser(std::move(lexerResult.tokens), "test.fscript");
  return parser.parse();
}

void testCompleteProgram()
{
  fosc::ParserResult result = parseSource(
    "var counter = 0\n"
    "function add(a, b)\nreturn a + b\nend\n"
    "on btn_ok.click\n"
    "counter = add(counter, 1)\n"
    "if counter > 5 then\n"
    "lbl_status.text = \"Hallo\"\n"
    "else\nlbl_status.text = \"Warten\"\nend\n"
    "end\n");

  require(result.success(), "complete program should parse");
  require(result.program.statements.size() == 3, "complete program statement count");
  require(result.program.statements[0]->kind == fosc::StatementKind::VariableDeclaration,
          "first statement should be variable");
  require(result.program.statements[1]->kind == fosc::StatementKind::FunctionDeclaration,
          "second statement should be function");
  require(result.program.statements[2]->kind == fosc::StatementKind::EventDeclaration,
          "third statement should be event");

  const auto& event = static_cast<const fosc::EventDeclaration&>(*result.program.statements[2]);
  require(event.objectName == "btn_ok" && event.eventName == "click", "event reference differs");
  require(event.body.size() == 2, "event body statement count");
  require(event.body[1]->kind == fosc::StatementKind::If, "event should contain if");
}

void testOperatorPrecedence()
{
  fosc::ParserResult result = parseSource("var result = 1 + 2 * 3 == 7 or false\n");
  require(result.success(), "precedence expression should parse");
  const auto& declaration = static_cast<const fosc::VariableDeclaration&>(*result.program.statements[0]);
  const auto& logicalOr = static_cast<const fosc::BinaryExpression&>(*declaration.initializer);
  require(logicalOr.operation == fosc::TokenType::Or, "outer operator should be OR");
  const auto& equality = static_cast<const fosc::BinaryExpression&>(*logicalOr.left);
  require(equality.operation == fosc::TokenType::EqualEqual, "equality precedence differs");
  const auto& addition = static_cast<const fosc::BinaryExpression&>(*equality.left);
  require(addition.operation == fosc::TokenType::Plus, "addition precedence differs");
  const auto& multiplication = static_cast<const fosc::BinaryExpression&>(*addition.right);
  require(multiplication.operation == fosc::TokenType::Star, "multiplication precedence differs");
}

void testMemberCallAndAssignment()
{
  fosc::ParserResult result = parseSource(
    "txt_name.clear()\n"
    "lbl_status.text = add(5, 10)\n");
  require(result.success(), "member calls and assignments should parse");
  require(result.program.statements.size() == 2, "member program statement count");
  const auto& first = static_cast<const fosc::ExpressionStatement&>(*result.program.statements[0]);
  require(first.expression->kind == fosc::ExpressionKind::Call, "first expression should be call");
  const auto& second = static_cast<const fosc::ExpressionStatement&>(*result.program.statements[1]);
  require(second.expression->kind == fosc::ExpressionKind::Assignment,
          "second expression should be assignment");
}

void testControlFlowStatements()
{
  fosc::ParserResult result = parseSource(
    "var total = 0\n"
    "for var i = 1 to 10 step 2 then\n"
    "if i == 3 then\n"
    "continue\n"
    "elseif i == 9 then\n"
    "break\n"
    "else\n"
    "total = total + i\n"
    "end\n"
    "end\n"
    "while total < 20 then\n"
    "total = total + 1\n"
    "end\n");

  require(result.success(), "control flow program should parse");
  require(result.program.statements.size() == 3, "control flow statement count");
  require(result.program.statements[1]->kind == fosc::StatementKind::For,
          "second statement should be for");
  const auto& loop = static_cast<const fosc::ForStatement&>(*result.program.statements[1]);
  require(loop.declaresVariable && loop.variableName == "i", "for loop variable differs");
  require(loop.body.size() == 1 && loop.body[0]->kind == fosc::StatementKind::If,
          "for body should contain if");
  const auto& conditional = static_cast<const fosc::IfStatement&>(*loop.body[0]);
  require(conditional.elseIfBranches.size() == 1, "elseif branch should be parsed");
  require(result.program.statements[2]->kind == fosc::StatementKind::While,
          "third statement should be while");
}

void requireDiagnostic(const std::string& source, const std::string& code)
{
  fosc::ParserResult result = parseSource(source);
  require(!result.success(), code + ": expected parser failure");
  bool found = false;
  for (const fosc::Diagnostic& diagnostic : result.diagnostics) {
    if (diagnostic.code == code) found = true;
  }
  require(found, code + ": expected diagnostic code");
}

void testDiagnostics()
{
  requireDiagnostic("if true var a = 1 end\n", "FS202");
  requireDiagnostic("add(1) = 2\n", "FS203");
  requireDiagnostic("on button\nend\n", "FS205");
  requireDiagnostic("else\n", "FS201");
  requireDiagnostic("var incomplete =", "FS201");
}

void testBlockRecoveryPreservesEnd()
{
  fosc::ParserResult result = parseSource(
    "function broken()\n"
    "var incomplete =\n"
    "end\n"
    "var after = 2\n");

  require(!result.success(), "broken function should produce a diagnostic");
  require(result.program.statements.size() == 2,
          "block recovery should retain function and following declaration");
  require(result.program.statements[0]->kind == fosc::StatementKind::FunctionDeclaration,
          "recovered first statement should remain a function");
  require(result.program.statements[1]->kind == fosc::StatementKind::VariableDeclaration,
          "statement after recovered end should remain top-level");
}
}  // namespace

int main()
{
  testCompleteProgram();
  testOperatorPrecedence();
  testMemberCallAndAssignment();
  testControlFlowStatements();
  testDiagnostics();
  testBlockRecoveryPreservesEnd();
  if (failures != 0) {
    std::cerr << failures << " parser test(s) failed.\n";
    return EXIT_FAILURE;
  }
  std::cout << "All parser tests passed.\n";
  return EXIT_SUCCESS;
}
