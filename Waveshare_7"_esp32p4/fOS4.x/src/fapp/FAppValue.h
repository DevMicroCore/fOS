#pragma once

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

enum class FAppValueType : uint8_t {
  Nil = 0,
  Boolean,
  Integer,
  Float,
  String
};

class FAppValue {
 public:
  static constexpr size_t kStringCapacity = 256;

  FAppValue() : type_(FAppValueType::Nil), integer_(0) {}

  static FAppValue nil() { return FAppValue(); }
  static FAppValue boolean(bool value) { FAppValue result; result.type_ = FAppValueType::Boolean; result.integer_ = value ? 1 : 0; return result; }
  static FAppValue integer(int32_t value) { FAppValue result; result.type_ = FAppValueType::Integer; result.integer_ = value; return result; }
  static FAppValue floating(double value) { FAppValue result; result.type_ = FAppValueType::Float; result.floating_ = value; return result; }
  static FAppValue string(const char * value) {
    FAppValue result;
    result.type_ = FAppValueType::String;
    snprintf(result.string_, sizeof(result.string_), "%s", value == nullptr ? "" : value);
    return result;
  }

  FAppValueType type() const { return type_; }
  bool truthy() const { return type_ != FAppValueType::Nil && !(type_ == FAppValueType::Boolean && integer_ == 0); }
  bool isNumber() const { return type_ == FAppValueType::Integer || type_ == FAppValueType::Float; }
  int32_t asInteger() const { return type_ == FAppValueType::Float ? static_cast<int32_t>(floating_) : integer_; }
  double asFloat() const { return type_ == FAppValueType::Integer ? static_cast<double>(integer_) : floating_; }
  bool asBoolean() const { return truthy(); }
  const char * asString() const { return type_ == FAppValueType::String ? string_ : ""; }

  bool toText(char * destination, size_t capacity) const {
    if (destination == nullptr || capacity == 0) return false;
    switch (type_) {
      case FAppValueType::Nil: snprintf(destination, capacity, "nil"); break;
      case FAppValueType::Boolean: snprintf(destination, capacity, "%s", integer_ ? "true" : "false"); break;
      case FAppValueType::Integer: snprintf(destination, capacity, "%ld", static_cast<long>(integer_)); break;
      case FAppValueType::Float: snprintf(destination, capacity, "%.8g", floating_); break;
      case FAppValueType::String: snprintf(destination, capacity, "%s", string_); break;
    }
    return true;
  }

  static bool equals(const FAppValue& left, const FAppValue& right) {
    if (left.isNumber() && right.isNumber()) return left.asFloat() == right.asFloat();
    if (left.type_ != right.type_) return false;
    if (left.type_ == FAppValueType::Nil) return true;
    if (left.type_ == FAppValueType::String) return strcmp(left.string_, right.string_) == 0;
    if (left.type_ == FAppValueType::Float) return left.floating_ == right.floating_;
    return left.integer_ == right.integer_;
  }

 private:
  FAppValueType type_;
  // A value can only hold one payload type at a time. Keeping all three
  // payloads next to each other wasted eight bytes for every VM value.
  union {
    int32_t integer_;
    double floating_;
    char string_[kStringCapacity];
  };
};

static_assert(
  sizeof(FAppValue) <= FAppValue::kStringCapacity + sizeof(double),
  "FAppValue payload must remain compact");
