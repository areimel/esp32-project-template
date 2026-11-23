// Host test: Controller gesture, playback-gating and abort behavior.
#include "Gesture.h"  // lib/input; named here so the dependency finder links it
#include "macro_controller_sim.h"
#include <unity.h>

// PLAY only when ready and the slot is assigned.
void test_play_only_when_ready_and_assigned() {
    Sim s(1000, singlesOnly());
    s.ready = false;
    s.step(10);
    s.tapA();
    s.tapB();
    TEST_ASSERT_TRUE(s.n() == 0);  // not ready: dropped
    s.ready = true;
    s.step(10);
    s.tapA();
    TEST_ASSERT_TRUE(s.n() == 1 && s.ev[0].type == CE::PLAY && s.ev[0].button == BTN_MAIN &&
           s.ev[0].gesture == G_SINGLE && s.ev[0].profile == 0);
    s.tapB();
    TEST_ASSERT_TRUE(s.n() == 2 && s.ev[1].button == BTN_SECONDARY);
}

// Unassigned slot emits nothing (single unassigned, hold assigned only).
void test_unassigned_slot_emits_nothing() {
    SlotMask m = noneAssigned();
    m.assigned[BTN_MAIN][G_HOLD] = true;
    Sim s(1000, m);
    s.step(10);
    s.tapA();
    s.tapB();
    TEST_ASSERT_TRUE(s.n() == 0);
    s.a = true;
    s.step(700);
    TEST_ASSERT_TRUE(s.n() == 1 && s.ev[0].gesture == G_HOLD);
    s.a = false;
    s.step(500);
    TEST_ASSERT_TRUE(s.n() == 1);
}

// Double gesture plays; single waits out the double window.
void test_double_plays_and_single_waits_for_window() {
    Sim s;
    s.step(10);
    s.tapA(50);
    s.tapA(50);
    TEST_ASSERT_TRUE(s.n() == 1 && s.ev[0].gesture == G_DOUBLE && s.ev[0].button == BTN_MAIN);
    s.clear();
    s.tapA(50);
    s.step(400);
    TEST_ASSERT_TRUE(s.n() == 1 && s.ev[0].gesture == G_SINGLE);
}

// Disabling single in the mask but keeping double: a lone tap plays nothing.
void test_lone_tap_plays_nothing_without_single_slot() {
    SlotMask m = noneAssigned();
    m.assigned[BTN_MAIN][G_DOUBLE] = true;
    Sim s(1000, m);
    s.step(10);
    s.tapA(50);
    s.step(500);
    TEST_ASSERT_TRUE(s.n() == 0);
}

// Readiness change discards a held button; a pre-connect hold cannot leak.
void test_readiness_change_discards_held_button() {
    Sim s;
    s.ready = false;
    s.step(10);
    s.a = true;
    s.step(1000);  // held for a long time while not ready
    TEST_ASSERT_TRUE(s.n() == 0);
    s.ready = true;
    s.step(1000);
    TEST_ASSERT_TRUE(s.n() == 0);  // still held after becoming ready: no hold event
    s.a = false;
    s.step(500);
    TEST_ASSERT_TRUE(s.n() == 0);  // release suppressed
    s.tapA();
    s.step(400);
    TEST_ASSERT_TRUE(s.n() == 1 && s.ev[0].gesture == G_SINGLE);
}

// Losing the link mid-hold discards pending gestures.
void test_link_loss_discards_pending_gestures() {
    Sim s;
    s.step(10);
    s.tapA(50);  // pending single (double assigned)
    s.ready = false;
    s.step(1000);
    TEST_ASSERT_TRUE(s.n() == 0);
}

// ABORT during playing; release afterwards emits nothing; no PLAY while playing.
void test_abort_while_playing_and_release_is_quiet() {
    Sim s;
    s.step(10);
    s.playing = true;
    s.step(10);
    s.a = true;
    s.step(100);
    TEST_ASSERT_TRUE(s.n() == 1 && s.ev[0].type == CE::ABORT);
    s.step(1000);  // held through a would-be hold; playing still true
    TEST_ASSERT_TRUE(s.n() == 1);
    s.playing = false;
    s.a = false;
    s.step(500);
    TEST_ASSERT_TRUE(s.n() == 1);  // release emits nothing
    s.tapB();
    s.step(400);
    TEST_ASSERT_TRUE(s.n() == 2 && s.ev[1].type == CE::PLAY);
}

// A short tap while playing aborts and does not also play.
void test_tap_while_playing_aborts_without_play() {
    Sim s(1000, singlesOnly());
    s.step(10);
    s.playing = true;
    s.tapB();
    TEST_ASSERT_TRUE(s.n() == 1 && s.ev[0].type == CE::ABORT);
    s.playing = false;
    s.step(500);
    TEST_ASSERT_TRUE(s.n() == 1);
}

// Button pressed before playback starts is not an abort edge (hold already fired PLAY, playback begins, release must be quiet).
void test_press_before_playback_is_not_abort_edge() {
    Sim s;
    s.step(10);
    s.a = true;
    s.step(700);
    TEST_ASSERT_TRUE(s.n() == 1 && s.ev[0].type == CE::PLAY);
    s.playing = true;
    s.step(100);
    TEST_ASSERT_TRUE(s.n() == 1);
    s.a = false;
    s.step(200);
    TEST_ASSERT_TRUE(s.n() == 1);
}

// Buttons held at begin() are blocked and cannot chord.
void test_buttons_held_at_begin_cannot_chord() {
    Controller c(25, 250, 650, 3);
    c.begin(true, true, 100, 0, allAssigned());
    uint32_t t = 100;
    for (int i = 0; i < 500; i++) {
      CE e = c.update(true, true, ++t, true, false);
      TEST_ASSERT_TRUE(e.type == CE::NONE);
    }
    TEST_ASSERT_TRUE(c.mode() == Mode::HOME);
}

void setUp() {}
void tearDown() {}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_play_only_when_ready_and_assigned);
  RUN_TEST(test_unassigned_slot_emits_nothing);
  RUN_TEST(test_double_plays_and_single_waits_for_window);
  RUN_TEST(test_lone_tap_plays_nothing_without_single_slot);
  RUN_TEST(test_readiness_change_discards_held_button);
  RUN_TEST(test_link_loss_discards_pending_gestures);
  RUN_TEST(test_abort_while_playing_and_release_is_quiet);
  RUN_TEST(test_tap_while_playing_aborts_without_play);
  RUN_TEST(test_press_before_playback_is_not_abort_edge);
  RUN_TEST(test_buttons_held_at_begin_cannot_chord);
  return UNITY_END();
}
