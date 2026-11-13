#pragma once
#include <stdint.h>

// Periodic gate for polling loops. Wrap-safe for millis() rollover.
class Every {
 public:
  explicit Every(uint32_t periodMs) : period_(periodMs) {}

  // Next due time is measured from the actual call time, so late calls drift and never burst.
  // True on the first call, then once per period.
  bool due(uint32_t now) {
    if (started_ && uint32_t(now - last_) < period_) return false;
    started_ = true;
    last_ = now;
    return true;
  }

 private:
  uint32_t period_;
  uint32_t last_ = 0;
  bool started_ = false;
};
