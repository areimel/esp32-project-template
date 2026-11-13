#pragma once
#include <Arduino.h>

#if BOARD_HAS_DISPLAY
#include <M5GFX.h>
#endif

// Hardware abstraction selected at build time by boards/boards.yaml
// (BOARD_DRIVER_M5 or BOARD_DRIVER_GPIO). Nothing here blocks on USB serial.
namespace board {
void begin(uint8_t brightness);  // M5.begin or pinMode; brightness ignored without a display
void update();                   // poll buttons (M5.update on m5)
uint8_t buttonCount();
bool isPressed(uint8_t i);       // false for an out-of-range index
int batteryPercent();            // -1 when the board has no battery gauge
bool charging();
void setStatusLed(bool on);      // no-op when BOARD_LED_PIN < 0
#if BOARD_HAS_DISPLAY
M5GFX& display();
#endif
}  // namespace board
