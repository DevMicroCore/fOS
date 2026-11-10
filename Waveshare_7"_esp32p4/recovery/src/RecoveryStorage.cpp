#include <SD_MMC.h>
bool board_storage_begin() {
 static bool mounted;
 if(mounted)return true;
 if(!SD_MMC.setPins(43,44,39,40,41,42))return false;
 mounted=SD_MMC.begin("/sdcard", false, false, SDMMC_FREQ_DEFAULT,10);
 return mounted;
}
