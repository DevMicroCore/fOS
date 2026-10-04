#pragma once

#include <string>

#include "FAppFile.h"

namespace fosc {

class Disassembler {
 public:
  static std::string disassemble(const ParsedFApp& file);
};

}  // namespace fosc
