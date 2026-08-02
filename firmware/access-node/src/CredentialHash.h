#pragma once
#include <Arduino.h>

// Mirrors backend/security.py:hash_credential_value — normalize (trim +
// uppercase) then SHA-256, formatted as lowercase hex to match Python's
// hashlib .hexdigest() exactly, since that's what's compared byte-for-byte
// against the value_hash strings the backend sends on every sync.
String sha256HexOfCredential(const String &rawValue);
