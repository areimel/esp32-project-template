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

bool ImprovParser::feed(uint8_t b) { return consume(b); }

void ImprovParser::resync() {
  uint8_t tmp[sizeof raw_];
  const uint8_t m = static_cast<uint8_t>(n_ - 1);
  memcpy(tmp, raw_ + 1, m);
  stage_ = Header;
  n_ = 0;
  // The replayed bytes hold no complete frame, so this cannot succeed or recurse deeply.
  for (uint8_t i = 0; i < m; ++i) consume(tmp[i]);
}

bool ImprovParser::consume(uint8_t b) {
  if (stage_ == Header) {  // the header's letters are distinct, so a mismatch restarts at b
    if (b == static_cast<uint8_t>(kHeader[n_])) {
      raw_[n_++] = b;
      if (n_ == sizeof kHeader) stage_ = Version;
    } else {
      n_ = 0;
      if (b == static_cast<uint8_t>(kHeader[0])) raw_[n_++] = b;
    }
    return false;
  }
  raw_[n_++] = b;
  bool lastByte = false;  // b was the checksum byte
  switch (stage_) {
    case Version:
      if (b != kVersion) resync();
      else stage_ = Type;
      return false;
    case Type:
      type_ = b;
      stage_ = Length;
      return false;
    case Length:
      len_ = b;
      stage_ = len_ ? Data : Checksum;
      return false;
    case Data:
      if (n_ == 9u + len_) stage_ = Checksum;
      break;
    case Checksum:
      lastByte = true;
      if (b == sumOf(raw_, n_ - 1)) {
        memcpy(data_, raw_ + 9, len_);
        stage_ = Header;
        n_ = 0;
        return true;
      }
      break;
    default:
      break;
  }
  // Data/Checksum bytes of a frame that did not complete. A full header inside them means the frame was
  // cut short: restart there. A bad checksum abandons the frame and replays its tail for a partial header.
  if (n_ > 7 && memcmp(raw_ + n_ - 7, kHeader, 6) == 0 && raw_[n_ - 1] == kVersion) {
    memmove(raw_, raw_ + n_ - 7, 7);
    n_ = 7;
    stage_ = Type;
  } else if (lastByte) {
    resync();
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
  if (sl == 0 || 3u + sl + 1u > len) return false;  // empty SSID is invalid
  const uint8_t pl = rpc[3 + sl];
  if (3u + sl + 1u + pl != len) return false;
  if (sl >= ssidCap || pl >= passCap) return false;
  memcpy(ssid, rpc + 3, sl);
  ssid[sl] = 0;
  memcpy(pass, rpc + 4 + sl, pl);
  pass[pl] = 0;
  return true;
}
