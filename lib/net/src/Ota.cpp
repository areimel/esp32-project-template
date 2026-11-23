#include "Net.h"

#if FEATURE_WIFI && FEATURE_OTA
#include <ArduinoOTA.h>

#include "Log.h"

namespace {
uint8_t lastPct = 255;
void (*startHook)() = nullptr;
}  // namespace

namespace net {

void beginOta(const char* hostname, const char* password, void (*onStart)()) {
  startHook = onStart;
  ArduinoOTA.setHostname(hostname);
  ArduinoOTA.setPassword(password);
  ArduinoOTA.onStart([]() {
    lastPct = 255;
    LOG_I("ota: start");
    if (startHook) startHook();
  });
  ArduinoOTA.onProgress([](unsigned int done, unsigned int total) {
    const uint8_t pct = total ? static_cast<uint8_t>(done * 100ULL / total) : 0;
    if (pct / 10 != lastPct / 10) LOG_I("ota: %u%%", pct);
    lastPct = pct;
  });
  ArduinoOTA.onEnd([]() { LOG_I("ota: done, restarting"); });
  ArduinoOTA.onError([](ota_error_t err) { LOG_E("ota: error %u", static_cast<unsigned>(err)); });
  ArduinoOTA.begin();
}

void updateOta() { ArduinoOTA.handle(); }

}  // namespace net
#endif
