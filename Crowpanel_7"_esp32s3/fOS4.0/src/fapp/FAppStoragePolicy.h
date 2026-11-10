#pragma once

#include "FAppPath.h"
#include <stdint.h>

// Store only the active app's folder grants, not 40 copies in the launcher.
class FAppStoragePolicy {
 public:
  static constexpr uint8_t kMaximumGrants = 8;
  static constexpr size_t kPathBytes = 512;
  FAppStoragePolicy() { legacyLocal(); }
  void clear() { used_ = 0; count_ = 0; localRead_ = localWrite_ = localAudio_ = false; }
  void legacyLocal() { clear(); localRead_ = localWrite_ = localAudio_ = true; }
  void allowLocalRead() { localRead_ = true; }
  void allowLocalWrite() { localWrite_ = true; }
  void allowLocalAudio() { localAudio_ = true; }

  bool add(const char * path, bool write)
  {
    if (path == nullptr || path[0] != '/') return false;
    char normalized[256];
    if (FAppPath::resolve("/", path, normalized, sizeof(normalized)) != FAppPath::Status::Ready) return false;
    const size_t length = strlen(normalized) + 1;
    if (count_ >= kMaximumGrants || used_ + length > kPathBytes) return false;
    offsets_[count_] = used_;
    writes_[count_++] = write;
    memcpy(paths_ + used_, normalized, length);
    used_ += static_cast<uint16_t>(length);
    return true;
  }

  bool allows(const char * normalizedPath, const char * appDirectory, bool write, bool audio = false) const
  {
    if (normalizedPath == nullptr) return false;
    char verified[256];
    if (FAppPath::resolve("/", normalizedPath, verified, sizeof(verified)) != FAppPath::Status::Ready) return false;
    if ((write ? localWrite_ : (localRead_ || (audio && localAudio_))) && appDirectory != nullptr) {
      char base[256];
      if (FAppPath::resolve("/", appDirectory, base, sizeof(base)) == FAppPath::Status::Ready && contains(base, verified)) return true;
    }
    for (uint8_t i = 0; i < count_; ++i) {
      if (writes_[i] == write && contains(paths_ + offsets_[i], verified)) return true;
    }
    return false;
  }

 private:
  static bool contains(const char * folder, const char * path)
  {
    if (strcmp(folder, "/") == 0) return true;
    size_t i = 0;
    for (; folder[i] != '\0'; ++i) {
      const char a = folder[i] >= 'A' && folder[i] <= 'Z' ? folder[i] + 32 : folder[i];
      const char b = path[i] >= 'A' && path[i] <= 'Z' ? path[i] + 32 : path[i];
      if (a != b) return false;
    }
    return path[i] == '\0' || path[i] == '/';
  }
  char paths_[kPathBytes];
  uint16_t offsets_[kMaximumGrants];
  uint16_t used_;
  bool writes_[kMaximumGrants];
  uint8_t count_;
  bool localRead_, localWrite_, localAudio_;
};
