#include "Board.h"
#if BOARD_DRIVER_M5
#include <M5Unified.h>
#include "Log.h"

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
  if (M5.getBoard() != m5::board_t::board_M5StickS3)
    LOG_W("expected Stick-S3, detected M5 board id %d", int(M5.getBoard()));
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
