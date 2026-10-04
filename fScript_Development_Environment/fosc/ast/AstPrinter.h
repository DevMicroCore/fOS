#pragma once

#include <string>

#include "Ast.h"

namespace fosc {

class AstPrinter {
 public:
  std::string print(const Program& program) const;

 private:
  void printStatement(const Statement& statement, std::string * output, int depth) const;
  void printExpression(const Expression& expression, std::string * output, int depth) const;
  static void appendLine(std::string * output, int depth, const std::string& text);
  static std::string operatorName(TokenType type);
  static std::string escape(const std::string& value);
};

}  // namespace fosc
