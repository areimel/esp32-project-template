#include "StatusView.h"
#if FEATURE_DISPLAY
#include <stdio.h>

#include "Theme.h"
#include "generated_config.h"

namespace {
using namespace Theme;

const char* bleText(BleStatus s) {
  switch (s) {
    case BleStatus::Ready: return "READY";
    case BleStatus::Connecting: return "CONNECTING";
    case BleStatus::Advertising: return "ADVERTISING";
    default: return "OFF";
  }
}

const char* wifiText(const StatusModel& m) {
  switch (m.wifi) {
    case WifiStatus::Connected: return m.ip;
    case WifiStatus::Connecting: return "CONNECTING";
    case WifiStatus::NoCredentials: return "NEEDS SETUP";
    default: return "OFF";
  }
}

// One "NAME  value" row: small dim name, larger value at x = 78.
void row(M5Canvas& c, int y, const char* name, const char* value, uint16_t color) {
  c.setFont(&fonts::Font0);
  c.setTextSize(1);
  c.setTextColor(DIM);
  c.setCursor(14, y);
  c.print(name);
  c.setTextSize(2);
  c.setTextColor(color);
  c.setCursor(78, y - 4);
  c.print(value);
}
}  // namespace

void drawStatus(Display& display, const StatusModel& m) {
  M5Canvas& c = display.canvas();
  c.fillScreen(BG);
  display.header(AppConfig::PROJECT_NAME, nullptr, m.battery, m.charging);
  c.drawFastHLine(8, 18, 224, LINE);
  row(c, 34, "BLE", bleText(m.ble), m.ble == BleStatus::Ready ? ACCENT : INK);
  row(c, 58, "WIFI", wifiText(m), m.wifi == WifiStatus::Connected ? ACCENT : INK);
  if (m.updating) row(c, 82, "OTA", "UPDATING", ACCENT);
  else row(c, 82, "LAST", m.lastAction[0] ? m.lastAction : "--", INK);
  c.drawFastHLine(8, 118, 224, LINE);
  c.setTextSize(1);
  c.setTextColor(DIM);
  c.setCursor(8, 124);
  c.print(BOARD_LABEL);
  c.setCursor(232 - 6 * int(strlen(FIRMWARE_VERSION)), 124);
  c.print(FIRMWARE_VERSION);
  display.push();
}
#endif  // FEATURE_DISPLAY
