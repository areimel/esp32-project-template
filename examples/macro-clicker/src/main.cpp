// Macro clicker: two buttons play compiled macros over BLE HID, with on-device
// profile switching. Wiring only: controller -> player -> keyboard -> ui.
#include <Arduino.h>

#include "BleKeyboard.h"
#include "Board.h"
#include "BootHold.h"
#include "Controller.h"
#include "Display.h"
#include "Every.h"
#include "Gesture.h"  // lib/input; Controller.h needs it, named here for the dependency finder
#include "Log.h"
#include "MacroPlayer.h"
#include "ProfileStore.h"
#include "Ui.h"
#include "generated_config.h"

#if !FEATURE_BLE_HID || !FEATURE_DISPLAY
#error "The macro clicker needs ble.enabled and display.enabled on a board with a display"
#endif
static_assert(BOARD_BUTTON_COUNT >= 2, "The macro clicker needs two buttons");

namespace {
using namespace AppConfig;
BleKeyboard keyboard;
MacroPlayer player;
ProfileStore store;
Controller controller(DEBOUNCE_MS, DOUBLE_CLICK_MS, LONG_PRESS_MS, PROFILE_COUNT);
Display display;
Ui ui(display);
UiModel shown;
Every batteryTimer(30000);
int battery = -1;
bool charging = false;
uint32_t lastScreen = 0;
LinkState previousLink = LinkState::SEARCHING;
uint32_t lastGeneration = 0;

constexpr uint32_t kBondHoldMs = 3000;
const char* const kButtonNames[] = {"A", "B"};
const char* const kGestureNames[] = {"single", "double", "long"};

SlotMask maskFor(uint8_t profile) {
  SlotMask mask = {};
  for (uint8_t b = 0; b < BUTTON_COUNT; ++b)
    for (uint8_t g = 0; g < GESTURE_COUNT; ++g)
      mask.assigned[b][g] = PROFILES[profile].slots[b][g].count > 0;
  return mask;
}

LinkState linkState() {
  return keyboard.ready() ? LinkState::SECURE
         : keyboard.connected() ? LinkState::HANDSHAKE
                                : LinkState::SEARCHING;
}

const char* linkName(LinkState link) {
  return link == LinkState::SECURE ? "READY" : link == LinkState::HANDSHAKE ? "CONNECTING" : "ADVERTISING";
}

void stopPlayback(const char* reason) {
  if (!player.active()) return;
  player.abort();
  keyboard.releaseAll();
  LOG_I("Macro aborted: %s", reason);
}

void readBattery() {
  battery = board::batteryPercent();
  charging = board::charging();
  if (battery >= 0) keyboard.setBatteryLevel(uint8_t(battery));
  LOG_I("Battery: %d%%, charging=%d", battery, charging);
}

void handle(const ControllerEvent& ev, uint32_t now) {
  switch (ev.type) {
    case ControllerEvent::PLAY: {
      const Macro& macro = PROFILES[ev.profile].slots[ev.button][ev.gesture];
      const bool started = player.start(macro, now);
      LOG_I("Macro %s %s %s (%u strokes): %s", kButtonNames[ev.button], kGestureNames[ev.gesture],
            macro.label, macro.count, started ? "started" : "discarded");
      break;
    }
    case ControllerEvent::ABORT:
      stopPlayback("button press");
      break;
    case ControllerEvent::PICKER_OPENED:
      stopPlayback("profile picker");
      LOG_I("Profile picker opened");
      break;
    case ControllerEvent::PICKER_SELECTED:
      controller.setActiveProfile(ev.profile, maskFor(ev.profile));
      store.save(ev.profile);
      LOG_I("Profile %u selected: %s", ev.profile + 1, PROFILES[ev.profile].name);
      break;
    case ControllerEvent::PICKER_CANCELLED:
      LOG_I("Profile picker cancelled");
      break;
    default:
      break;
  }
}

UiModel model() {
  UiModel m;
  m.screen = player.active() ? Screen::TRANSMIT
             : controller.mode() == Mode::PICKER ? Screen::PICKER
                                                 : Screen::HOME;
  m.link = linkState();
  m.battery = battery;
  m.charging = charging;
  m.deviceName = BLE_NAME;
  m.profileIndex = controller.activeProfile();
  m.profileCount = PROFILE_COUNT;
  m.profiles = PROFILES;
  m.highlighted = controller.highlighted();
  if (player.active() && player.macro()) {
    m.macroLabel = player.macro()->label;
    m.sent = player.sent();
    m.total = player.total();
  }
  return m;
}

void refreshScreen(uint32_t now) {
  const UiModel next = model();
  const uint32_t since = now - lastScreen;
  // Throttle progress-only redraws so drawing never paces the keystrokes.
  const bool progressOnly = next.screen == Screen::TRANSMIT && shown.screen == Screen::TRANSMIT;
  // Unlinked screens carry a blinking status dot, so refresh them faster.
  const uint32_t idleRefresh = next.link == LinkState::SECURE ? 1000 : 250;
  if ((next != shown && (!progressOnly || since >= 100)) || since >= idleRefresh) {
    ui.render(next);
    shown = next;
    lastScreen = now;
  }
  if (next.link != previousLink) {
    LOG_I("Bluetooth state: %s", linkName(next.link));
    previousLink = next.link;
  }
}
}  // namespace

void setup() {
  Serial.begin(115200);  // never wait for a USB serial host
  board::begin(DISPLAY_BRIGHTNESS);
  display.begin(board::display(), DISPLAY_BRIGHTNESS);
  LOG_I("%s %s on %s, profiles=%u", PROJECT_NAME, FIRMWARE_VERSION, BOARD_LABEL, PROFILE_COUNT);
  keyboard.begin(BLE_NAME, KEYSTROKE_MS);
  display.splash(PROJECT_NAME, FIRMWARE_VERSION);
  // Only enter this maintenance action when both buttons are held at boot. No
  // macro input is processed until they are released.
  holdAllButtonsAtBoot(
      kBondHoldMs,
      [](uint32_t remaining) { display.holdProgress("BLUETOOTH", remaining, remaining == 0); },
      [] {
        keyboard.clearBonds();
        LOG_I("Bluetooth bonds cleared by startup A+B hold");
      });
  const uint8_t profile = store.load(PROFILE_COUNT);
  LOG_I("Active profile %u: %s", profile + 1, PROFILES[profile].name);
  board::update();
  controller.begin(board::isPressed(0), board::isPressed(1), millis(), profile, maskFor(profile));
  batteryTimer.due(millis());  // first reading happens here, the next one 30 s later
  readBattery();
  refreshScreen(millis());
}

void loop() {
  board::update();
  const uint32_t now = millis();
  const bool ready = keyboard.ready();
  const uint32_t generation = keyboard.linkGeneration();
  // Input during disconnection is discarded and a macro never resumes later.
  // Any link change (even a quick resubscribe) may have dropped a stroke.
  if (!ready) stopPlayback("link lost");
  else if (generation != lastGeneration) stopPlayback("link changed");
  lastGeneration = generation;
  handle(controller.update(board::isPressed(0), board::isPressed(1), now, ready, player.active()), now);
  Stroke stroke;
  if (player.next(now, keyboard.idle(), stroke) && !keyboard.send(stroke.mods, stroke.usage)) {
    stopPlayback("transport rejected stroke");
  }
  keyboard.update(now);
  if (batteryTimer.due(now)) readBattery();
  refreshScreen(now);
  delay(1);
}
