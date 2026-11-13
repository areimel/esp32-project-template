#pragma once
#include <Arduino.h>
#include "Board.h"

// Returns true if every button stays held for holdMs from boot, calling
// progress(remainingMs) while held. Returns false as soon as any button is
// absent, or if a button is released before the time elapses. After a
// successful hold, waits for all buttons to be released so the hold does not
// leak into normal input.
template <class F>
bool holdAllButtonsAtBoot(uint32_t holdMs, F progress) {
  const auto allHeld = [] {
    for (uint8_t i = 0; i < board::buttonCount(); ++i)
      if (!board::isPressed(i)) return false;
    return board::buttonCount() > 0;
  };
  const auto anyHeld = [] {
    for (uint8_t i = 0; i < board::buttonCount(); ++i)
      if (board::isPressed(i)) return true;
    return false;
  };
  board::update();
  if (!allHeld()) return false;
  const uint32_t started = millis();
  while (allHeld()) {
    const uint32_t elapsed = millis() - started;
    if (elapsed >= holdMs) {
      progress(0);
      while (anyHeld()) {
        delay(10);
        board::update();
      }
      return true;
    }
    progress(holdMs - elapsed);
    delay(20);
    board::update();
  }
  return false;
}
