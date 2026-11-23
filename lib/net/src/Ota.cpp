#include "Net.h"

#if FEATURE_WIFI && FEATURE_OTA
#include <ArduinoOTA.h>

#include "Log.h"

namespace {
bool active = false;
uint8_t lastPct = 255;
}  // namespace

namespace net {

void beginOta(const char* hostname, const char* password) {
  ArduinoOTA.setHostname(hostname);
  ArduinoOTA.setPassword(password);
  ArduinoOTA.onStart([]() {
    active = true;
    lastPct = 255;
    LOG_I("ota: start");
  });
  ArduinoOTA.onProgress([](unsigned int done, unsigned int total) {
    const uint8_t pct = total ? static_cast<uint8_t>(done * 100ULL / total) : 0;
    if (pct / 10 != lastPct / 10) LOG_I("ota: %u%%", pct);
    lastPct = pct;
  });
  ArduinoOTA.onEnd([]() {
    active = false;
    LOG_I("ota: done");
  });
  ArduinoOTA.onError([](ota_error_t err) {
    active = false;
    LOG_E("ota: error %u", static_cast<unsigned>(err));
  });
  ArduinoOTA.begin();
}

void updateOta() { ArduinoOTA.handle(); }

bool otaActive() { return active; }

}  // namespace net
#endif
