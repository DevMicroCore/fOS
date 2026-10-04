#pragma once

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "../lexer/Token.h"

namespace fosc {

enum class ExpressionKind {
  Literal,
  Variable,
  Unary,
  Binary,
  Assignment,
  Member,
  Call,
  Grouping
};

enum class LiteralKind {
  Integer,
  Float,
  String,
  Boolean,
  Nil
};

struct Expression {
  ExpressionKind kind;
  SourceLocation location;

  Expression(ExpressionKind expressionKind, SourceLocation sourceLocation)
    : kind(expressionKind), location(sourceLocation) {}
  virtual ~Expression() = default;
};

using ExpressionPtr = std::unique_ptr<Expression>;

struct LiteralExpression final : Expression {
  LiteralKind literalKind;
  std::string value;

  LiteralExpression(LiteralKind type, std::string literalValue, SourceLocation sourceLocation)
    : Expression(ExpressionKind::Literal, sourceLocation),
      literalKind(type),
      value(std::move(literalValue)) {}
};

struct VariableExpression final : Expression {
  std::string name;

  VariableExpression(std::string variableName, SourceLocation sourceLocation)
    : Expression(ExpressionKind::Variable, sourceLocation), name(std::move(variableName)) {}
};

struct UnaryExpression final : Expression {
  TokenType operation;
  ExpressionPtr operand;

  UnaryExpression(TokenType op, ExpressionPtr value, SourceLocation sourceLocation)
    : Expression(ExpressionKind::Unary, sourceLocation),
      operation(op),
      operand(std::move(value)) {}
};

struct BinaryExpression final : Expression {
  ExpressionPtr left;
  TokenType operation;
  ExpressionPtr right;

  BinaryExpression(ExpressionPtr lhs, TokenType op, ExpressionPtr rhs, SourceLocation sourceLocation)
    : Expression(ExpressionKind::Binary, sourceLocation),
      left(std::move(lhs)),
      operation(op),
      right(std::move(rhs)) {}
};

struct AssignmentExpression final : Expression {
  ExpressionPtr target;
  ExpressionPtr value;

  AssignmentExpression(ExpressionPtr assignmentTarget, ExpressionPtr assignmentValue, SourceLocation sourceLocation)
    : Expression(ExpressionKind::Assignment, sourceLocation),
      target(std::move(assignmentTarget)),
      value(std::move(assignmentValue)) {}
};

struct MemberExpression final : Expression {
  ExpressionPtr object;
  std::string member;

  MemberExpression(ExpressionPtr parent, std::string memberName, SourceLocation sourceLocation)
    : Expression(ExpressionKind::Member, sourceLocation),
      object(std::move(parent)),
      member(std::move(memberName)) {}
};

struct CallExpression final : Expression {
  ExpressionPtr callee;
  std::vector<ExpressionPtr> arguments;

  CallExpression(ExpressionPtr callable, std::vector<ExpressionPtr> args, SourceLocation sourceLocation)
    : Expression(ExpressionKind::Call, sourceLocation),
      callee(std::move(callable)),
      arguments(std::move(args)) {}
};

struct GroupingExpression final : Expression {
  ExpressionPtr expression;

  GroupingExpression(ExpressionPtr value, SourceLocation sourceLocation)
    : Expression(ExpressionKind::Grouping, sourceLocation), expression(std::move(value)) {}
};

enum class StatementKind {
  VariableDeclaration,
  FunctionDeclaration,
  EventDeclaration,
  If,
  While,
  For,
  Break,
  Continue,
  Return,
  Expression
};

struct Statement {
  StatementKind kind;
  SourceLocation location;

  Statement(StatementKind statementKind, SourceLocation sourceLocation)
    : kind(statementKind), location(sourceLocation) {}
  virtual ~Statement() = default;
};

using StatementPtr = std::unique_ptr<Statement>;
using StatementList = std::vector<StatementPtr>;

struct VariableDeclaration final : Statement {
  std::string name;
  ExpressionPtr initializer;

  VariableDeclaration(std::string variableName, ExpressionPtr value, SourceLocation sourceLocation)
    : Statement(StatementKind::VariableDeclaration, sourceLocation),
      name(std::move(variableName)),
      initializer(std::move(value)) {}
};

struct FunctionDeclaration final : Statement {
  std::string name;
  std::vector<std::string> parameters;
  StatementList body;

  FunctionDeclaration(
    std::string functionName,
    std::vector<std::string> functionParameters,
    StatementList functionBody,
    SourceLocation sourceLocation)
    : Statement(StatementKind::FunctionDeclaration, sourceLocation),
      name(std::move(functionName)),
      parameters(std::move(functionParameters)),
      body(std::move(functionBody)) {}
};

struct EventDeclaration final : Statement {
  std::string objectName;
  std::string eventName;
  StatementList body;

  EventDeclaration(
    std::string object,
    std::string event,
    StatementList eventBody,
    SourceLocation sourceLocation)
    : Statement(StatementKind::EventDeclaration, sourceLocation),
      objectName(std::move(object)),
      eventName(std::move(event)),
      body(std::move(eventBody)) {}
};

struct IfStatement final : Statement {
  struct ElseIfBranch {
    ExpressionPtr condition;
    StatementList body;

    ElseIfBranch(ExpressionPtr branchCondition, StatementList branchBody)
      : condition(std::move(branchCondition)), body(std::move(branchBody)) {}
  };

  ExpressionPtr condition;
  StatementList thenBranch;
  std::vector<ElseIfBranch> elseIfBranches;
  StatementList elseBranch;

  IfStatement(
    ExpressionPtr conditionExpression,
    StatementList thenStatements,
    std::vector<ElseIfBranch> elseIfStatements,
    StatementList elseStatements,
    SourceLocation sourceLocation)
    : Statement(StatementKind::If, sourceLocation),
      condition(std::move(conditionExpression)),
      thenBranch(std::move(thenStatements)),
      elseIfBranches(std::move(elseIfStatements)),
      elseBranch(std::move(elseStatements)) {}
};

struct WhileStatement final : Statement {
  ExpressionPtr condition;
  StatementList body;

  WhileStatement(ExpressionPtr conditionExpression, StatementList bodyStatements, SourceLocation sourceLocation)
    : Statement(StatementKind::While, sourceLocation),
      condition(std::move(conditionExpression)),
      body(std::move(bodyStatements)) {}
};

struct ForStatement final : Statement {
  bool declaresVariable = false;
  std::string variableName;
  ExpressionPtr start;
  ExpressionPtr end;
  ExpressionPtr step;
  StatementList body;

  ForStatement(
    bool declaresLoopVariable,
    std::string loopVariableName,
    ExpressionPtr startExpression,
    ExpressionPtr endExpression,
    ExpressionPtr stepExpression,
    StatementList bodyStatements,
    SourceLocation sourceLocation)
    : Statement(StatementKind::For, sourceLocation),
      declaresVariable(declaresLoopVariable),
      variableName(std::move(loopVariableName)),
      start(std::move(startExpression)),
      end(std::move(endExpression)),
      step(std::move(stepExpression)),
      body(std::move(bodyStatements)) {}
};

struct BreakStatement final : Statement {
  explicit BreakStatement(SourceLocation sourceLocation)
    : Statement(StatementKind::Break, sourceLocation) {}
};

struct ContinueStatement final : Statement {
  explicit ContinueStatement(SourceLocation sourceLocation)
    : Statement(StatementKind::Continue, sourceLocation) {}
};

struct ReturnStatement final : Statement {
  ExpressionPtr value;

  ReturnStatement(ExpressionPtr returnValue, SourceLocation sourceLocation)
    : Statement(StatementKind::Return, sourceLocation), value(std::move(returnValue)) {}
};

struct ExpressionStatement final : Statement {
  ExpressionPtr expression;

  ExpressionStatement(ExpressionPtr value, SourceLocation sourceLocation)
    : Statement(StatementKind::Expression, sourceLocation), expression(std::move(value)) {}
};

struct Program {
  StatementList statements;
};

}  // namespace fosc
