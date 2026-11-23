// Compile the production transport against controllable BLE/clock substitutes.
#include "BleKeyboard.h"
#include <Arduino.h>
#include <algorithm>
#include <unity.h>

struct Fixture {
  BleKeyboard keyboard;
  NimBLEConnInfo peer;
  explicit Fixture(uint16_t keystrokeMs = 25) {
    fakeMillis = 100;
    keyboard.begin("Test macro", keystrokeMs);
  }
  ~Fixture() { delete NimBLEHIDDevice::last; }
  NimBLECharacteristic& input() { return NimBLEHIDDevice::last->input; }
  void connect() { NimBLEDevice::server.callbacks->onConnect(&NimBLEDevice::server, peer); }
  void encrypt(bool value = true) {
    peer.encrypted = value;
    NimBLEDevice::server.callbacks->onAuthenticationComplete(peer);
  }
  void subscribe(uint16_t value = 1) { input().callbacks->onSubscribe(&input(), peer, value); }
  void disconnect() {
    NimBLEDevice::server.callbacks->onDisconnect(&NimBLEDevice::server, peer, 0);
  }
  void tick(uint32_t t) { fakeMillis = t; keyboard.update(t); }
  void ready() { connect(); encrypt(); subscribe(); tick(fakeMillis); }
  void expect(size_t index, uint8_t mods, uint8_t usage) {
    TEST_ASSERT_TRUE(index < input().reports.size());
    TEST_ASSERT_TRUE(input().reports[index] == std::vector<uint8_t>({mods, 0, usage, 0, 0, 0, 0, 0}));
    TEST_ASSERT_TRUE(input().handles[index] == peer.handle);
  }
  size_t count() { return input().reports.size(); }
};

void test_security_descriptor_and_pacing() {  // // security setup, descriptor, readiness gating, baseline, press/hold/gap
  Fixture f;
  TEST_ASSERT_TRUE(NimBLEDevice::bonding && NimBLEDevice::secureConnections);
  TEST_ASSERT_TRUE(NimBLEDevice::server.autoAdvertise && NimBLEDevice::advertising.started);
  TEST_ASSERT_TRUE(NimBLEHIDDevice::last->manufacturer == "ESP32 Starter Template");
  const std::vector<uint8_t>& map = NimBLEHIDDevice::last->reportMap;
  const std::vector<uint8_t> keyArray = {0x15, 0x00, 0x26, 0xE7, 0x00,
                                         0x05, 0x07, 0x19, 0x00, 0x29, 0xE7, 0x81, 0x00, 0xC0};
  TEST_ASSERT_TRUE(map.size() >= keyArray.size());
  TEST_ASSERT_TRUE(std::equal(keyArray.begin(), keyArray.end(), map.end() - keyArray.size()));
  TEST_ASSERT_TRUE(!f.keyboard.connected() && !f.keyboard.ready() && f.keyboard.idle());
  TEST_ASSERT_TRUE(!f.keyboard.send(0, 0x4f));
  f.connect();
  TEST_ASSERT_TRUE(f.keyboard.connected() && !f.keyboard.ready());
  f.subscribe();
  TEST_ASSERT_TRUE(!f.keyboard.ready());
  TEST_ASSERT_TRUE(!f.keyboard.send(0, 0x4f));
  f.encrypt();
  TEST_ASSERT_TRUE(f.keyboard.ready());
  f.tick(100);
  TEST_ASSERT_TRUE(f.count() == 1);
  f.expect(0, 0, 0);  // baseline
  f.tick(110);
  TEST_ASSERT_TRUE(f.count() == 1);  // baseline only once
  TEST_ASSERT_TRUE(f.keyboard.send(0x09, 0x06));
  TEST_ASSERT_TRUE(!f.keyboard.idle());
  TEST_ASSERT_TRUE(f.count() == 1);  // notify happens in update()
  TEST_ASSERT_TRUE(!f.keyboard.send(0, 0x07));  // busy
  f.tick(110);
  f.expect(1, 0x09, 0x06);  // modifier byte present
  f.tick(134);
  TEST_ASSERT_TRUE(f.count() == 2);
  f.tick(135);
  f.expect(2, 0, 0);  // explicit release after hold
  TEST_ASSERT_TRUE(!f.keyboard.idle());
  f.tick(159);
  TEST_ASSERT_TRUE(!f.keyboard.idle() && !f.keyboard.send(0, 0x07));
  f.tick(160);
  TEST_ASSERT_TRUE(f.keyboard.idle());
  TEST_ASSERT_TRUE(f.keyboard.send(0, 0x07));
  f.tick(160);
  f.expect(3, 0, 0x07);
}

void test_invalid_strokes_and_modifier_chord() {  // // invalid strokes and modifier-only chord
  Fixture f;
  f.ready();
  TEST_ASSERT_TRUE(!f.keyboard.send(0, 0));
  TEST_ASSERT_TRUE(!f.keyboard.send(0, 0xE8));
  TEST_ASSERT_TRUE(!f.keyboard.send(0x01, 0xE8));
  TEST_ASSERT_TRUE(f.keyboard.idle());
  TEST_ASSERT_TRUE(f.keyboard.send(0x01, 0xE7));  // max usage allowed
  f.tick(100);
  f.expect(1, 0x01, 0xE7);
  f.tick(125); f.tick(150);
  TEST_ASSERT_TRUE(f.keyboard.idle());
  TEST_ASSERT_TRUE(f.keyboard.send(0x0A, 0));  // modifier-only chord
  f.tick(150);
  f.expect(3, 0x0A, 0);
}

void test_configurable_keystroke_ms() {  // // configurable keystrokeMs
  Fixture f(40);
  f.ready();
  TEST_ASSERT_TRUE(f.keyboard.send(0, 0x04));
  f.tick(100);
  f.tick(139);
  TEST_ASSERT_TRUE(f.count() == 2);
  f.tick(140);
  f.expect(2, 0, 0);
  f.tick(179);
  TEST_ASSERT_TRUE(!f.keyboard.idle());
  f.tick(180);
  TEST_ASSERT_TRUE(f.keyboard.idle());
}

void test_failed_reports_retry() {  // // failed press retries; failed release retries and never advances
  Fixture f;
  f.ready();
  f.input().failCount = 1;
  TEST_ASSERT_TRUE(f.keyboard.send(0, 0x04));
  f.tick(100);
  TEST_ASSERT_TRUE(f.count() == 1 && !f.keyboard.idle());
  f.tick(101);
  f.expect(1, 0, 0x04);  // press succeeded on retry
  f.input().failCount = 2;
  f.tick(126);  // 101 + 25
  f.tick(127);
  TEST_ASSERT_TRUE(f.count() == 2);
  TEST_ASSERT_TRUE(!f.keyboard.idle() && !f.keyboard.send(0, 0x05));
  f.tick(128);
  f.expect(2, 0, 0);
  f.tick(152);
  TEST_ASSERT_TRUE(!f.keyboard.idle());
  f.tick(153);
  TEST_ASSERT_TRUE(f.keyboard.idle());
}

void test_reconnect_drops_stroke() {  // // reconnect between accept and send drops the stroke
  Fixture f;
  f.ready();
  TEST_ASSERT_TRUE(f.keyboard.send(0, 0x04));
  f.disconnect();
  TEST_ASSERT_TRUE(!f.keyboard.ready() && !f.keyboard.send(0, 0x05));
  // Full reconnect between loop iterations: generation still clears work.
  f.connect(); f.encrypt(); f.subscribe(); f.tick(125);
  TEST_ASSERT_TRUE(f.count() == 2);
  f.expect(1, 0, 0);  // only the new baseline, no stale press
  TEST_ASSERT_TRUE(f.keyboard.idle());
  f.tick(200);
  TEST_ASSERT_TRUE(f.count() == 2);
  // Resubscribe also invalidates an accepted stroke.
  TEST_ASSERT_TRUE(f.keyboard.send(0, 0x04));
  f.subscribe(0);
  f.subscribe(1);
  f.tick(201);
  TEST_ASSERT_TRUE(f.count() == 3);
  f.expect(2, 0, 0);
  TEST_ASSERT_TRUE(f.keyboard.idle());
}

void test_reconnect_while_held() {  // // reconnect while key held: stale release is not replayed on new link
  Fixture f;
  f.ready();
  TEST_ASSERT_TRUE(f.keyboard.send(0x02, 0x04));
  f.tick(100);
  TEST_ASSERT_TRUE(f.count() == 2);
  f.disconnect();
  f.connect(); f.encrypt(); f.subscribe(); f.tick(130);
  TEST_ASSERT_TRUE(f.count() == 3);
  f.expect(2, 0, 0);  // baseline neutral report
  f.tick(300);
  TEST_ASSERT_TRUE(f.count() == 3 && f.keyboard.idle());
}

void test_release_all() {  // // releaseAll mid-press and before press
  Fixture f;
  f.ready();
  TEST_ASSERT_TRUE(f.keyboard.send(0x01, 0x06));
  f.tick(100);
  f.expect(1, 0x01, 0x06);
  fakeMillis = 105;
  f.keyboard.releaseAll();
  f.expect(2, 0, 0);  // released immediately, before hold elapsed
  TEST_ASSERT_TRUE(!f.keyboard.idle());
  f.tick(130);
  TEST_ASSERT_TRUE(f.keyboard.idle());
  // Accepted but unsent stroke is dropped.
  TEST_ASSERT_TRUE(f.keyboard.send(0, 0x04));
  f.keyboard.releaseAll();
  TEST_ASSERT_TRUE(f.keyboard.idle());
  f.tick(200);
  TEST_ASSERT_TRUE(f.count() == 3);
  // Failed immediate release is retried on the next update without waiting.
  TEST_ASSERT_TRUE(f.keyboard.send(0, 0x04));
  f.tick(200);
  f.expect(3, 0, 0x04);
  f.input().failCount = 1;
  f.keyboard.releaseAll();
  TEST_ASSERT_TRUE(f.count() == 4 && !f.keyboard.idle());
  f.tick(201);
  f.expect(4, 0, 0);
  // Nothing to do when idle.
  f.tick(300);
  f.keyboard.releaseAll();
  TEST_ASSERT_TRUE(f.count() == 5);
}

void test_clear_bonds() {  // // clearBonds releases, invalidates, disconnects
  Fixture f;
  f.ready();
  TEST_ASSERT_TRUE(f.keyboard.send(0, 0x04));
  f.tick(100);
  f.keyboard.clearBonds();
  f.expect(2, 0, 0);
  TEST_ASSERT_TRUE(!f.keyboard.ready() && f.keyboard.idle());
  TEST_ASSERT_TRUE(NimBLEDevice::bondsDeleted);
  TEST_ASSERT_TRUE(NimBLEDevice::server.disconnected.back() == f.peer.handle);
  f.tick(125);
  TEST_ASSERT_TRUE(f.count() == 3);
}

void test_unencrypted_dropped_battery_clamp() {  // // unencrypted link is dropped; battery clamps
  Fixture f;
  f.connect();
  f.encrypt(false);
  TEST_ASSERT_TRUE(!f.keyboard.ready());
  TEST_ASSERT_TRUE(NimBLEDevice::server.disconnected.back() == f.peer.handle);
  f.keyboard.setBatteryLevel(255);
  TEST_ASSERT_TRUE(NimBLEHIDDevice::last->battery == 100);
  f.keyboard.setBatteryLevel(42);
  TEST_ASSERT_TRUE(NimBLEHIDDevice::last->battery == 42);
}

void test_millis_wraparound() {  // // millis wraparound for hold and gap
  Fixture f;
  f.ready();
  fakeMillis = UINT32_MAX - 10;
  TEST_ASSERT_TRUE(f.keyboard.send(0, 0x04));
  f.tick(fakeMillis);
  TEST_ASSERT_TRUE(f.count() == 2);
  f.tick(13);  // 24 ms later
  TEST_ASSERT_TRUE(f.count() == 2);
  f.tick(14);
  f.expect(2, 0, 0);  // release deadline survives wraparound
  f.tick(38);
  TEST_ASSERT_TRUE(!f.keyboard.idle());
  f.tick(39);
  TEST_ASSERT_TRUE(f.keyboard.idle());
}

void setUp() {}
void tearDown() {}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_security_descriptor_and_pacing);
  RUN_TEST(test_invalid_strokes_and_modifier_chord);
  RUN_TEST(test_configurable_keystroke_ms);
  RUN_TEST(test_failed_reports_retry);
  RUN_TEST(test_reconnect_drops_stroke);
  RUN_TEST(test_reconnect_while_held);
  RUN_TEST(test_release_all);
  RUN_TEST(test_clear_bonds);
  RUN_TEST(test_unencrypted_dropped_battery_clamp);
  RUN_TEST(test_millis_wraparound);
  return UNITY_END();
}
