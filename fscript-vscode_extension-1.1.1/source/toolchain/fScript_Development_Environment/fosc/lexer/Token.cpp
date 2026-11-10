#include "Token.h"

namespace fosc {

const char * tokenTypeName(TokenType type)
{
  switch (type) {
    case TokenType::EndOfFile: return "EOF";
    case TokenType::Identifier: return "IDENTIFIER";
    case TokenType::IntegerLiteral: return "INTEGER";
    case TokenType::FloatLiteral: return "FLOAT";
    case TokenType::StringLiteral: return "STRING";
    case TokenType::Var: return "VAR";
    case TokenType::Function: return "FUNCTION";
    case TokenType::Return: return "RETURN";
    case TokenType::If: return "IF";
    case TokenType::Then: return "THEN";
    case TokenType::Else: return "ELSE";
    case TokenType::ElseIf: return "ELSEIF";
    case TokenType::End: return "END";
    case TokenType::While: return "WHILE";
    case TokenType::For: return "FOR";
    case TokenType::To: return "TO";
    case TokenType::Step: return "STEP";
    case TokenType::Break: return "BREAK";
    case TokenType::Continue: return "CONTINUE";
    case TokenType::On: return "ON";
    case TokenType::True: return "TRUE";
    case TokenType::False: return "FALSE";
    case TokenType::Nil: return "NIL";
    case TokenType::And: return "AND";
    case TokenType::Or: return "OR";
    case TokenType::Not: return "NOT";
    case TokenType::LeftParen: return "LEFT_PAREN";
    case TokenType::RightParen: return "RIGHT_PAREN";
    case TokenType::Comma: return "COMMA";
    case TokenType::Dot: return "DOT";
    case TokenType::Semicolon: return "SEMICOLON";
    case TokenType::Plus: return "PLUS";
    case TokenType::Minus: return "MINUS";
    case TokenType::Star: return "STAR";
    case TokenType::Slash: return "SLASH";
    case TokenType::Percent: return "PERCENT";
    case TokenType::Equal: return "EQUAL";
    case TokenType::EqualEqual: return "EQUAL_EQUAL";
    case TokenType::Bang: return "BANG";
    case TokenType::BangEqual: return "BANG_EQUAL";
    case TokenType::Less: return "LESS";
    case TokenType::LessEqual: return "LESS_EQUAL";
    case TokenType::Greater: return "GREATER";
    case TokenType::GreaterEqual: return "GREATER_EQUAL";
    case TokenType::AndAnd: return "AND_AND";
    case TokenType::OrOr: return "OR_OR";
  }
  return "UNKNOWN";
}

}  // namespace fosc
