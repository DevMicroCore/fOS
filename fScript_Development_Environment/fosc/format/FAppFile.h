#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "../bytecode/Bytecode.h"

namespace fosc {

struct FAppWriteResult {
  std::vector<std::uint8_t> image;
  std::string error;

  bool success() const { return error.empty(); }
};

struct FAppFunctionRecord {
  std::uint32_t codeOffset = 0;
  std::uint32_t codeSize = 0;
  std::uint16_t arity = 0;
  std::uint16_t localCount = 0;
  std::uint16_t maxStack = 0;
  std::uint16_t flags = 0;
};

struct ParsedFApp {
  fapp::FAppHeader header;
  std::vector<std::string> stringConstants;
  std::vector<FAppFunctionRecord> functions;
  std::vector<fapp::EventBinding> events;
  std::vector<std::uint8_t> image;
};

struct FAppReadResult {
  ParsedFApp file;
  std::string error;

  bool success() const { return error.empty(); }
};

class FAppFile {
 public:
  static FAppWriteResult write(const fapp::BytecodeModule& module);
  static FAppReadResult read(std::vector<std::uint8_t> image);
};

}  // namespace fosc
