#include "Board.h"
#if BOARD_DRIVER_GPIO
namespace {
constexpr uint8_t kPins[] = BOARD_BUTTON_PINS;  // active-low, INPUT_PULLUP
}

namespace board {
void begin(uint8_t) {
  for (uint8_t pin : kPins) pinMode(pin, INPUT_PULLUP);
#if BOARD_LED_PIN >= 0
  pinMode(BOARD_LED_PIN, OUTPUT);
  setStatusLed(false);
#endif
}

void update() {}  // GPIO is read directly; consumers debounce
uint8_t buttonCount() { return sizeof(kPins); }
bool isPressed(uint8_t i) { return i < sizeof(kPins) && digitalRead(kPins[i]) == LOW; }
int batteryPercent() { return -1; }
bool charging() { return false; }

void setStatusLed(bool on) {
#if BOARD_LED_PIN >= 0
#if BOARD_LED_RGB
  rgbLedWrite(BOARD_LED_PIN, on ? 32 : 0, on ? 32 : 0, on ? 32 : 0);
#else
  digitalWrite(BOARD_LED_PIN, on ? HIGH : LOW);
#endif
#else
  (void)on;
#endif
}
}  // namespace board
#endif  // BOARD_DRIVER_GPIO
