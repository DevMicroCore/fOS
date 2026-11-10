#include "FAppUiRegistry.h"

FAppUiRegistry::FAppUiRegistry()
  : count_(0)
{
  clear();
}

void FAppUiRegistry::clear()
{
  for (uint8_t i = 0; i < kCapacity; ++i) {
    objects_[i] = nullptr;
    types_[i] = FAppUiObjectType::Unknown;
  }
  count_ = 0;
}

bool FAppUiRegistry::registerObject(
  uint16_t objectId,
  lv_obj_t * object,
  FAppUiObjectType type)
{
  if (objectId == 0 || objectId > kCapacity || object == nullptr || count_ >= kCapacity) {
    return false;
  }

  const uint8_t index = static_cast<uint8_t>(objectId - 1);
  if (objects_[index] != nullptr) {
    return false;
  }

  objects_[index] = object;
  types_[index] = type;
  ++count_;
  return true;
}

lv_obj_t * FAppUiRegistry::resolve(uint16_t objectId) const
{
  if (objectId == 0 || objectId > kCapacity) {
    return nullptr;
  }
  return objects_[objectId - 1];
}

uint16_t FAppUiRegistry::objectIdFor(lv_obj_t * object) const
{
  if (object == nullptr) return 0;
  for (uint8_t index = 0; index < kCapacity; ++index) {
    if (objects_[index] == object) return static_cast<uint16_t>(index + 1);
  }
  return 0;
}

FAppUiObjectType FAppUiRegistry::objectType(uint16_t objectId) const
{
  if (objectId == 0 || objectId > kCapacity) {
    return FAppUiObjectType::Unknown;
  }
  return types_[objectId - 1];
}

size_t FAppUiRegistry::count() const
{
  return count_;
}
