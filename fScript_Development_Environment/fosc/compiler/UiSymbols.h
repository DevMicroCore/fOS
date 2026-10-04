#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "../diagnostics/Diagnostic.h"

namespace fosc {

struct UiSymbol {
  std::uint16_t id = 0;
  std::string type;
};

struct UiSymbolsResult {
  std::unordered_map<std::string, UiSymbol> symbols;
  std::vector<Diagnostic> diagnostics;

  bool success() const { return diagnostics.empty(); }
};

class UiSymbols {
 public:
  static UiSymbolsResult parse(const std::string& source, const std::string& filename);
};

}  // namespace fosc
