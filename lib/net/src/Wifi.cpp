#include "Net.h"

#if FEATURE_WIFI
#include <WiFi.h>
#include <esp_wifi.h>

#include "Every.h"
#include "Log.h"
#include "NetInternal.h"

namespace {
Every retry(10000);
bool haveCreds = false;
bool storageFlash = true;  // IDF default after esp_wifi_init with WiFi.persistent(true)
char ip[16] = "";
}  // namespace

namespace net {

bool staConfigured() {
  wifi_config_t c{};
  return esp_wifi_get_config(WIFI_IF_STA, &c) == ESP_OK && c.sta.ssid[0] != 0;
}

void setStaStorageFlash(bool flash) {
  if (esp_wifi_set_storage(flash ? WIFI_STORAGE_FLASH : WIFI_STORAGE_RAM) == ESP_OK) storageFlash = flash;
}

bool staStorageFlash() { return storageFlash; }

void beginWifi(const char* hostname, const char* devSsid, const char* devPass) {
  WiFi.setHostname(hostname);  // before mode(): the STA netif takes the hostname when STA is enabled
  WiFi.persistent(true);
  WiFi.mode(WIFI_STA);         // loads any stored STA config, so staConfigured() can see it
  haveCreds = staConfigured();
  if (haveCreds) {
    WiFi.begin();  // stored credentials win
  } else if (devSsid && devSsid[0]) {
    LOG_I("wifi: using dev credentials for %s", devSsid);
    // RAM storage for this whole session: dev credentials never reach flash, so they never
    // replace Improv-provisioned ones. Improv switches back to flash when it saves credentials.
    setStaStorageFlash(false);
    WiFi.begin(devSsid, devPass ? devPass : "");
    haveCreds = true;
  } else {
    LOG_I("wifi: no credentials; provision via Improv");
  }
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
  if (improvProvisioning()) return;  // an Improv attempt owns the connection; do not fight it
  // Follows the live config rather than latching, so credentials restored or cleared after a failed
  // Improv attempt are reflected here.
  haveCreds = staConfigured();
  // Skip while the stack is already mid-attempt (WL_IDLE_STATUS); it also auto-reconnects on its own.
  if (haveCreds && WiFi.status() != WL_IDLE_STATUS && retry.due(now)) {
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
