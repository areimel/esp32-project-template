#pragma once
#include <stdint.h>

// Tiny NVS-backed settings store. Each call opens the namespace read-write
// and closes it again, so no handle is held between calls.
class Settings {
 public:
  explicit Settings(const char* ns) : ns_(ns) {}

  // Returns the stored value, or def if the key (or namespace) is missing.
  uint8_t getU8(const char* key, uint8_t def);

  // Persists value. Returns true without writing when the stored value
  // already equals value (avoids flash wear); false on NVS failure.
  bool putU8(const char* key, uint8_t value);

 private:
  const char* ns_;
};
