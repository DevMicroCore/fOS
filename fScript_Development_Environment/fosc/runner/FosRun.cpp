#include <algorithm>
#include <cmath>
#include <cctype>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "../bytecode/Bytecode.h"
#include "../compiler/UiSymbols.h"
#include "src/fapp/FAppLoader.h"
#include "src/fapp/FAppNativeApi.h"
#include "src/fapp/FAppRuntime.h"

namespace {

struct UiObjectState {
  std::uint16_t id = 0;
  std::string name;
  std::string type;
  std::string parent;
  std::string text;
  std::string placeholder;
  std::string target;
  std::string bg;
  std::string fg;
  std::string align;
  std::vector<std::string> options;
  std::vector<std::string> drawCommands;
  int x = 0;
  int y = 0;
  int w = 120;
  int h = 40;
  int font = 20;
  int radius = 6;
  int value = 0;
  bool checked = false;
  bool hidden = false;
  bool enabled = true;
  bool focused = false;
  bool clickable = true;
};

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

std::string unescape(std::string value)
{
  std::string result;
  for (std::size_t index = 0; index < value.size(); ++index) {
    if (value[index] != '\\' || index + 1 >= value.size()) {
      result += value[index];
      continue;
    }
    const char next = value[++index];
    if (next == 'n') result += '\n';
    else if (next == 'r') result += '\r';
    else if (next == 't') result += '\t';
    else result += next;
  }
  return result;
}

std::string escape(std::string value)
{
  std::string result;
  for (char character : value) {
    if (character == '\n') result += "\\n";
    else if (character == '\r') result += "\\r";
    else if (character == '\t') result += "\\t";
    else if (character == '"') result += "\\\"";
    else result += character;
  }
  return result;
}

std::string jsonEscape(const std::string& value)
{
  std::string result;
  for (unsigned char character : value) {
    switch (character) {
      case '\\': result += "\\\\"; break;
      case '"': result += "\\\""; break;
      case '\n': result += "\\n"; break;
      case '\r': result += "\\r"; break;
      case '\t': result += "\\t"; break;
      default:
        if (character < 0x20) {
          char buffer[7];
          std::snprintf(buffer, sizeof(buffer), "\\u%04x", character);
          result += buffer;
        } else {
          result += static_cast<char>(character);
        }
        break;
    }
  }
  return result;
}

std::string urlDecode(const std::string& value)
{
  std::string result;
  for (std::size_t index = 0; index < value.size(); ++index) {
    if (value[index] == '%' && index + 2 < value.size()) {
      const std::string hex = value.substr(index + 1, 2);
      char * end = nullptr;
      const long decoded = std::strtol(hex.c_str(), &end, 16);
      if (end != hex.c_str() && *end == '\0') {
        result += static_cast<char>(decoded);
        index += 2;
        continue;
      }
    }
    result += value[index] == '+' ? ' ' : value[index];
  }
  return result;
}

int parseIntField(const std::unordered_map<std::string, std::string>& fields, const char * name, int fallback)
{
  const auto found = fields.find(name);
  if (found == fields.end()) return fallback;
  char * end = nullptr;
  const long parsed = std::strtol(found->second.c_str(), &end, 10);
  return end != found->second.c_str() ? static_cast<int>(parsed) : fallback;
}

std::string cssColor(const std::string& value, const std::string& fallback)
{
  const std::string clean = trim(value);
  if (clean.empty()) return fallback;
  if (lower(clean) == "theme") return "#2D9BF0";
  if (clean.size() == 8 && clean[0] == '0' && (clean[1] == 'x' || clean[1] == 'X')) {
    return "#" + clean.substr(2);
  }
  return clean;
}

bool parseBoolField(
  const std::unordered_map<std::string, std::string>& fields,
  const char * name,
  bool fallback)
{
  const auto found = fields.find(name);
  if (found == fields.end()) return fallback;
  const std::string value = lower(trim(found->second));
  if (value == "true" || value == "1" || value == "yes" || value == "on") return true;
  if (value == "false" || value == "0" || value == "no" || value == "off") return false;
  return fallback;
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

std::vector<std::string> splitLines(const std::string& value)
{
  std::vector<std::string> lines;
  std::size_t start = 0;
  while (start <= value.size()) {
    const std::size_t end = value.find('\n', start);
    lines.push_back(value.substr(start, end == std::string::npos ? end : end - start));
    if (end == std::string::npos) break;
    start = end + 1;
  }
  return lines;
}

bool readTextFile(const std::string& path, std::string * output)
{
  std::ifstream file(path, std::ios::binary);
  if (!file) return false;
  std::ostringstream buffer;
  buffer << file.rdbuf();
  *output = buffer.str();
  return true;
}

bool readBinaryFile(const std::string& path, std::vector<std::uint8_t> * output)
{
  std::ifstream file(path, std::ios::binary);
  if (!file) return false;
  file.seekg(0, std::ios::end);
  const std::streamoff size = file.tellg();
  if (size < 0) return false;
  file.seekg(0, std::ios::beg);
  output->resize(static_cast<std::size_t>(size));
  if (size > 0) file.read(reinterpret_cast<char *>(output->data()), size);
  return static_cast<bool>(file) || size == 0;
}

void valueToText(const FAppValue& value, char * buffer, std::size_t size)
{
  if (!value.toText(buffer, size) && size > 0) buffer[0] = '\0';
}

FAppValue parseValue(const std::string& text)
{
  const std::string normalized = lower(trim(text));
  if (normalized == "true") return FAppValue::boolean(true);
  if (normalized == "false") return FAppValue::boolean(false);
  if (normalized == "nil") return FAppValue::nil();
  char * end = nullptr;
  const long integer = std::strtol(text.c_str(), &end, 10);
  if (end != text.c_str() && *end == '\0') return FAppValue::integer(static_cast<std::int32_t>(integer));
  end = nullptr;
  const double floating = std::strtod(text.c_str(), &end);
  if (end != text.c_str() && *end == '\0') return FAppValue::floating(floating);
  return FAppValue::string(text.c_str());
}

class RunnerUi : public FAppUiApi {
 public:
  bool loadLayout(const std::string& path)
  {
    std::string source;
    if (!readTextFile(path, &source)) return false;
    const fosc::UiSymbolsResult parsed = fosc::UiSymbols::parse(source, path);
    if (!parsed.success()) return false;

    std::istringstream input(source);
    std::string line;
    while (std::getline(input, line)) {
      const std::string clean = trim(line);
      if (clean.empty() || clean.front() == '#') continue;
      const auto fields = parseFields(clean);
      const auto idIt = fields.find("id");
      const auto typeIt = fields.find("type");
      if (idIt == fields.end() || typeIt == fields.end()) continue;
      const auto symbol = parsed.symbols.find(idIt->second);
      if (symbol == parsed.symbols.end()) continue;

      UiObjectState object;
      object.id = symbol->second.id;
      object.name = idIt->second;
      object.type = lower(typeIt->second);
      if (const auto parentIt = fields.find("parent"); parentIt != fields.end()) object.parent = parentIt->second;
      if (const auto textIt = fields.find("text"); textIt != fields.end()) object.text = unescape(textIt->second);
      if (const auto placeholderIt = fields.find("placeholder"); placeholderIt != fields.end()) {
        object.placeholder = unescape(placeholderIt->second);
      }
      if (const auto targetIt = fields.find("target"); targetIt != fields.end()) object.target = targetIt->second;
      if (const auto bgIt = fields.find("bg"); bgIt != fields.end()) object.bg = bgIt->second;
      if (const auto fgIt = fields.find("fg"); fgIt != fields.end()) object.fg = fgIt->second;
      if (const auto alignIt = fields.find("align"); alignIt != fields.end()) object.align = lower(alignIt->second);
      object.x = parseIntField(fields, "x", object.x);
      object.y = parseIntField(fields, "y", object.y);
      object.w = parseIntField(fields, "w", object.w);
      object.h = parseIntField(fields, "h", object.h);
      object.font = parseIntField(fields, "font", object.font);
      object.radius = parseIntField(fields, "radius", object.radius);
      object.clickable = parseBoolField(fields, "clickable", object.clickable);
      if (object.type == "roller" || object.type == "dropdown") object.options = splitLines(object.text);
      if (const auto valueIt = fields.find("value"); valueIt != fields.end()) object.value = parseValue(valueIt->second).asInteger();
      object.hidden = parseBoolField(fields, "hidden", object.hidden);
      objectsById_[object.id] = object;
      idsByName_[object.name] = object.id;
    }
    return true;
  }

  bool getProperty(std::uint16_t objectId, std::uint8_t property, FAppValue * result) override
  {
    auto object = objectsById_.find(objectId);
    if (object == objectsById_.end() || result == nullptr) return false;
    switch (static_cast<fosc::fapp::UiProperty>(property)) {
      case fosc::fapp::UiProperty::Text:
        if ((object->second.type == "roller" || object->second.type == "dropdown") &&
            object->second.value >= 0 &&
            static_cast<std::size_t>(object->second.value) < object->second.options.size()) {
          *result = FAppValue::string(object->second.options[object->second.value].c_str());
        } else {
          *result = FAppValue::string(object->second.text.c_str());
        }
        return true;
      case fosc::fapp::UiProperty::Value:
        if (object->second.type == "textarea") *result = FAppValue::string(object->second.text.c_str());
        else *result = FAppValue::integer(object->second.value);
        return true;
      case fosc::fapp::UiProperty::Checked:
        *result = FAppValue::boolean(object->second.checked);
        return true;
      case fosc::fapp::UiProperty::Hidden:
        *result = FAppValue::boolean(object->second.hidden);
        return true;
      case fosc::fapp::UiProperty::Enabled:
        *result = FAppValue::boolean(object->second.enabled);
        return true;
    }
    return false;
  }

  bool setProperty(std::uint16_t objectId, std::uint8_t property, const FAppValue& value) override
  {
    auto object = objectsById_.find(objectId);
    if (object == objectsById_.end()) return false;
    char text[FAppValue::kStringCapacity];
    valueToText(value, text, sizeof(text));
    switch (static_cast<fosc::fapp::UiProperty>(property)) {
      case fosc::fapp::UiProperty::Text:
        object->second.text = text;
        if (object->second.type == "roller" || object->second.type == "dropdown") {
          object->second.options = splitLines(object->second.text);
        }
        return true;
      case fosc::fapp::UiProperty::Value:
        if (object->second.type == "textarea") object->second.text = text;
        else object->second.value = value.asInteger();
        return true;
      case fosc::fapp::UiProperty::Checked:
        object->second.checked = value.truthy();
        return true;
      case fosc::fapp::UiProperty::Hidden:
        object->second.hidden = value.truthy();
        return true;
      case fosc::fapp::UiProperty::Enabled:
        object->second.enabled = value.truthy();
        return true;
    }
    return false;
  }

  bool callMethod(
    std::uint16_t objectId,
    std::uint8_t method,
    const FAppValue * arguments,
    std::uint8_t argumentCount,
    FAppValue * result) override
  {
    auto object = objectsById_.find(objectId);
    if (object == objectsById_.end() || result == nullptr) return false;
    const auto parsedMethod = static_cast<fosc::fapp::UiMethod>(method);
    if (parsedMethod == fosc::fapp::UiMethod::LoadFile ||
        parsedMethod == fosc::fapp::UiMethod::SaveFile) {
      if (argumentCount != 1 || object->second.type != "textarea") return false;
      *result = FAppValue::boolean(true);
      return true;
    }
    if (object->second.type == "canvas") {
      auto number = [&](std::size_t index) { return arguments[index].asInteger(); };
      std::ostringstream command;
      switch (parsedMethod) {
        case fosc::fapp::UiMethod::Clear:
          if (argumentCount != 1) return false;
          object->second.drawCommands.clear();
          command << "{\"op\":\"clear\",\"color\":" << number(0) << '}';
          break;
        case fosc::fapp::UiMethod::Pixel:
          if (argumentCount != 3) return false;
          command << "{\"op\":\"pixel\",\"x\":" << number(0) << ",\"y\":" << number(1)
                  << ",\"color\":" << number(2) << '}';
          break;
        case fosc::fapp::UiMethod::Line:
          if (argumentCount != 6) return false;
          command << "{\"op\":\"line\",\"x1\":" << number(0) << ",\"y1\":" << number(1)
                  << ",\"x2\":" << number(2) << ",\"y2\":" << number(3)
                  << ",\"color\":" << number(4) << ",\"width\":" << number(5) << '}';
          break;
        case fosc::fapp::UiMethod::Rect:
          if (argumentCount != 6) return false;
          command << "{\"op\":\"rect\",\"x\":" << number(0) << ",\"y\":" << number(1)
                  << ",\"w\":" << number(2) << ",\"h\":" << number(3)
                  << ",\"color\":" << number(4) << ",\"filled\":"
                  << (arguments[5].truthy() ? "true" : "false") << '}';
          break;
        case fosc::fapp::UiMethod::Circle:
          if (argumentCount != 5) return false;
          command << "{\"op\":\"circle\",\"x\":" << number(0) << ",\"y\":" << number(1)
                  << ",\"r\":" << number(2) << ",\"color\":" << number(3)
                  << ",\"filled\":" << (arguments[4].truthy() ? "true" : "false") << '}';
          break;
        case fosc::fapp::UiMethod::DrawText:
          if (argumentCount != 5 || arguments[2].type() != FAppValueType::String) return false;
          command << "{\"op\":\"text\",\"x\":" << number(0) << ",\"y\":" << number(1)
                  << ",\"text\":\"" << jsonEscape(arguments[2].asString()) << "\",\"color\":"
                  << number(3) << ",\"size\":" << number(4) << '}';
          break;
        default: return false;
      }
      object->second.drawCommands.push_back(command.str());
      *result = FAppValue::boolean(true);
      return true;
    }
    if (argumentCount != 0) return false;
    switch (parsedMethod) {
      case fosc::fapp::UiMethod::Clear:
        object->second.text.clear();
        object->second.options.clear();
        break;
      case fosc::fapp::UiMethod::Focus:
        clearFocus();
        object->second.focused = true;
        object->second.hidden = false;
        break;
      case fosc::fapp::UiMethod::Blur:
        object->second.focused = false;
        break;
      case fosc::fapp::UiMethod::ScrollToTop:
      case fosc::fapp::UiMethod::ScrollToBottom:
        break;
      case fosc::fapp::UiMethod::LoadFile:
      case fosc::fapp::UiMethod::SaveFile:
      case fosc::fapp::UiMethod::Pixel:
      case fosc::fapp::UiMethod::Line:
      case fosc::fapp::UiMethod::Rect:
      case fosc::fapp::UiMethod::Circle:
      case fosc::fapp::UiMethod::DrawText:
        break;
    }
    *result = FAppValue::nil();
    return true;
  }

  bool setNamedProperty(const std::string& expression)
  {
    const std::size_t equals = expression.find('=');
    const std::size_t dot = expression.find('.');
    if (equals == std::string::npos || dot == std::string::npos || dot > equals) return false;
    const std::string name = expression.substr(0, dot);
    const std::string property = expression.substr(dot + 1, equals - dot - 1);
    const auto id = idsByName_.find(name);
    if (id == idsByName_.end()) return false;
    fosc::fapp::UiProperty parsed;
    if (!fosc::fapp::parseUiProperty(property, &parsed)) return false;
    return setProperty(id->second, static_cast<std::uint8_t>(parsed), parseValue(expression.substr(equals + 1)));
  }

  bool setByName(const std::string& name, const std::string& property, const std::string& value)
  {
    const auto id = idsByName_.find(name);
    if (id == idsByName_.end()) return false;
    fosc::fapp::UiProperty parsed;
    if (!fosc::fapp::parseUiProperty(property, &parsed)) return false;
    return setProperty(id->second, static_cast<std::uint8_t>(parsed), parseValue(value));
  }

  bool appendToText(const std::string& name, const std::string& text)
  {
    const auto id = idsByName_.find(name);
    if (id == idsByName_.end()) return false;
    auto object = objectsById_.find(id->second);
    if (object == objectsById_.end()) return false;
    object->second.text += text;
    return true;
  }

  bool backspaceText(const std::string& name)
  {
    const auto id = idsByName_.find(name);
    if (id == idsByName_.end()) return false;
    auto object = objectsById_.find(id->second);
    if (object == objectsById_.end()) return false;
    if (!object->second.text.empty()) object->second.text.pop_back();
    return true;
  }

  std::string keyboardTarget(const std::string& name) const
  {
    const auto id = idsByName_.find(name);
    if (id == idsByName_.end()) return "";
    const auto object = objectsById_.find(id->second);
    if (object == objectsById_.end()) return "";
    return object->second.target;
  }

  bool resolveEvent(const std::string& expression, std::uint16_t * objectId, std::uint8_t * eventType) const
  {
    const std::size_t dot = expression.find('.');
    if (dot == std::string::npos || objectId == nullptr || eventType == nullptr) return false;
    const std::string name = expression.substr(0, dot);
    const std::string event = expression.substr(dot + 1);
    fosc::fapp::UiEvent parsed;
    if (fosc::fapp::parseSystemEvent(name, event, &parsed)) {
      *objectId = 0;
      *eventType = static_cast<std::uint8_t>(parsed);
      return true;
    }
    const auto id = idsByName_.find(name);
    if (id == idsByName_.end() || !fosc::fapp::parseUiEvent(event, &parsed)) return false;
    *objectId = id->second;
    *eventType = static_cast<std::uint8_t>(parsed);
    return true;
  }

  std::string toJson() const
  {
    std::vector<const UiObjectState *> objects;
    for (const auto& item : objectsById_) objects.push_back(&item.second);
    std::sort(objects.begin(), objects.end(), [](const UiObjectState * left, const UiObjectState * right) {
      return left->id < right->id;
    });

    std::ostringstream output;
    output << "{\"objects\":[";
    for (std::size_t index = 0; index < objects.size(); ++index) {
      const UiObjectState * object = objects[index];
      if (index > 0) output << ',';
      output << "{\"id\":" << object->id
             << ",\"name\":\"" << jsonEscape(object->name) << "\""
             << ",\"type\":\"" << jsonEscape(object->type) << "\""
             << ",\"parent\":\"" << jsonEscape(object->parent) << "\""
             << ",\"text\":\"" << jsonEscape(object->text) << "\""
             << ",\"placeholder\":\"" << jsonEscape(object->placeholder) << "\""
             << ",\"target\":\"" << jsonEscape(object->target) << "\""
             << ",\"bg\":\"" << jsonEscape(cssColor(object->bg, "#28272F")) << "\""
             << ",\"fg\":\"" << jsonEscape(cssColor(object->fg, "#FFFFFF")) << "\""
             << ",\"align\":\"" << jsonEscape(object->align) << "\""
             << ",\"x\":" << object->x
             << ",\"y\":" << object->y
             << ",\"w\":" << object->w
             << ",\"h\":" << object->h
             << ",\"font\":" << object->font
             << ",\"radius\":" << object->radius
             << ",\"value\":" << object->value
             << ",\"checked\":" << (object->checked ? "true" : "false")
             << ",\"hidden\":" << (object->hidden ? "true" : "false")
             << ",\"enabled\":" << (object->enabled ? "true" : "false")
             << ",\"focused\":" << (object->focused ? "true" : "false")
             << ",\"clickable\":" << (object->clickable ? "true" : "false")
             << ",\"options\":[";
      for (std::size_t optionIndex = 0; optionIndex < object->options.size(); ++optionIndex) {
        if (optionIndex > 0) output << ',';
        output << '"' << jsonEscape(object->options[optionIndex]) << '"';
      }
      output << "],\"draw\":[";
      for (std::size_t commandIndex = 0; commandIndex < object->drawCommands.size(); ++commandIndex) {
        if (commandIndex > 0) output << ',';
        output << object->drawCommands[commandIndex];
      }
      output << "]}";
    }
    output << "]}";
    return output.str();
  }

  void dump(std::ostream& output) const
  {
    std::vector<const UiObjectState *> objects;
    for (const auto& item : objectsById_) objects.push_back(&item.second);
    std::sort(objects.begin(), objects.end(), [](const UiObjectState * left, const UiObjectState * right) {
      return left->id < right->id;
    });
    output << "UI state\n";
    for (const UiObjectState * object : objects) {
      output << "  #" << object->id << ' ' << object->name << " (" << object->type << ")"
             << " text=\"" << escape(object->text) << "\""
             << " value=" << object->value
             << " hidden=" << (object->hidden ? "true" : "false")
             << " enabled=" << (object->enabled ? "true" : "false");
      if (object->focused) output << " focused=true";
      output << '\n';
    }
  }

 private:
  void clearFocus()
  {
    for (auto& item : objectsById_) item.second.focused = false;
  }

  std::map<std::uint16_t, UiObjectState> objectsById_;
  std::unordered_map<std::string, std::uint16_t> idsByName_;
};

bool calcOperator(char character)
{
  return character == '+' || character == '-' || character == '*' || character == '/';
}

int calcPrecedence(char character)
{
  if (character == '*' || character == '/') return 2;
  if (character == '+' || character == '-') return 1;
  return 0;
}

bool calcApply(double * values, int * valueTop, char op, bool * divisionByZero)
{
  if (*valueTop < 2) return false;
  const double right = values[--(*valueTop)];
  const double left = values[--(*valueTop)];
  double value = 0.0;
  switch (op) {
    case '+': value = left + right; break;
    case '-': value = left - right; break;
    case '*': value = left * right; break;
    case '/':
      if (std::fabs(right) < 1e-12) {
        *divisionByZero = true;
        return false;
      }
      value = left / right;
      break;
    default:
      return false;
  }
  values[(*valueTop)++] = value;
  return true;
}

bool calcEvaluate(const std::string& expression, double * output, bool * divisionByZero)
{
  if (output == nullptr || divisionByZero == nullptr) return false;
  *divisionByZero = false;
  std::string normalized;
  normalized.reserve(expression.size());
  for (char character : expression) normalized += character == ',' ? '.' : character;

  constexpr int kMaxStack = 32;
  double values[kMaxStack];
  char operators[kMaxStack];
  int valueTop = 0;
  int operatorTop = 0;
  const char * cursor = normalized.c_str();
  bool expectNumber = true;

  while (*cursor != '\0') {
    if (std::isspace(static_cast<unsigned char>(*cursor))) {
      ++cursor;
      continue;
    }
    if (expectNumber) {
      char * end = nullptr;
      const double number = std::strtod(cursor, &end);
      if (end == cursor || valueTop >= kMaxStack) return false;
      values[valueTop++] = number;
      cursor = end;
      expectNumber = false;
      continue;
    }
    const char op = *cursor;
    if (!calcOperator(op)) return false;
    while (operatorTop > 0 && calcPrecedence(operators[operatorTop - 1]) >= calcPrecedence(op)) {
      if (!calcApply(values, &valueTop, operators[--operatorTop], divisionByZero)) return false;
    }
    if (operatorTop >= kMaxStack) return false;
    operators[operatorTop++] = op;
    ++cursor;
    expectNumber = true;
  }
  if (expectNumber) return false;
  while (operatorTop > 0) {
    if (!calcApply(values, &valueTop, operators[--operatorTop], divisionByZero)) return false;
  }
  if (valueTop != 1 || !std::isfinite(values[0])) return false;
  *output = values[0];
  return true;
}

std::string mathEvalText(const std::string& expression)
{
  double value = 0.0;
  bool divisionByZero = false;
  if (!calcEvaluate(expression, &value, &divisionByZero)) return "Math Error";
  char buffer[48];
  std::snprintf(buffer, sizeof(buffer), "%.10g", value);
  for (char * cursor = buffer; *cursor != '\0'; ++cursor) {
    if (*cursor == '.') *cursor = ',';
  }
  return buffer;
}

class RunnerNative : public FAppNativeApi {
 public:
  bool call(FAppNativeFunction function, const FAppValue * arguments, std::uint8_t argumentCount, FAppValue * result) override
  {
    if (result == nullptr) return false;
    char buffer[FAppValue::kStringCapacity];
    const auto textArgument = [&](std::uint8_t index) -> std::string {
      if (arguments == nullptr || index >= argumentCount) return "";
      valueToText(arguments[index], buffer, sizeof(buffer));
      return buffer;
    };
    switch (function) {
      case FAppNativeFunction::HttpGet:
        lastUrl_ = textArgument(0);
        log("http_get", lastUrl_);
        *result = FAppValue::boolean(true);
        return true;
      case FAppNativeFunction::HttpStatus:
        *result = FAppValue::integer(200);
        return true;
      case FAppNativeFunction::HttpJson: {
        const std::string key = textArgument(0);
        const auto value = json_.find(key);
        *result = FAppValue::string((value == json_.end() ? defaultJson(key) : value->second).c_str());
        return true;
      }
      case FAppNativeFunction::HttpText:
        *result = FAppValue::string("{}");
        return true;
      case FAppNativeFunction::UrlEncode:
        *result = FAppValue::string(urlEncode(textArgument(0)).c_str());
        return true;
      case FAppNativeFunction::DateWeekday:
        *result = FAppValue::string(weekday(textArgument(0)).c_str());
        return true;
      case FAppNativeFunction::FileRead:
        *result = FAppValue::string(files_[textArgument(0)].c_str());
        return true;
      case FAppNativeFunction::FileWrite:
        files_[textArgument(0)] = textArgument(1);
        *result = FAppValue::boolean(true);
        return true;
      case FAppNativeFunction::FileList:
        *result = FAppValue::string("Welcome.txt");
        return true;
      case FAppNativeFunction::AudioPlay:
        log("audio_play", textArgument(0));
        *result = FAppValue::boolean(true);
        return true;
      case FAppNativeFunction::WifiStatus:
        *result = FAppValue::boolean(wifiOnline_);
        return true;
      case FAppNativeFunction::SystemRestart:
        restarted_ = true;
        log("system.restart", "");
        *result = FAppValue::nil();
        return true;
      case FAppNativeFunction::TimerStart:
        timerMs_ = arguments == nullptr ? 0 : arguments[0].asInteger();
        *result = FAppValue::boolean(true);
        return true;
      case FAppNativeFunction::SerialPrintf:
        std::cout << "[Serial] " << textArgument(0) << '\n';
        *result = FAppValue::nil();
        return true;
      case FAppNativeFunction::Text:
        *result = FAppValue::string(textArgument(0).c_str());
        return true;
      case FAppNativeFunction::NumberParse:
        *result = FAppValue::floating(std::strtod(textArgument(0).c_str(), nullptr));
        return true;
      case FAppNativeFunction::Round:
        *result = FAppValue::integer(static_cast<std::int32_t>(std::lround(arguments[0].asFloat())));
        return true;
      case FAppNativeFunction::StringLength:
        *result = FAppValue::integer(static_cast<std::int32_t>(textArgument(0).size()));
        return true;
      case FAppNativeFunction::StringSlice: {
        const std::string text = textArgument(0);
        int beginValue = arguments[1].asInteger();
        const int total = static_cast<int>(text.size());
        if (beginValue < 0) beginValue = total + beginValue;
        beginValue = std::max(0, std::min(total, beginValue));
        const int length = std::max(0, std::min(total - beginValue, arguments[2].asInteger()));
        *result = FAppValue::string(text.substr(static_cast<std::size_t>(beginValue), static_cast<std::size_t>(length)).c_str());
        return true;
      }
      case FAppNativeFunction::MathEval:
        *result = FAppValue::string(mathEvalText(textArgument(0)).c_str());
        return true;
      case FAppNativeFunction::StringLastIndex: {
        const std::string text = textArgument(0);
        const std::string needle = textArgument(1);
        const std::size_t found = text.rfind(needle);
        *result = FAppValue::integer(found == std::string::npos ? -1 : static_cast<std::int32_t>(found));
        return true;
      }
      case FAppNativeFunction::EventX:
        *result = FAppValue::integer(eventContext_.pointer ? eventContext_.x : 0);
        return true;
      case FAppNativeFunction::EventY:
        *result = FAppValue::integer(eventContext_.pointer ? eventContext_.y : 0);
        return true;
      case FAppNativeFunction::EventScreenX:
        *result = FAppValue::integer(eventContext_.pointer ? eventContext_.screenX : 0);
        return true;
      case FAppNativeFunction::EventScreenY:
        *result = FAppValue::integer(eventContext_.pointer ? eventContext_.screenY : 0);
        return true;
      case FAppNativeFunction::EventPressed:
        *result = FAppValue::boolean(eventContext_.pointer && eventContext_.pressed);
        return true;
    }
    lastError_ = "unsupported native call";
    return false;
  }

  const char * errorMessage() const override { return lastError_.c_str(); }
  void reset() override {}
  void setEventContext(const FAppEventContext& context) override { eventContext_ = context; }
  void clearEventContext() override { eventContext_ = FAppEventContext{}; }
  void setJson(const std::string& key, const std::string& value) { json_[key] = value; }
  void setWifiOnline(bool online) { wifiOnline_ = online; }
  void setTrace(bool enabled) { trace_ = enabled; }

 private:
  void log(const std::string& name, const std::string& value) const
  {
    if (!trace_) return;
    std::cout << "[native] " << name;
    if (!value.empty()) std::cout << ' ' << value;
    std::cout << '\n';
  }

  static std::string urlEncode(const std::string& input)
  {
    std::ostringstream output;
    output << std::uppercase << std::hex;
    for (unsigned char character : input) {
      if (std::isalnum(character) || character == '-' || character == '_' || character == '.' || character == '~') {
        output << static_cast<char>(character);
      } else if (character == ' ') {
        output << "%20";
      } else {
        output << '%' << std::setw(2) << std::setfill('0') << static_cast<int>(character);
      }
    }
    return output.str();
  }

  static std::string weekday(const std::string& date)
  {
    if (date.size() < 10) return "";
    const int y = std::atoi(date.substr(0, 4).c_str());
    int m = std::atoi(date.substr(5, 2).c_str());
    const int d = std::atoi(date.substr(8, 2).c_str());
    int year = y;
    if (m < 3) { m += 12; --year; }
    const int k = year % 100;
    const int j = year / 100;
    const int h = (d + (13 * (m + 1)) / 5 + k + k / 4 + j / 4 + 5 * j) % 7;
    static const char * names[] = {"Sat", "Sun", "Mon", "Tue", "Wed", "Thu", "Fri"};
    return names[h];
  }

  static std::string defaultJson(const std::string& key)
  {
    static const std::map<std::string, std::string> defaults = {
      {"lat", "52.52"}, {"lon", "13.41"}, {"city", "Berlin"}, {"country", "Germany"},
      {"current.temperature_2m", "21"}, {"current.relative_humidity_2m", "45"},
      {"current.weather_code", "1"},
      {"daily.time[0]", "2026-08-30"}, {"daily.time[1]", "2026-08-31"},
      {"daily.time[2]", "2026-09-01"}, {"daily.time[3]", "2026-09-02"},
      {"daily.temperature_2m_min[0]", "14"}, {"daily.temperature_2m_min[1]", "15"},
      {"daily.temperature_2m_min[2]", "16"}, {"daily.temperature_2m_min[3]", "16"},
      {"daily.temperature_2m_max[0]", "23"}, {"daily.temperature_2m_max[1]", "24"},
      {"daily.temperature_2m_max[2]", "22"}, {"daily.temperature_2m_max[3]", "21"},
      {"daily.weather_code[0]", "1"}, {"daily.weather_code[1]", "2"},
      {"daily.weather_code[2]", "61"}, {"daily.weather_code[3]", "3"},
      {"results[0].name", "Berlin"}, {"results[0].country", "Germany"},
      {"results[0].latitude", "52.52"}, {"results[0].longitude", "13.41"}
    };
    const auto value = defaults.find(key);
    return value == defaults.end() ? "" : value->second;
  }

  std::map<std::string, std::string> json_;
  std::map<std::string, std::string> files_;
  std::string lastUrl_;
  std::string lastError_ = "native API failure";
  bool wifiOnline_ = true;
  bool trace_ = false;
  bool restarted_ = false;
  std::int32_t timerMs_ = 0;
  FAppEventContext eventContext_;
};

void printUsage()
{
  std::cout << "fosrun 0.2.1\n\n"
            << "Usage:\n"
            << "  fosrun <main.fapp> --ui layout.ui [options]\n\n"
            << "Options:\n"
            << "  --serve                   Start a local browser UI at http://127.0.0.1:8765\n"
            << "  --port number             Port for --serve, default 8765\n"
            << "  --open                    Open the browser UI automatically when possible\n"
            << "  --event object.event       Queue an event, e.g. btn_7.click or app.timer\n"
            << "  --set object.property=val  Set a simulated UI property before events\n"
            << "  --json path=value          Override http_json(path) result\n"
            << "  --no-start                 Do not run app.start automatically\n"
            << "  --no-dump                  Do not print final UI state\n"
            << "  --trace-native             Print simulated native API calls\n"
            << "  --wifi-off                 Make wifi_status() return false\n";
}

bool runUntilReady(FAppRuntime * runtime, std::uint16_t budget = 512)
{
  for (int iteration = 0; iteration < 512 && runtime->isActive(); ++iteration) {
    runtime->update(budget, 0);
    if (runtime->state() == FAppRuntimeState::Ready) return true;
  }
  return runtime->state() == FAppRuntimeState::Ready;
}

std::unordered_map<std::string, std::string> parseQuery(const std::string& query)
{
  std::unordered_map<std::string, std::string> fields;
  std::size_t start = 0;
  while (start <= query.size()) {
    const std::size_t end = query.find('&', start);
    const std::string field = query.substr(start, end == std::string::npos ? end : end - start);
    const std::size_t equals = field.find('=');
    if (equals != std::string::npos) {
      fields[urlDecode(field.substr(0, equals))] = urlDecode(field.substr(equals + 1));
    }
    if (end == std::string::npos) break;
    start = end + 1;
  }
  return fields;
}

void sendHttp(int client, const std::string& status, const std::string& contentType, const std::string& body)
{
  std::ostringstream header;
  header << "HTTP/1.1 " << status << "\r\n"
         << "Content-Type: " << contentType << "\r\n"
         << "Content-Length: " << body.size() << "\r\n"
         << "Cache-Control: no-store\r\n"
         << "Connection: close\r\n\r\n";
  const std::string headerText = header.str();
  ::send(client, headerText.data(), headerText.size(), 0);
  ::send(client, body.data(), body.size(), 0);
}

bool queueNamedEvent(
  FAppRuntime * runtime,
  const RunnerUi& ui,
  const std::string& event,
  std::string * error,
  bool allowMissingHandler,
  bool pointer = false,
  int x = 0,
  int y = 0,
  int screenX = 0,
  int screenY = 0,
  bool pressed = false)
{
  std::uint16_t objectId = 0;
  std::uint8_t eventType = 0;
  if (!ui.resolveEvent(event, &objectId, &eventType)) {
    if (error != nullptr) *error = "Unknown event target '" + event + "'.";
    return false;
  }
  if (!runtime->hasEventHandler(objectId, eventType)) {
    if (allowMissingHandler) return true;
    if (error != nullptr) *error = "No handler for event '" + event + "'.";
    return false;
  }
  const bool queued = pointer
    ? runtime->queuePointerEvent(objectId, eventType, x, y, screenX, screenY, pressed)
    : runtime->queueEvent(objectId, eventType);
  if (!queued) {
    if (error != nullptr) *error = "Event queue is full for '" + event + "'.";
    return false;
  }
  if (!runUntilReady(runtime)) {
    if (error != nullptr) *error = runtime->errorMessage();
    return false;
  }
  return true;
}

std::string renderHtml()
{
  return R"HTML(<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>fosrun UI</title>
  <style>
    :root { color-scheme: dark; font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", sans-serif; }
    body { margin: 0; background: #11161c; color: #f7f7f7; }
    header { height: 44px; display: flex; align-items: center; gap: 14px; padding: 0 14px; background: #171d24; border-bottom: 1px solid #2c3440; }
    header strong { font-size: 14px; }
    header span { color: #aeb8c3; font-size: 13px; }
    main { padding: 16px; }
    #viewport { width: min(100%, var(--screen-width, 800px)); aspect-ratio: var(--screen-ratio, 1.6667); overflow: auto; border: 1px solid #303945; background: #000; box-shadow: 0 10px 32px rgba(0,0,0,.35); }
    #screen { position: relative; width: var(--screen-width, 800px); height: var(--screen-height, 480px); transform-origin: top left; background: #0d1217; }
    .obj { position: absolute; box-sizing: border-box; overflow: hidden; font-family: inherit; }
    .panel { background: #28272f; }
    .label { display: flex; align-items: center; white-space: pre-wrap; background: transparent; pointer-events: none; line-height: 1.15; }
    .label.center { justify-content: center; text-align: center; }
    button.obj { appearance: none; border: 0; cursor: pointer; color: #fff; background: #2d9bf0; display: flex; align-items: center; justify-content: center; line-height: 1; padding: 0; }
    button.obj:active { filter: brightness(0.86); }
    textarea.obj, .display-field, select.obj { appearance: none; border: 0; outline: none; padding: 10px 14px; resize: none; line-height: 1.2; }
    .display-field { display: flex; align-items: center; white-space: pre; }
    select.obj { padding: 0 12px; text-align: center; }
    select.obj option { background: #28272f; color: #fff; }
    input.switch { width: 28px; height: 28px; }
    .keyboard { display: grid; gap: 8px; padding: 8px; background: #141b20; }
    .keyrow { display: flex; gap: 8px; }
    .key { flex: 1; min-width: 0; border: 0; border-radius: 8px; background: #28272f; color: #f5f5f5; font: inherit; cursor: pointer; }
    .key.special { background: #253032; }
    #status { margin-top: 10px; font: 13px ui-monospace, SFMono-Regular, Menlo, monospace; color: #9fb0c0; min-height: 18px; }
  </style>
</head>
<body>
  <header><strong>fosrun UI</strong><span id="summary">loading...</span></header>
  <main>
    <div id="viewport"><div id="screen"></div></div>
    <div id="status"></div>
  </main>
  <script>
    let model = {objects: []};

    function enc(value) { return encodeURIComponent(value); }
    function byName(name) { return model.objects.find(o => o.name === name); }
    function parentHidden(object) {
      let current = object;
      while (current && current.parent) {
        current = byName(current.parent);
        if (current && current.hidden) return true;
      }
      return false;
    }
    function styleBox(el, object) {
      Object.assign(el.style, {
        left: object.x + 'px', top: object.y + 'px', width: object.w + 'px', height: object.h + 'px',
        fontSize: object.font + 'px', borderRadius: object.radius + 'px',
        color: object.fg || '#fff', background: object.bg || 'transparent',
        display: (object.hidden || parentHidden(object)) ? 'none' : ''
      });
      el.classList.add('obj', object.type);
      el.dataset.name = object.name;
      if (!object.enabled) el.disabled = true;
    }
    async function loadState() {
      const response = await fetch('/state');
      model = await response.json();
      render();
    }
    async function sendEvent(name, allowError = true) {
      const response = await fetch('/event?name=' + enc(name));
      const result = await response.json();
      if (!result.ok && allowError) document.getElementById('status').textContent = result.error;
      model = result.state;
      render();
    }
    async function sendPointer(object, event, x, y, pressed) {
      const query = '/event?name=' + enc(object.name + '.' + event) + '&x=' + x + '&y=' + y +
        '&screen_x=' + (object.x + x) + '&screen_y=' + (object.y + y) + '&pressed=' + (pressed ? '1' : '0');
      const response = await fetch(query);
      const result = await response.json();
      if (!result.ok) document.getElementById('status').textContent = result.error;
      model = result.state;
      const updated = model.objects.find(item => item.name === object.name);
      const current = document.querySelector('canvas[data-name="' + object.name + '"]');
      if (updated && current) paintCanvas(current, updated);
    }
    function color(value) { return '#' + Number(value >>> 0).toString(16).padStart(6, '0').slice(-6); }
    function paintCanvas(el, object) {
      const ctx = el.getContext('2d');
      ctx.clearRect(0, 0, el.width, el.height);
      for (const command of object.draw || []) {
        ctx.fillStyle = color(command.color);
        ctx.strokeStyle = color(command.color);
        if (command.op === 'clear') ctx.fillRect(0, 0, object.w, object.h);
        else if (command.op === 'pixel') ctx.fillRect(command.x, command.y, 1, 1);
        else if (command.op === 'line') {
          ctx.lineWidth = command.width; ctx.lineCap = 'round'; ctx.beginPath();
          ctx.moveTo(command.x1, command.y1); ctx.lineTo(command.x2, command.y2); ctx.stroke();
        } else if (command.op === 'rect') {
          if (command.filled) ctx.fillRect(command.x, command.y, command.w, command.h);
          else ctx.strokeRect(command.x, command.y, command.w, command.h);
        } else if (command.op === 'circle') {
          ctx.beginPath(); ctx.arc(command.x, command.y, command.r, 0, Math.PI * 2);
          command.filled ? ctx.fill() : ctx.stroke();
        } else if (command.op === 'text') {
          ctx.font = command.size + 'px sans-serif'; ctx.textBaseline = 'top'; ctx.fillText(command.text, command.x, command.y);
        }
      }
    }
    function createCanvas(object) {
      const el = document.createElement('canvas');
      el.width = object.w;
      el.height = object.h;
      styleBox(el, object);
      paintCanvas(el, object);
      let down = false;
      const point = event => {
        const bounds = el.getBoundingClientRect();
        return [Math.round((event.clientX - bounds.left) * object.w / bounds.width),
          Math.round((event.clientY - bounds.top) * object.h / bounds.height)];
      };
      el.onpointerdown = event => { down = true; el.setPointerCapture(event.pointerId); const p = point(event); sendPointer(object, 'pointer_down', p[0], p[1], true); };
      el.onpointermove = event => { if (!down) return; const p = point(event); sendPointer(object, 'pointer_move', p[0], p[1], true); };
      el.onpointerup = event => { down = false; const p = point(event); sendPointer(object, 'pointer_up', p[0], p[1], false); };
      return el;
    }
    async function sendSet(name, property, value) {
      const response = await fetch('/set?name=' + enc(name) + '&property=' + enc(property) + '&value=' + enc(value));
      const result = await response.json();
      if (!result.ok) document.getElementById('status').textContent = result.error;
      model = result.state;
      render();
    }
    async function keyboard(name, key) {
      const response = await fetch('/keyboard?name=' + enc(name) + '&key=' + enc(key));
      const result = await response.json();
      if (!result.ok) document.getElementById('status').textContent = result.error;
      model = result.state;
      render();
    }
    function createKeyboard(object) {
      const el = document.createElement('div');
      styleBox(el, object);
      const rows = [['q','w','e','r','t','y','u','i','o','p'], ['a','s','d','f','g','h','j','k','l'], ['z','x','c','v','b','n','m'], ['Space','Backspace','Enter']];
      for (const row of rows) {
        const rowEl = document.createElement('div');
        rowEl.className = 'keyrow';
        for (const key of row) {
          const btn = document.createElement('button');
          btn.className = 'key' + (key.length > 1 ? ' special' : '');
          btn.textContent = key === 'Space' ? 'space' : key;
          btn.onclick = () => keyboard(object.name, key);
          rowEl.appendChild(btn);
        }
        el.appendChild(rowEl);
      }
      return el;
    }
    function createObject(object) {
      let el;
      if (object.type === 'canvas') {
        return createCanvas(object);
      } else if (object.type === 'button') {
        el = document.createElement('button');
        el.textContent = object.text;
        el.onclick = () => sendEvent(object.name + '.click');
      } else if (object.type === 'textarea') {
        if (object.clickable === false) {
          el = document.createElement('div');
          el.className = 'display-field';
          el.textContent = object.text;
        } else {
          el = document.createElement('textarea');
          el.value = object.text;
          el.placeholder = object.placeholder || '';
          el.onchange = async () => { await sendSet(object.name, 'text', el.value); await sendEvent(object.name + '.value_changed', false); };
        }
      } else if (object.type === 'dropdown' || object.type === 'roller') {
        el = document.createElement('select');
        if (object.type === 'roller') el.size = Math.max(2, Math.min(6, object.options.length || 2));
        for (const [index, text] of object.options.entries()) {
          const option = document.createElement('option');
          option.value = index;
          option.textContent = text;
          el.appendChild(option);
        }
        el.value = String(object.value);
        el.onchange = async () => { await sendSet(object.name, 'value', el.value); await sendEvent(object.name + '.changed', false); };
      } else if (object.type === 'checkbox' || object.type === 'switch') {
        el = document.createElement('input');
        el.type = 'checkbox';
        el.checked = object.checked;
        el.className = 'switch';
        el.onchange = async () => { await sendSet(object.name, 'checked', el.checked ? 'true' : 'false'); await sendEvent(object.name + '.changed', false); };
      } else if (object.type === 'keyboard') {
        return createKeyboard(object);
      } else if (object.type === 'panel') {
        el = document.createElement('div');
      } else {
        el = document.createElement('div');
        el.textContent = object.text;
      }
      styleBox(el, object);
      if (object.type === 'label') {
        el.textContent = object.text;
        if (object.align === 'center' || object.w > 180) el.classList.add('center');
      }
      return el;
    }
    function render() {
      const screen = document.getElementById('screen');
      screen.textContent = '';
      const maxX = Math.max(800, ...model.objects.map(o => o.x + o.w));
      const maxY = Math.max(480, ...model.objects.map(o => o.y + o.h));
      document.documentElement.style.setProperty('--screen-width', maxX + 'px');
      document.documentElement.style.setProperty('--screen-height', maxY + 'px');
      document.documentElement.style.setProperty('--screen-ratio', String(maxX / maxY));
      const nodes = new Map();
      for (const object of model.objects) nodes.set(object.name, createObject(object));
      for (const object of model.objects) {
        const node = nodes.get(object.name);
        const parent = object.parent ? nodes.get(object.parent) : null;
        (parent || screen).appendChild(node);
      }
      document.getElementById('summary').textContent = model.objects.length + ' UI objects loaded';
    }
    loadState();
  </script>
</body>
</html>)HTML";
}

std::string stateResponse(const RunnerUi& ui, bool ok, const std::string& error = "")
{
  std::ostringstream output;
  output << "{\"ok\":" << (ok ? "true" : "false")
         << ",\"error\":\"" << jsonEscape(error) << "\""
         << ",\"state\":" << ui.toJson() << "}";
  return output.str();
}

int serveUi(FAppRuntime * runtime, RunnerUi * ui, int port, bool openBrowser)
{
  const int server = ::socket(AF_INET, SOCK_STREAM, 0);
  if (server < 0) {
    std::cerr << "Error FS710: Cannot create UI server socket: " << std::strerror(errno) << '\n';
    return 1;
  }
  int reuse = 1;
  setsockopt(server, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

  sockaddr_in address {};
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  address.sin_port = htons(static_cast<std::uint16_t>(port));
  if (::bind(server, reinterpret_cast<sockaddr *>(&address), sizeof(address)) < 0 ||
      ::listen(server, 8) < 0) {
    std::cerr << "Error FS711: Cannot start UI server on 127.0.0.1:" << port
              << ": " << std::strerror(errno) << '\n';
    ::close(server);
    return 1;
  }

  const std::string url = "http://127.0.0.1:" + std::to_string(port) + "/";
  std::cout << "fosrun UI server running at " << url << '\n'
            << "Press Ctrl-C to stop.\n";
  if (openBrowser) {
#if defined(__APPLE__)
    const int openStatus = std::system(("open \"" + url + "\" >/dev/null 2>&1 &").c_str());
    (void)openStatus;
#elif defined(__linux__)
    const int openStatus = std::system(("xdg-open \"" + url + "\" >/dev/null 2>&1 &").c_str());
    (void)openStatus;
#endif
  }

  while (true) {
    const int client = ::accept(server, nullptr, nullptr);
    if (client < 0) continue;

    char buffer[8192];
    const ssize_t count = ::recv(client, buffer, sizeof(buffer) - 1, 0);
    if (count <= 0) {
      ::close(client);
      continue;
    }
    buffer[count] = '\0';
    std::istringstream request(buffer);
    std::string method;
    std::string target;
    std::string version;
    request >> method >> target >> version;

    const std::size_t question = target.find('?');
    const std::string path = target.substr(0, question);
    const auto query = parseQuery(question == std::string::npos ? "" : target.substr(question + 1));

    if (method != "GET") {
      sendHttp(client, "405 Method Not Allowed", "text/plain; charset=utf-8", "Only GET is supported.");
    } else if (path == "/") {
      sendHttp(client, "200 OK", "text/html; charset=utf-8", renderHtml());
    } else if (path == "/state") {
      sendHttp(client, "200 OK", "application/json; charset=utf-8", ui->toJson());
    } else if (path == "/set") {
      const std::string name = query.count("name") ? query.at("name") : "";
      const std::string property = query.count("property") ? query.at("property") : "";
      const std::string value = query.count("value") ? query.at("value") : "";
      std::string error;
      const bool ok = ui->setByName(name, property, value);
      if (!ok) error = "Cannot set " + name + "." + property + ".";
      sendHttp(client, "200 OK", "application/json; charset=utf-8", stateResponse(*ui, ok, error));
    } else if (path == "/event") {
      const std::string name = query.count("name") ? query.at("name") : "";
      const bool pointer = query.count("x") && query.count("y");
      const int x = pointer ? std::atoi(query.at("x").c_str()) : 0;
      const int y = pointer ? std::atoi(query.at("y").c_str()) : 0;
      const int screenX = query.count("screen_x") ? std::atoi(query.at("screen_x").c_str()) : x;
      const int screenY = query.count("screen_y") ? std::atoi(query.at("screen_y").c_str()) : y;
      const bool pressed = query.count("pressed") && query.at("pressed") == "1";
      std::string error;
      const bool ok = queueNamedEvent(runtime, *ui, name, &error, true,
        pointer, x, y, screenX, screenY, pressed);
      sendHttp(client, "200 OK", "application/json; charset=utf-8", stateResponse(*ui, ok, error));
    } else if (path == "/keyboard") {
      const std::string name = query.count("name") ? query.at("name") : "";
      const std::string key = query.count("key") ? query.at("key") : "";
      const std::string targetName = ui->keyboardTarget(name);
      std::string error;
      bool ok = !targetName.empty();
      if (!ok) {
        error = "Keyboard '" + name + "' has no target.";
      } else if (key == "Enter") {
        ok = queueNamedEvent(runtime, *ui, name + ".ready", &error, true);
      } else if (key == "Backspace") {
        ok = ui->backspaceText(targetName);
      } else if (key == "Space") {
        ok = ui->appendToText(targetName, " ");
      } else {
        ok = ui->appendToText(targetName, key);
      }
      sendHttp(client, "200 OK", "application/json; charset=utf-8", stateResponse(*ui, ok, error));
    } else {
      sendHttp(client, "404 Not Found", "text/plain; charset=utf-8", "Not found.");
    }
    ::close(client);
  }
}

}  // namespace

int main(int argc, char ** argv)
{
  if (argc == 1 || (argc == 2 && std::string(argv[1]) == "--help")) {
    printUsage();
    return argc == 1 ? 1 : 0;
  }

  const std::string fappPath = argv[1];
  std::string uiPath;
  std::vector<std::string> events;
  std::vector<std::string> assignments;
  std::vector<std::string> jsonAssignments;
  bool start = true;
  bool dump = true;
  bool traceNative = false;
  bool wifiOnline = true;
  bool serve = false;
  bool openBrowser = false;
  int port = 8765;

  for (int index = 2; index < argc; ++index) {
    const std::string argument = argv[index];
    if (argument == "--ui" && index + 1 < argc) uiPath = argv[++index];
    else if (argument == "--serve") serve = true;
    else if (argument == "--port" && index + 1 < argc) port = std::atoi(argv[++index]);
    else if (argument == "--open") openBrowser = true;
    else if (argument == "--event" && index + 1 < argc) events.push_back(argv[++index]);
    else if (argument == "--set" && index + 1 < argc) assignments.push_back(argv[++index]);
    else if (argument == "--json" && index + 1 < argc) jsonAssignments.push_back(argv[++index]);
    else if (argument == "--no-start") start = false;
    else if (argument == "--no-dump") dump = false;
    else if (argument == "--trace-native") traceNative = true;
    else if (argument == "--wifi-off") wifiOnline = false;
    else {
      std::cerr << "Error FS700: Unknown or incomplete fosrun option '" << argument << "'.\n";
      return 2;
    }
  }
  if (uiPath.empty()) {
    std::cerr << "Error FS700: fosrun needs --ui layout.ui for named event and UI state simulation.\n";
    return 2;
  }
  if (port < 1024 || port > 65535) {
    std::cerr << "Error FS700: --port must be between 1024 and 65535.\n";
    return 2;
  }

  std::vector<std::uint8_t> image;
  std::string uiSource;
  if (!readBinaryFile(fappPath, &image)) {
    std::cerr << "Error FS000: Cannot open fAPP file.\nFile: " << fappPath << '\n';
    return 1;
  }
  if (!readTextFile(uiPath, &uiSource)) {
    std::cerr << "Error FS000: Cannot open UI layout file.\nFile: " << uiPath << '\n';
    return 1;
  }

  RunnerUi ui;
  if (!ui.loadLayout(uiPath)) {
    std::cerr << "Error FS701: Cannot parse UI symbols.\nFile: " << uiPath << '\n';
    return 1;
  }
  for (const std::string& assignment : assignments) {
    if (!ui.setNamedProperty(assignment)) {
      std::cerr << "Error FS702: Cannot apply UI assignment '" << assignment << "'.\n";
      return 1;
    }
  }

  fs::FS filesystem;
  filesystem.addFile("/apps/run/layout.ui", std::vector<std::uint8_t>(uiSource.begin(), uiSource.end()));
  filesystem.addFile("/apps/run/main.fapp", std::move(image));
  FAppLoader loader;
  loader.begin(filesystem);
  const FAppLoaderStatus loaderStatus = loader.prepare("/apps/run", "layout.ui", "main.fapp");
  if (loaderStatus != FAppLoaderStatus::ExecutableReady) {
    std::cerr << "Error FS703: Loader rejected fAPP with status "
              << static_cast<unsigned int>(loaderStatus) << ".\n";
    return 1;
  }

  RunnerNative native;
  native.setTrace(traceNative);
  native.setWifiOnline(wifiOnline);
  for (const std::string& assignment : jsonAssignments) {
    const std::size_t equals = assignment.find('=');
    if (equals == std::string::npos) {
      std::cerr << "Error FS704: Expected --json path=value.\n";
      return 2;
    }
    native.setJson(assignment.substr(0, equals), assignment.substr(equals + 1));
  }

  const FAppPermissions permissions =
    static_cast<FAppPermissions>(FAppPermission::Ui) |
    static_cast<FAppPermissions>(FAppPermission::Network) |
    static_cast<FAppPermissions>(FAppPermission::StorageRead) |
    static_cast<FAppPermissions>(FAppPermission::StorageWrite) |
    static_cast<FAppPermissions>(FAppPermission::Audio) |
    static_cast<FAppPermissions>(FAppPermission::SystemRestart);

  FAppRuntime runtime;
  if (!runtime.begin(filesystem, loader, ui, permissions, &native)) {
    std::cerr << "Error FS705: Runtime begin failed: " << runtime.errorMessage() << '\n';
    return 1;
  }
  if (start) {
    if (!runtime.start() || !runUntilReady(&runtime)) {
      std::cerr << "Error FS706: app.start failed: " << runtime.errorMessage() << '\n';
      return 1;
    }
  }
  for (const std::string& event : events) {
    std::string error;
    if (!queueNamedEvent(&runtime, ui, event, &error, false)) {
      std::cerr << "Error FS709: Event '" << event << "' failed: " << error << '\n';
      return 1;
    }
  }

  if (serve) return serveUi(&runtime, &ui, port, openBrowser);

  std::cout << "fosrun completed\n"
            << "Instructions: " << runtime.executedInstructions() << '\n';
  if (dump) ui.dump(std::cout);
  return 0;
}
