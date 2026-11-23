#include "Improv.h"

#include <string.h>

namespace {
const char kHeader[6] = {'I', 'M', 'P', 'R', 'O', 'V'};
constexpr uint8_t kVersion = 1;

uint8_t sumOf(const uint8_t* p, size_t n) {
  uint8_t s = 0;
  for (size_t i = 0; i < n; ++i) s = static_cast<uint8_t>(s + p[i]);
  return s;
}
}  // namespace

bool ImprovParser::feed(uint8_t b) {
  switch (stage_) {
    case Header:  // matched byte by byte; a mismatch restarts, re-testing b as a first byte
      if (b == static_cast<uint8_t>(kHeader[pos_])) {
        sum_ = static_cast<uint8_t>(pos_ == 0 ? b : sum_ + b);
        if (++pos_ == sizeof kHeader) stage_ = Version;
      } else {
        pos_ = 0;
        if (b == static_cast<uint8_t>(kHeader[0])) {
          sum_ = b;
          pos_ = 1;
        }
      }
      return false;
    case Version:
      if (b != kVersion) {
        stage_ = Header;
        pos_ = 0;
        feed(b);
        return false;
      }
      sum_ = static_cast<uint8_t>(sum_ + b);
      stage_ = Type;
      return false;
    case Type:
      type_ = b;
      sum_ = static_cast<uint8_t>(sum_ + b);
      stage_ = Length;
      return false;
    case Length:
      len_ = b;
      pos_ = 0;
      sum_ = static_cast<uint8_t>(sum_ + b);
      stage_ = len_ ? Data : Checksum;
      return false;
    case Data:
      data_[pos_++] = b;
      sum_ = static_cast<uint8_t>(sum_ + b);
      if (pos_ == len_) stage_ = Checksum;
      return false;
    case Checksum: {
      const bool ok = b == sum_;
      stage_ = Header;
      pos_ = 0;
      if (!ok) feed(b);  // the bad byte may start a new header
      return ok;
    }
  }
  return false;
}

size_t improvFrame(ImprovType t, const uint8_t* data, uint8_t len, uint8_t* out, size_t cap) {
  const size_t total = 10 + static_cast<size_t>(len);
  if (cap < total) return 0;
  memcpy(out, kHeader, 6);
  out[6] = kVersion;
  out[7] = static_cast<uint8_t>(t);
  out[8] = len;
  if (len) memcpy(out + 9, data, len);
  out[9 + len] = sumOf(out, 9 + len);
  return total;
}

size_t improvRpcResult(ImprovCommand c, const char* const* strings, uint8_t count, uint8_t* out, size_t cap) {
  uint8_t body[255];
  size_t n = 2;  // command, total string length (patched below)
  body[0] = static_cast<uint8_t>(c);
  for (uint8_t i = 0; i < count; ++i) {
    const size_t sl = strlen(strings[i]);
    if (sl > 255 || n + 1 + sl > sizeof body) return 0;
    body[n++] = static_cast<uint8_t>(sl);
    memcpy(body + n, strings[i], sl);
    n += sl;
  }
  body[1] = static_cast<uint8_t>(n - 2);
  return improvFrame(ImprovType::RpcResult, body, static_cast<uint8_t>(n), out, cap);
}

bool improvWifiSettings(const uint8_t* rpc, uint8_t len, char* ssid, size_t ssidCap, char* pass, size_t passCap) {
  if (len < 4 || rpc[0] != static_cast<uint8_t>(ImprovCommand::WifiSettings)) return false;
  if (rpc[1] + 2u != len) return false;
  const uint8_t sl = rpc[2];
  if (3u + sl + 1u > len) return false;
  const uint8_t pl = rpc[3 + sl];
  if (3u + sl + 1u + pl != len) return false;
  if (sl >= ssidCap || pl >= passCap) return false;
  memcpy(ssid, rpc + 3, sl);
  ssid[sl] = 0;
  memcpy(pass, rpc + 4 + sl, pl);
  pass[pl] = 0;
  return true;
}
