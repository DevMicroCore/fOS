#pragma once
struct FosUsbStorageHooks {
  bool (*canClaim)();
  void (*prepareClaim)();
  void (*released)(bool mounted);
};
void fos_usb_storage_begin(FosUsbStorageHooks hooks);
bool fos_usb_storage_enabled();
bool fos_usb_storage_set_enabled(bool enabled);
void fos_usb_storage_poll();
bool fos_usb_storage_blocking();
bool fos_usb_storage_active();
