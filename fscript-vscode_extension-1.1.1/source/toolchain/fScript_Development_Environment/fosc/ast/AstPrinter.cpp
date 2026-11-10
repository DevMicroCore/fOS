#include "AstPrinter.h"

#include <sstream>

namespace fosc {

std::string AstPrinter::print(const Program& program) const
{
  std::string output;
  appendLine(&output, 0, "Program");
  for (const StatementPtr& statement : program.statements) {
    printStatement(*statement, &output, 1);
  }
  return output;
}

void AstPrinter::printStatement(const Statement& statement, std::string * output, int depth) const
{
  switch (statement.kind) {
    case StatementKind::VariableDeclaration: {
      const auto& node = static_cast<const VariableDeclaration&>(statement);
      appendLine(output, depth, "Variable " + node.name);
      if (node.initializer) printExpression(*node.initializer, output, depth + 1);
      return;
    }
    case StatementKind::FunctionDeclaration: {
      const auto& node = static_cast<const FunctionDeclaration&>(statement);
      std::string header = "Function " + node.name + "(";
      for (std::size_t i = 0; i < node.parameters.size(); ++i) {
        if (i > 0) header += ", ";
        header += node.parameters[i];
      }
      header += ")";
      appendLine(output, depth, header);
      for (const StatementPtr& child : node.body) printStatement(*child, output, depth + 1);
      return;
    }
    case StatementKind::EventDeclaration: {
      const auto& node = static_cast<const EventDeclaration&>(statement);
      appendLine(output, depth, "Event " + node.objectName + "." + node.eventName);
      for (const StatementPtr& child : node.body) printStatement(*child, output, depth + 1);
      return;
    }
    case StatementKind::If: {
      const auto& node = static_cast<const IfStatement&>(statement);
      appendLine(output, depth, "If");
      appendLine(output, depth + 1, "Condition");
      printExpression(*node.condition, output, depth + 2);
      appendLine(output, depth + 1, "Then");
      for (const StatementPtr& child : node.thenBranch) printStatement(*child, output, depth + 2);
      for (const auto& branch : node.elseIfBranches) {
        appendLine(output, depth + 1, "ElseIf");
        appendLine(output, depth + 2, "Condition");
        printExpression(*branch.condition, output, depth + 3);
        appendLine(output, depth + 2, "Then");
        for (const StatementPtr& child : branch.body) printStatement(*child, output, depth + 3);
      }
      if (!node.elseBranch.empty()) {
        appendLine(output, depth + 1, "Else");
        for (const StatementPtr& child : node.elseBranch) printStatement(*child, output, depth + 2);
      }
      return;
    }
    case StatementKind::While: {
      const auto& node = static_cast<const WhileStatement&>(statement);
      appendLine(output, depth, "While");
      appendLine(output, depth + 1, "Condition");
      printExpression(*node.condition, output, depth + 2);
      appendLine(output, depth + 1, "Body");
      for (const StatementPtr& child : node.body) printStatement(*child, output, depth + 2);
      return;
    }
    case StatementKind::For: {
      const auto& node = static_cast<const ForStatement&>(statement);
      appendLine(output, depth, std::string("For ") + (node.declaresVariable ? "var " : "") + node.variableName);
      appendLine(output, depth + 1, "Start");
      printExpression(*node.start, output, depth + 2);
      appendLine(output, depth + 1, "End");
      printExpression(*node.end, output, depth + 2);
      appendLine(output, depth + 1, "Step");
      printExpression(*node.step, output, depth + 2);
      appendLine(output, depth + 1, "Body");
      for (const StatementPtr& child : node.body) printStatement(*child, output, depth + 2);
      return;
    }
    case StatementKind::Break:
      appendLine(output, depth, "Break");
      return;
    case StatementKind::Continue:
      appendLine(output, depth, "Continue");
      return;
    case StatementKind::Return: {
      const auto& node = static_cast<const ReturnStatement&>(statement);
      appendLine(output, depth, "Return");
      if (node.value) printExpression(*node.value, output, depth + 1);
      return;
    }
    case StatementKind::Expression: {
      const auto& node = static_cast<const ExpressionStatement&>(statement);
      appendLine(output, depth, "Expression");
      printExpression(*node.expression, output, depth + 1);
      return;
    }
  }
}

void AstPrinter::printExpression(const Expression& expression, std::string * output, int depth) const
{
  switch (expression.kind) {
    case ExpressionKind::Literal: {
      const auto& node = static_cast<const LiteralExpression&>(expression);
      const char * kind = "Nil";
      if (node.literalKind == LiteralKind::Integer) kind = "Integer";
      else if (node.literalKind == LiteralKind::Float) kind = "Float";
      else if (node.literalKind == LiteralKind::String) kind = "String";
      else if (node.literalKind == LiteralKind::Boolean) kind = "Boolean";
      appendLine(output, depth, std::string("Literal ") + kind + " \"" + escape(node.value) + "\"");
      return;
    }
    case ExpressionKind::Variable: {
      const auto& node = static_cast<const VariableExpression&>(expression);
      appendLine(output, depth, "VariableRef " + node.name);
      return;
    }
    case ExpressionKind::Unary: {
      const auto& node = static_cast<const UnaryExpression&>(expression);
      appendLine(output, depth, "Unary " + operatorName(node.operation));
      printExpression(*node.operand, output, depth + 1);
      return;
    }
    case ExpressionKind::Binary: {
      const auto& node = static_cast<const BinaryExpression&>(expression);
      appendLine(output, depth, "Binary " + operatorName(node.operation));
      printExpression(*node.left, output, depth + 1);
      printExpression(*node.right, output, depth + 1);
      return;
    }
    case ExpressionKind::Assignment: {
      const auto& node = static_cast<const AssignmentExpression&>(expression);
      appendLine(output, depth, "Assignment");
      printExpression(*node.target, output, depth + 1);
      printExpression(*node.value, output, depth + 1);
      return;
    }
    case ExpressionKind::Member: {
      const auto& node = static_cast<const MemberExpression&>(expression);
      appendLine(output, depth, "Member ." + node.member);
      printExpression(*node.object, output, depth + 1);
      return;
    }
    case ExpressionKind::Call: {
      const auto& node = static_cast<const CallExpression&>(expression);
      appendLine(output, depth, "Call");
      appendLine(output, depth + 1, "Callee");
      printExpression(*node.callee, output, depth + 2);
      if (!node.arguments.empty()) appendLine(output, depth + 1, "Arguments");
      for (const ExpressionPtr& argument : node.arguments) printExpression(*argument, output, depth + 2);
      return;
    }
    case ExpressionKind::Grouping: {
      const auto& node = static_cast<const GroupingExpression&>(expression);
      appendLine(output, depth, "Grouping");
      printExpression(*node.expression, output, depth + 1);
      return;
    }
  }
}

void AstPrinter::appendLine(std::string * output, int depth, const std::string& text)
{
  output->append(static_cast<std::size_t>(depth * 2), ' ');
  *output += text;
  *output += '\n';
}

std::string AstPrinter::operatorName(TokenType type)
{
  return tokenTypeName(type);
}

std::string AstPrinter::escape(const std::string& value)
{
  std::string output;
  for (char character : value) {
    if (character == '\n') output += "\\n";
    else if (character == '\r') output += "\\r";
    else if (character == '\t') output += "\\t";
    else if (character == '"') output += "\\\"";
    else if (character == '\\') output += "\\\\";
    else output += character;
  }
  return output;
}

}  // namespace fosc
