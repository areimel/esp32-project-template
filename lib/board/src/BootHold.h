#pragma once
#include <Arduino.h>
#include "Board.h"
#include "Log.h"

// Returns true if every button stays held for holdMs from boot, calling
// progress(remainingMs) while held. Returns false as soon as any button is
// absent, or if a button is released before the time elapses. When the hold
// completes, onHeld() runs first (do the maintenance action there), then
// progress(0) is drawn so no screen reports the action before it happened.
// Then waits (bounded) for all buttons to be released so the hold does not
// leak into normal input.
template <class F, class G>
bool holdAllButtonsAtBoot(uint32_t holdMs, F progress, G onHeld) {
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
      onHeld();
      progress(0);
      const uint32_t releaseStart = millis();
      while (anyHeld()) {
        if (millis() - releaseStart >= 5000) {
          LOG_W("boot hold: buttons not released after 5000 ms, continuing");
          break;
        }
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
