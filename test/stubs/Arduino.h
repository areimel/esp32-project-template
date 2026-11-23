#pragma once
#include <cstdint>
inline uint32_t fakeMillis = 0;
inline uint32_t millis() { return fakeMillis; }
inline void delay(uint32_t ms) { fakeMillis += ms; }
struct FakeSerial {
  template <typename... Args> void printf(const char*, Args...) {}
};
inline FakeSerial Serial;
