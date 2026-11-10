#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "../diagnostics/Diagnostic.h"
#include "Token.h"

namespace fosc {

struct LexerResult {
  std::vector<Token> tokens;
  std::vector<Diagnostic> diagnostics;

  bool success() const { return diagnostics.empty(); }
};

class Lexer {
 public:
  Lexer(std::string source, std::string filename);
  LexerResult scan();

 private:
  bool isAtEnd() const;
  char advance();
  char peek() const;
  char peekNext() const;
  bool match(char expected);
  void scanToken();
  void scanIdentifier();
  void scanNumber();
  void scanString(char quote);
  void skipLineComment();
  void skipBlockComment();
  void addToken(TokenType type);
  void addToken(TokenType type, std::string literal);
  void addError(const char * code, std::string message, SourceLocation location);
  static bool isIdentifierStart(char value);
  static bool isIdentifierPart(char value);
  static bool isDigit(char value);

  std::string source_;
  std::string filename_;
  std::vector<Token> tokens_;
  std::vector<Diagnostic> diagnostics_;
  std::size_t start_ = 0;
  std::size_t current_ = 0;
  std::size_t line_ = 1;
  std::size_t column_ = 1;
  SourceLocation tokenStart_;
};

}  // namespace fosc
