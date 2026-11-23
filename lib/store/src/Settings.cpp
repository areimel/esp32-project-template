#include "Settings.h"
#include <Preferences.h>
#include <nvs.h>

uint8_t Settings::getU8(const char* key, uint8_t def) {
  // Read-only, so a plain read never creates the namespace. Before the first putU8 the namespace
  // does not exist (ESP_ERR_NVS_NOT_FOUND): that is "no value yet", so return def. The NVS API is
  // used directly because Preferences::begin() logs that expected case as an error.
  nvs_handle_t handle;
  if (nvs_open(ns_, NVS_READONLY, &handle) != ESP_OK) return def;
  uint8_t value = def;
  if (nvs_get_u8(handle, key, &value) != ESP_OK) value = def;  // same storage as Preferences::putUChar
  nvs_close(handle);
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
