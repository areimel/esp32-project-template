#include "Net.h"

#if FEATURE_WIFI
#include <WiFi.h>

#include "Improv.h"
#include "Log.h"

namespace {
ImprovParser parser;
const char* info[4] = {"", "", "", ""};  // firmware, version, chip, device name
constexpr uint32_t kConnectTimeoutMs = 15000;

void send(ImprovType t, uint8_t v) {
  uint8_t f[16];
  const size_t n = improvFrame(t, &v, 1, f, sizeof f);
  Serial.write(f, n);
}

void sendError(ImprovError e) { send(ImprovType::ErrorState, static_cast<uint8_t>(e)); }
void sendState(ImprovState s) { send(ImprovType::CurrentState, static_cast<uint8_t>(s)); }

void sendResult(ImprovCommand c, const char* const* strings, uint8_t count) {
  uint8_t f[200];
  const size_t n = improvRpcResult(c, strings, count, f, sizeof f);
  if (n) Serial.write(f, n);
}

ImprovState currentState() {
  return WiFi.status() == WL_CONNECTED ? ImprovState::Provisioned : ImprovState::Authorized;
}

void provision(const uint8_t* rpc, uint8_t len) {
  char ssid[33], pass[65];
  if (!improvWifiSettings(rpc, len, ssid, sizeof ssid, pass, sizeof pass)) {
    sendError(ImprovError::InvalidRpc);
    return;
  }
  sendError(ImprovError::None);
  sendState(ImprovState::Provisioning);
  LOG_I("improv: connecting to %s", ssid);
  WiFi.begin(ssid, pass);  // persisted by WiFi.persistent(true)
  const uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < kConnectTimeoutMs) delay(100);
  if (WiFi.status() != WL_CONNECTED) {
    sendError(ImprovError::UnableToConnect);
    sendState(ImprovState::Authorized);
    return;
  }
  char url[32];
  snprintf(url, sizeof url, "http://%s", WiFi.localIP().toString().c_str());
  const char* strings[1] = {url};
  sendState(ImprovState::Provisioned);
  sendResult(ImprovCommand::WifiSettings, strings, 1);
}

void handleRpc(const uint8_t* d, uint8_t len) {
  if (len < 2) {
    sendError(ImprovError::InvalidRpc);
    return;
  }
  switch (static_cast<ImprovCommand>(d[0])) {
    case ImprovCommand::GetState:
      sendState(currentState());
      break;
    case ImprovCommand::GetInfo:
      sendResult(ImprovCommand::GetInfo, info, 4);
      break;
    case ImprovCommand::WifiSettings:
      provision(d, len);
      break;
    default:
      sendError(ImprovError::UnknownRpc);
  }
}
}  // namespace

namespace net {

void beginImprov(const char* firmware, const char* version, const char* chip, const char* deviceName) {
  info[0] = firmware;
  info[1] = version;
  info[2] = chip;
  info[3] = deviceName;
  sendState(currentState());
}

void updateImprov() {
  while (Serial.available()) {
    if (parser.feed(static_cast<uint8_t>(Serial.read())) && parser.type() == ImprovType::Rpc)
      handleRpc(parser.data(), parser.length());
  }
}

}  // namespace net
#endif
