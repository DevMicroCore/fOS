#pragma once
#include <FS.h>
#include <ctype.h>
#include <string>

// Bounded, streaming reader for name<separator>value configuration files.
// Both UI and native access use the same row validation and indexing.
namespace FAppFilePairs {
constexpr size_t kMaximumRows = 40;
constexpr size_t kMaximumBytes = 16384;
inline std::string trim(const std::string& input) {
  size_t first = 0, last = input.size();
  while (first < last && isspace(static_cast<unsigned char>(input[first]))) ++first;
  while (last > first && isspace(static_cast<unsigned char>(input[last - 1]))) --last;
  return input.substr(first, last - first);
}
inline bool append(const std::string& line, char separator, size_t * rows,
  std::string * names, int32_t requested, std::string * selected) {
  const size_t split = line.find(separator);
  if (split == std::string::npos) return false;
  const std::string name = trim(line.substr(0, split));
  const std::string value = trim(line.substr(split + 1));
  if (name.empty() || value.empty() || name.size() > 255 || value.size() > 255) return false;
  if (names != nullptr) { if (!names->empty()) *names += '\n'; *names += name; }
  if (selected != nullptr && static_cast<int32_t>(*rows) == requested) *selected = value;
  ++*rows;
  return true;
}
inline bool read(fs::FS& filesystem, const char * path, char separator,
  std::string * names, int32_t requested, std::string * selected) {
  if (names != nullptr) names->clear();
  if (selected != nullptr) selected->clear();
  if (separator == '\0' || separator == '\n' || separator == '\r') return false;
  fs::File file = filesystem.open(path, FILE_READ);
  if (!file || file.isDirectory() || file.size() > kMaximumBytes) { if (file) file.close(); return false; }
  size_t rows = 0;
  std::string line;
  bool overflow = false;
  const size_t bytes = file.size();
  for (size_t i = 0; i < bytes && rows < kMaximumRows; ++i) {
    uint8_t c;
    if (file.read(&c, 1) != 1) { file.close(); return false; }
    if (c == '\n') {
      if (!overflow) append(line, separator, &rows, names, requested, selected);
      line.clear(); overflow = false;
      if (selected != nullptr && !selected->empty()) break;
    } else if (line.size() < 1024) line += static_cast<char>(c);
    else overflow = true;
  }
  if (rows < kMaximumRows && !line.empty() && !overflow && (selected == nullptr || selected->empty()))
    append(line, separator, &rows, names, requested, selected);
  file.close();
  return rows > 0;
}
inline bool matchesExtension(const std::string& name, const char * filter) {
  std::string lower = name;
  for (char& c : lower) c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
  std::string extensions(filter == nullptr ? "" : filter);
  for (char& c : extensions) c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
  size_t first = 0;
  while (first <= extensions.size()) {
    const size_t end = extensions.find(',', first);
    std::string extension = trim(extensions.substr(first, end == std::string::npos ? end : end - first));
    if (!extension.empty() && extension.front() != '.') extension = "." + extension;
    if (!extension.empty() && lower.size() >= extension.size() && lower.compare(lower.size() - extension.size(), extension.size(), extension) == 0) return true;
    if (end == std::string::npos) break;
    first = end + 1;
  }
  return false;
}
}
