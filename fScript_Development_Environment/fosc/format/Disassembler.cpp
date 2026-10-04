#include "Disassembler.h"

#include <iomanip>
#include <sstream>

namespace fosc {
namespace {

std::string escaped(const std::string& value)
{
  std::string output;
  for (char character : value) {
    if (character == '\n') output += "\\n";
    else if (character == '\r') output += "\\r";
    else if (character == '\t') output += "\\t";
    else if (character == '"') output += "\\\"";
    else if (character == '\\') output += "\\\\";
    else output += character;
  }
  return output;
}

void invalidInstruction(std::ostringstream * output)
{
  *output << " <truncated or unknown instruction>\n";
}

}  // namespace

std::string Disassembler::disassemble(const ParsedFApp& file)
{
  std::ostringstream output;
  output << "fAPP " << static_cast<unsigned int>(file.header.formatMajor) << '.'
         << static_cast<unsigned int>(file.header.formatMinor)
         << " | bytecode " << static_cast<unsigned int>(file.header.bytecodeMajor) << '.'
         << static_cast<unsigned int>(file.header.bytecodeMinor) << '\n';
  output << "Size: " << file.header.fileSize << " bytes | CRC32: 0x"
         << std::hex << std::uppercase << std::setw(8) << std::setfill('0')
         << file.header.crc32 << std::dec << std::nouppercase << std::setfill(' ') << '\n';
  output << "Globals: " << file.header.globalCount
         << " | Constants: " << file.header.constantCount
         << " | Functions: " << file.header.functionCount
         << " | Events: " << file.header.eventCount << "\n\n";

  if (!file.stringConstants.empty()) {
    output << "Constants\n";
    for (std::size_t index = 0; index < file.stringConstants.size(); ++index) {
      output << "  #" << index << " string \"" << escaped(file.stringConstants[index]) << "\"\n";
    }
    output << '\n';
  }

  if (!file.events.empty()) {
    output << "Events\n";
    for (const fapp::EventBinding& event : file.events) {
      output << "  object #" << event.objectId << '.' << fapp::uiEventName(event.event)
             << " -> function #" << event.functionIndex << '\n';
    }
    output << '\n';
  }

  for (std::size_t functionIndex = 0; functionIndex < file.functions.size(); ++functionIndex) {
    const FAppFunctionRecord& function = file.functions[functionIndex];
    output << "Function #" << functionIndex;
    if (functionIndex == file.header.initFunction) output << " <init>";
    output << " (arity=" << function.arity << ", locals=" << function.localCount
           << ", max_stack=" << function.maxStack << ")\n";

    const std::size_t begin = file.header.codeOffset + function.codeOffset;
    const std::size_t end = begin + function.codeSize;
    std::size_t offset = begin;
    while (offset < end) {
      const std::size_t relativeOffset = offset - begin;
      const auto opcode = static_cast<fapp::Opcode>(file.image[offset++]);
      output << "  " << std::setw(4) << std::setfill('0') << relativeOffset
             << std::setfill(' ') << "  " << fapp::opcodeName(opcode);

      auto require = [&](std::size_t size) { return offset + size <= end; };
      std::uint16_t u16 = 0;
      std::int32_t i32 = 0;
      double f64 = 0;
      switch (opcode) {
        case fapp::Opcode::PushInt:
          if (!require(4) || !fapp::readI32(file.image, offset, &i32)) { invalidInstruction(&output); offset = end; continue; }
          output << ' ' << i32;
          offset += 4;
          break;
        case fapp::Opcode::PushFloat:
          if (!require(8) || !fapp::readF64(file.image, offset, &f64)) { invalidInstruction(&output); offset = end; continue; }
          output << ' ' << f64;
          offset += 8;
          break;
        case fapp::Opcode::PushString:
        case fapp::Opcode::LoadGlobal:
        case fapp::Opcode::StoreGlobal:
        case fapp::Opcode::LoadLocal:
        case fapp::Opcode::StoreLocal:
          if (!require(2) || !fapp::readU16(file.image, offset, &u16)) { invalidInstruction(&output); offset = end; continue; }
          output << " #" << u16;
          if (opcode == fapp::Opcode::PushString && u16 < file.stringConstants.size()) {
            output << " \"" << escaped(file.stringConstants[u16]) << '"';
          }
          offset += 2;
          break;
        case fapp::Opcode::GetUiProperty:
        case fapp::Opcode::SetUiProperty:
          if (!require(3) || !fapp::readU16(file.image, offset, &u16)) { invalidInstruction(&output); offset = end; continue; }
          output << " object #" << u16 << '.'
                 << fapp::uiPropertyName(static_cast<fapp::UiProperty>(file.image[offset + 2]));
          offset += 3;
          break;
        case fapp::Opcode::CallFunction:
          if (!require(3) || !fapp::readU16(file.image, offset, &u16)) { invalidInstruction(&output); offset = end; continue; }
          output << " #" << u16 << " argc=" << static_cast<unsigned int>(file.image[offset + 2]);
          offset += 3;
          break;
        case fapp::Opcode::CallUiMethod:
          if (!require(4) || !fapp::readU16(file.image, offset, &u16)) { invalidInstruction(&output); offset = end; continue; }
          output << " object #" << u16 << '.'
                 << fapp::uiMethodName(static_cast<fapp::UiMethod>(file.image[offset + 2]))
                 << " argc=" << static_cast<unsigned int>(file.image[offset + 3]);
          offset += 4;
          break;
        case fapp::Opcode::CallNative:
          if (!require(2)) { invalidInstruction(&output); offset = end; continue; }
          output << ' ' << fapp::nativeFunctionName(
            static_cast<fapp::NativeFunction>(file.image[offset]))
                 << " argc=" << static_cast<unsigned int>(file.image[offset + 1]);
          offset += 2;
          break;
        case fapp::Opcode::Jump:
        case fapp::Opcode::JumpIfFalse:
          if (!require(4) || !fapp::readI32(file.image, offset, &i32)) { invalidInstruction(&output); offset = end; continue; }
          output << " -> " << static_cast<std::int64_t>(offset + 4 - begin) + i32;
          offset += 4;
          break;
        case fapp::Opcode::Nop:
        case fapp::Opcode::PushNil:
        case fapp::Opcode::PushFalse:
        case fapp::Opcode::PushTrue:
        case fapp::Opcode::Pop:
        case fapp::Opcode::Duplicate:
        case fapp::Opcode::Negate:
        case fapp::Opcode::LogicalNot:
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
        case fapp::Opcode::Return:
          break;
        default:
          invalidInstruction(&output);
          offset = end;
          continue;
      }
      output << '\n';
    }
    output << '\n';
  }
  return output.str();
}

}  // namespace fosc
