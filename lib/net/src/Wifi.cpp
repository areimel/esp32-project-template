#include "Net.h"

#if FEATURE_WIFI
#include <WiFi.h>

#include "Every.h"
#include "Log.h"

namespace {
Every retry(10000);
bool haveCreds = false;
char ip[16] = "";
}  // namespace

namespace net {

void beginWifi(const char* hostname, const char* devSsid, const char* devPass) {
  WiFi.persistent(true);
  WiFi.mode(WIFI_STA);
  WiFi.setHostname(hostname);
  WiFi.begin();  // stored credentials, if any
  haveCreds = WiFi.SSID().length() > 0;
  if (!haveCreds && devSsid && devSsid[0]) {
    LOG_I("wifi: using dev credentials for %s", devSsid);
    WiFi.begin(devSsid, devPass ? devPass : "");
    haveCreds = true;
  }
  if (!haveCreds) LOG_I("wifi: no credentials; provision via Improv");
}

void updateWifi(uint32_t now) {
  if (WiFi.status() == WL_CONNECTED) {
    if (!ip[0]) {
      snprintf(ip, sizeof ip, "%s", WiFi.localIP().toString().c_str());
      LOG_I("wifi: connected, ip %s", ip);
    }
    return;
  }
  ip[0] = 0;
  haveCreds = haveCreds || WiFi.SSID().length() > 0;  // Improv may have stored new credentials
  if (haveCreds && retry.due(now)) {
    LOG_I("wifi: reconnecting");
    WiFi.reconnect();
  }
}

WifiState wifiState() {
  if (WiFi.status() == WL_CONNECTED) return WifiState::Connected;
  return haveCreds ? WifiState::Connecting : WifiState::NoCredentials;
}

const char* ipAddress() { return WiFi.status() == WL_CONNECTED && ip[0] ? ip : ""; }

}  // namespace net
#endif
