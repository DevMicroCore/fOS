#pragma once

#include <cstddef>
#include <string>
#include <unordered_map>
#include <vector>

#include "../ast/Ast.h"
#include "../compiler/UiSymbols.h"
#include "../diagnostics/Diagnostic.h"
#include "Type.h"

namespace fosc {

struct SemanticAnalyzerResult {
  std::vector<Diagnostic> diagnostics;
  std::size_t globalCount = 0;
  std::size_t functionCount = 0;
  std::size_t eventCount = 0;

  bool success() const { return diagnostics.empty(); }
};

class SemanticAnalyzer {
 public:
  SemanticAnalyzer(
    const Program& program,
    const std::unordered_map<std::string, UiSymbol>& uiSymbols,
    std::string filename);

  SemanticAnalyzerResult analyze();

 private:
  const Program& program_;
  const std::unordered_map<std::string, UiSymbol>& uiSymbols_;
  std::string filename_;
};

}  // namespace fosc
