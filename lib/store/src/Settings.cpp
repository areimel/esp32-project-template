#include "Settings.h"
#include <Preferences.h>

uint8_t Settings::getU8(const char* key, uint8_t def) {
  Preferences prefs;
  if (!prefs.begin(ns_, /*readOnly=*/false)) return def;
  const uint8_t value = prefs.getUChar(key, def);
  prefs.end();
  return value;
}

bool Settings::putU8(const char* key, uint8_t value) {
  Preferences prefs;
  if (!prefs.begin(ns_, /*readOnly=*/false)) return false;
  // Read first so an unchanged value never costs a flash write.
  const bool unchanged = prefs.isKey(key) && prefs.getUChar(key, 0) == value;
  const bool ok = unchanged || prefs.putUChar(key, value) > 0;
  prefs.end();
  return ok;
}
