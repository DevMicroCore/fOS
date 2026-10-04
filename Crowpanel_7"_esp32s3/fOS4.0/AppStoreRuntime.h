#pragma once

#include <Arduino.h>
#include <FS.h>
#include <lvgl.h>

// Creates the AppStore configuration on the SD card without contacting a server.
void AppStoreEnsureSystemFiles(fs::FS& filesystem);

// Launcher assignment access. Slots are zero based (0..6).
bool AppStoreHasExplicitLauncherAssignments();
String AppStoreGetAssignedFolder(uint8_t slot);

#ifdef __cplusplus
extern "C" {
#endif

// Entry points used by ui_events.c and the generated AppStore UI.
void StartAppStore_Data(lv_event_t * e);
void AppStoreDownloadApp(lv_event_t * e);
void AppStoreDeleteApp(lv_event_t * e);
void AssignAppsConfirm(lv_event_t * e);

#ifdef __cplusplus
}
#endif
