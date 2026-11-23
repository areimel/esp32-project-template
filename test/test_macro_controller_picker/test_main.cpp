// Host test: Controller chord and profile-picker behavior.
#include "Gesture.h"  // lib/input; named here so the dependency finder links it
#include "macro_controller_sim.h"
#include <unity.h>

// Chord opens picker when connected; releases emit nothing.
void test_chord_opens_picker_when_connected() {
    Sim s;
    s.step(50);
    s.chord();
    TEST_ASSERT_TRUE(s.n() == 1 && s.ev[0].type == CE::PICKER_OPENED);
    TEST_ASSERT_TRUE(s.ev[0].profile == 0);
    TEST_ASSERT_TRUE(s.c.mode() == Mode::PICKER && s.c.highlighted() == 0);
    s.step(500);
    TEST_ASSERT_TRUE(s.n() == 1);
}

// Chord opens picker when disconnected; highlight starts at active profile.
void test_chord_opens_picker_when_disconnected() {
    Sim s(1000, allAssigned(), 4, 2);
    s.ready = false;
    s.step(50);
    s.chord();
    TEST_ASSERT_TRUE(s.n() == 1 && s.ev[0].type == CE::PICKER_OPENED);
    TEST_ASSERT_TRUE(s.ev[0].profile == 2 && s.c.highlighted() == 2);
    s.step(500);
    TEST_ASSERT_TRUE(s.n() == 1);
}

// Staggered chord (A down, B 100 ms later) with A double+hold assigned: no stray single.
void test_staggered_chord_a_first_no_stray_single() {
    Sim s;
    s.step(10);
    s.chord(100);
    s.step(2000);
    TEST_ASSERT_TRUE(s.n() == 1 && s.ev[0].type == CE::PICKER_OPENED);
}

// Staggered chord, B first.
void test_staggered_chord_b_first() {
    Sim s;
    s.step(10);
    s.b = true;
    s.step(120);
    s.a = true;
    s.step(100);
    s.a = s.b = false;
    s.step(2000);
    TEST_ASSERT_TRUE(s.n() == 1 && s.ev[0].type == CE::PICKER_OPENED);
}

// Chord beats a pending single of the same button (tap A, then chord within window).
void test_chord_beats_pending_single() {
    Sim s;
    s.tapA(40);  // pending: double assigned; ~60 ms since release
    s.a = true;
    s.step(10);
    s.b = true;
    s.step(100);
    s.a = s.b = false;
    s.step(2000);
    TEST_ASSERT_TRUE(s.n() == 1 && s.ev[0].type == CE::PICKER_OPENED);
}

// No chord once a hold already fired: B behaves normally.
void test_no_chord_after_hold_fired() {
    Sim s;
    s.step(10);
    s.a = true;
    s.step(700);
    TEST_ASSERT_TRUE(s.n() == 1 && s.ev[0].type == CE::PLAY && s.ev[0].button == BTN_MAIN &&
           s.ev[0].gesture == G_HOLD);
    s.clear();
    s.b = true;
    s.step(50);
    s.b = false;
    s.step(400);  // B single (double assigned -> waits for window)
    TEST_ASSERT_TRUE(s.n() == 1 && s.ev[0].type == CE::PLAY && s.ev[0].button == BTN_SECONDARY &&
           s.ev[0].gesture == G_SINGLE);
    s.a = false;
    s.step(500);
    TEST_ASSERT_TRUE(s.n() == 1 && s.c.mode() == Mode::HOME);
}

// Picker navigation wraps; select returns to HOME with the new profile.
void test_picker_navigation_wraps_and_select_returns_home() {
    Sim s(1000, allAssigned(), 3, 0);
    s.step(10);
    s.chord();
    s.clear();
    s.tapB();
    TEST_ASSERT_TRUE(s.n() == 1 && s.ev[0].type == CE::PICKER_MOVED && s.ev[0].profile == 1);
    s.tapB();
    TEST_ASSERT_TRUE(s.n() == 2 && s.ev[1].profile == 2);
    s.tapB();
    TEST_ASSERT_TRUE(s.n() == 3 && s.ev[2].profile == 0 && s.c.highlighted() == 0);
    s.tapB();
    s.tapB();  // highlight 2
    s.clear();
    s.tapA();
    TEST_ASSERT_TRUE(s.n() == 1 && s.ev[0].type == CE::PICKER_SELECTED && s.ev[0].profile == 2);
    TEST_ASSERT_TRUE(s.c.mode() == Mode::HOME && s.c.activeProfile() == 2);
    s.c.setActiveProfile(2, singlesOnly());
    s.mask = singlesOnly();
    s.clear();
    s.tapA();  // new mask: double unassigned -> immediate single, PLAY of profile 2
    TEST_ASSERT_TRUE(s.n() == 1 && s.ev[0].type == CE::PLAY && s.ev[0].profile == 2 &&
           s.ev[0].button == BTN_MAIN && s.ev[0].gesture == G_SINGLE);
}

// Picker select with link not ready still works.
void test_picker_select_works_when_link_not_ready() {
    Sim s(1000, allAssigned(), 3, 0);
    s.ready = false;
    s.step(10);
    s.chord();
    s.clear();
    s.tapB();
    s.tapA();
    TEST_ASSERT_TRUE(s.n() == 2 && s.ev[1].type == CE::PICKER_SELECTED && s.ev[1].profile == 1);
}

// Picker taps are instant even when the profile has double+hold: tap A with no wait.
void test_picker_taps_are_instant() {
    Sim s;
    s.step(10);
    s.chord();
    s.clear();
    s.b = true;
    s.step(40);
    s.b = false;
    s.step(30);  // far shorter than double window
    TEST_ASSERT_TRUE(s.n() == 1 && s.ev[0].type == CE::PICKER_MOVED);
}

// Holding in picker does not fire a hold; release acts as tap.
void test_holding_in_picker_acts_as_tap_on_release() {
    Sim s;
    s.step(10);
    s.chord();
    s.clear();
    s.b = true;
    s.step(1500);
    TEST_ASSERT_TRUE(s.n() == 0);
    s.b = false;
    s.step(60);
    TEST_ASSERT_TRUE(s.n() == 1 && s.ev[0].type == CE::PICKER_MOVED);
}

// Chord in picker cancels; active unchanged; releases emit nothing.
void test_chord_in_picker_cancels() {
    Sim s(1000, allAssigned(), 3, 1);
    s.step(10);
    s.chord();
    s.tapB();
    TEST_ASSERT_TRUE(s.c.highlighted() == 2);
    s.clear();
    s.chord();
    TEST_ASSERT_TRUE(s.n() == 1 && s.ev[0].type == CE::PICKER_CANCELLED);
    TEST_ASSERT_TRUE(s.c.mode() == Mode::HOME && s.c.activeProfile() == 1);
    s.step(1000);
    TEST_ASSERT_TRUE(s.n() == 1);
}

// Timeout cancels; any press refreshes it.
void test_picker_timeout_cancels_and_presses_refresh() {
    Sim s(1000, allAssigned(), 3, 1);
    s.step(10);
    s.chord();  // opens ~ (t) ; chord ends with 100 ms idle
    s.clear();
    s.step(9000);
    TEST_ASSERT_TRUE(s.n() == 0 && s.c.mode() == Mode::PICKER);
    s.tapB();  // refresh
    s.clear();
    s.step(9800);
    TEST_ASSERT_TRUE(s.n() == 0 && s.c.mode() == Mode::PICKER);
    s.step(400);
    TEST_ASSERT_TRUE(s.n() == 1 && s.ev[0].type == CE::PICKER_CANCELLED);
    TEST_ASSERT_TRUE(s.c.mode() == Mode::HOME && s.c.activeProfile() == 1);
}

// Exact timeout boundary measured from the opening tick.
void test_picker_timeout_exact_boundary() {
    Sim s;
    s.step(10);
    s.a = true;
    s.b = true;
    s.step(200);
    TEST_ASSERT_TRUE(s.n() == 1);
    const uint32_t opened = s.t;  // may be later than the open tick; bound it
    (void)opened;
    s.a = s.b = false;
    s.step(9000);
    TEST_ASSERT_TRUE(s.n() == 1);
    s.step(1500);
    TEST_ASSERT_TRUE(s.n() == 2 && s.ev[1].type == CE::PICKER_CANCELLED);
    s.step(200);
    TEST_ASSERT_TRUE(s.n() == 2);
}

// Timeout across millis() wraparound.
void test_picker_timeout_across_millis_wraparound() {
    Sim s(UINT32_MAX - 3000);
    s.step(10);
    s.chord();
    s.clear();
    s.step(9000);
    TEST_ASSERT_TRUE(s.n() == 0 && s.c.mode() == Mode::PICKER);
    s.step(1500);
    TEST_ASSERT_TRUE(s.n() == 1 && s.ev[0].type == CE::PICKER_CANCELLED);
}

// Timeout while a button is held: release afterwards emits nothing.
void test_picker_timeout_while_button_held() {
    Sim s;
    s.step(10);
    s.chord();
    s.clear();
    s.step(5000);
    s.b = true;
    s.step(10100);  // edge refreshed activity; hold for >10 s
    TEST_ASSERT_TRUE(s.n() == 1 && s.ev[0].type == CE::PICKER_CANCELLED);
    s.b = false;
    s.step(500);
    TEST_ASSERT_TRUE(s.n() == 1);
}

void setUp() {}
void tearDown() {}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_chord_opens_picker_when_connected);
  RUN_TEST(test_chord_opens_picker_when_disconnected);
  RUN_TEST(test_staggered_chord_a_first_no_stray_single);
  RUN_TEST(test_staggered_chord_b_first);
  RUN_TEST(test_chord_beats_pending_single);
  RUN_TEST(test_no_chord_after_hold_fired);
  RUN_TEST(test_picker_navigation_wraps_and_select_returns_home);
  RUN_TEST(test_picker_select_works_when_link_not_ready);
  RUN_TEST(test_picker_taps_are_instant);
  RUN_TEST(test_holding_in_picker_acts_as_tap_on_release);
  RUN_TEST(test_chord_in_picker_cancels);
  RUN_TEST(test_picker_timeout_cancels_and_presses_refresh);
  RUN_TEST(test_picker_timeout_exact_boundary);
  RUN_TEST(test_picker_timeout_across_millis_wraparound);
  RUN_TEST(test_picker_timeout_while_button_held);
  return UNITY_END();
}
