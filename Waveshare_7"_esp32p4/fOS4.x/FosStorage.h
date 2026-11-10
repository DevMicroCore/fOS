#pragma once
#include <SD_MMC.h>
// All fOS paths and permission rules are retained; only the bus changes.
bool board_storage_begin();

// Local FAT is unmounted while raw card sectors are owned by USB MSC.
bool board_storage_usb_claim();
bool board_storage_usb_release();
