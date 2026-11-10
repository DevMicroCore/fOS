#pragma once

#include <cstddef>
#include <string>

namespace fosc {

enum class TokenType {
  EndOfFile = 0,
  Identifier,
  IntegerLiteral,
  FloatLiteral,
  StringLiteral,
  Var,
  Function,
  Return,
  If,
  Then,
  Else,
  ElseIf,
  End,
  While,
  For,
  To,
  Step,
  Break,
  Continue,
  On,
  True,
  False,
  Nil,
  And,
  Or,
  Not,
  LeftParen,
  RightParen,
  Comma,
  Dot,
  Semicolon,
  Plus,
  Minus,
  Star,
  Slash,
  Percent,
  Equal,
  EqualEqual,
  Bang,
  BangEqual,
  Less,
  LessEqual,
  Greater,
  GreaterEqual,
  AndAnd,
  OrOr
};

struct SourceLocation {
  std::size_t line = 1;
  std::size_t column = 1;
  std::size_t offset = 0;
};

struct Token {
  TokenType type = TokenType::EndOfFile;
  std::string lexeme;
  std::string literal;
  SourceLocation location;
};

const char * tokenTypeName(TokenType type);

}  // namespace fosc
