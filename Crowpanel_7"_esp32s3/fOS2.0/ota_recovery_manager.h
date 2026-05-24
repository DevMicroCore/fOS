#pragma once

#include <lvgl.h>

#ifdef __cplusplus
extern "C" {
#endif

void OTARecovery_Init(void);
void OTARecovery_Tick(void);
void OTARecovery_InstallUpdateEvent(lv_event_t * e);
bool OTARecovery_IsBusy(void);

#ifdef __cplusplus
}
#endif
