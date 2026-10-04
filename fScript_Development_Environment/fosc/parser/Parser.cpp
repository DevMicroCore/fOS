#include "Parser.h"

#include <stdexcept>
#include <utility>

namespace fosc {
namespace {

class ParseFailure final : public std::runtime_error {
 public:
  ParseFailure() : std::runtime_error("parse failure") {}
};

}  // namespace

Parser::Parser(std::vector<Token> tokens, std::string filename)
  : tokens_(std::move(tokens)), filename_(std::move(filename))
{
}

ParserResult Parser::parse()
{
  Program program;
  diagnostics_.clear();
  current_ = 0;

  while (!isAtEnd()) {
    try {
      StatementPtr parsed = declaration();
      if (parsed) program.statements.push_back(std::move(parsed));
    } catch (const ParseFailure&) {
      synchronize();
    }
  }

  return {std::move(program), std::move(diagnostics_)};
}

StatementPtr Parser::declaration()
{
  if (match({TokenType::Semicolon})) return nullptr;
  if (match({TokenType::Function})) return functionDeclaration(previous());
  if (match({TokenType::Var})) return variableDeclaration(previous());
  if (match({TokenType::On})) return eventDeclaration(previous());

  if (check(TokenType::End) || check(TokenType::Else) || check(TokenType::ElseIf)) {
    const Token unexpected = advance();
    addError(unexpected, "FS201", "Unexpected token '" + unexpected.lexeme + "'.");
    throw ParseFailure();
  }
  return statement();
}

StatementPtr Parser::functionDeclaration(const Token& keyword)
{
  const Token& name = consume(TokenType::Identifier, "FS202", "Expected function name.");
  consume(TokenType::LeftParen, "FS202", "Expected '(' after function name.");

  std::vector<std::string> parameters;
  if (!check(TokenType::RightParen)) {
    do {
      if (parameters.size() >= 255) {
        addError(peek(), "FS204", "A function may not have more than 255 parameters.");
        throw ParseFailure();
      }
      parameters.push_back(
        consume(TokenType::Identifier, "FS202", "Expected parameter name.").lexeme);
    } while (match({TokenType::Comma}));
  }
  consume(TokenType::RightParen, "FS202", "Expected ')' after function parameters.");

  StatementList body = block({TokenType::End});
  consume(TokenType::End, "FS202", "Expected 'end' after function body.");
  match({TokenType::Semicolon});
  return std::make_unique<FunctionDeclaration>(
    name.lexeme, std::move(parameters), std::move(body), keyword.location);
}

StatementPtr Parser::variableDeclaration(const Token& keyword)
{
  const Token& name = consume(TokenType::Identifier, "FS202", "Expected variable name.");
  ExpressionPtr initializer;
  if (match({TokenType::Equal})) initializer = expression();
  match({TokenType::Semicolon});
  return std::make_unique<VariableDeclaration>(name.lexeme, std::move(initializer), keyword.location);
}

StatementPtr Parser::eventDeclaration(const Token& keyword)
{
  const Token& object = consume(
    TokenType::Identifier, "FS205", "Expected UI object or event source after 'on'.");
  consume(TokenType::Dot, "FS205", "Expected '.' in event reference.");
  const Token& event = consume(TokenType::Identifier, "FS205", "Expected event name after '.'.");

  StatementList body = block({TokenType::End});
  consume(TokenType::End, "FS202", "Expected 'end' after event body.");
  match({TokenType::Semicolon});
  return std::make_unique<EventDeclaration>(
    object.lexeme, event.lexeme, std::move(body), keyword.location);
}

StatementPtr Parser::statement()
{
  if (match({TokenType::If})) return ifStatement(previous());
  if (match({TokenType::While})) return whileStatement(previous());
  if (match({TokenType::For})) return forStatement(previous());
  if (match({TokenType::Break})) return breakStatement(previous());
  if (match({TokenType::Continue})) return continueStatement(previous());
  if (match({TokenType::Return})) return returnStatement(previous());
  return expressionStatement();
}

StatementPtr Parser::ifStatement(const Token& keyword)
{
  ExpressionPtr condition = expression();
  consume(TokenType::Then, "FS202", "Expected 'then' after if condition.");

  StatementList thenBranch = block({TokenType::ElseIf, TokenType::Else, TokenType::End});
  std::vector<IfStatement::ElseIfBranch> elseIfBranches;
  while (match({TokenType::ElseIf})) {
    ExpressionPtr elseIfCondition = expression();
    consume(TokenType::Then, "FS202", "Expected 'then' after elseif condition.");
    StatementList elseIfBody = block({TokenType::ElseIf, TokenType::Else, TokenType::End});
    elseIfBranches.emplace_back(std::move(elseIfCondition), std::move(elseIfBody));
  }
  StatementList elseBranch;
  if (match({TokenType::Else})) elseBranch = block({TokenType::End});
  consume(TokenType::End, "FS202", "Expected 'end' after if statement.");
  match({TokenType::Semicolon});

  return std::make_unique<IfStatement>(
    std::move(condition), std::move(thenBranch), std::move(elseIfBranches),
    std::move(elseBranch), keyword.location);
}

StatementPtr Parser::whileStatement(const Token& keyword)
{
  ExpressionPtr condition = expression();
  consume(TokenType::Then, "FS202", "Expected 'then' after while condition.");
  StatementList body = block({TokenType::End});
  consume(TokenType::End, "FS202", "Expected 'end' after while statement.");
  match({TokenType::Semicolon});
  return std::make_unique<WhileStatement>(
    std::move(condition), std::move(body), keyword.location);
}

StatementPtr Parser::forStatement(const Token& keyword)
{
  bool declaresVariable = false;
  if (match({TokenType::Var})) declaresVariable = true;
  const Token& variable = consume(TokenType::Identifier, "FS202", "Expected loop variable name.");
  consume(TokenType::Equal, "FS202", "Expected '=' after loop variable.");
  ExpressionPtr start = expression();
  consume(TokenType::To, "FS202", "Expected 'to' after for start expression.");
  ExpressionPtr end = expression();
  ExpressionPtr step;
  if (match({TokenType::Step})) step = expression();
  else step = std::make_unique<LiteralExpression>(LiteralKind::Integer, "1", keyword.location);
  consume(TokenType::Then, "FS202", "Expected 'then' after for range.");
  StatementList body = block({TokenType::End});
  consume(TokenType::End, "FS202", "Expected 'end' after for statement.");
  match({TokenType::Semicolon});
  return std::make_unique<ForStatement>(
    declaresVariable, variable.lexeme, std::move(start), std::move(end),
    std::move(step), std::move(body), keyword.location);
}

StatementPtr Parser::breakStatement(const Token& keyword)
{
  match({TokenType::Semicolon});
  return std::make_unique<BreakStatement>(keyword.location);
}

StatementPtr Parser::continueStatement(const Token& keyword)
{
  match({TokenType::Semicolon});
  return std::make_unique<ContinueStatement>(keyword.location);
}

StatementPtr Parser::returnStatement(const Token& keyword)
{
  ExpressionPtr value;
  if (!startsDeclarationOrTerminator()) value = expression();
  match({TokenType::Semicolon});
  return std::make_unique<ReturnStatement>(std::move(value), keyword.location);
}

StatementPtr Parser::expressionStatement()
{
  const SourceLocation location = peek().location;
  ExpressionPtr value = expression();
  match({TokenType::Semicolon});
  return std::make_unique<ExpressionStatement>(std::move(value), location);
}

StatementList Parser::block(std::initializer_list<TokenType> terminators)
{
  StatementList statements;
  while (!isAtEnd() && !isBlockTerminator(terminators)) {
    try {
      StatementPtr parsed = declaration();
      if (parsed) statements.push_back(std::move(parsed));
    } catch (const ParseFailure&) {
      synchronize();
    }
  }
  return statements;
}

ExpressionPtr Parser::expression() { return assignment(); }

ExpressionPtr Parser::assignment()
{
  ExpressionPtr left = logicalOr();
  if (!match({TokenType::Equal})) return left;

  const Token equals = previous();
  ExpressionPtr value = assignment();
  if (left->kind != ExpressionKind::Variable && left->kind != ExpressionKind::Member) {
    addError(equals, "FS203", "Invalid assignment target. Expected variable or property.");
    throw ParseFailure();
  }
  return std::make_unique<AssignmentExpression>(
    std::move(left), std::move(value), equals.location);
}

ExpressionPtr Parser::logicalOr()
{
  ExpressionPtr value = logicalAnd();
  while (match({TokenType::Or, TokenType::OrOr})) {
    const Token operation = previous();
    value = std::make_unique<BinaryExpression>(
      std::move(value), operation.type, logicalAnd(), operation.location);
  }
  return value;
}

ExpressionPtr Parser::logicalAnd()
{
  ExpressionPtr value = equality();
  while (match({TokenType::And, TokenType::AndAnd})) {
    const Token operation = previous();
    value = std::make_unique<BinaryExpression>(
      std::move(value), operation.type, equality(), operation.location);
  }
  return value;
}

ExpressionPtr Parser::equality()
{
  ExpressionPtr value = comparison();
  while (match({TokenType::EqualEqual, TokenType::BangEqual})) {
    const Token operation = previous();
    value = std::make_unique<BinaryExpression>(
      std::move(value), operation.type, comparison(), operation.location);
  }
  return value;
}

ExpressionPtr Parser::comparison()
{
  ExpressionPtr value = term();
  while (match({TokenType::Less, TokenType::LessEqual, TokenType::Greater, TokenType::GreaterEqual})) {
    const Token operation = previous();
    value = std::make_unique<BinaryExpression>(
      std::move(value), operation.type, term(), operation.location);
  }
  return value;
}

ExpressionPtr Parser::term()
{
  ExpressionPtr value = factor();
  while (match({TokenType::Plus, TokenType::Minus})) {
    const Token operation = previous();
    value = std::make_unique<BinaryExpression>(
      std::move(value), operation.type, factor(), operation.location);
  }
  return value;
}

ExpressionPtr Parser::factor()
{
  ExpressionPtr value = unary();
  while (match({TokenType::Star, TokenType::Slash, TokenType::Percent})) {
    const Token operation = previous();
    value = std::make_unique<BinaryExpression>(
      std::move(value), operation.type, unary(), operation.location);
  }
  return value;
}

ExpressionPtr Parser::unary()
{
  if (match({TokenType::Bang, TokenType::Not, TokenType::Minus, TokenType::Plus})) {
    const Token operation = previous();
    return std::make_unique<UnaryExpression>(operation.type, unary(), operation.location);
  }
  return call();
}

ExpressionPtr Parser::call()
{
  ExpressionPtr value = primary();
  while (true) {
    if (match({TokenType::LeftParen})) {
      value = finishCall(std::move(value), previous().location);
    } else if (match({TokenType::Dot})) {
      const Token& member = consume(TokenType::Identifier, "FS202", "Expected property name after '.'.");
      value = std::make_unique<MemberExpression>(
        std::move(value), member.lexeme, member.location);
    } else {
      break;
    }
  }
  return value;
}

ExpressionPtr Parser::finishCall(ExpressionPtr callee, SourceLocation location)
{
  std::vector<ExpressionPtr> arguments;
  if (!check(TokenType::RightParen)) {
    do {
      if (arguments.size() >= 255) {
        addError(peek(), "FS204", "A call may not have more than 255 arguments.");
        throw ParseFailure();
      }
      arguments.push_back(expression());
    } while (match({TokenType::Comma}));
  }
  consume(TokenType::RightParen, "FS202", "Expected ')' after arguments.");
  return std::make_unique<CallExpression>(
    std::move(callee), std::move(arguments), location);
}

ExpressionPtr Parser::primary()
{
  if (match({TokenType::IntegerLiteral})) {
    const Token token = previous();
    return std::make_unique<LiteralExpression>(LiteralKind::Integer, token.literal, token.location);
  }
  if (match({TokenType::FloatLiteral})) {
    const Token token = previous();
    return std::make_unique<LiteralExpression>(LiteralKind::Float, token.literal, token.location);
  }
  if (match({TokenType::StringLiteral})) {
    const Token token = previous();
    return std::make_unique<LiteralExpression>(LiteralKind::String, token.literal, token.location);
  }
  if (match({TokenType::True, TokenType::False})) {
    const Token token = previous();
    return std::make_unique<LiteralExpression>(LiteralKind::Boolean, token.lexeme, token.location);
  }
  if (match({TokenType::Nil})) {
    const Token token = previous();
    return std::make_unique<LiteralExpression>(LiteralKind::Nil, "nil", token.location);
  }
  if (match({TokenType::Identifier})) {
    const Token token = previous();
    return std::make_unique<VariableExpression>(token.lexeme, token.location);
  }
  if (match({TokenType::LeftParen})) {
    const Token opening = previous();
    ExpressionPtr value = expression();
    consume(TokenType::RightParen, "FS202", "Expected ')' after expression.");
    return std::make_unique<GroupingExpression>(std::move(value), opening.location);
  }

  const Token unexpected = peek();
  if (!isAtEnd() && unexpected.type != TokenType::End &&
      unexpected.type != TokenType::Else && unexpected.type != TokenType::ElseIf) {
    advance();
  }
  const std::string display = unexpected.type == TokenType::EndOfFile
    ? "end of file"
    : "'" + unexpected.lexeme + "'";
  addError(unexpected, "FS201", "Unexpected " + display + ". Expected expression.");
  throw ParseFailure();
}

bool Parser::match(std::initializer_list<TokenType> types)
{
  for (TokenType type : types) {
    if (check(type)) {
      advance();
      return true;
    }
  }
  return false;
}

bool Parser::check(TokenType type) const
{
  if (isAtEnd()) return type == TokenType::EndOfFile;
  return peek().type == type;
}

const Token& Parser::advance()
{
  if (!isAtEnd()) ++current_;
  return previous();
}

bool Parser::isAtEnd() const { return peek().type == TokenType::EndOfFile; }
const Token& Parser::peek() const { return tokens_[current_]; }
const Token& Parser::previous() const { return tokens_[current_ == 0 ? 0 : current_ - 1]; }

const Token& Parser::consume(TokenType type, const char * code, const std::string& message)
{
  if (check(type)) return advance();
  addError(peek(), code, message);
  throw ParseFailure();
}

void Parser::addError(const Token& token, const char * code, const std::string& message)
{
  diagnostics_.push_back({code, message, filename_, token.location});
}

void Parser::synchronize()
{
  if (isAtEnd() || check(TokenType::End) || check(TokenType::Else) || check(TokenType::ElseIf)) return;
  advance();
  while (!isAtEnd()) {
    if (previous().type == TokenType::Semicolon) return;
    if (startsDeclarationOrTerminator()) return;
    advance();
  }
}

bool Parser::startsDeclarationOrTerminator() const
{
  return check(TokenType::Var) || check(TokenType::Function) ||
         check(TokenType::On) || check(TokenType::If) ||
         check(TokenType::While) || check(TokenType::For) ||
         check(TokenType::Break) || check(TokenType::Continue) ||
         check(TokenType::Return) || check(TokenType::End) ||
         check(TokenType::Else) || check(TokenType::ElseIf) ||
         check(TokenType::EndOfFile);
}

bool Parser::isBlockTerminator(std::initializer_list<TokenType> terminators) const
{
  for (TokenType type : terminators) {
    if (check(type)) return true;
  }
  return false;
}

}  // namespace fosc
