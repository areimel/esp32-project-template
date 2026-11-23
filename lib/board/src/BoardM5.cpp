#include "Board.h"
#if BOARD_DRIVER_M5
#include <M5Unified.h>
#include "Log.h"

namespace {
// M5GFX autodetect probes the Stick-S3 power chip over I2C and was observed to miss it on
// some resets, reporting another ESP32-S3-PICO board with the wrong display, button and
// power mapping. A restart re-runs the probe; the counter survives it in RTC memory and
// bounds the retries so a genuinely different board still boots, with a warning.
constexpr uint32_t kRetryMagic = 0xB0A2D000;
constexpr uint32_t kMaxDetectRetries = 3;
RTC_NOINIT_ATTR uint32_t detectRetries;

void checkDetectedBoard() {
  const bool counting = (detectRetries & 0xFFFFFF00) == kRetryMagic;
  const uint32_t tries = counting ? (detectRetries & 0xFF) : 0;
  if (M5.getBoard() == m5::board_t::board_M5StickS3) {
    if (tries) LOG_I("Stick-S3 detected after %u restart(s)", unsigned(tries));
    detectRetries = 0;
    return;
  }
  LOG_W("expected Stick-S3, detected M5 board id %d", int(M5.getBoard()));
  if (tries >= kMaxDetectRetries) {
    detectRetries = 0;
    return;
  }
  detectRetries = kRetryMagic | (tries + 1);
  ESP.restart();
}
}  // namespace

namespace board {
void begin(uint8_t brightness) {
  auto cfg = M5.config();
  cfg.internal_spk = false;
  cfg.internal_mic = false;
  cfg.internal_imu = false;
  cfg.internal_rtc = false;
  cfg.output_power = false;
  M5.begin(cfg);
#if BOARD_HAS_DISPLAY
  M5.Display.setRotation(1);
  M5.Display.setBrightness(brightness);
#else
  (void)brightness;
#endif
  // Callers own debounce; M5Unified only supplies the current button state.
  M5.BtnA.setDebounceThresh(0);
  M5.BtnB.setDebounceThresh(0);
  checkDetectedBoard();
}

void update() { M5.update(); }
uint8_t buttonCount() { return 2; }

bool isPressed(uint8_t i) {
  return i == 0 ? M5.BtnA.isPressed() : i == 1 ? M5.BtnB.isPressed() : false;
}

int batteryPercent() {
  const int level = M5.Power.getBatteryLevel();
  return level > 100 ? 100 : level;
}

bool charging() { return M5.Power.isCharging() == m5::Power_Class::is_charging; }
void setStatusLed(bool) {}  // the Stick-S3 has no status LED exposed through this layer

#if BOARD_HAS_DISPLAY
M5GFX& display() { return M5.Display; }
#endif
}  // namespace board
#endif  // BOARD_DRIVER_M5
