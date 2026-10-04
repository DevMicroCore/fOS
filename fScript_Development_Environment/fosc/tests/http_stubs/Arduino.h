#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <cstdio>

class String {
 public:
  String() = default;
  String(const char * value) : value_(value == nullptr ? "" : value) {}
  String(const std::string& value) : value_(value) {}
  void reserve(std::size_t size) { value_.reserve(size); }
  std::size_t length() const { return value_.size(); }
  const char * c_str() const { return value_.c_str(); }
  char operator[](std::size_t index) const { return value_[index]; }
  void trim() {
    const auto begin = value_.find_first_not_of(" \t\r\n");
    if (begin == std::string::npos) { value_.clear(); return; }
    const auto end = value_.find_last_not_of(" \t\r\n");
    value_ = value_.substr(begin, end - begin + 1);
  }
  bool startsWith(const char * prefix) const {
    const std::string text = prefix == nullptr ? "" : prefix;
    return value_.rfind(text, 0) == 0;
  }
  int lastIndexOf(char needle) const {
    const auto found = value_.rfind(needle);
    return found == std::string::npos ? -1 : static_cast<int>(found);
  }
  String substring(std::size_t start) const {
    return start >= value_.size() ? String("") : String(value_.substr(start));
  }
  void remove(std::size_t start) { if (start < value_.size()) value_.erase(start); }
  String& operator=(const char * value) { value_ = value == nullptr ? "" : value; return *this; }
  String& operator=(const String&) = default;
  String& operator+=(const char * value) { value_ += value == nullptr ? "" : value; return *this; }
  String& operator+=(const String& value) { value_ += value.value_; return *this; }
  friend String operator+(const String& left, const char * right) {
    return String(left.value_ + (right == nullptr ? "" : right));
  }
  friend String operator+(const String& left, const String& right) {
    return String(left.value_ + right.value_);
  }

 private:
  std::string value_;
};

class SerialClass {
 public:
  int printf(const char * format, const char * value)
  {
    last = value == nullptr ? "" : value;
    return std::snprintf(nullptr, 0, format, value);
  }
  std::string last;
};
inline SerialClass Serial;
