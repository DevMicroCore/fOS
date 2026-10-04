#include "SecureCredentials.h"

#include <Preferences.h>
#include <esp_system.h>
#include <mbedtls/base64.h>
#include <mbedtls/gcm.h>
#include <mbedtls/sha256.h>
#include <mbedtls/version.h>
#include <stdlib.h>
#include <string.h>

namespace {

#define CREDENTIAL_ENCRYPTED_PREFIX "enc:v1:"
#define CREDENTIAL_PREFERENCES_NAMESPACE "fos_secure"
#define CREDENTIAL_KEY_NAME "sd_key"
#define CREDENTIAL_KEY_SIZE 32U
#define CREDENTIAL_NONCE_SIZE 12U
#define CREDENTIAL_TAG_SIZE 16U
#define CREDENTIAL_MAX_LENGTH 512U
static const uint8_t kAdditionalData[] = {
  'f', 'O', 'S', '-', 'S', 'D', '-', 'c', 'r', 'e', 'd', '-', 'v', '1'
};

uint8_t gDeviceKey[CREDENTIAL_KEY_SIZE];
bool gDeviceKeyLoaded = false;

void secureZero(void * data, size_t length)
{
  volatile uint8_t * bytes = static_cast<volatile uint8_t *>(data);
  while (length-- > 0) *bytes++ = 0;
}

bool loadOrCreateDeviceKey()
{
  if (gDeviceKeyLoaded) return true;

  uint8_t deviceSecret[CREDENTIAL_KEY_SIZE];
  Preferences preferences;
  if (!preferences.begin(CREDENTIAL_PREFERENCES_NAMESPACE, false)) return false;

  bool ok = false;
  if (preferences.getBytesLength(CREDENTIAL_KEY_NAME) == CREDENTIAL_KEY_SIZE) {
    ok = preferences.getBytes(CREDENTIAL_KEY_NAME, deviceSecret, CREDENTIAL_KEY_SIZE) == CREDENTIAL_KEY_SIZE;
  } else {
    esp_fill_random(deviceSecret, CREDENTIAL_KEY_SIZE);
    ok = preferences.putBytes(CREDENTIAL_KEY_NAME, deviceSecret, CREDENTIAL_KEY_SIZE) == CREDENTIAL_KEY_SIZE;
  }
  preferences.end();

  if (!ok) {
    secureZero(deviceSecret, sizeof(deviceSecret));
    secureZero(gDeviceKey, sizeof(gDeviceKey));
    return false;
  }

  // Bind the NVS secret to the immutable eFuse MAC of this particular ESP32.
  uint8_t keyMaterial[CREDENTIAL_KEY_SIZE + sizeof(uint64_t)];
  memcpy(keyMaterial, deviceSecret, CREDENTIAL_KEY_SIZE);
  const uint64_t efuseMac = ESP.getEfuseMac();
  memcpy(keyMaterial + CREDENTIAL_KEY_SIZE, &efuseMac, sizeof(efuseMac));
#if MBEDTLS_VERSION_MAJOR >= 3
  ok = mbedtls_sha256(keyMaterial, sizeof(keyMaterial), gDeviceKey, 0) == 0;
#else
  ok = mbedtls_sha256_ret(keyMaterial, sizeof(keyMaterial), gDeviceKey, 0) == 0;
#endif
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
  return value.startsWith(CREDENTIAL_ENCRYPTED_PREFIX);
}

bool encrypt(const String& plainText, String * encodedValue)
{
  if (encodedValue == nullptr) return false;
  clear(encodedValue);
  if (plainText.length() > CREDENTIAL_MAX_LENGTH) return false;
  if (!loadOrCreateDeviceKey()) return false;

  const size_t plainLength = plainText.length();
  const size_t binaryLength = CREDENTIAL_NONCE_SIZE + plainLength + CREDENTIAL_TAG_SIZE;
  uint8_t * binary = static_cast<uint8_t *>(malloc(binaryLength));
  if (binary == nullptr) return false;

  uint8_t * nonce = binary;
  uint8_t * cipherText = binary + CREDENTIAL_NONCE_SIZE;
  uint8_t * tag = cipherText + plainLength;
  esp_fill_random(nonce, CREDENTIAL_NONCE_SIZE);

  mbedtls_gcm_context context;
  mbedtls_gcm_init(&context);
  int result = mbedtls_gcm_setkey(&context, MBEDTLS_CIPHER_ID_AES, gDeviceKey, 256);
  if (result == 0) {
    result = mbedtls_gcm_crypt_and_tag(
      &context,
      MBEDTLS_GCM_ENCRYPT,
      plainLength,
      nonce,
      CREDENTIAL_NONCE_SIZE,
      kAdditionalData,
      sizeof(kAdditionalData),
      reinterpret_cast<const uint8_t *>(plainText.c_str()),
      cipherText,
      CREDENTIAL_TAG_SIZE,
      tag
    );
  }
  mbedtls_gcm_free(&context);

  String base64;
  const bool encoded = result == 0 && encodeBase64(binary, binaryLength, &base64);
  secureZero(binary, binaryLength);
  free(binary);

  if (!encoded) return false;
  *encodedValue = String(CREDENTIAL_ENCRYPTED_PREFIX) + base64;
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
  if (!decodeBase64(encodedValue.substring(strlen(CREDENTIAL_ENCRYPTED_PREFIX)), &binary, &binaryLength)) return false;
  if (binaryLength < CREDENTIAL_NONCE_SIZE + CREDENTIAL_TAG_SIZE ||
      binaryLength - CREDENTIAL_NONCE_SIZE - CREDENTIAL_TAG_SIZE > CREDENTIAL_MAX_LENGTH) {
    secureZero(binary, binaryLength);
    free(binary);
    return false;
  }

  const size_t cipherLength = binaryLength - CREDENTIAL_NONCE_SIZE - CREDENTIAL_TAG_SIZE;
  const uint8_t * nonce = binary;
  const uint8_t * cipherText = binary + CREDENTIAL_NONCE_SIZE;
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
      CREDENTIAL_NONCE_SIZE,
      kAdditionalData,
      sizeof(kAdditionalData),
      tag,
      CREDENTIAL_TAG_SIZE,
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
