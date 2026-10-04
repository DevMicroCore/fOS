#pragma once

#include <Arduino.h>

namespace SecureCredentials {

// Format stored on the SD card: enc:v1:<base64(nonce | ciphertext | GCM tag)>.
// The 256-bit key is generated once and kept in the ESP32's internal NVS.
bool isEncrypted(const String& value);
bool encrypt(const String& plainText, String * encodedValue);
bool decrypt(const String& encodedValue, String * plainText);
void clear(String * value);

}  // namespace SecureCredentials
