#pragma once
#if FEATURE_DISPLAY
#include <stdint.h>
#include <string.h>

#include "Display.h"

enum class BleStatus : uint8_t { Off, Advertising, Connecting, Ready };
enum class WifiStatus : uint8_t { Off, NoCredentials, Connecting, Connected };

// Plain description of the status screen. main fills it; drawStatus renders it.
struct StatusModel {
  BleStatus ble = BleStatus::Off;
  WifiStatus wifi = WifiStatus::Off;
  char ip[16] = "";
  int battery = -1;  // percent, -1 unknown
  bool charging = false;
  bool updating = false;        // OTA in progress
  const char* lastAction = "";  // label of the last button action sent

  bool operator==(const StatusModel& o) const {
    return ble == o.ble && wifi == o.wifi && strcmp(ip, o.ip) == 0 &&
           battery == o.battery && charging == o.charging && updating == o.updating &&
           (lastAction == o.lastAction || strcmp(lastAction, o.lastAction) == 0);
  }
  bool operator!=(const StatusModel& o) const { return !(*this == o); }
};

#if FEATURE_WIFI
#include "Net.h"
inline WifiStatus toStatus(WifiState s) {
  return s == WifiState::Connected    ? WifiStatus::Connected
         : s == WifiState::Connecting ? WifiStatus::Connecting
                                      : WifiStatus::NoCredentials;
}
#endif

// Full redraw of the status screen, then push to the panel.
void drawStatus(Display& display, const StatusModel& m);
#endif  // FEATURE_DISPLAY
