#pragma once
// Private to lib/net: shared state between Wifi.cpp and ImprovSerial.cpp. Not part of the public API.
#if FEATURE_WIFI
namespace net {
// True while an Improv provisioning attempt owns the WiFi connection; Wifi.cpp must not reconnect then.
bool improvProvisioning();

// True when the driver's STA config has an SSID (stored, Improv-provisioned or dev credentials).
// Use this, not WiFi.SSID(), which is the associated AP's SSID and "" whenever not connected.
bool staConfigured();

// esp_wifi_set_storage() with the choice remembered (IDF has no getter). RAM storage keeps
// credentials out of flash; it must stay RAM while they are in use, because the framework's
// auto-reconnect re-applies the STA config (esp_wifi_set_config) on every reconnect.
void setStaStorageFlash(bool flash);
bool staStorageFlash();
}  // namespace net
#endif
