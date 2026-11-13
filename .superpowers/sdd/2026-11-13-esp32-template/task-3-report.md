# Task 3 report: lib/input, lib/store, lib/ble_hid

## Implemented
- git mv: Gesture.h -> lib/input/src; BleKeyboard.h/.cpp -> lib/ble_hid/src (class renamed MacroKeyboard -> BleKeyboard; BleKeyboard.cpp wrapped in `#if FEATURE_BLE_HID`); tests/ble/* -> test/stubs/.
- New lib/store/src/Settings.{h,cpp} (Preferences, read-write open per call, putU8 returns true without writing when unchanged).
- Deleted include/ProfileStore.h and src/ProfileStore.cpp.
- platformio.ini [env:native] build_flags gained -DFEATURE_BLE_HID=1.
- src/main.cpp untouched.

## Test conversion (old block -> test name)
Assert count preserved: gesture 79 -> 79, ble 77 -> 77 (assert( -> TEST_ASSERT_TRUE().
Gesture: noise/taps without assignments -> test_taps_without_assignments; hold -> test_hold_fires_once; release before threshold -> test_release_before_threshold_taps; double -> test_double_window; second-press hold -> test_second_press_hold_wins; reset -> test_reset_discards_pending; wrap -> test_clock_wraparound; two instances -> test_assignments_are_per_instance; cancel blocks -> test_cancel_drops_pending_single, test_cancel_drops_second_press, test_cancel_blocks_held_button, test_cancel_after_hold, test_cancel_idle_noop; setAssigned -> test_set_assigned_between_gestures, test_set_assigned_mid_hold, test_disable_double_resolves_pending.
BLE: test_security_descriptor_and_pacing, test_invalid_strokes_and_modifier_chord, test_configurable_keystroke_ms, test_failed_reports_retry, test_reconnect_drops_stroke, test_reconnect_while_held, test_release_all, test_clear_bonds, test_unencrypted_dropped_battery_clamp, test_millis_wraparound.

## Test output
`pio test -e native -f test_gesture -f test_ble`: 26 test cases: 26 succeeded (16 gesture, 10 ble).

## Build
`pio run -e sticks3 -e esp32c3`: both SUCCESS.

## Concerns
- main.cpp does not include BleKeyboard.h or Settings.h, so LDF did not compile BleKeyboard.cpp or Settings.cpp for device envs; the device builds do not prove those compile against real NimBLE/Preferences. Tasks 6/7 will exercise them.
- The BLE manufacturer string in the test stub assertion ("Stick-S3 Macro") is unchanged.
