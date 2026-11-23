#pragma once
#if FEATURE_DISPLAY
#include <M5GFX.h>
#include <stdint.h>

// Shared screen chrome for the 240x135 panel. Drawing only: an app composes its
// own screens on canvas() and calls push() to show them. Colors come from Theme.h.
class Display {
 public:
  // Creates the 240x135 sprite and clears the panel. Rotation 1 and brightness
  // match board::begin(), so this is safe to call after it.
  void begin(M5GFX& gfx, uint8_t brightness);
  M5Canvas& canvas() { return canvas_; }
  void push();  // copy the sprite to the panel

  // Title bar: marker, title, optional status left of the battery readout
  // (battery -1 shows "--"). Draws only; does not clear or push.
  void header(const char* title, const char* status, int battery, bool charging);
  void splash(const char* name, const char* version);  // ~700 ms, blocking
  // Bond-reset screen. `action` is shown large (truncated to 11 characters).
  void holdProgress(const char* action, uint32_t remainingMs, bool done);
  void fatal(const char* msg);  // red halt screen; pushes

 private:
  M5GFX* gfx_ = nullptr;
  M5Canvas canvas_;
};
#endif  // FEATURE_DISPLAY
