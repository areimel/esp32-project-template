#include "ProfileStore.h"
#include "Settings.h"  // lib/store; named in the .cpp so PlatformIO's dependency finder links it

uint8_t ProfileStore::load(uint8_t count) {
  if (count == 0) return 0;
  const uint8_t value = settings_.getU8("profile", 0);
  return value >= count ? 0 : value;
}

bool ProfileStore::save(uint8_t idx) { return settings_.putU8("profile", idx); }
