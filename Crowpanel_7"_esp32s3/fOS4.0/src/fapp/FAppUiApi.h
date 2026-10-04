#pragma once

#include <stddef.h>
#include <stdint.h>

#include "FAppValue.h"

enum class FAppPermission : uint8_t {
  Ui = 1u << 0,
  StorageRead = 1u << 1,
  StorageWrite = 1u << 2,
  Network = 1u << 3,
  Audio = 1u << 4,
  SystemRestart = 1u << 5
};

using FAppPermissions = uint8_t;

inline bool fappHasPermission(FAppPermissions permissions, FAppPermission permission)
{
  return (permissions & static_cast<FAppPermissions>(permission)) != 0;
}

class FAppUiApi {
 public:
  virtual ~FAppUiApi() = default;
  virtual bool getProperty(uint16_t objectId, uint8_t property, FAppValue * result) = 0;
  virtual bool setProperty(uint16_t objectId, uint8_t property, const FAppValue& value) = 0;
  virtual bool callMethod(
    uint16_t objectId,
    uint8_t method,
    const FAppValue * arguments,
    uint8_t argumentCount,
    FAppValue * result) = 0;
};
