#include <unity.h>
#include <string.h>
#include <string>
#include <vector>
#include "Improv.h"

void setUp() {}
void tearDown() {}

static int feedAll(ImprovParser& p, const uint8_t* b, size_t n) {
  int frames = 0;
  for (size_t i = 0; i < n; ++i) frames += p.feed(b[i]) ? 1 : 0;
  return frames;
}

void test_roundtrip_state() {
  uint8_t d[] = {2}, f[32];
  size_t n = improvFrame(ImprovType::CurrentState, d, 1, f, sizeof f);
  TEST_ASSERT_EQUAL(11, n);
  TEST_ASSERT_EQUAL_MEMORY("IMPROV", f, 6);
  TEST_ASSERT_EQUAL(1, f[6]);
  ImprovParser p;
  TEST_ASSERT_EQUAL(1, feedAll(p, f, n));
  TEST_ASSERT_EQUAL(1, (int)p.type());
  TEST_ASSERT_EQUAL(1, p.length());
  TEST_ASSERT_EQUAL(2, p.data()[0]);
}

void test_garbage_then_frame() {
  uint8_t d[] = {2}, f[32];
  size_t n = improvFrame(ImprovType::CurrentState, d, 1, f, sizeof f);
  std::string in = "[I] boot\nIMPRO";
  std::vector<uint8_t> bytes(in.begin(), in.end());
  bytes.insert(bytes.end(), f, f + n);
  ImprovParser p;
  TEST_ASSERT_EQUAL(1, feedAll(p, bytes.data(), bytes.size()));
  TEST_ASSERT_EQUAL(2, p.data()[0]);
}

void test_bad_checksum_resyncs() {
  uint8_t d[] = {3}, bad[32], good[32];
  size_t n = improvFrame(ImprovType::CurrentState, d, 1, bad, sizeof bad);
  bad[n - 1] ^= 0x55;
  ImprovParser p;
  TEST_ASSERT_EQUAL(0, feedAll(p, bad, n));
  n = improvFrame(ImprovType::CurrentState, d, 1, good, sizeof good);
  TEST_ASSERT_EQUAL(1, feedAll(p, good, n));
  TEST_ASSERT_EQUAL(3, p.data()[0]);
}

void test_truncated_length() {
  // Declares 200 data bytes; data runs out (filler), checksum is wrong, then a valid frame follows.
  uint8_t d[] = {4}, good[32];
  size_t n = improvFrame(ImprovType::CurrentState, d, 1, good, sizeof good);
  std::vector<uint8_t> bytes = {'I', 'M', 'P', 'R', 'O', 'V', 1, 1, 200};
  bytes.insert(bytes.end(), 201, 0xEE);
  bytes.insert(bytes.end(), good, good + n);
  ImprovParser p;
  TEST_ASSERT_EQUAL(1, feedAll(p, bytes.data(), bytes.size()));
  TEST_ASSERT_EQUAL(4, p.data()[0]);
}

static std::vector<uint8_t> goodFrame(uint8_t v) {
  uint8_t d[] = {v}, f[32];
  size_t n = improvFrame(ImprovType::CurrentState, d, 1, f, sizeof f);
  return std::vector<uint8_t>(f, f + n);
}

void test_short_frame_then_valid_frame() {
  // Declares 200 data bytes but only 20 arrive; a valid frame follows immediately, with nothing after it.
  std::vector<uint8_t> bytes = {'I', 'M', 'P', 'R', 'O', 'V', 1, 1, 200};
  bytes.insert(bytes.end(), 20, 0xEE);
  std::vector<uint8_t> good = goodFrame(5);
  bytes.insert(bytes.end(), good.begin(), good.end());
  ImprovParser p;
  TEST_ASSERT_EQUAL(1, feedAll(p, bytes.data(), bytes.size()));
  TEST_ASSERT_EQUAL(5, p.data()[0]);
}

void test_valid_frame_inside_bad_checksum_frame() {
  std::vector<uint8_t> inner = goodFrame(6);
  std::vector<uint8_t> bytes = {'I', 'M', 'P', 'R', 'O', 'V', 1, 1, (uint8_t)inner.size()};
  bytes.insert(bytes.end(), inner.begin(), inner.end());
  bytes.push_back(0x00);  // wrong checksum for the outer frame
  ImprovParser p;
  TEST_ASSERT_EQUAL(1, feedAll(p, bytes.data(), bytes.size()));
  TEST_ASSERT_EQUAL(6, p.data()[0]);
  std::vector<uint8_t> next = goodFrame(7);
  TEST_ASSERT_EQUAL(1, feedAll(p, next.data(), next.size()));
  TEST_ASSERT_EQUAL(7, p.data()[0]);
}

void test_header_straddling_bad_checksum() {
  // The bad frame's body ends with "IMPRO" and its (wrong) checksum byte is 'V': the header starts inside
  // the discarded bytes and is completed by the checksum byte. The rest of a valid frame follows.
  std::vector<uint8_t> good = goodFrame(8);
  std::vector<uint8_t> bytes = {'I', 'M', 'P', 'R', 'O', 'V', 1, 1, 7, 'a', 'b', 'I', 'M', 'P', 'R', 'O', 'V'};
  bytes.insert(bytes.end(), good.begin() + 6, good.end());
  ImprovParser p;
  TEST_ASSERT_EQUAL(1, feedAll(p, bytes.data(), bytes.size()));
  TEST_ASSERT_EQUAL(8, p.data()[0]);
}

void test_max_payload_roundtrip() {
  uint8_t d[255], f[300];
  for (int i = 0; i < 255; ++i) d[i] = (uint8_t)(i * 7 + 3);
  d[10] = 0;  // avoid accidental header bytes; content is otherwise arbitrary
  size_t n = improvFrame(ImprovType::Rpc, d, 255, f, sizeof f);
  TEST_ASSERT_EQUAL(265, n);
  ImprovParser p;
  TEST_ASSERT_EQUAL(1, feedAll(p, f, n));
  TEST_ASSERT_EQUAL(255, p.length());
  TEST_ASSERT_EQUAL_MEMORY(d, p.data(), 255);
}

void test_max_payload_bad_checksum_then_valid() {
  uint8_t d[255], f[300];
  for (int i = 0; i < 255; ++i) d[i] = (uint8_t)(i * 5 + 1);
  size_t n = improvFrame(ImprovType::Rpc, d, 255, f, sizeof f);
  f[n - 1] ^= 0x55;
  ImprovParser p;
  TEST_ASSERT_EQUAL(0, feedAll(p, f, n));
  std::vector<uint8_t> good = goodFrame(9);
  TEST_ASSERT_EQUAL(1, feedAll(p, good.data(), good.size()));
  TEST_ASSERT_EQUAL(1, p.length());
  TEST_ASSERT_EQUAL(9, p.data()[0]);
}

void test_payload_containing_header_is_dropped() {
  // Documented trade-off: a correctly checksummed frame whose payload holds "IMPROV\x01" is cut short.
  const uint8_t d[] = {'I', 'M', 'P', 'R', 'O', 'V', 1, 'x'};
  uint8_t f[32];
  size_t n = improvFrame(ImprovType::Rpc, d, sizeof d, f, sizeof f);
  ImprovParser p;
  TEST_ASSERT_EQUAL(0, feedAll(p, f, n));
}

void test_wifi_settings_empty_ssid() {
  const uint8_t rpc[] = {1, 2, 0, 0};
  char ssid[33], pass[65];
  TEST_ASSERT_FALSE(improvWifiSettings(rpc, sizeof rpc, ssid, sizeof ssid, pass, sizeof pass));
}

void test_wifi_settings() {
  const uint8_t rpc[] = {1, 10, 4, 'h', 'o', 'm', 'e', 4, 'p', '@', 's', 's'};
  char ssid[33], pass[65];
  TEST_ASSERT_TRUE(improvWifiSettings(rpc, sizeof rpc, ssid, sizeof ssid, pass, sizeof pass));
  TEST_ASSERT_EQUAL_STRING("home", ssid);
  TEST_ASSERT_EQUAL_STRING("p@ss", pass);
  char tiny[3];
  TEST_ASSERT_FALSE(improvWifiSettings(rpc, sizeof rpc, tiny, sizeof tiny, pass, sizeof pass));
  const uint8_t shortRpc[] = {1, 11, 9, 'h'};
  TEST_ASSERT_FALSE(improvWifiSettings(shortRpc, sizeof shortRpc, ssid, sizeof ssid, pass, sizeof pass));
}

void test_rpc_result() {
  const char* s[] = {"ESP32 Starter", "v1.0.0", "ESP32-S3", "Stick"};
  uint8_t f[128];
  size_t n = improvRpcResult(ImprovCommand::GetInfo, s, 4, f, sizeof f);
  const uint8_t total = 13 + 6 + 8 + 5 + 4;
  TEST_ASSERT_EQUAL(10 + 2 + total, n);
  TEST_ASSERT_EQUAL(4, f[7]);
  TEST_ASSERT_EQUAL(2 + total, f[8]);
  TEST_ASSERT_EQUAL(3, f[9]);
  TEST_ASSERT_EQUAL(total, f[10]);
  TEST_ASSERT_EQUAL(13, f[11]);
  TEST_ASSERT_EQUAL_MEMORY("ESP32 Starter", f + 12, 13);
  ImprovParser p;
  TEST_ASSERT_EQUAL(1, feedAll(p, f, n));
  TEST_ASSERT_EQUAL(4, (int)p.type());
  uint8_t tiny[8];
  TEST_ASSERT_EQUAL(0, improvRpcResult(ImprovCommand::GetInfo, s, 4, tiny, sizeof tiny));
}

// The device runs no web server, so a successful provision answers with no redirect URL.
void test_rpc_result_without_strings() {
  uint8_t f[16];
  const size_t n = improvRpcResult(ImprovCommand::WifiSettings, nullptr, 0, f, sizeof f);
  TEST_ASSERT_EQUAL(10 + 2, n);
  TEST_ASSERT_EQUAL(4, f[7]);  // RpcResult
  TEST_ASSERT_EQUAL(2, f[8]);  // payload: command, total string length
  TEST_ASSERT_EQUAL(1, f[9]);  // WifiSettings
  TEST_ASSERT_EQUAL(0, f[10]);
  ImprovParser p;
  TEST_ASSERT_EQUAL(1, feedAll(p, f, n));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_roundtrip_state);
  RUN_TEST(test_garbage_then_frame);
  RUN_TEST(test_bad_checksum_resyncs);
  RUN_TEST(test_truncated_length);
  RUN_TEST(test_short_frame_then_valid_frame);
  RUN_TEST(test_valid_frame_inside_bad_checksum_frame);
  RUN_TEST(test_header_straddling_bad_checksum);
  RUN_TEST(test_max_payload_roundtrip);
  RUN_TEST(test_max_payload_bad_checksum_then_valid);
  RUN_TEST(test_payload_containing_header_is_dropped);
  RUN_TEST(test_wifi_settings_empty_ssid);
  RUN_TEST(test_wifi_settings);
  RUN_TEST(test_rpc_result);
  RUN_TEST(test_rpc_result_without_strings);
  return UNITY_END();
}
