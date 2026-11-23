#pragma once
#include <Arduino.h>
#include "Board.h"
#include "Log.h"

// How long after the call a boot hold may still START. On the GPIO boards the only button is a
// strapping pin (GPIO0, or GPIO9 on the C3): held during reset it selects ROM download mode, so
// it can only be pressed right after reset. Boots without a hold continue after this window.
constexpr uint32_t kBootHoldGraceMs = 1000;

// Returns true if every button is held for holdMs, with the hold starting within
// kBootHoldGraceMs of the call (buttons held through reset count from the call),
// calling progress(remainingMs) while held. Returns false if no hold starts in the
// window (or the board has no buttons), or if a button is released before the time
// elapses. When the hold completes, onHeld() runs first (do the maintenance action
// there), then progress(0) is drawn so no screen reports the action before it
// happened. Then waits (bounded) for all buttons to be released so the hold does
// not leak into normal input. Never waits for serial.
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
  if (board::buttonCount() == 0) return false;
  board::update();
  const uint32_t called = millis();
  while (!allHeld()) {
    if (millis() - called >= kBootHoldGraceMs) return false;
    delay(10);
    board::update();
  }
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
