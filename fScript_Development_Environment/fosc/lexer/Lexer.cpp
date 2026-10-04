#include "Lexer.h"

#include <unordered_map>
#include <utility>

namespace fosc {
namespace {

const std::unordered_map<std::string, TokenType> kKeywords = {
  {"var", TokenType::Var}, {"function", TokenType::Function},
  {"return", TokenType::Return}, {"if", TokenType::If},
  {"then", TokenType::Then}, {"else", TokenType::Else},
  {"elseif", TokenType::ElseIf}, {"end", TokenType::End},
  {"while", TokenType::While}, {"for", TokenType::For},
  {"to", TokenType::To}, {"step", TokenType::Step},
  {"break", TokenType::Break}, {"continue", TokenType::Continue},
  {"on", TokenType::On},
  {"true", TokenType::True}, {"false", TokenType::False},
  {"nil", TokenType::Nil}, {"and", TokenType::And},
  {"or", TokenType::Or}, {"not", TokenType::Not}
};

}  // namespace

Lexer::Lexer(std::string source, std::string filename)
  : source_(std::move(source)), filename_(std::move(filename)) {}

LexerResult Lexer::scan()
{
  tokens_.clear();
  diagnostics_.clear();
  start_ = 0;
  current_ = 0;
  line_ = 1;
  column_ = 1;

  while (!isAtEnd()) {
    start_ = current_;
    tokenStart_ = {line_, column_, current_};
    scanToken();
  }

  Token endToken;
  endToken.type = TokenType::EndOfFile;
  endToken.location = {line_, column_, current_};
  tokens_.push_back(std::move(endToken));
  return {std::move(tokens_), std::move(diagnostics_)};
}

bool Lexer::isAtEnd() const { return current_ >= source_.size(); }

char Lexer::advance()
{
  const char value = source_[current_++];
  if (value == '\n') {
    ++line_;
    column_ = 1;
  } else {
    ++column_;
  }
  return value;
}

char Lexer::peek() const { return isAtEnd() ? '\0' : source_[current_]; }

char Lexer::peekNext() const
{
  return current_ + 1 >= source_.size() ? '\0' : source_[current_ + 1];
}

bool Lexer::match(char expected)
{
  if (isAtEnd() || source_[current_] != expected) return false;
  advance();
  return true;
}

void Lexer::scanToken()
{
  const char value = advance();
  switch (value) {
    case ' ': case '\t': case '\r': case '\n': return;
    case '(': addToken(TokenType::LeftParen); return;
    case ')': addToken(TokenType::RightParen); return;
    case ',': addToken(TokenType::Comma); return;
    case '.': addToken(TokenType::Dot); return;
    case ';': addToken(TokenType::Semicolon); return;
    case '+': addToken(TokenType::Plus); return;
    case '*': addToken(TokenType::Star); return;
    case '%': addToken(TokenType::Percent); return;
    case '-':
      if (match('-')) skipLineComment();
      else addToken(TokenType::Minus);
      return;
    case '/':
      if (match('/')) skipLineComment();
      else if (match('*')) skipBlockComment();
      else addToken(TokenType::Slash);
      return;
    case '=': addToken(match('=') ? TokenType::EqualEqual : TokenType::Equal); return;
    case '!': addToken(match('=') ? TokenType::BangEqual : TokenType::Bang); return;
    case '<': addToken(match('=') ? TokenType::LessEqual : TokenType::Less); return;
    case '>': addToken(match('=') ? TokenType::GreaterEqual : TokenType::Greater); return;
    case '&':
      if (match('&')) addToken(TokenType::AndAnd);
      else addError("FS001", "Unexpected character '&'. Did you mean '&&'?", tokenStart_);
      return;
    case '|':
      if (match('|')) addToken(TokenType::OrOr);
      else addError("FS001", "Unexpected character '|'. Did you mean '||'?", tokenStart_);
      return;
    case '"': case '\'': scanString(value); return;
    default: break;
  }

  if (isDigit(value)) {
    scanNumber();
    return;
  }
  if (isIdentifierStart(value)) {
    scanIdentifier();
    return;
  }

  std::string message = "Unexpected character '";
  message += value;
  message += "'.";
  addError("FS001", std::move(message), tokenStart_);
}

void Lexer::scanIdentifier()
{
  while (isIdentifierPart(peek())) advance();
  const std::string lexeme = source_.substr(start_, current_ - start_);
  const auto keyword = kKeywords.find(lexeme);
  addToken(keyword == kKeywords.end() ? TokenType::Identifier : keyword->second);
}

void Lexer::scanNumber()
{
  while (isDigit(peek())) advance();
  bool isFloat = false;

  if (peek() == '.' && isDigit(peekNext())) {
    isFloat = true;
    advance();
    while (isDigit(peek())) advance();
  }

  if (peek() == 'e' || peek() == 'E') {
    const std::size_t exponentStart = current_;
    const SourceLocation exponentLocation = {line_, column_, current_};
    advance();
    if (peek() == '+' || peek() == '-') advance();
    if (!isDigit(peek())) {
      addError("FS005", "Malformed exponent in number literal.", exponentLocation);
      current_ = exponentStart;
      column_ = exponentLocation.column;
    } else {
      isFloat = true;
      while (isDigit(peek())) advance();
    }
  }

  addToken(isFloat ? TokenType::FloatLiteral : TokenType::IntegerLiteral,
           source_.substr(start_, current_ - start_));
}

void Lexer::scanString(char quote)
{
  std::string decoded;
  bool unterminatedReported = false;

  while (!isAtEnd() && peek() != quote && peek() != '\n') {
    char value = advance();
    if (value != '\\') {
      decoded += value;
      continue;
    }

    if (isAtEnd() || peek() == '\n') {
      unterminatedReported = true;
      addError("FS002", "Unterminated string literal.", tokenStart_);
      break;
    }

    const SourceLocation escapeLocation = {line_, column_ - 1, current_ - 1};
    const char escaped = advance();
    switch (escaped) {
      case 'n': decoded += '\n'; break;
      case 'r': decoded += '\r'; break;
      case 't': decoded += '\t'; break;
      case '\\': decoded += '\\'; break;
      case '"': decoded += '"'; break;
      case '\'': decoded += '\''; break;
      default:
        addError("FS003", std::string("Unknown escape sequence \\") + escaped + ".", escapeLocation);
        decoded += escaped;
        break;
    }
  }

  if (!isAtEnd() && peek() == quote) {
    advance();
    addToken(TokenType::StringLiteral, std::move(decoded));
    return;
  }
  if (!unterminatedReported) addError("FS002", "Unterminated string literal.", tokenStart_);
}

void Lexer::skipLineComment()
{
  while (!isAtEnd() && peek() != '\n') advance();
}

void Lexer::skipBlockComment()
{
  while (!isAtEnd()) {
    if (peek() == '*' && peekNext() == '/') {
      advance();
      advance();
      return;
    }
    advance();
  }
  addError("FS004", "Unterminated block comment.", tokenStart_);
}

void Lexer::addToken(TokenType type) { addToken(type, {}); }

void Lexer::addToken(TokenType type, std::string literal)
{
  Token token;
  token.type = type;
  token.lexeme = source_.substr(start_, current_ - start_);
  token.literal = std::move(literal);
  token.location = tokenStart_;
  tokens_.push_back(std::move(token));
}

void Lexer::addError(const char * code, std::string message, SourceLocation location)
{
  diagnostics_.push_back({code, std::move(message), filename_, location});
}

bool Lexer::isIdentifierStart(char value)
{
  return (value >= 'a' && value <= 'z') ||
         (value >= 'A' && value <= 'Z') || value == '_';
}

bool Lexer::isIdentifierPart(char value)
{
  return isIdentifierStart(value) || isDigit(value);
}

bool Lexer::isDigit(char value) { return value >= '0' && value <= '9'; }

}  // namespace fosc
