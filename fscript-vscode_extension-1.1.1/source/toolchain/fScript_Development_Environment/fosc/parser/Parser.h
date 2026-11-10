#pragma once

#include <initializer_list>
#include <string>
#include <vector>

#include "../ast/Ast.h"
#include "../diagnostics/Diagnostic.h"
#include "../lexer/Token.h"

namespace fosc {

struct ParserResult {
  Program program;
  std::vector<Diagnostic> diagnostics;

  bool success() const { return diagnostics.empty(); }
};

class Parser {
 public:
  Parser(std::vector<Token> tokens, std::string filename);
  ParserResult parse();

 private:
  StatementPtr declaration();
  StatementPtr functionDeclaration(const Token& keyword);
  StatementPtr variableDeclaration(const Token& keyword);
  StatementPtr eventDeclaration(const Token& keyword);
  StatementPtr statement();
  StatementPtr ifStatement(const Token& keyword);
  StatementPtr whileStatement(const Token& keyword);
  StatementPtr forStatement(const Token& keyword);
  StatementPtr breakStatement(const Token& keyword);
  StatementPtr continueStatement(const Token& keyword);
  StatementPtr returnStatement(const Token& keyword);
  StatementPtr expressionStatement();
  StatementList block(std::initializer_list<TokenType> terminators);

  ExpressionPtr expression();
  ExpressionPtr assignment();
  ExpressionPtr logicalOr();
  ExpressionPtr logicalAnd();
  ExpressionPtr equality();
  ExpressionPtr comparison();
  ExpressionPtr term();
  ExpressionPtr factor();
  ExpressionPtr unary();
  ExpressionPtr call();
  ExpressionPtr finishCall(ExpressionPtr callee, SourceLocation location);
  ExpressionPtr primary();

  bool match(std::initializer_list<TokenType> types);
  bool check(TokenType type) const;
  const Token& advance();
  bool isAtEnd() const;
  const Token& peek() const;
  const Token& previous() const;
  const Token& consume(TokenType type, const char * code, const std::string& message);
  void addError(const Token& token, const char * code, const std::string& message);
  void synchronize();
  bool startsDeclarationOrTerminator() const;
  bool isBlockTerminator(std::initializer_list<TokenType> terminators) const;

  std::vector<Token> tokens_;
  std::string filename_;
  std::vector<Diagnostic> diagnostics_;
  std::size_t current_ = 0;
};

}  // namespace fosc
