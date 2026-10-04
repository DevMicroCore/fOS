#include "FAppFile.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace fosc {
namespace {

void patchU16(std::vector<std::uint8_t> * output, std::size_t offset, std::uint16_t value)
{
  if (offset + 2 > output->size()) return;
  (*output)[offset] = static_cast<std::uint8_t>(value);
  (*output)[offset + 1] = static_cast<std::uint8_t>(value >> 8u);
}

void padToFour(std::vector<std::uint8_t> * output)
{
  while (output->size() % 4u != 0u) output->push_back(0);
}

bool rangeIsValid(std::uint32_t offset, std::uint32_t size, std::size_t fileSize)
{
  return offset <= fileSize && size <= fileSize - offset;
}

bool readHeader(const std::vector<std::uint8_t>& image, fapp::FAppHeader * header)
{
  if (image.size() < fapp::kHeaderSize || header == nullptr) return false;
  header->formatMajor = image[4];
  header->formatMinor = image[5];
  header->bytecodeMajor = image[6];
  header->bytecodeMinor = image[7];
  return fapp::readU16(image, 8, &header->headerSize) &&
         fapp::readU16(image, 10, &header->flags) &&
         fapp::readU32(image, 12, &header->fileSize) &&
         fapp::readU32(image, 16, &header->crc32) &&
         fapp::readU32(image, 20, &header->constantsOffset) &&
         fapp::readU32(image, 24, &header->constantsSize) &&
         fapp::readU16(image, 28, &header->constantCount) &&
         fapp::readU16(image, 30, &header->globalCount) &&
         fapp::readU32(image, 32, &header->functionsOffset) &&
         fapp::readU32(image, 36, &header->functionsSize) &&
         fapp::readU16(image, 40, &header->functionCount) &&
         fapp::readU16(image, 42, &header->initFunction) &&
         fapp::readU32(image, 44, &header->eventsOffset) &&
         fapp::readU32(image, 48, &header->eventsSize) &&
         fapp::readU16(image, 52, &header->eventCount) &&
         fapp::readU32(image, 56, &header->codeOffset) &&
         fapp::readU32(image, 60, &header->codeSize) &&
         fapp::readU32(image, 64, &header->sourceHash) &&
         fapp::readU16(image, 68, &header->minimumFosMajor) &&
         fapp::readU16(image, 70, &header->minimumFosMinor) &&
         fapp::readU16(image, 72, &header->minimumFosPatch);
}

}  // namespace

FAppWriteResult FAppFile::write(const fapp::BytecodeModule& module)
{
  FAppWriteResult result;
  if (module.stringConstants.size() > std::numeric_limits<std::uint16_t>::max() ||
      module.functions.size() > std::numeric_limits<std::uint16_t>::max() ||
      module.events.size() > std::numeric_limits<std::uint16_t>::max()) {
    result.error = "FS401: A section contains too many entries.";
    return result;
  }
  if (module.functions.empty() || module.initFunction >= module.functions.size()) {
    result.error = "FS402: The module has no valid initialization function.";
    return result;
  }

  std::vector<std::uint8_t> constants;
  for (const std::string& value : module.stringConstants) {
    if (value.size() > std::numeric_limits<std::uint16_t>::max()) {
      result.error = "FS403: A string constant is longer than 65535 bytes.";
      return result;
    }
    constants.push_back(static_cast<std::uint8_t>(fapp::ConstantKind::String));
    constants.push_back(0);
    fapp::appendU16(&constants, static_cast<std::uint16_t>(value.size()));
    constants.insert(constants.end(), value.begin(), value.end());
  }

  std::vector<std::uint8_t> functions;
  std::vector<std::uint8_t> code;
  for (const fapp::BytecodeFunction& function : module.functions) {
    if (code.size() > std::numeric_limits<std::uint32_t>::max() ||
        function.code.size() > std::numeric_limits<std::uint32_t>::max() - code.size()) {
      result.error = "FS404: Bytecode is larger than the fAPP format permits.";
      return result;
    }
    fapp::appendU32(&functions, static_cast<std::uint32_t>(code.size()));
    fapp::appendU32(&functions, static_cast<std::uint32_t>(function.code.size()));
    fapp::appendU16(&functions, function.arity);
    fapp::appendU16(&functions, function.localCount);
    fapp::appendU16(&functions, function.maxStack);
    fapp::appendU16(&functions, function.flags);
    code.insert(code.end(), function.code.begin(), function.code.end());
  }

  std::vector<std::uint8_t> events;
  for (const fapp::EventBinding& event : module.events) {
    const bool validObject = event.objectId == 0
      ? fapp::isSystemEvent(event.event)
      : event.objectId <= fapp::kMaximumUiObjects && !fapp::isSystemEvent(event.event);
    if (!validObject ||
        event.functionIndex >= module.functions.size()) {
      result.error = "FS405: An event binding contains an invalid object or function index.";
      return result;
    }
    fapp::appendU16(&events, event.objectId);
    events.push_back(static_cast<std::uint8_t>(event.event));
    events.push_back(0);
    fapp::appendU16(&events, event.functionIndex);
    fapp::appendU16(&events, 0);
  }

  result.image.assign(fapp::kHeaderSize, 0);
  result.image[0] = 'F';
  result.image[1] = 'A';
  result.image[2] = 'P';
  result.image[3] = 'P';
  result.image[4] = fapp::kFormatMajor;
  result.image[5] = fapp::kFormatMinor;
  result.image[6] = fapp::kBytecodeMajor;
  result.image[7] = fapp::kBytecodeMinor;
  patchU16(&result.image, 8, fapp::kHeaderSize);

  const std::uint32_t constantsOffset = static_cast<std::uint32_t>(result.image.size());
  result.image.insert(result.image.end(), constants.begin(), constants.end());
  padToFour(&result.image);
  const std::uint32_t functionsOffset = static_cast<std::uint32_t>(result.image.size());
  result.image.insert(result.image.end(), functions.begin(), functions.end());
  padToFour(&result.image);
  const std::uint32_t eventsOffset = static_cast<std::uint32_t>(result.image.size());
  result.image.insert(result.image.end(), events.begin(), events.end());
  padToFour(&result.image);
  const std::uint32_t codeOffset = static_cast<std::uint32_t>(result.image.size());
  result.image.insert(result.image.end(), code.begin(), code.end());

  if (result.image.size() > std::numeric_limits<std::uint32_t>::max()) {
    result.image.clear();
    result.error = "FS404: fAPP image is larger than 4 GiB.";
    return result;
  }

  fapp::patchU32(&result.image, 12, static_cast<std::uint32_t>(result.image.size()));
  fapp::patchU32(&result.image, 20, constantsOffset);
  fapp::patchU32(&result.image, 24, static_cast<std::uint32_t>(constants.size()));
  patchU16(&result.image, 28, static_cast<std::uint16_t>(module.stringConstants.size()));
  patchU16(&result.image, 30, module.globalCount);
  fapp::patchU32(&result.image, 32, functionsOffset);
  fapp::patchU32(&result.image, 36, static_cast<std::uint32_t>(functions.size()));
  patchU16(&result.image, 40, static_cast<std::uint16_t>(module.functions.size()));
  patchU16(&result.image, 42, module.initFunction);
  fapp::patchU32(&result.image, 44, eventsOffset);
  fapp::patchU32(&result.image, 48, static_cast<std::uint32_t>(events.size()));
  patchU16(&result.image, 52, static_cast<std::uint16_t>(module.events.size()));
  fapp::patchU32(&result.image, 56, codeOffset);
  fapp::patchU32(&result.image, 60, static_cast<std::uint32_t>(code.size()));
  fapp::patchU32(&result.image, 64, module.sourceHash);
  patchU16(&result.image, 68, 4);
  patchU16(&result.image, 70, 0);
  patchU16(&result.image, 72, 0);
  fapp::patchU32(&result.image, 16, fapp::crc32(result.image, 16, 4));
  return result;
}

FAppReadResult FAppFile::read(std::vector<std::uint8_t> image)
{
  FAppReadResult result;
  if (image.size() < fapp::kHeaderSize || image[0] != 'F' || image[1] != 'A' ||
      image[2] != 'P' || image[3] != 'P') {
    result.error = "FS411: Invalid fAPP magic or truncated header.";
    return result;
  }
  if (!readHeader(image, &result.file.header)) {
    result.error = "FS411: Cannot decode the fAPP header.";
    return result;
  }
  const fapp::FAppHeader& header = result.file.header;
  if (header.headerSize != fapp::kHeaderSize || header.fileSize != image.size()) {
    result.error = "FS412: Header or file size is inconsistent.";
    return result;
  }
  if (header.formatMajor != fapp::kFormatMajor || header.bytecodeMajor != fapp::kBytecodeMajor ||
      header.formatMinor > fapp::kFormatMinor || header.bytecodeMinor > fapp::kBytecodeMinor) {
    result.error = "FS413: Unsupported fAPP or bytecode version.";
    return result;
  }
  if (fapp::crc32(image, 16, 4) != header.crc32) {
    result.error = "FS414: CRC-32 validation failed.";
    return result;
  }
  if (!rangeIsValid(header.constantsOffset, header.constantsSize, image.size()) ||
      !rangeIsValid(header.functionsOffset, header.functionsSize, image.size()) ||
      !rangeIsValid(header.eventsOffset, header.eventsSize, image.size()) ||
      !rangeIsValid(header.codeOffset, header.codeSize, image.size()) ||
      header.constantsOffset < header.headerSize ||
      header.constantsOffset + header.constantsSize > header.functionsOffset ||
      header.functionsOffset + header.functionsSize > header.eventsOffset ||
      header.eventsOffset + header.eventsSize > header.codeOffset ||
      header.codeOffset + header.codeSize != header.fileSize) {
    result.error = "FS415: A section lies outside the fAPP file.";
    return result;
  }
  if (header.functionsSize != static_cast<std::uint32_t>(header.functionCount) * 16u ||
      header.eventsSize != static_cast<std::uint32_t>(header.eventCount) * 8u ||
      header.functionCount == 0 || header.initFunction >= header.functionCount) {
    result.error = "FS416: Function or event table size is invalid.";
    return result;
  }

  std::size_t constantOffset = header.constantsOffset;
  const std::size_t constantsEnd = header.constantsOffset + header.constantsSize;
  for (std::uint16_t index = 0; index < header.constantCount; ++index) {
    if (constantOffset + 4 > constantsEnd ||
        image[constantOffset] != static_cast<std::uint8_t>(fapp::ConstantKind::String)) {
      result.error = "FS417: Invalid constant table entry.";
      return result;
    }
    std::uint16_t size = 0;
    fapp::readU16(image, constantOffset + 2, &size);
    constantOffset += 4;
    if (constantOffset + size > constantsEnd) {
      result.error = "FS417: Truncated string constant.";
      return result;
    }
    result.file.stringConstants.emplace_back(
      reinterpret_cast<const char *>(&image[constantOffset]), size);
    constantOffset += size;
  }
  if (constantOffset != constantsEnd) {
    result.error = "FS417: Constant table size does not match its entries.";
    return result;
  }

  for (std::uint16_t index = 0; index < header.functionCount; ++index) {
    const std::size_t offset = header.functionsOffset + static_cast<std::size_t>(index) * 16u;
    FAppFunctionRecord function;
    fapp::readU32(image, offset, &function.codeOffset);
    fapp::readU32(image, offset + 4, &function.codeSize);
    fapp::readU16(image, offset + 8, &function.arity);
    fapp::readU16(image, offset + 10, &function.localCount);
    fapp::readU16(image, offset + 12, &function.maxStack);
    fapp::readU16(image, offset + 14, &function.flags);
    if (function.codeOffset > header.codeSize ||
        function.codeSize > header.codeSize - function.codeOffset) {
      result.error = "FS418: Function bytecode lies outside the code section.";
      return result;
    }
    result.file.functions.push_back(function);
  }

  for (std::uint16_t index = 0; index < header.eventCount; ++index) {
    const std::size_t offset = header.eventsOffset + static_cast<std::size_t>(index) * 8u;
    std::uint16_t objectId = 0;
    std::uint16_t functionIndex = 0;
    fapp::readU16(image, offset, &objectId);
    fapp::readU16(image, offset + 4, &functionIndex);
    const std::uint8_t eventType = image[offset + 2];
    const fapp::UiEvent decodedEvent = static_cast<fapp::UiEvent>(eventType);
    const bool validObject = objectId == 0
      ? fapp::isSystemEvent(decodedEvent)
      : objectId <= fapp::kMaximumUiObjects && !fapp::isSystemEvent(decodedEvent) &&
        eventType >= static_cast<std::uint8_t>(fapp::UiEvent::Click) &&
        eventType <= static_cast<std::uint8_t>(fapp::UiEvent::PointerUp);
    if (!validObject ||
        functionIndex >= header.functionCount) {
      result.error = "FS419: Event binding contains an invalid index.";
      return result;
    }
    result.file.events.push_back({
      objectId,
      decodedEvent,
      functionIndex
    });
  }

  result.file.image = std::move(image);
  return result;
}

}  // namespace fosc
