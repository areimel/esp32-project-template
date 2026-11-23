#include "Gesture.h"
#include <unity.h>
#include <cstdint>

static const GestureEvent NONE = GestureEvent::NONE, TAP = GestureEvent::SINGLE,
                          DOUBLE = GestureEvent::DOUBLE, HOLD = GestureEvent::HOLD;

// Times supplied to press/release denote raw electrical edges; debounce is 25ms.
static void press(Gesture& g, uint32_t t) {
  TEST_ASSERT_TRUE(g.update(true, t) == NONE);
  TEST_ASSERT_TRUE(g.update(true, t + 25) == NONE);
}
static GestureEvent release(Gesture& g, uint32_t t) {
  TEST_ASSERT_TRUE(g.update(false, t) == NONE);
  return g.update(false, t + 25);
}
static Gesture make(bool dbl, bool hold) {
  Gesture g(25, 250, 650);
  g.setAssigned(dbl, hold);
  return g;
}

void test_taps_without_assignments() {
  Gesture g = make(false, false);
  TEST_ASSERT_TRUE(g.update(true, 10) == NONE);
  TEST_ASSERT_TRUE(g.update(false, 20) == NONE); // noise never becomes a press
  TEST_ASSERT_TRUE(g.update(false, 45) == NONE);
  TEST_ASSERT_TRUE(!g.pressed());
  press(g, 100);
  TEST_ASSERT_TRUE(g.pressed());
  TEST_ASSERT_TRUE(release(g, 150) == TAP);
  press(g, 180);
  TEST_ASSERT_TRUE(release(g, 230) == TAP); // no double assignment: two immediate taps
  press(g, 300);
  TEST_ASSERT_TRUE(g.update(true, 2000) == NONE);
  TEST_ASSERT_TRUE(!g.holdFired());
  TEST_ASSERT_TRUE(release(g, 2100) == TAP); // unassigned hold is ordinary tap
}

void test_hold_fires_once() {
  Gesture g = make(false, true);
  press(g, 10);
  TEST_ASSERT_TRUE(g.update(true, 684) == NONE);
  TEST_ASSERT_TRUE(!g.holdFired());
  TEST_ASSERT_TRUE(g.update(true, 685) == HOLD);
  TEST_ASSERT_TRUE(g.holdFired());
  TEST_ASSERT_TRUE(g.update(true, 2000) == NONE);
  TEST_ASSERT_TRUE(release(g, 2100) == NONE);
  TEST_ASSERT_TRUE(!g.holdFired());
  press(g, 2200);
  TEST_ASSERT_TRUE(release(g, 2300) == TAP);
}

void test_release_before_threshold_taps() {
  Gesture g = make(false, true);
  press(g, 10);
  TEST_ASSERT_TRUE(g.update(false, 680) == NONE);
  TEST_ASSERT_TRUE(g.update(false, 685) == NONE); // release before threshold cannot hold
  TEST_ASSERT_TRUE(g.update(false, 705) == TAP);
}

void test_double_window() {
  Gesture g = make(true, true);
  press(g, 10);
  TEST_ASSERT_TRUE(release(g, 60) == NONE);
  TEST_ASSERT_TRUE(g.update(false, 334) == NONE);
  TEST_ASSERT_TRUE(g.update(false, 335) == TAP);
  TEST_ASSERT_TRUE(g.update(false, 500) == NONE);
  press(g, 600);
  TEST_ASSERT_TRUE(release(g, 650) == NONE);
  press(g, 700);
  TEST_ASSERT_TRUE(release(g, 800) == DOUBLE);
  TEST_ASSERT_TRUE(g.update(false, 1100) == NONE);
}

void test_second_press_hold_wins() {
  Gesture g = make(true, true);
  press(g, 10);
  TEST_ASSERT_TRUE(release(g, 60) == NONE);
  press(g, 100);
  TEST_ASSERT_TRUE(g.update(true, 400) == NONE); // second press locks arbitration
  TEST_ASSERT_TRUE(g.update(true, 775) == HOLD); // second hold wins
  TEST_ASSERT_TRUE(release(g, 900) == NONE);
  TEST_ASSERT_TRUE(g.update(false, 1500) == NONE);
}

void test_reset_discards_pending() {
  Gesture g = make(true, true);
  press(g, 10);
  TEST_ASSERT_TRUE(release(g, 60) == NONE);
  g.reset(false, 100); // pending action discarded on disconnect
  TEST_ASSERT_TRUE(g.update(false, 500) == NONE);
  g.reset(true, 600); // held button across reconnect must release first
  TEST_ASSERT_TRUE(g.pressed());
  TEST_ASSERT_TRUE(g.update(true, 2000) == NONE);
  TEST_ASSERT_TRUE(release(g, 2100) == NONE);
  press(g, 2200);
  TEST_ASSERT_TRUE(g.update(true, 2875) == HOLD);
  TEST_ASSERT_TRUE(release(g, 3000) == NONE);
}

void test_clock_wraparound() {
  Gesture g = make(true, true);
  const uint32_t start = UINT32_MAX - 100;
  g.reset(false, start);
  press(g, start + 10);
  TEST_ASSERT_TRUE(release(g, start + 60) == NONE);
  TEST_ASSERT_TRUE(g.update(false, start + 335) == TAP); // wrapped unsigned clock
  press(g, start + 400);
  TEST_ASSERT_TRUE(g.update(true, start + 1075) == HOLD);
}

void test_assignments_are_per_instance() {
  Gesture a = make(false, false), b = make(false, true);
  press(a, 10);
  press(b, 10);
  TEST_ASSERT_TRUE(release(a, 100) == TAP);
  TEST_ASSERT_TRUE(b.update(true, 685) == HOLD);
  TEST_ASSERT_TRUE(release(b, 700) == NONE);
}

// cancel(): drops a pending single so it never fires
void test_cancel_drops_pending_single() {
  Gesture g = make(true, true);
  press(g, 10);
  TEST_ASSERT_TRUE(release(g, 60) == NONE); // pending
  g.cancel();
  TEST_ASSERT_TRUE(g.update(false, 400) == NONE);
  TEST_ASSERT_TRUE(g.update(false, 2000) == NONE);
  press(g, 3000);
  TEST_ASSERT_TRUE(release(g, 3050) == NONE); // new pending works normally
  TEST_ASSERT_TRUE(g.update(false, 3400) == TAP);
}

// cancel(): drops a second press in progress (no double on release)
void test_cancel_drops_second_press() {
  Gesture g = make(true, true);
  press(g, 10);
  TEST_ASSERT_TRUE(release(g, 60) == NONE);
  press(g, 100); // second press
  g.cancel();
  TEST_ASSERT_TRUE(release(g, 200) == NONE);
  TEST_ASSERT_TRUE(g.update(false, 1000) == NONE);
}

// cancel(): blocks a held button until release; no tap, no hold
void test_cancel_blocks_held_button() {
  Gesture g = make(false, true);
  press(g, 10);
  g.cancel();
  TEST_ASSERT_TRUE(g.pressed());
  TEST_ASSERT_TRUE(g.update(true, 2000) == NONE); // no hold after cancel
  TEST_ASSERT_TRUE(release(g, 2100) == NONE);
  press(g, 2200);
  TEST_ASSERT_TRUE(release(g, 2300) == TAP); // fully recovered
}

// cancel() after a hold fired: holdFired cleared, release silent
void test_cancel_after_hold() {
  Gesture g = make(false, true);
  press(g, 10);
  TEST_ASSERT_TRUE(g.update(true, 685) == HOLD);
  g.cancel();
  TEST_ASSERT_TRUE(!g.holdFired());
  TEST_ASSERT_TRUE(release(g, 800) == NONE);
  press(g, 900);
  TEST_ASSERT_TRUE(release(g, 950) == TAP);
}

// cancel() while idle is a no-op
void test_cancel_idle_noop() {
  Gesture g = make(false, false);
  g.cancel();
  press(g, 10);
  TEST_ASSERT_TRUE(release(g, 60) == TAP);
}

// setAssigned between gestures takes effect immediately
void test_set_assigned_between_gestures() {
  Gesture g = make(false, false);
  press(g, 10);
  TEST_ASSERT_TRUE(release(g, 60) == TAP); // immediate single
  g.setAssigned(true, false);
  press(g, 200);
  TEST_ASSERT_TRUE(release(g, 250) == NONE); // now waits for double window
  TEST_ASSERT_TRUE(g.update(false, 525) == TAP);
  g.setAssigned(false, true);
  press(g, 600);
  TEST_ASSERT_TRUE(g.update(true, 1275) == HOLD);
  TEST_ASSERT_TRUE(release(g, 1400) == NONE);
  g.setAssigned(false, false);
  press(g, 1500);
  TEST_ASSERT_TRUE(g.update(true, 3000) == NONE);
  TEST_ASSERT_TRUE(release(g, 3100) == TAP);
}

// setAssigned mid-gesture is safe: disabling hold while held keeps release silent
void test_set_assigned_mid_hold() {
  Gesture g = make(false, true);
  press(g, 10);
  TEST_ASSERT_TRUE(g.update(true, 685) == HOLD);
  g.setAssigned(false, false);
  TEST_ASSERT_TRUE(release(g, 800) == NONE);
  TEST_ASSERT_TRUE(!g.holdFired());
}

// disabling double while single is pending: the pending single still resolves
void test_disable_double_resolves_pending() {
  Gesture g = make(true, false);
  press(g, 10);
  TEST_ASSERT_TRUE(release(g, 60) == NONE);
  g.setAssigned(false, false);
  TEST_ASSERT_TRUE(g.update(false, 335) == TAP);
  TEST_ASSERT_TRUE(g.update(false, 1000) == NONE);
}

void setUp() {}
void tearDown() {}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_taps_without_assignments);
  RUN_TEST(test_hold_fires_once);
  RUN_TEST(test_release_before_threshold_taps);
  RUN_TEST(test_double_window);
  RUN_TEST(test_second_press_hold_wins);
  RUN_TEST(test_reset_discards_pending);
  RUN_TEST(test_clock_wraparound);
  RUN_TEST(test_assignments_are_per_instance);
  RUN_TEST(test_cancel_drops_pending_single);
  RUN_TEST(test_cancel_drops_second_press);
  RUN_TEST(test_cancel_blocks_held_button);
  RUN_TEST(test_cancel_after_hold);
  RUN_TEST(test_cancel_idle_noop);
  RUN_TEST(test_set_assigned_between_gestures);
  RUN_TEST(test_set_assigned_mid_hold);
  RUN_TEST(test_disable_double_resolves_pending);
  return UNITY_END();
}
