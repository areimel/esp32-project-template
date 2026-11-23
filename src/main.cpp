// Temporary placeholder while the firmware modules move into lib/ (replaced in a later task).
#include <Arduino.h>
#include "Board.h"
#include "Every.h"
#include "Log.h"
#include "generated_config.h"
#if FEATURE_WIFI
#include <Net.h>
#endif

namespace {
Every blink(500);
Every heartbeat(5000);
bool ledOn = false;
bool last[8] = {};
}  // namespace

void setup() {
  Serial.begin(115200);  // never wait for a USB serial host
  board::begin(AppConfig::DISPLAY_BRIGHTNESS);
  LOG_I("%s on %s (%u buttons)", AppConfig::PROJECT_NAME, BOARD_LABEL, board::buttonCount());
}

void loop() {
  board::update();
  const uint32_t now = millis();
  for (uint8_t i = 0; i < board::buttonCount() && i < 8; ++i) {
    const bool down = board::isPressed(i);
    if (down != last[i]) LOG_I("button %u %s", i, down ? "down" : "up");
    last[i] = down;
  }
  if (blink.due(now)) board::setStatusLed(ledOn = !ledOn);
  if (heartbeat.due(now)) LOG_I("battery=%d%% charging=%d", board::batteryPercent(), board::charging());
  delay(1);
}
