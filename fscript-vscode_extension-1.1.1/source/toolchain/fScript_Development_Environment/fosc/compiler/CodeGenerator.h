#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include "../ast/Ast.h"
#include "../bytecode/Bytecode.h"
#include "../diagnostics/Diagnostic.h"
#include "UiSymbols.h"

namespace fosc {

struct CodeGeneratorResult {
  fapp::BytecodeModule module;
  std::vector<Diagnostic> diagnostics;

  bool success() const { return diagnostics.empty(); }
};

class CodeGenerator {
 public:
  CodeGenerator(
    const Program& program,
    const std::unordered_map<std::string, UiSymbol>& uiSymbols,
    std::string filename,
    std::string source,
    bool optimizeConstants = true);

  CodeGeneratorResult generate();

 private:
  const Program& program_;
  const std::unordered_map<std::string, UiSymbol>& uiSymbols_;
  std::string filename_;
  std::string source_;
  bool optimizeConstants_;
};

}  // namespace fosc
