#include "Type.h"

namespace fosc {

const char * valueTypeName(ValueType type)
{
  switch (type) {
    case ValueType::Unknown: return "unknown";
    case ValueType::Any: return "any";
    case ValueType::Nil: return "nil";
    case ValueType::Boolean: return "boolean";
    case ValueType::Integer: return "integer";
    case ValueType::Float: return "float";
    case ValueType::String: return "string";
  }
  return "unknown";
}

bool isNumericType(ValueType type)
{
  return type == ValueType::Integer || type == ValueType::Float;
}

bool isAssignableType(ValueType target, ValueType value)
{
  if (target == ValueType::Unknown || target == ValueType::Any ||
      value == ValueType::Unknown || value == ValueType::Any ||
      value == ValueType::Nil) {
    return true;
  }
  if (target == value) return true;
  return target == ValueType::Float && value == ValueType::Integer;
}

ValueType commonNumericType(ValueType left, ValueType right)
{
  if (left == ValueType::Any || right == ValueType::Any ||
      left == ValueType::Unknown || right == ValueType::Unknown) {
    return ValueType::Any;
  }
  return left == ValueType::Float || right == ValueType::Float
    ? ValueType::Float
    : ValueType::Integer;
}

}  // namespace fosc
