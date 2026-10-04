#pragma once

#include <string>

#include "../lexer/Token.h"

namespace fosc {

struct Diagnostic {
  std::string code;
  std::string message;
  std::string filename;
  SourceLocation location;
};

std::string formatDiagnostic(const Diagnostic& diagnostic);

}  // namespace fosc
