#pragma once

#include <FS.h>
#include <stddef.h>
#include <stdint.h>

#include "FAppUiApi.h"

enum class FAppManifestStatus : uint8_t {
  NotFound = 0,
  Ready,
  OpenFailed,
  TooLarge,
  InvalidJson,
  MissingField,
  InvalidValue,
  IncompatibleFos
};

class FAppManifest {
 public:
  static constexpr size_t kMaximumSize = 4096;

  FAppManifest();
  FAppManifestStatus load(fs::FS& filesystem, const char * path);
  void reset();

  FAppManifestStatus status() const { return status_; }
  const char * id() const { return id_; }
  const char * name() const { return name_; }
  const char * version() const { return version_; }
  const char * minimumFos() const { return minimumFos_; }
  const char * type() const { return type_; }
  const char * icon() const { return icon_; }
  const char * layout() const { return layout_; }
  const char * executable() const { return executable_; }
  bool scrollable() const { return scrollable_; }
  FAppPermissions permissions() const { return permissions_; }
  const char * errorMessage() const;

  static bool isSafeFilename(const char * value);
  static bool parseSemVer(const char * value, uint16_t * major, uint16_t * minor, uint16_t * patch);

 private:
  static bool copyStringField(const char * json, const char * key, char * output, size_t capacity, bool required);
  static bool readBoolField(const char * json, const char * key, bool fallback);
  bool readPermissions(const char * json);

  FAppManifestStatus status_;
  char id_[65];
  char name_[65];
  char version_[17];
  char minimumFos_[17];
  char type_[17];
  char icon_[17];
  char layout_[65];
  char executable_[65];
  bool scrollable_;
  FAppPermissions permissions_;
};
