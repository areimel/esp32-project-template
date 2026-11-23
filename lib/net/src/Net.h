#pragma once
#include <stdint.h>

#if FEATURE_WIFI
enum class WifiState : uint8_t { NoCredentials, Connecting, Connected };

namespace net {
// Non-blocking. Stored credentials win; dev credentials are used only if none are stored.
void beginWifi(const char* hostname, const char* devSsid, const char* devPass);
void updateWifi(uint32_t now);
WifiState wifiState();
const char* ipAddress();  // "" when not connected

// Improv Serial shares the log port; non-Improv bytes are ignored.
void beginImprov(const char* firmware, const char* version, const char* chip, const char* deviceName);
void updateImprov();  // drains Serial, answers RPCs, provisions via WiFi.begin + persist

#if FEATURE_OTA
// onStart runs once when a transfer begins, before any data is received: release held keys and
// draw an updating screen there. The loop does not run during the transfer.
void beginOta(const char* hostname, const char* password, void (*onStart)() = nullptr);
// Runs a whole transfer inline (ArduinoOTA.handle()): success restarts the device; on failure it
// returns to the caller after onError, and the app resumes.
void updateOta();
#endif
}  // namespace net
#endif
