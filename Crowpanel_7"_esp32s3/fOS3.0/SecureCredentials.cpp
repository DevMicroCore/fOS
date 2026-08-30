#include "SecureCredentials.h"

#include <Preferences.h>
#include <esp_system.h>
#include <mbedtls/base64.h>
#include <mbedtls/gcm.h>
#include <mbedtls/sha256.h>
#include <stdlib.h>
#include <string.h>

namespace {

constexpr char kEncryptedPrefix[] = "enc:v1:";
constexpr char kPreferencesNamespace[] = "fos_secure";
constexpr char kKeyName[] = "sd_key";
constexpr size_t kKeySize = 32;
constexpr size_t kNonceSize = 12;
constexpr size_t kTagSize = 16;
constexpr size_t kMaxCredentialLength = 512;
constexpr uint8_t kAdditionalData[] = {
  'f', 'O', 'S', '-', 'S', 'D', '-', 'c', 'r', 'e', 'd', '-', 'v', '1'
};

uint8_t gDeviceKey[kKeySize];
bool gDeviceKeyLoaded = false;

void secureZero(void * data, size_t length)
{
  volatile uint8_t * bytes = static_cast<volatile uint8_t *>(data);
  while (length-- > 0) *bytes++ = 0;
}

bool loadOrCreateDeviceKey()
{
  if (gDeviceKeyLoaded) return true;

  uint8_t deviceSecret[kKeySize];
  Preferences preferences;
  if (!preferences.begin(kPreferencesNamespace, false)) return false;

  bool ok = false;
  if (preferences.getBytesLength(kKeyName) == kKeySize) {
    ok = preferences.getBytes(kKeyName, deviceSecret, kKeySize) == kKeySize;
  } else {
    esp_fill_random(deviceSecret, kKeySize);
    ok = preferences.putBytes(kKeyName, deviceSecret, kKeySize) == kKeySize;
  }
  preferences.end();

  if (!ok) {
    secureZero(deviceSecret, sizeof(deviceSecret));
    secureZero(gDeviceKey, sizeof(gDeviceKey));
    return false;
  }

  // Bind the NVS secret to the immutable eFuse MAC of this particular ESP32.
  uint8_t keyMaterial[kKeySize + sizeof(uint64_t)];
  memcpy(keyMaterial, deviceSecret, kKeySize);
  const uint64_t efuseMac = ESP.getEfuseMac();
  memcpy(keyMaterial + kKeySize, &efuseMac, sizeof(efuseMac));
  ok = mbedtls_sha256_ret(keyMaterial, sizeof(keyMaterial), gDeviceKey, 0) == 0;
  secureZero(deviceSecret, sizeof(deviceSecret));
  secureZero(keyMaterial, sizeof(keyMaterial));
  if (!ok) {
    secureZero(gDeviceKey, sizeof(gDeviceKey));
    return false;
  }

  gDeviceKeyLoaded = true;
  return true;
}

bool encodeBase64(const uint8_t * data, size_t dataLength, String * encoded)
{
  if (encoded == nullptr) return false;
  const size_t capacity = 4 * ((dataLength + 2) / 3) + 1;
  unsigned char * output = static_cast<unsigned char *>(malloc(capacity));
  if (output == nullptr) return false;

  size_t outputLength = 0;
  // Mbed TLS also needs room for its terminating NUL byte. `capacity` already
  // includes that byte, so pass the complete allocation size here.
  const int result = mbedtls_base64_encode(output, capacity, &outputLength, data, dataLength);
  if (result == 0) {
    output[outputLength] = '\0';
    *encoded = String(reinterpret_cast<const char *>(output));
  }

  secureZero(output, capacity);
  free(output);
  return result == 0;
}

bool decodeBase64(const String& encoded, uint8_t ** decoded, size_t * decodedLength)
{
  if (decoded == nullptr || decodedLength == nullptr || encoded.length() == 0) return false;

  const size_t capacity = (encoded.length() * 3) / 4 + 3;
  uint8_t * output = static_cast<uint8_t *>(malloc(capacity));
  if (output == nullptr) return false;

  size_t outputLength = 0;
  const int result = mbedtls_base64_decode(
    output,
    capacity,
    &outputLength,
    reinterpret_cast<const unsigned char *>(encoded.c_str()),
    encoded.length()
  );
  if (result != 0) {
    secureZero(output, capacity);
    free(output);
    return false;
  }

  *decoded = output;
  *decodedLength = outputLength;
  return true;
}

}  // namespace

namespace SecureCredentials {

bool isEncrypted(const String& value)
{
  return value.startsWith(kEncryptedPrefix);
}

bool encrypt(const String& plainText, String * encodedValue)
{
  if (encodedValue == nullptr) return false;
  clear(encodedValue);
  if (plainText.length() > kMaxCredentialLength) return false;
  if (!loadOrCreateDeviceKey()) return false;

  const size_t plainLength = plainText.length();
  const size_t binaryLength = kNonceSize + plainLength + kTagSize;
  uint8_t * binary = static_cast<uint8_t *>(malloc(binaryLength));
  if (binary == nullptr) return false;

  uint8_t * nonce = binary;
  uint8_t * cipherText = binary + kNonceSize;
  uint8_t * tag = cipherText + plainLength;
  esp_fill_random(nonce, kNonceSize);

  mbedtls_gcm_context context;
  mbedtls_gcm_init(&context);
  int result = mbedtls_gcm_setkey(&context, MBEDTLS_CIPHER_ID_AES, gDeviceKey, 256);
  if (result == 0) {
    result = mbedtls_gcm_crypt_and_tag(
      &context,
      MBEDTLS_GCM_ENCRYPT,
      plainLength,
      nonce,
      kNonceSize,
      kAdditionalData,
      sizeof(kAdditionalData),
      reinterpret_cast<const uint8_t *>(plainText.c_str()),
      cipherText,
      kTagSize,
      tag
    );
  }
  mbedtls_gcm_free(&context);

  String base64;
  const bool encoded = result == 0 && encodeBase64(binary, binaryLength, &base64);
  secureZero(binary, binaryLength);
  free(binary);

  if (!encoded) return false;
  *encodedValue = String(kEncryptedPrefix) + base64;
  clear(&base64);
  return true;
}

bool decrypt(const String& encodedValue, String * plainText)
{
  if (plainText == nullptr) return false;
  clear(plainText);
  if (!isEncrypted(encodedValue) || !loadOrCreateDeviceKey()) return false;

  uint8_t * binary = nullptr;
  size_t binaryLength = 0;
  if (!decodeBase64(encodedValue.substring(strlen(kEncryptedPrefix)), &binary, &binaryLength)) return false;
  if (binaryLength < kNonceSize + kTagSize ||
      binaryLength - kNonceSize - kTagSize > kMaxCredentialLength) {
    secureZero(binary, binaryLength);
    free(binary);
    return false;
  }

  const size_t cipherLength = binaryLength - kNonceSize - kTagSize;
  const uint8_t * nonce = binary;
  const uint8_t * cipherText = binary + kNonceSize;
  const uint8_t * tag = cipherText + cipherLength;
  uint8_t * plain = static_cast<uint8_t *>(malloc(cipherLength + 1));
  if (plain == nullptr) {
    secureZero(binary, binaryLength);
    free(binary);
    return false;
  }

  mbedtls_gcm_context context;
  mbedtls_gcm_init(&context);
  int result = mbedtls_gcm_setkey(&context, MBEDTLS_CIPHER_ID_AES, gDeviceKey, 256);
  if (result == 0) {
    result = mbedtls_gcm_auth_decrypt(
      &context,
      cipherLength,
      nonce,
      kNonceSize,
      kAdditionalData,
      sizeof(kAdditionalData),
      tag,
      kTagSize,
      cipherText,
      plain
    );
  }
  mbedtls_gcm_free(&context);

  bool ok = result == 0;
  if (ok) {
    plain[cipherLength] = '\0';
    *plainText = String(reinterpret_cast<const char *>(plain));
  } else {
    plainText->remove(0);
  }

  secureZero(plain, cipherLength + 1);
  secureZero(binary, binaryLength);
  free(plain);
  free(binary);
  return ok;
}

void clear(String * value)
{
  if (value == nullptr) return;
  for (size_t i = 0; i < value->length(); ++i) value->setCharAt(i, '\0');
  value->remove(0);
}

}  // namespace SecureCredentials
