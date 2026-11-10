#pragma once
// Onboard ESP32-P4 RTC, not an external I2C calendar chip.
void fos_rtc_begin();
void fos_rtc_poll();
// Save current UTC + RTC counter before intentionally entering battery sleep.
bool fos_rtc_prepare_backup_sleep();
