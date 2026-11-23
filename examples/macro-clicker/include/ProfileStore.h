#pragma once
#include <stdint.h>
#include "Settings.h"

// Remembers the active profile index in NVS (namespace "macro", key "profile").
class ProfileStore {
 public:
  // Returns saved index clamped to [0, count); 0 if nothing saved or count == 0.
  // A reflash with fewer profiles falls back to 0.
  uint8_t load(uint8_t count);

  // Persists idx. Settings skips the write when the stored value is unchanged
  // (avoids flash wear). Returns false on NVS failure.
  bool save(uint8_t idx);

 private:
  Settings settings_{"macro"};
};
