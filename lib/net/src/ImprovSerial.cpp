#include "Net.h"

#if FEATURE_WIFI
#include <WiFi.h>
#include <esp_wifi.h>
#include <string.h>

#include "Improv.h"
#include "Log.h"
#include "NetInternal.h"

// Provisioning is a non-blocking state machine advanced by updateImprov():
//   WifiSettings RPC -> Provisioning sent, attempt started (RAM-only credentials)
//   each poll: connected -> persist credentials, Provisioned + URL result
//              15 s elapsed -> restore the previous credentials, UnableToConnect, back to Authorized
// Nothing is written to flash until the connection succeeds, so a failed attempt never overwrites
// working stored credentials. Passwords are never logged.
namespace {
ImprovParser parser;
const char* info[4] = {"", "", "", ""};  // firmware, version, chip, device name
constexpr uint32_t kConnectTimeoutMs = 15000;

bool connecting = false;
bool droppedOld = false;  // disconnect() is async: success only counts once the old link was seen down
uint32_t startedMs = 0;
wifi_config_t previous;  // STA config before the attempt, restored on failure

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

void sendRedirect() {
  char url[32];
  snprintf(url, sizeof url, "http://%s", WiFi.localIP().toString().c_str());
  const char* strings[1] = {url};
  sendResult(ImprovCommand::WifiSettings, strings, 1);
}

ImprovState currentState() {
  if (connecting) return ImprovState::Provisioning;
  return WiFi.status() == WL_CONNECTED ? ImprovState::Provisioned : ImprovState::Authorized;
}

void startProvisioning(const uint8_t* rpc, uint8_t len) {
  char ssid[33], pass[65];
  if (connecting || !improvWifiSettings(rpc, len, ssid, sizeof ssid, pass, sizeof pass)) {
    sendError(ImprovError::InvalidRpc);
    return;
  }
  sendError(ImprovError::None);
  sendState(ImprovState::Provisioning);
  LOG_I("improv: connecting to %s", ssid);
  if (esp_wifi_get_config(WIFI_IF_STA, &previous) != ESP_OK) memset(&previous, 0, sizeof previous);
  WiFi.persistent(false);
  esp_wifi_set_storage(WIFI_STORAGE_RAM);  // keep the attempted credentials out of flash
  WiFi.disconnect();                       // drop any current network so the old IP cannot pass for success
  WiFi.begin(ssid, pass);
  startedMs = millis();
  droppedOld = false;
  connecting = true;
}

void finishProvisioning(bool ok) {
  connecting = false;
  if (ok) {
    wifi_config_t now;
    WiFi.persistent(true);
    esp_wifi_set_storage(WIFI_STORAGE_FLASH);
    if (esp_wifi_get_config(WIFI_IF_STA, &now) == ESP_OK) esp_wifi_set_config(WIFI_IF_STA, &now);  // persist now
    LOG_I("improv: connected, credentials saved");
    sendState(ImprovState::Provisioned);
    sendRedirect();
    return;
  }
  WiFi.disconnect();
  // Still RAM storage here, so flash was never touched. With nothing to restore (first-time setup) or a
  // rejected restore, leave an empty config so Wifi.cpp reports NoCredentials instead of retrying the
  // failed credentials (stored flash credentials, if any, come back on reboot).
  bool restored = previous.sta.ssid[0] != 0 && esp_wifi_set_config(WIFI_IF_STA, &previous) == ESP_OK;
  if (!restored && previous.sta.ssid[0] != 0) LOG_E("improv: restoring previous credentials failed");
  if (!restored) {
    wifi_config_t empty;
    memset(&empty, 0, sizeof empty);
    if (esp_wifi_set_config(WIFI_IF_STA, &empty) != ESP_OK) LOG_E("improv: clearing failed credentials failed");
  }
  esp_wifi_set_storage(WIFI_STORAGE_FLASH);
  WiFi.persistent(true);
  if (restored) LOG_I("improv: connect failed, previous credentials restored");
  else LOG_I("improv: connect failed");
  sendError(ImprovError::UnableToConnect);
  sendState(ImprovState::Authorized);
}

void pollProvisioning() {
  if (!connecting) return;
  const bool up = WiFi.status() == WL_CONNECTED;
  if (!up) droppedOld = true;  // the disconnect from the previous network has been observed
  if (up && droppedOld) finishProvisioning(true);
  else if (millis() - startedMs >= kConnectTimeoutMs) finishProvisioning(false);
}

void handleRpc(const uint8_t* d, uint8_t len) {
  if (len < 2) {
    sendError(ImprovError::InvalidRpc);
    return;
  }
  switch (static_cast<ImprovCommand>(d[0])) {
    case ImprovCommand::GetState:
      sendState(currentState());
      if (currentState() == ImprovState::Provisioned) sendRedirect();
      break;
    case ImprovCommand::GetInfo:
      sendResult(ImprovCommand::GetInfo, info, 4);
      break;
    case ImprovCommand::WifiSettings:
      startProvisioning(d, len);
      break;
    default:
      sendError(ImprovError::UnknownRpc);
  }
}
}  // namespace

namespace net {

bool improvProvisioning() { return connecting; }

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
  pollProvisioning();
}

}  // namespace net
#endif
