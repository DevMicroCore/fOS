#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "../format/FAppFile.h"

namespace fosc {

struct VerificationIssue {
  std::string code;
  std::string message;
  std::size_t functionIndex = 0;
  std::size_t instructionOffset = 0;
};

struct BytecodeVerifierResult {
  std::vector<VerificationIssue> issues;

  bool success() const { return issues.empty(); }
};

class BytecodeVerifier {
 public:
  static BytecodeVerifierResult verify(const ParsedFApp& file);
  static std::string formatIssue(const VerificationIssue& issue);
};

}  // namespace fosc
