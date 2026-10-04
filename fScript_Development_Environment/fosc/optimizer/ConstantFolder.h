#pragma once

#include <cstdint>
#include <string>

#include "../ast/Ast.h"

namespace fosc {

enum class ConstantValueKind {
  Nil,
  Boolean,
  Integer,
  Float,
  String
};

struct ConstantValue {
  ConstantValueKind kind = ConstantValueKind::Nil;
  bool booleanValue = false;
  std::int32_t integerValue = 0;
  double floatValue = 0.0;
  std::string stringValue;
};

class ConstantFolder {
 public:
  static bool evaluate(const Expression& expression, ConstantValue * result);
};

}  // namespace fosc
