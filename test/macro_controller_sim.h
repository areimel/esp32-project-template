#pragma once
// Shared fixtures for the Controller host tests: slot masks and a 1 ms tick simulator.
#include "Controller.h"
#include <cstdint>
#include <vector>

typedef ControllerEvent CE;

static inline SlotMask allAssigned() {
  SlotMask m;
  for (int b = 0; b < BUTTON_COUNT; b++)
    for (int g = 0; g < GESTURE_COUNT; g++) m.assigned[b][g] = true;
  return m;
}
static inline SlotMask noneAssigned() {
  SlotMask m = allAssigned();
  for (int b = 0; b < BUTTON_COUNT; b++)
    for (int g = 0; g < GESTURE_COUNT; g++) m.assigned[b][g] = false;
  return m;
}
static inline SlotMask singlesOnly() {
  SlotMask m = noneAssigned();
  m.assigned[BTN_MAIN][G_SINGLE] = m.assigned[BTN_SECONDARY][G_SINGLE] = true;
  return m;
}

// 1 ms tick simulator; collects every non-NONE event.
struct Sim {
  Controller c;
  bool a = false, b = false, ready = true, playing = false;
  uint32_t t;
  std::vector<CE> ev;
  SlotMask mask;
  explicit Sim(uint32_t start = 1000, const SlotMask& m = allAssigned(),
               uint8_t profiles = 3, uint8_t active = 0)
      : c(25, 250, 650, profiles), t(start), mask(m) {
    c.begin(false, false, t, active, m);
  }
  void step(uint32_t ms) {
    for (uint32_t i = 0; i < ms; i++) {
      t++;
      CE e = c.update(a, b, t, ready, playing);
      if (e.type != CE::NONE) ev.push_back(e);
    }
  }
  void tapA(uint32_t holdMs = 50) { a = true; step(holdMs); a = false; step(60); }
  void tapB(uint32_t holdMs = 50) { b = true; step(holdMs); b = false; step(60); }
  void chord(uint32_t gap = 0) {
    a = true;
    step(gap ? gap : 1);
    b = true;
    step(100);
    a = b = false;
    step(100);
  }
  size_t n() const { return ev.size(); }
  void clear() { ev.clear(); }
};
