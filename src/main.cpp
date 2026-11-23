// Minimal default app: buttons send configured key combos over BLE HID, WiFi is
// provisioned over Improv Serial, OTA is optional, and the status display shows it all.
#include <Arduino.h>

#include "Board.h"
#include "BootHold.h"
#include "Every.h"
#include "Gesture.h"
#include "Log.h"
#include "generated_config.h"
#if FEATURE_BLE_HID
#include "BleKeyboard.h"
#endif
#if FEATURE_WIFI
#include "Net.h"
#endif
#if FEATURE_DISPLAY
#include "Display.h"
#include "StatusView.h"
#endif

namespace {
using AppConfig::BUTTON_ACTIONS;
using AppConfig::BUTTON_SLOTS;
using AppConfig::ButtonAction;

constexpr uint32_t kLedBlinkMs = 80;
// 1-button boards need a longer hold so a stray press cannot clear bonds.
constexpr uint32_t kBondHoldMs = BUTTON_SLOTS >= 2 ? 3000 : 5000;

struct Button {
  Gesture gesture{AppConfig::DEBOUNCE_MS, AppConfig::DOUBLE_CLICK_MS, AppConfig::LONG_PRESS_MS};
} buttons[BUTTON_SLOTS];
Every batteryTimer(30000);
Every redrawTimer(1000);  // idle redraw
int battery = -1;
bool charging = false;
const char* lastAction = "";
uint32_t ledOffAt = 0;
bool ledOn = false;
bool wasUpdating = true;  // forces a gesture reset on the first loop pass
#if FEATURE_BLE_HID
BleKeyboard keyboard;
uint32_t lastGeneration = 0;
#endif
#if FEATURE_DISPLAY
Display display;
StatusModel shown;
#endif

bool assigned(const ButtonAction& a) { return a.mods != 0 || a.usage != 0; }

bool linkReady() {
#if FEATURE_BLE_HID
  return keyboard.ready();
#endif
  return true;  // no transport: actions are only logged
}

bool updating() {
#if FEATURE_OTA
  return net::otaActive();
#endif
  return false;
}

// A send is never retried or queued; input on a not-ready link is discarded.
void handle(uint8_t button, GestureEvent event, uint32_t now) {
  if (event == GestureEvent::NONE) return;
  const uint8_t slot = uint8_t(event) - 1;  // SINGLE, DOUBLE, HOLD -> 0, 1, 2
  const ButtonAction& action = BUTTON_ACTIONS[button][slot];
  if (!assigned(action)) return;
  if (!linkReady()) return;
#if FEATURE_BLE_HID
  if (!keyboard.send(action.mods, action.usage)) return;  // busy: dropped
#endif
  lastAction = action.label;
  LOG_I("button %u slot %u: %s", button, slot, action.label);
  board::setStatusLed(ledOn = true);
  ledOffAt = now + kLedBlinkMs;
}

void readBattery() {
  battery = board::batteryPercent();
  charging = board::charging();
#if FEATURE_BLE_HID
  if (battery >= 0) keyboard.setBatteryLevel(uint8_t(battery > 100 ? 100 : battery));
#endif
  if (battery >= 0) LOG_I("battery %d%% charging=%d", battery, charging);
}

#if FEATURE_DISPLAY
StatusModel model() {
  StatusModel m;
#if FEATURE_BLE_HID
  m.ble = keyboard.ready()       ? BleStatus::Ready
          : keyboard.connected() ? BleStatus::Connecting
                                 : BleStatus::Advertising;
#endif
#if FEATURE_WIFI
  m.wifi = toStatus(net::wifiState());
  snprintf(m.ip, sizeof m.ip, "%s", net::ipAddress());
#endif
  m.battery = battery;
  m.charging = charging;
  m.updating = updating();
  m.lastAction = lastAction;
  return m;
}

void refreshScreen(uint32_t now) {
  const StatusModel next = model();
  if (next == shown && !redrawTimer.due(now)) return;
  drawStatus(display, shown = next);
}
#endif  // FEATURE_DISPLAY

void resetGestures(uint32_t now) {
  for (uint8_t i = 0; i < BUTTON_SLOTS; ++i) {
    buttons[i].gesture.setAssigned(assigned(BUTTON_ACTIONS[i][1]), assigned(BUTTON_ACTIONS[i][2]));
    buttons[i].gesture.reset(board::isPressed(i), now);
  }
}
}  // namespace

void setup() {
  Serial.begin(115200);  // never wait for a USB serial host
  board::begin(AppConfig::DISPLAY_BRIGHTNESS);
  LOG_I("%s %s on %s (%u buttons)", AppConfig::PROJECT_NAME, FIRMWARE_VERSION, BOARD_LABEL,
        board::buttonCount());
#if FEATURE_DISPLAY
  display.begin(board::display(), AppConfig::DISPLAY_BRIGHTNESS);
  display.splash(AppConfig::PROJECT_NAME, FIRMWARE_VERSION);
#endif
#if FEATURE_BLE_HID
  // begin() first: clearing bonds needs NimBLE up. Holding all buttons at boot
  // clears bonds before the screen says so.
  keyboard.begin(AppConfig::BLE_NAME, AppConfig::KEYSTROKE_MS);
  holdAllButtonsAtBoot(
      kBondHoldMs,
      [](uint32_t remaining) {
#if FEATURE_DISPLAY
        display.holdProgress("BONDS", remaining, remaining == 0);
#endif
        (void)remaining;
      },
      [] {
        keyboard.clearBonds();
        LOG_I("Bluetooth bonds cleared; forget the device on the host and re-pair");
      });
#endif
#if FEATURE_WIFI
  net::beginWifi(AppConfig::WIFI_HOSTNAME, AppConfig::DEV_WIFI_SSID, AppConfig::DEV_WIFI_PASSWORD);
  net::beginImprov(AppConfig::PROJECT_NAME, FIRMWARE_VERSION, ESP.getChipModel(), AppConfig::PROJECT_NAME);
#endif
#if FEATURE_OTA
  net::beginOta(AppConfig::WIFI_HOSTNAME, AppConfig::OTA_PASSWORD);
#endif
  board::update();
  resetGestures(millis());
  readBattery();
}

void loop() {
  board::update();
  const uint32_t now = millis();
#if FEATURE_WIFI
  net::updateWifi(now);
  net::updateImprov();
#endif
#if FEATURE_OTA
  net::updateOta();
#endif
  const bool busy = updating();
  if (busy) {
#if FEATURE_BLE_HID
    keyboard.releaseAll();
#endif
  } else {
    if (wasUpdating) resetGestures(now);
#if FEATURE_BLE_HID
    // A link change may have dropped a stroke; it is never resumed.
    if (keyboard.linkGeneration() != lastGeneration) {
      lastGeneration = keyboard.linkGeneration();
      for (Button& b : buttons) b.gesture.cancel();
    }
#endif
    for (uint8_t i = 0; i < BUTTON_SLOTS; ++i) {
      handle(i, buttons[i].gesture.update(board::isPressed(i), now), now);
    }
  }
  wasUpdating = busy;
#if FEATURE_BLE_HID
  keyboard.update(now);
#endif
  if (ledOn && int32_t(now - ledOffAt) >= 0) board::setStatusLed(ledOn = false);
  if (batteryTimer.due(now)) readBattery();
#if FEATURE_DISPLAY
  refreshScreen(now);
#endif
  delay(1);
}
