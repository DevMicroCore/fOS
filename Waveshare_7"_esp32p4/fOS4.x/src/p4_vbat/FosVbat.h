#pragma once
#include <stdbool.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
// Runs with cache disabled too: no logging, allocation, locks or flash data.
bool fos_vbat_apply_power(bool deep_sleep);
uint32_t fos_vbat_mode(void);
uint32_t fos_vbat_latch_failures(void);
#ifdef __cplusplus
}
#endif
