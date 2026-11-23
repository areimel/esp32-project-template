// Host test for MacroPlayer (pure logic; no Arduino or BLE).
#include "MacroPlayer.h"
#include <unity.h>
#include <cstdint>

static const Stroke kCombo[] = {{0x09, 0x06, 0}};  // GUI+C
static const Macro combo = {"COPY", kCombo, 1};
static const Stroke kText[] = {{0x02, 0x0B, 0}, {0, 0x08, 0}, {0, 0x28, 0}};
static const Macro text = {"HE", kText, 3};
static const Stroke kSeq[] = {{0, 0x04, 0}, {0, 0, 100}, {0, 0x05, 0}};
static const Macro seq = {"SEQ", kSeq, 3};
static const Stroke kTrail[] = {{0, 0x04, 0}, {0, 0, 50}};
static const Macro trail = {"TRAIL", kTrail, 2};
static const Macro empty = {"", nullptr, 0};

// combo: one stroke, active until the transport is idle again
void test_combo_active_until_transport_idle() {
  Stroke out{};
    MacroPlayer p;
    TEST_ASSERT_TRUE(!p.active() && p.total() == 0 && p.macro() == nullptr);
    TEST_ASSERT_TRUE(p.start(combo, 0));
    TEST_ASSERT_TRUE(p.active() && p.total() == 1 && p.sent() == 0 && p.macro() == &combo);
    TEST_ASSERT_TRUE(p.next(0, true, out));
    TEST_ASSERT_TRUE(out.mods == 0x09 && out.usage == 0x06);
    TEST_ASSERT_TRUE(p.sent() == 1);
    TEST_ASSERT_TRUE(!p.next(1, false, out) && p.active());  // transport busy: still active
    TEST_ASSERT_TRUE(!p.next(2, false, out) && p.active());
    TEST_ASSERT_TRUE(!p.next(30, true, out) && !p.active());  // idle: completes
    TEST_ASSERT_TRUE(p.sent() == 1);
}

// text: strokes only while transport idle
void test_text_strokes_only_while_idle() {
  Stroke out{};
    MacroPlayer p;
    TEST_ASSERT_TRUE(p.start(text, 0));
    TEST_ASSERT_TRUE(p.next(0, true, out) && out.mods == 0x02 && out.usage == 0x0B);
    TEST_ASSERT_TRUE(!p.next(1, false, out));
    TEST_ASSERT_TRUE(p.next(30, true, out) && out.usage == 0x08);
    TEST_ASSERT_TRUE(p.next(60, true, out) && out.usage == 0x28);
    TEST_ASSERT_TRUE(p.sent() == 3 && p.active());
    TEST_ASSERT_TRUE(!p.next(61, true, out) && !p.active());
}

// pause timing: starts when transport idle after previous stroke
void test_pause_starts_when_transport_idle() {
  Stroke out{};
    MacroPlayer p;
    TEST_ASSERT_TRUE(p.start(seq, 0));
    TEST_ASSERT_TRUE(p.next(0, true, out) && out.usage == 0x04);
    TEST_ASSERT_TRUE(!p.next(10, false, out));      // busy: pause not started
    TEST_ASSERT_TRUE(!p.next(40, true, out));       // pause starts at 40
    TEST_ASSERT_TRUE(p.sent() == 1);
    TEST_ASSERT_TRUE(!p.next(139, true, out));      // 99 ms
    TEST_ASSERT_TRUE(p.sent() == 1);
    TEST_ASSERT_TRUE(p.next(140, true, out) && out.usage == 0x05);
    TEST_ASSERT_TRUE(p.sent() == 3);
}

// pause waits for idle even if time has long passed
void test_pause_waits_for_idle_not_elapsed_time() {
  Stroke out{};
    MacroPlayer p;
    TEST_ASSERT_TRUE(p.start(seq, 0));
    TEST_ASSERT_TRUE(p.next(0, true, out));
    TEST_ASSERT_TRUE(!p.next(500, false, out));
    TEST_ASSERT_TRUE(!p.next(500, true, out));  // pause starts now, not at 0
    TEST_ASSERT_TRUE(!p.next(599, true, out));
    TEST_ASSERT_TRUE(p.next(600, true, out));
}

// wraparound during pause
void test_pause_across_millis_wraparound() {
  Stroke out{};
    MacroPlayer p;
    TEST_ASSERT_TRUE(p.start(seq, UINT32_MAX - 200));
    TEST_ASSERT_TRUE(p.next(UINT32_MAX - 200, true, out));
    TEST_ASSERT_TRUE(!p.next(UINT32_MAX - 50, true, out));  // pause starts
    TEST_ASSERT_TRUE(!p.next(UINT32_MAX, true, out));       // 50 ms
    TEST_ASSERT_TRUE(!p.next(48, true, out));               // 99 ms across the wrap
    TEST_ASSERT_TRUE(p.next(49, true, out) && out.usage == 0x05);
}

// trailing pause completes the macro in the same call
void test_trailing_pause_completes_macro() {
  Stroke out{};
    MacroPlayer p;
    TEST_ASSERT_TRUE(p.start(trail, 0));
    TEST_ASSERT_TRUE(p.next(0, true, out));
    TEST_ASSERT_TRUE(!p.next(1, true, out) && p.active());
    TEST_ASSERT_TRUE(!p.next(50, true, out) && p.active());
    TEST_ASSERT_TRUE(!p.next(51, true, out) && !p.active());
    TEST_ASSERT_TRUE(p.sent() == 2);
}

// abort mid-way stops output, also during a pause
void test_abort_stops_output_and_allows_restart() {
  Stroke out{};
    MacroPlayer p;
    TEST_ASSERT_TRUE(p.start(text, 0));
    TEST_ASSERT_TRUE(p.next(0, true, out));
    p.abort();
    TEST_ASSERT_TRUE(!p.active());
    TEST_ASSERT_TRUE(!p.next(30, true, out) && !p.next(60, true, out));
    TEST_ASSERT_TRUE(p.sent() == 1);
    MacroPlayer q;
    TEST_ASSERT_TRUE(q.start(seq, 0));
    TEST_ASSERT_TRUE(q.next(0, true, out));
    TEST_ASSERT_TRUE(!q.next(10, true, out));
    q.abort();
    TEST_ASSERT_TRUE(!q.next(1000, true, out) && !q.active());
    TEST_ASSERT_TRUE(q.start(combo, 0));  // can restart after abort
    TEST_ASSERT_TRUE(q.next(0, true, out) && out.usage == 0x06);
}

// start rejected when active or empty
void test_start_rejected_when_active_or_empty() {
  Stroke out{};
    MacroPlayer p;
    TEST_ASSERT_TRUE(!p.start(empty, 0) && !p.active());
    TEST_ASSERT_TRUE(p.start(combo, 0));
    TEST_ASSERT_TRUE(!p.start(text, 0));
    TEST_ASSERT_TRUE(p.macro() == &combo);
    TEST_ASSERT_TRUE(p.next(0, true, out));
    TEST_ASSERT_TRUE(!p.start(text, 0));  // still active until transport idle
    TEST_ASSERT_TRUE(!p.next(1, true, out));
    TEST_ASSERT_TRUE(p.start(text, 2));  // finished: new macro accepted
    TEST_ASSERT_TRUE(p.total() == 3 && p.sent() == 0);
}

void setUp() {}
void tearDown() {}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_combo_active_until_transport_idle);
  RUN_TEST(test_text_strokes_only_while_idle);
  RUN_TEST(test_pause_starts_when_transport_idle);
  RUN_TEST(test_pause_waits_for_idle_not_elapsed_time);
  RUN_TEST(test_pause_across_millis_wraparound);
  RUN_TEST(test_trailing_pause_completes_macro);
  RUN_TEST(test_abort_stops_output_and_allows_restart);
  RUN_TEST(test_start_rejected_when_active_or_empty);
  return UNITY_END();
}
