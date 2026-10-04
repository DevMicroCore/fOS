#include "UiSymbols.h"

#include <algorithm>
#include <cctype>
#include <sstream>
#include <unordered_set>

#include "../bytecode/Bytecode.h"

namespace fosc {
namespace {

std::string trim(const std::string& value)
{
  std::size_t begin = 0;
  while (begin < value.size() && std::isspace(static_cast<unsigned char>(value[begin]))) ++begin;
  std::size_t end = value.size();
  while (end > begin && std::isspace(static_cast<unsigned char>(value[end - 1]))) --end;
  return value.substr(begin, end - begin);
}

std::string lower(std::string value)
{
  std::transform(value.begin(), value.end(), value.begin(), [](unsigned char character) {
    return static_cast<char>(std::tolower(character));
  });
  return value;
}

bool isIdentifier(const std::string& value)
{
  if (value.empty()) return false;
  const auto first = static_cast<unsigned char>(value.front());
  if (!(std::isalpha(first) || first == '_')) return false;
  for (std::size_t index = 1; index < value.size(); ++index) {
    const auto character = static_cast<unsigned char>(value[index]);
    if (!(std::isalnum(character) || character == '_')) return false;
  }
  return true;
}

std::unordered_map<std::string, std::string> parseFields(const std::string& line)
{
  std::unordered_map<std::string, std::string> fields;
  std::size_t start = 0;
  while (start <= line.size()) {
    const std::size_t end = line.find(';', start);
    const std::string field = line.substr(start, end == std::string::npos ? end : end - start);
    const std::size_t equals = field.find('=');
    if (equals != std::string::npos) {
      fields[lower(trim(field.substr(0, equals)))] = trim(field.substr(equals + 1));
    }
    if (end == std::string::npos) break;
    start = end + 1;
  }
  return fields;
}

bool isSupportedObjectType(const std::string& type)
{
  static const std::unordered_set<std::string> supported = {
    "label", "button", "textarea", "switch", "checkbox", "panel",
    "roller", "dropdown", "keyboard", "canvas"
  };
  return supported.find(type) != supported.end();
}

void addDiagnostic(
  UiSymbolsResult * result,
  const std::string& code,
  const std::string& message,
  const std::string& filename,
  std::size_t line)
{
  result->diagnostics.push_back({code, message, filename, {line, 1, 0}});
}

}  // namespace

UiSymbolsResult UiSymbols::parse(const std::string& source, const std::string& filename)
{
  UiSymbolsResult result;
  std::istringstream input(source);
  std::string line;
  std::size_t lineNumber = 0;
  std::uint16_t nextId = 1;

  while (std::getline(input, line)) {
    ++lineNumber;
    const std::string clean = trim(line);
    if (clean.empty() || clean.front() == '#') continue;

    const auto fields = parseFields(clean);
    const auto typeIt = fields.find("type");
    const auto idIt = fields.find("id");
    if (typeIt == fields.end() || idIt == fields.end() || idIt->second.empty()) continue;
    const std::string type = lower(typeIt->second);
    if (!isSupportedObjectType(type)) continue;

    const std::string& name = idIt->second;
    if (!isIdentifier(name)) {
      addDiagnostic(
        &result,
        "FS301",
        "UI id '" + name + "' is not a valid fScript identifier.",
        filename,
        lineNumber);
      continue;
    }
    if (result.symbols.find(name) != result.symbols.end()) {
      addDiagnostic(
        &result,
        "FS302",
        "Duplicate UI id '" + name + "'.",
        filename,
        lineNumber);
      continue;
    }
    if (nextId > fapp::kMaximumUiObjects) {
      addDiagnostic(
        &result,
        "FS303",
        "The layout contains more than 64 named UI objects.",
        filename,
        lineNumber);
      continue;
    }

    result.symbols.emplace(name, UiSymbol{nextId, type});
    ++nextId;
  }

  return result;
}

}  // namespace fosc
