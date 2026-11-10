#pragma once
#include "lvgl.h"
#include "src/squareline_api.h"
#include "native_ui/ui_Email.h"
#include "native_ui/ui_AppStore.h"
#ifdef __cplusplus
extern "C" {
#endif
void AppStoreDownloadApp(lv_event_t *e);
void AppStoreDeleteApp(lv_event_t *e);
void AssignAppsConfirm(lv_event_t *e);
void SendEmail(lv_event_t *e);
#ifdef __cplusplus
}
#endif
