#pragma once
#include <stdint.h>
#include <stddef.h>
#include "esp_err.h"
#ifdef __cplusplus
extern "C" {
#endif
esp_err_t fos_codec_write(const uint8_t *data, size_t size);
esp_err_t fos_codec_read(uint8_t reg, uint8_t *value);
#ifdef __cplusplus
}
#endif
