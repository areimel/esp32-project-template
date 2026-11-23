#pragma once
#include <stdint.h>
#include <string.h>
#include "Display.h"
#include "Macro.h"

#if !FEATURE_DISPLAY
#error "The macro clicker needs display.enabled on a board with a display"
#endif

enum class LinkState : uint8_t { SEARCHING, HANDSHAKE, SECURE };
enum class Screen : uint8_t { HOME, PICKER, TRANSMIT };

// Plain description of what the display should show. main fills it; Ui draws it.
struct UiModel {
  Screen screen = Screen::HOME;
  LinkState link = LinkState::SEARCHING;
  int battery = -1;  // percent, -1 unknown
  bool charging = false;
  const char* deviceName = "";
  uint8_t profileIndex = 0, profileCount = 0;
  const Profile* profiles = nullptr;  // active = profiles[profileIndex]
  uint8_t highlighted = 0;            // picker
  const char* macroLabel = "";        // transmit
  uint16_t sent = 0, total = 0;       // transmit

  bool operator==(const UiModel& o) const {
    return screen == o.screen && link == o.link && battery == o.battery &&
           charging == o.charging && same(deviceName, o.deviceName) &&
           profileIndex == o.profileIndex && profileCount == o.profileCount &&
           profiles == o.profiles && highlighted == o.highlighted &&
           same(macroLabel, o.macroLabel) && sent == o.sent && total == o.total;
  }
  bool operator!=(const UiModel& o) const { return !(*this == o); }

 private:
  static bool same(const char* a, const char* b) {
    return a == b || (a && b && strcmp(a, b) == 0);
  }
};

// Home, Picker and Transmit screens drawn on the shared Display canvas. The
// splash, boot-hold and fatal screens come from Display itself.
class Ui {
 public:
  explicit Ui(Display& display) : display_(display) {}
  void render(const UiModel& m);  // full redraw + push

 private:
  Display& display_;
};
