#include "ConstantFolder.h"

#include <cmath>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <utility>

namespace fosc {
namespace {

bool numeric(const ConstantValue& value)
{
  return value.kind == ConstantValueKind::Integer || value.kind == ConstantValueKind::Float;
}

double number(const ConstantValue& value)
{
  return value.kind == ConstantValueKind::Integer
    ? static_cast<double>(value.integerValue)
    : value.floatValue;
}

bool integerResult(std::int64_t value, ConstantValue * result)
{
  if (value < std::numeric_limits<std::int32_t>::min() ||
      value > std::numeric_limits<std::int32_t>::max()) return false;
  result->kind = ConstantValueKind::Integer;
  result->integerValue = static_cast<std::int32_t>(value);
  return true;
}

bool floatingResult(double value, ConstantValue * result)
{
  if (!std::isfinite(value)) return false;
  result->kind = ConstantValueKind::Float;
  result->floatValue = value;
  return true;
}

bool equal(const ConstantValue& left, const ConstantValue& right)
{
  if (left.kind == ConstantValueKind::Nil || right.kind == ConstantValueKind::Nil) {
    return left.kind == right.kind;
  }
  if (numeric(left) && numeric(right)) return number(left) == number(right);
  if (left.kind != right.kind) return false;
  if (left.kind == ConstantValueKind::Boolean) return left.booleanValue == right.booleanValue;
  if (left.kind == ConstantValueKind::String) return left.stringValue == right.stringValue;
  return false;
}

}  // namespace

bool ConstantFolder::evaluate(const Expression& expression, ConstantValue * result)
{
  if (result == nullptr) return false;
  if (expression.kind == ExpressionKind::Literal) {
    const auto& literal = static_cast<const LiteralExpression&>(expression);
    try {
      switch (literal.literalKind) {
        case LiteralKind::Nil:
          result->kind = ConstantValueKind::Nil;
          return true;
        case LiteralKind::Boolean:
          result->kind = ConstantValueKind::Boolean;
          result->booleanValue = literal.value == "true";
          return true;
        case LiteralKind::Integer: {
          const long long value = std::stoll(literal.value);
          return integerResult(value, result);
        }
        case LiteralKind::Float:
          return floatingResult(std::stod(literal.value), result);
        case LiteralKind::String:
          result->kind = ConstantValueKind::String;
          result->stringValue = literal.value;
          return true;
      }
    } catch (const std::exception&) {
      return false;
    }
  }

  if (expression.kind == ExpressionKind::Grouping) {
    return evaluate(*static_cast<const GroupingExpression&>(expression).expression, result);
  }

  if (expression.kind == ExpressionKind::Unary) {
    const auto& unary = static_cast<const UnaryExpression&>(expression);
    ConstantValue operand;
    if (!evaluate(*unary.operand, &operand)) return false;
    if (unary.operation == TokenType::Plus) {
      if (!numeric(operand)) return false;
      *result = std::move(operand);
      return true;
    }
    if (unary.operation == TokenType::Minus) {
      if (operand.kind == ConstantValueKind::Integer) {
        return integerResult(-static_cast<std::int64_t>(operand.integerValue), result);
      }
      if (operand.kind == ConstantValueKind::Float) return floatingResult(-operand.floatValue, result);
      return false;
    }
    if (unary.operation == TokenType::Bang || unary.operation == TokenType::Not) {
      if (operand.kind != ConstantValueKind::Boolean) return false;
      result->kind = ConstantValueKind::Boolean;
      result->booleanValue = !operand.booleanValue;
      return true;
    }
    return false;
  }

  if (expression.kind != ExpressionKind::Binary) return false;
  const auto& binary = static_cast<const BinaryExpression&>(expression);
  ConstantValue left;
  ConstantValue right;
  if (!evaluate(*binary.left, &left) || !evaluate(*binary.right, &right)) return false;

  if (binary.operation == TokenType::Plus &&
      left.kind == ConstantValueKind::String && right.kind == ConstantValueKind::String) {
    result->kind = ConstantValueKind::String;
    result->stringValue = left.stringValue + right.stringValue;
    return true;
  }

  if (binary.operation == TokenType::And || binary.operation == TokenType::AndAnd ||
      binary.operation == TokenType::Or || binary.operation == TokenType::OrOr) {
    if (left.kind != ConstantValueKind::Boolean || right.kind != ConstantValueKind::Boolean) return false;
    result->kind = ConstantValueKind::Boolean;
    result->booleanValue = binary.operation == TokenType::And || binary.operation == TokenType::AndAnd
      ? left.booleanValue && right.booleanValue
      : left.booleanValue || right.booleanValue;
    return true;
  }

  if (binary.operation == TokenType::EqualEqual || binary.operation == TokenType::BangEqual) {
    result->kind = ConstantValueKind::Boolean;
    result->booleanValue = equal(left, right);
    if (binary.operation == TokenType::BangEqual) result->booleanValue = !result->booleanValue;
    return true;
  }

  if (binary.operation == TokenType::Less || binary.operation == TokenType::LessEqual ||
      binary.operation == TokenType::Greater || binary.operation == TokenType::GreaterEqual) {
    bool comparison = false;
    if (numeric(left) && numeric(right)) {
      if (binary.operation == TokenType::Less) comparison = number(left) < number(right);
      else if (binary.operation == TokenType::LessEqual) comparison = number(left) <= number(right);
      else if (binary.operation == TokenType::Greater) comparison = number(left) > number(right);
      else comparison = number(left) >= number(right);
    } else if (left.kind == ConstantValueKind::String && right.kind == ConstantValueKind::String) {
      if (binary.operation == TokenType::Less) comparison = left.stringValue < right.stringValue;
      else if (binary.operation == TokenType::LessEqual) comparison = left.stringValue <= right.stringValue;
      else if (binary.operation == TokenType::Greater) comparison = left.stringValue > right.stringValue;
      else comparison = left.stringValue >= right.stringValue;
    } else {
      return false;
    }
    result->kind = ConstantValueKind::Boolean;
    result->booleanValue = comparison;
    return true;
  }

  if (!numeric(left) || !numeric(right)) return false;
  if (left.kind == ConstantValueKind::Integer && right.kind == ConstantValueKind::Integer &&
      binary.operation != TokenType::Slash) {
    const std::int64_t lhs = left.integerValue;
    const std::int64_t rhs = right.integerValue;
    if (binary.operation == TokenType::Plus) return integerResult(lhs + rhs, result);
    if (binary.operation == TokenType::Minus) return integerResult(lhs - rhs, result);
    if (binary.operation == TokenType::Star) return integerResult(lhs * rhs, result);
    if (binary.operation == TokenType::Percent) {
      if (rhs == 0) return false;
      return integerResult(lhs % rhs, result);
    }
  }

  const double lhs = number(left);
  const double rhs = number(right);
  if ((binary.operation == TokenType::Slash || binary.operation == TokenType::Percent) && rhs == 0.0) {
    return false;
  }
  if (binary.operation == TokenType::Plus) return floatingResult(lhs + rhs, result);
  if (binary.operation == TokenType::Minus) return floatingResult(lhs - rhs, result);
  if (binary.operation == TokenType::Star) return floatingResult(lhs * rhs, result);
  if (binary.operation == TokenType::Slash) return floatingResult(lhs / rhs, result);
  return false;
}

}  // namespace fosc
