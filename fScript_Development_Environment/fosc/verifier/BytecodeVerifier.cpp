#include "BytecodeVerifier.h"

#include <cstdint>
#include <deque>
#include <limits>
#include <sstream>
#include <unordered_map>
#include <unordered_set>

#include "../bytecode/Bytecode.h"

namespace fosc {
namespace {

struct Instruction {
  fapp::Opcode opcode = fapp::Opcode::Nop;
  std::size_t offset = 0;
  std::size_t size = 1;
  int pops = 0;
  int pushes = 0;
  bool hasJump = false;
  bool conditionalJump = false;
  bool returns = false;
  std::size_t jumpTarget = 0;
};

void issue(
  BytecodeVerifierResult * result,
  const char * code,
  const std::string& message,
  std::size_t functionIndex,
  std::size_t offset)
{
  result->issues.push_back({code, message, functionIndex, offset});
}

bool validProperty(std::uint8_t value)
{
  return value >= static_cast<std::uint8_t>(fapp::UiProperty::Text) &&
         value <= static_cast<std::uint8_t>(fapp::UiProperty::Enabled);
}

bool validMethod(std::uint8_t value)
{
  return value >= static_cast<std::uint8_t>(fapp::UiMethod::Clear) &&
         value <= static_cast<std::uint8_t>(fapp::UiMethod::DrawText);
}

bool validNative(std::uint8_t value, std::uint8_t arity)
{
  fapp::NativeFunction function = static_cast<fapp::NativeFunction>(value);
  std::uint8_t expected = 0;
  switch (function) {
    case fapp::NativeFunction::HttpGet: expected = 1; break;
    case fapp::NativeFunction::HttpStatus: expected = 0; break;
    case fapp::NativeFunction::HttpJson: expected = 1; break;
    case fapp::NativeFunction::HttpText: expected = 1; break;
    case fapp::NativeFunction::UrlEncode: expected = 1; break;
    case fapp::NativeFunction::DateWeekday: expected = 1; break;
    case fapp::NativeFunction::FileRead: expected = 1; break;
    case fapp::NativeFunction::FileWrite: expected = 2; break;
    case fapp::NativeFunction::AudioPlay: expected = 1; break;
    case fapp::NativeFunction::WifiStatus: expected = 0; break;
    case fapp::NativeFunction::SystemRestart: expected = 0; break;
    case fapp::NativeFunction::TimerStart: expected = 1; break;
    case fapp::NativeFunction::SerialPrintf: expected = 1; break;
    case fapp::NativeFunction::Text: expected = 1; break;
    case fapp::NativeFunction::NumberParse: expected = 1; break;
    case fapp::NativeFunction::Round: expected = 1; break;
    case fapp::NativeFunction::StringLength: expected = 1; break;
    case fapp::NativeFunction::StringSlice: expected = 3; break;
    case fapp::NativeFunction::MathEval: expected = 1; break;
    case fapp::NativeFunction::StringLastIndex: expected = 2; break;
    case fapp::NativeFunction::FileList: expected = 1; break;
    case fapp::NativeFunction::EventX:
    case fapp::NativeFunction::EventY:
    case fapp::NativeFunction::EventScreenX:
    case fapp::NativeFunction::EventScreenY:
    case fapp::NativeFunction::EventPressed: expected = 0; break;
    default: return false;
  }
  return arity == expected;
}

bool decodeFunction(
  const ParsedFApp& file,
  std::size_t functionIndex,
  std::vector<Instruction> * instructions,
  BytecodeVerifierResult * result)
{
  const FAppFunctionRecord& function = file.functions[functionIndex];
  const std::size_t codeBegin = file.header.codeOffset + function.codeOffset;
  const std::size_t codeSize = function.codeSize;
  std::size_t offset = 0;
  bool valid = true;

  auto require = [&](std::size_t current, std::size_t operandSize) {
    if (current + operandSize <= codeSize) return true;
    issue(result, "FS601", "Truncated instruction operand.", functionIndex, offset);
    valid = false;
    return false;
  };

  while (offset < codeSize) {
    const std::size_t start = offset;
    Instruction instruction;
    instruction.offset = start;
    instruction.opcode = static_cast<fapp::Opcode>(file.image[codeBegin + offset++]);
    std::uint16_t u16 = 0;
    std::int32_t i32 = 0;

    switch (instruction.opcode) {
      case fapp::Opcode::Nop:
      case fapp::Opcode::PushNil:
      case fapp::Opcode::PushFalse:
      case fapp::Opcode::PushTrue:
        instruction.pushes = instruction.opcode == fapp::Opcode::Nop ? 0 : 1;
        break;
      case fapp::Opcode::PushInt:
        if (!require(offset, 4)) return false;
        offset += 4;
        instruction.pushes = 1;
        break;
      case fapp::Opcode::PushFloat:
        if (!require(offset, 8)) return false;
        offset += 8;
        instruction.pushes = 1;
        break;
      case fapp::Opcode::PushString:
        if (!require(offset, 2)) return false;
        fapp::readU16(file.image, codeBegin + offset, &u16);
        if (u16 >= file.stringConstants.size()) {
          issue(result, "FS603", "String constant index is out of range.", functionIndex, start);
          valid = false;
        }
        offset += 2;
        instruction.pushes = 1;
        break;
      case fapp::Opcode::Pop:
        instruction.pops = 1;
        break;
      case fapp::Opcode::Duplicate:
        instruction.pops = 1;
        instruction.pushes = 2;
        break;
      case fapp::Opcode::LoadGlobal:
      case fapp::Opcode::StoreGlobal:
        if (!require(offset, 2)) return false;
        fapp::readU16(file.image, codeBegin + offset, &u16);
        if (u16 >= file.header.globalCount) {
          issue(result, "FS603", "Global variable index is out of range.", functionIndex, start);
          valid = false;
        }
        offset += 2;
        instruction.pops = instruction.opcode == fapp::Opcode::StoreGlobal ? 1 : 0;
        instruction.pushes = instruction.opcode == fapp::Opcode::LoadGlobal ? 1 : 0;
        break;
      case fapp::Opcode::LoadLocal:
      case fapp::Opcode::StoreLocal:
        if (!require(offset, 2)) return false;
        fapp::readU16(file.image, codeBegin + offset, &u16);
        if (u16 >= function.localCount) {
          issue(result, "FS603", "Local variable index is out of range.", functionIndex, start);
          valid = false;
        }
        offset += 2;
        instruction.pops = instruction.opcode == fapp::Opcode::StoreLocal ? 1 : 0;
        instruction.pushes = instruction.opcode == fapp::Opcode::LoadLocal ? 1 : 0;
        break;
      case fapp::Opcode::GetUiProperty:
      case fapp::Opcode::SetUiProperty:
        if (!require(offset, 3)) return false;
        fapp::readU16(file.image, codeBegin + offset, &u16);
        if (u16 == 0 || u16 > fapp::kMaximumUiObjects ||
            !validProperty(file.image[codeBegin + offset + 2])) {
          issue(result, "FS603", "UI object or property index is invalid.", functionIndex, start);
          valid = false;
        }
        offset += 3;
        instruction.pops = instruction.opcode == fapp::Opcode::SetUiProperty ? 1 : 0;
        instruction.pushes = instruction.opcode == fapp::Opcode::GetUiProperty ? 1 : 0;
        break;
      case fapp::Opcode::CallFunction: {
        if (!require(offset, 3)) return false;
        fapp::readU16(file.image, codeBegin + offset, &u16);
        const std::uint8_t argumentCount = file.image[codeBegin + offset + 2];
        if (u16 >= file.functions.size()) {
          issue(result, "FS603", "Function index is out of range.", functionIndex, start);
          valid = false;
        } else if (argumentCount != file.functions[u16].arity) {
          issue(result, "FS604", "Call argument count does not match function arity.", functionIndex, start);
          valid = false;
        }
        offset += 3;
        instruction.pops = argumentCount;
        instruction.pushes = 1;
        break;
      }
      case fapp::Opcode::CallUiMethod: {
        if (!require(offset, 4)) return false;
        fapp::readU16(file.image, codeBegin + offset, &u16);
        const std::uint8_t method = file.image[codeBegin + offset + 2];
        const std::uint8_t argumentCount = file.image[codeBegin + offset + 3];
        if (u16 == 0 || u16 > fapp::kMaximumUiObjects || !validMethod(method)) {
          issue(result, "FS603", "UI object or method index is invalid.", functionIndex, start);
          valid = false;
        }
        std::uint8_t expectedArguments = 0;
        if (method == static_cast<std::uint8_t>(fapp::UiMethod::LoadFile) ||
            method == static_cast<std::uint8_t>(fapp::UiMethod::SaveFile)) expectedArguments = 1;
        else if (method == static_cast<std::uint8_t>(fapp::UiMethod::Pixel)) expectedArguments = 3;
        else if (method == static_cast<std::uint8_t>(fapp::UiMethod::Line)) expectedArguments = 6;
        else if (method == static_cast<std::uint8_t>(fapp::UiMethod::Rect)) expectedArguments = 6;
        else if (method == static_cast<std::uint8_t>(fapp::UiMethod::Circle)) expectedArguments = 5;
        else if (method == static_cast<std::uint8_t>(fapp::UiMethod::DrawText)) expectedArguments = 5;
        const bool validClearCount = method == static_cast<std::uint8_t>(fapp::UiMethod::Clear) &&
          (argumentCount == 0 || argumentCount == 1);
        if (!validClearCount && argumentCount != expectedArguments) {
          issue(result, "FS604", "UI method argument count is invalid.", functionIndex, start);
          valid = false;
        }
        offset += 4;
        instruction.pops = argumentCount;
        instruction.pushes = 1;
        break;
      }
      case fapp::Opcode::CallNative: {
        if (!require(offset, 2)) return false;
        const std::uint8_t native = file.image[codeBegin + offset];
        const std::uint8_t argumentCount = file.image[codeBegin + offset + 1];
        if (!validNative(native, argumentCount)) {
          issue(result, "FS604", "Native function or argument count is invalid.", functionIndex, start);
          valid = false;
        }
        offset += 2;
        instruction.pops = argumentCount;
        instruction.pushes = 1;
        break;
      }
      case fapp::Opcode::Negate:
      case fapp::Opcode::LogicalNot:
        instruction.pops = 1;
        instruction.pushes = 1;
        break;
      case fapp::Opcode::Add:
      case fapp::Opcode::Subtract:
      case fapp::Opcode::Multiply:
      case fapp::Opcode::Divide:
      case fapp::Opcode::Modulo:
      case fapp::Opcode::Equal:
      case fapp::Opcode::NotEqual:
      case fapp::Opcode::Less:
      case fapp::Opcode::LessEqual:
      case fapp::Opcode::Greater:
      case fapp::Opcode::GreaterEqual:
      case fapp::Opcode::LogicalAnd:
      case fapp::Opcode::LogicalOr:
        instruction.pops = 2;
        instruction.pushes = 1;
        break;
      case fapp::Opcode::Jump:
      case fapp::Opcode::JumpIfFalse: {
        if (!require(offset, 4)) return false;
        fapp::readI32(file.image, codeBegin + offset, &i32);
        const std::int64_t target = static_cast<std::int64_t>(offset + 4) + i32;
        if (target < 0 || target >= static_cast<std::int64_t>(codeSize)) {
          issue(result, "FS605", "Jump target lies outside the function.", functionIndex, start);
          valid = false;
        } else {
          instruction.jumpTarget = static_cast<std::size_t>(target);
        }
        offset += 4;
        instruction.hasJump = true;
        instruction.conditionalJump = instruction.opcode == fapp::Opcode::JumpIfFalse;
        instruction.pops = instruction.conditionalJump ? 1 : 0;
        break;
      }
      case fapp::Opcode::Return:
        instruction.pops = 1;
        instruction.returns = true;
        break;
      default:
        issue(result, "FS602", "Unknown opcode.", functionIndex, start);
        return false;
    }
    instruction.size = offset - start;
    instructions->push_back(instruction);
  }
  return valid;
}

void verifyControlFlow(
  const ParsedFApp& file,
  std::size_t functionIndex,
  const std::vector<Instruction>& instructions,
  BytecodeVerifierResult * result)
{
  if (instructions.empty()) {
    issue(result, "FS607", "Function contains no instructions.", functionIndex, 0);
    return;
  }
  std::unordered_map<std::size_t, std::size_t> instructionByOffset;
  for (std::size_t index = 0; index < instructions.size(); ++index) {
    instructionByOffset.emplace(instructions[index].offset, index);
  }
  bool invalidBoundary = false;
  for (const Instruction& instruction : instructions) {
    if (instruction.hasJump && instructionByOffset.find(instruction.jumpTarget) == instructionByOffset.end()) {
      issue(result, "FS605", "Jump target is not an instruction boundary.", functionIndex, instruction.offset);
      invalidBoundary = true;
    }
  }
  if (invalidBoundary) return;

  const FAppFunctionRecord& function = file.functions[functionIndex];
  std::unordered_map<std::size_t, int> depths;
  std::deque<std::size_t> work;
  depths.emplace(0, 0);
  work.push_back(0);
  int observedMaximum = 0;

  auto schedule = [&](std::size_t target, int depth, const Instruction& from) {
    const auto existing = depths.find(target);
    if (existing == depths.end()) {
      depths.emplace(target, depth);
      work.push_back(target);
    } else if (existing->second != depth) {
      issue(result, "FS606", "Control-flow paths reach an instruction with different stack depths.",
            functionIndex, from.offset);
    }
  };

  while (!work.empty()) {
    const std::size_t offset = work.front();
    work.pop_front();
    const auto found = instructionByOffset.find(offset);
    if (found == instructionByOffset.end()) continue;
    const Instruction& instruction = instructions[found->second];
    const int before = depths[offset];
    if (before < instruction.pops) {
      issue(result, "FS606", "Operand stack underflow.", functionIndex, instruction.offset);
      continue;
    }
    const int after = before - instruction.pops + instruction.pushes;
    if (after > observedMaximum) observedMaximum = after;
    if (instruction.returns) {
      if (after != 0) {
        issue(result, "FS606", "Return leaves additional values on the operand stack.",
              functionIndex, instruction.offset);
      }
      continue;
    }
    if (instruction.hasJump) {
      schedule(instruction.jumpTarget, after, instruction);
      if (!instruction.conditionalJump) continue;
    }
    const std::size_t next = instruction.offset + instruction.size;
    if (next >= function.codeSize) {
      issue(result, "FS607", "Reachable control flow falls off the end of the function.",
            functionIndex, instruction.offset);
    } else {
      schedule(next, after, instruction);
    }
  }

  if (observedMaximum > function.maxStack) {
    issue(result, "FS608", "Declared maximum stack depth is too small.", functionIndex, 0);
  }
}

}  // namespace

BytecodeVerifierResult BytecodeVerifier::verify(const ParsedFApp& file)
{
  BytecodeVerifierResult result;
  if (file.functions.size() != file.header.functionCount) {
    issue(&result, "FS603", "Function table count differs from the header.", 0, 0);
    return result;
  }
  if (file.header.initFunction >= file.functions.size() ||
      file.functions[file.header.initFunction].arity != 0) {
    issue(&result, "FS604", "Initialization function must exist and have arity zero.", 0, 0);
  }
  for (std::size_t index = 0; index < file.functions.size(); ++index) {
    if (file.functions[index].localCount < file.functions[index].arity) {
      issue(&result, "FS603", "Function has fewer local slots than parameters.", index, 0);
      continue;
    }
    std::vector<Instruction> instructions;
    const std::size_t issueCount = result.issues.size();
    decodeFunction(file, index, &instructions, &result);
    if (result.issues.size() == issueCount) verifyControlFlow(file, index, instructions, &result);
  }
  return result;
}

std::string BytecodeVerifier::formatIssue(const VerificationIssue& issue)
{
  std::ostringstream output;
  output << "Error " << issue.code << ": " << issue.message << '\n'
         << "Function " << issue.functionIndex << ", bytecode offset "
         << issue.instructionOffset;
  return output.str();
}

}  // namespace fosc
