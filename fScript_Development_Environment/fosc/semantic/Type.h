#pragma once

#include <string>

namespace fosc {

enum class ValueType {
  Unknown,
  Any,
  Nil,
  Boolean,
  Integer,
  Float,
  String
};

const char * valueTypeName(ValueType type);
bool isNumericType(ValueType type);
bool isAssignableType(ValueType target, ValueType value);
ValueType commonNumericType(ValueType left, ValueType right);

}  // namespace fosc
