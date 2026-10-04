#include "FAppManifest.h"

#include "../core/FOSVersion.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace {

const char * fieldValue(const char * json, const char * key)
{
  char pattern[80];
  const int length = snprintf(pattern, sizeof(pattern), "\"%s\"", key);
  if (length <= 0 || static_cast<size_t>(length) >= sizeof(pattern)) return nullptr;
  const char * cursor = strstr(json, pattern);
  if (cursor == nullptr) return nullptr;
  cursor += length;
  while (*cursor != '\0' && isspace(static_cast<unsigned char>(*cursor))) ++cursor;
  if (*cursor++ != ':') return nullptr;
  while (*cursor != '\0' && isspace(static_cast<unsigned char>(*cursor))) ++cursor;
  return cursor;
}

bool validId(const char * value)
{
  if (value == nullptr || *value == '\0') return false;
  for (const char * cursor = value; *cursor != '\0'; ++cursor) {
    if (!isalnum(static_cast<unsigned char>(*cursor)) && *cursor != '.' && *cursor != '-' && *cursor != '_') return false;
  }
  return true;
}

}  // namespace

FAppManifest::FAppManifest() { reset(); }

void FAppManifest::reset()
{
  status_ = FAppManifestStatus::NotFound;
  id_[0] = name_[0] = version_[0] = minimumFos_[0] = icon_[0] = '\0';
  snprintf(type_, sizeof(type_), "ui");
  snprintf(layout_, sizeof(layout_), "layout.ui");
  snprintf(executable_, sizeof(executable_), "main.fapp");
  scrollable_ = false;
  permissions_ = static_cast<FAppPermissions>(FAppPermission::Ui);
}

FAppManifestStatus FAppManifest::load(fs::FS& filesystem, const char * path)
{
  reset();
  if (path == nullptr || !filesystem.exists(path)) return status_;
  fs::File file = filesystem.open(path, FILE_READ);
  if (!file) { status_ = FAppManifestStatus::OpenFailed; return status_; }
  const size_t size = file.size();
  if (size == 0 || size > kMaximumSize) {
    file.close(); status_ = size > kMaximumSize ? FAppManifestStatus::TooLarge : FAppManifestStatus::InvalidJson; return status_;
  }
  char json[kMaximumSize + 1];
  if (file.read(reinterpret_cast<uint8_t *>(json), size) != size) {
    file.close(); status_ = FAppManifestStatus::OpenFailed; return status_;
  }
  file.close();
  json[size] = '\0';
  const char * first = json;
  while (*first != '\0' && isspace(static_cast<unsigned char>(*first))) ++first;
  if (*first != '{' || strrchr(first, '}') == nullptr) { status_ = FAppManifestStatus::InvalidJson; return status_; }
  if (!copyStringField(json, "id", id_, sizeof(id_), true) ||
      !copyStringField(json, "name", name_, sizeof(name_), true) ||
      !copyStringField(json, "version", version_, sizeof(version_), true)) {
    status_ = FAppManifestStatus::MissingField; return status_;
  }
  if (!copyStringField(json, "min_fos", minimumFos_, sizeof(minimumFos_), false) ||
      !copyStringField(json, "type", type_, sizeof(type_), false) ||
      !copyStringField(json, "icon", icon_, sizeof(icon_), false) ||
      !copyStringField(json, "layout", layout_, sizeof(layout_), false) ||
      !copyStringField(json, "executable", executable_, sizeof(executable_), false)) {
    status_ = FAppManifestStatus::InvalidJson; return status_;
  }
  scrollable_ = readBoolField(json, "scrollable", false);
  uint16_t major = 0, minor = 0, patch = 0;
  if (!validId(id_) || !parseSemVer(version_, &major, &minor, &patch) ||
      !isSafeFilename(layout_) || !isSafeFilename(executable_) || !readPermissions(json)) {
    status_ = FAppManifestStatus::InvalidValue; return status_;
  }
  if (minimumFos_[0] != '\0') {
    if (!parseSemVer(minimumFos_, &major, &minor, &patch)) { status_ = FAppManifestStatus::InvalidValue; return status_; }
    const bool newer = major > FOSVersion::kMajor ||
      (major == FOSVersion::kMajor && minor > FOSVersion::kMinor) ||
      (major == FOSVersion::kMajor && minor == FOSVersion::kMinor && patch > FOSVersion::kPatch);
    if (newer) { status_ = FAppManifestStatus::IncompatibleFos; return status_; }
  }
  status_ = FAppManifestStatus::Ready;
  return status_;
}

bool FAppManifest::copyStringField(
  const char * json, const char * key, char * output, size_t capacity, bool required)
{
  const char * cursor = fieldValue(json, key);
  if (cursor == nullptr) return !required;
  if (*cursor++ != '"') return false;
  size_t length = 0;
  while (*cursor != '\0' && *cursor != '"') {
    char value = *cursor++;
    if (value == '\\') {
      value = *cursor++;
      if (value == 'n') value = '\n';
      else if (value == 't') value = '\t';
      else if (value != '"' && value != '\\' && value != '/') return false;
    }
    if (length + 1 >= capacity || static_cast<unsigned char>(value) < 0x20) return false;
    output[length++] = value;
  }
  if (*cursor != '"') return false;
  output[length] = '\0';
  return !required || length > 0;
}

bool FAppManifest::readBoolField(const char * json, const char * key, bool fallback)
{
  const char * value = fieldValue(json, key);
  if (value == nullptr) return fallback;
  if (strncmp(value, "true", 4) == 0) return true;
  if (strncmp(value, "false", 5) == 0) return false;
  return fallback;
}

bool FAppManifest::readPermissions(const char * json)
{
  const char * cursor = fieldValue(json, "permissions");
  if (cursor == nullptr) return true;
  if (*cursor++ != '[') return false;
  permissions_ = 0;
  while (*cursor != '\0') {
    while (isspace(static_cast<unsigned char>(*cursor)) || *cursor == ',') ++cursor;
    if (*cursor == ']') return true;
    if (*cursor++ != '"') return false;
    char permission[24];
    size_t length = 0;
    while (*cursor != '\0' && *cursor != '"') {
      if (length + 1 >= sizeof(permission)) return false;
      permission[length++] = *cursor++;
    }
    if (*cursor++ != '"') return false;
    permission[length] = '\0';
    if (strcmp(permission, "ui") == 0) permissions_ |= static_cast<FAppPermissions>(FAppPermission::Ui);
    else if (strcmp(permission, "storage.read") == 0) permissions_ |= static_cast<FAppPermissions>(FAppPermission::StorageRead);
    else if (strcmp(permission, "storage.write") == 0) permissions_ |= static_cast<FAppPermissions>(FAppPermission::StorageWrite);
    else if (strcmp(permission, "network") == 0) permissions_ |= static_cast<FAppPermissions>(FAppPermission::Network);
    else if (strcmp(permission, "audio") == 0) permissions_ |= static_cast<FAppPermissions>(FAppPermission::Audio);
    else if (strcmp(permission, "system.restart") == 0) permissions_ |= static_cast<FAppPermissions>(FAppPermission::SystemRestart);
    else return false;
  }
  return false;
}

bool FAppManifest::isSafeFilename(const char * value)
{
  return value != nullptr && value[0] != '\0' && strchr(value, '/') == nullptr &&
         strchr(value, '\\') == nullptr && strstr(value, "..") == nullptr;
}

bool FAppManifest::parseSemVer(
  const char * value, uint16_t * major, uint16_t * minor, uint16_t * patch)
{
  if (value == nullptr || major == nullptr || minor == nullptr || patch == nullptr) return false;
  unsigned long parts[3] = {0, 0, 0};
  const char * cursor = value;
  for (uint8_t index = 0; index < 3; ++index) {
    if (!isdigit(static_cast<unsigned char>(*cursor))) return false;
    char * end = nullptr;
    parts[index] = strtoul(cursor, &end, 10);
    if (parts[index] > 65535 || end == cursor) return false;
    if (index < 2) { if (*end != '.') return false; cursor = end + 1; }
    else if (*end != '\0') return false;
  }
  *major = static_cast<uint16_t>(parts[0]);
  *minor = static_cast<uint16_t>(parts[1]);
  *patch = static_cast<uint16_t>(parts[2]);
  return true;
}

const char * FAppManifest::errorMessage() const
{
  switch (status_) {
    case FAppManifestStatus::NotFound: return "app.json not found";
    case FAppManifestStatus::Ready: return "manifest ready";
    case FAppManifestStatus::OpenFailed: return "cannot open app.json";
    case FAppManifestStatus::TooLarge: return "app.json exceeds 4096 bytes";
    case FAppManifestStatus::InvalidJson: return "invalid app.json structure";
    case FAppManifestStatus::MissingField: return "required manifest field missing";
    case FAppManifestStatus::InvalidValue: return "invalid manifest value or permission";
    case FAppManifestStatus::IncompatibleFos: return "app requires a newer fOS version";
  }
  return "unknown manifest error";
}
