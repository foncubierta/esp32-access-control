#include "CredentialHash.h"
#include <mbedtls/sha256.h>

String sha256HexOfCredential(const String &rawValue) {
  String normalized = rawValue;
  normalized.trim();
  normalized.toUpperCase();

  unsigned char digest[32];
  mbedtls_sha256_context ctx;
  mbedtls_sha256_init(&ctx);
  mbedtls_sha256_starts(&ctx, 0);  // 0 = SHA-256 (not the SHA-224 variant)
  mbedtls_sha256_update(&ctx, (const unsigned char *)normalized.c_str(), normalized.length());
  mbedtls_sha256_finish(&ctx, digest);
  mbedtls_sha256_free(&ctx);

  char hex[65];
  for (int i = 0; i < 32; i++) {
    sprintf(hex + i * 2, "%02x", digest[i]);
  }
  hex[64] = '\0';
  return String(hex);
}
