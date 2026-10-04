#pragma once

#include <FS.h>

#include "FAppUiApi.h"
#include "FAppUiRegistry.h"

class LvglFAppUiApi : public FAppUiApi {
 public:
  explicit LvglFAppUiApi(FAppUiRegistry& registry)
    : registry_(registry), filesystem_(nullptr) { appDirectory_[0] = '\0'; }
  void configureFileContext(fs::FS& filesystem, const char * appDirectory);
  void clearFileContext();
  bool getProperty(uint16_t objectId, uint8_t property, FAppValue * result) override;
  bool setProperty(uint16_t objectId, uint8_t property, const FAppValue& value) override;
  bool callMethod(
    uint16_t objectId,
    uint8_t method,
    const FAppValue * arguments,
    uint8_t argumentCount,
    FAppValue * result) override;

 private:
  static lv_obj_t * textObject(lv_obj_t * object, FAppUiObjectType type);
  bool resolveAppPath(const char * relativePath, char * output, size_t capacity) const;
  bool ensureParentDirectories(const char * fullPath);
  FAppUiRegistry& registry_;
  fs::FS * filesystem_;
  char appDirectory_[160];
};
