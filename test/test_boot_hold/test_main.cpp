// holdAllButtonsAtBoot against a scripted board and the fake clock (delay() advances it).
#include <Arduino.h>
#include <unity.h>

#include "Log.h"
#include "../../lib/board/src/BootHold.h"

namespace {
uint8_t count = 1;
uint32_t pressFrom = UINT32_MAX;  // buttons read as pressed from this time ...
uint32_t pressUntil = UINT32_MAX;  // ... until (not including) this one
bool pressed = false;
}  // namespace

namespace board {
void update() { pressed = fakeMillis >= pressFrom && fakeMillis < pressUntil; }
uint8_t buttonCount() { return count; }
bool isPressed(uint8_t i) { return i < count && pressed; }
}  // namespace board

struct Run {
  bool result = false;
  int held = 0;
  uint32_t lastProgress = UINT32_MAX;
  uint32_t returnedAt = 0;
};

Run run(uint32_t holdMs) {
  Run r;
  r.result = holdAllButtonsAtBoot(
      holdMs, [&](uint32_t remaining) { r.lastProgress = remaining; }, [&] { r.held++; });
  r.returnedAt = fakeMillis;
  return r;
}

void setUp() {
  fakeMillis = 0;
  count = 1;
  pressFrom = pressUntil = UINT32_MAX;
  pressed = false;
}
void tearDown() {}

void test_no_press_continues_after_grace_window() {
  const Run r = run(3000);
  TEST_ASSERT_FALSE(r.result);
  TEST_ASSERT_EQUAL(0, r.held);
  TEST_ASSERT_TRUE(r.returnedAt >= kBootHoldGraceMs && r.returnedAt < kBootHoldGraceMs + 50);
}

void test_held_through_reset_completes() {  // Stick-S3: buttons held while the device resets
  pressFrom = 0;
  pressUntil = 3500;
  const Run r = run(3000);
  TEST_ASSERT_TRUE(r.result);
  TEST_ASSERT_EQUAL(1, r.held);
  TEST_ASSERT_EQUAL(0, r.lastProgress);  // progress(0) is drawn after onHeld
  TEST_ASSERT_TRUE(r.returnedAt >= 3500);  // waited for the release
}

void test_press_starting_inside_grace_window_completes() {  // strapping-pin BOOT pressed after reset
  pressFrom = kBootHoldGraceMs - 200;
  pressUntil = pressFrom + 5500;
  const Run r = run(5000);
  TEST_ASSERT_TRUE(r.result);
  TEST_ASSERT_EQUAL(1, r.held);
}

void test_press_after_grace_window_is_normal_input() {
  pressFrom = kBootHoldGraceMs + 100;
  const Run r = run(3000);
  TEST_ASSERT_FALSE(r.result);
  TEST_ASSERT_EQUAL(0, r.held);
  TEST_ASSERT_TRUE(r.returnedAt <= kBootHoldGraceMs + 50);
}

void test_release_before_hold_time_does_nothing() {
  pressFrom = 300;
  pressUntil = 2000;
  const Run r = run(3000);
  TEST_ASSERT_FALSE(r.result);
  TEST_ASSERT_EQUAL(0, r.held);
  TEST_ASSERT_TRUE(r.lastProgress > 0 && r.lastProgress != UINT32_MAX);
}

void test_board_without_buttons_does_not_wait() {
  count = 0;
  const Run r = run(3000);
  TEST_ASSERT_FALSE(r.result);
  TEST_ASSERT_EQUAL(0, r.returnedAt);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_no_press_continues_after_grace_window);
  RUN_TEST(test_held_through_reset_completes);
  RUN_TEST(test_press_starting_inside_grace_window_completes);
  RUN_TEST(test_press_after_grace_window_is_normal_input);
  RUN_TEST(test_release_before_hold_time_does_nothing);
  RUN_TEST(test_board_without_buttons_does_not_wait);
  return UNITY_END();
}
