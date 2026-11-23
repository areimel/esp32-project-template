#pragma once
// Improv Serial codec (https://www.improv-wifi.com/serial/). Pure C++, no Arduino.
// Frame: "IMPROV" | version 1 | type | len | data[len] | checksum (low byte of sum of all prior bytes).
#include <stddef.h>
#include <stdint.h>

enum class ImprovType : uint8_t { CurrentState = 1, ErrorState = 2, Rpc = 3, RpcResult = 4 };
enum class ImprovState : uint8_t { Authorized = 2, Provisioning = 3, Provisioned = 4 };
enum class ImprovError : uint8_t { None = 0, InvalidRpc = 1, UnknownRpc = 2, UnableToConnect = 3, Unknown = 0xFF };
enum class ImprovCommand : uint8_t { WifiSettings = 1, GetState = 2, GetInfo = 3, Scan = 4 };

class ImprovParser {
 public:
  // Feed one byte; true when a valid frame completed. Garbage and bad frames resync.
  bool feed(uint8_t b);
  ImprovType type() const { return static_cast<ImprovType>(type_); }
  const uint8_t* data() const { return data_; }
  uint8_t length() const { return len_; }

 private:
  enum Stage : uint8_t { Header, Version, Type, Length, Data, Checksum };
  Stage stage_ = Header;
  uint8_t pos_ = 0, type_ = 0, len_ = 0, sum_ = 0;
  uint8_t data_[255];
};

size_t improvFrame(ImprovType t, const uint8_t* data, uint8_t len, uint8_t* out, size_t cap);
// Full RpcResult frame: cmd, total length, then (length, bytes) per string.
size_t improvRpcResult(ImprovCommand c, const char* const* strings, uint8_t count, uint8_t* out, size_t cap);
// Parses an Rpc frame's data (cmd, len, ssid_len, ssid, pass_len, pass); false if malformed or too long.
bool improvWifiSettings(const uint8_t* rpc, uint8_t len, char* ssid, size_t ssidCap, char* pass, size_t passCap);
