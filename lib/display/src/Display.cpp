#include "Display.h"
#if FEATURE_DISPLAY
#include <Arduino.h>
#include <string.h>
#include "Theme.h"

namespace {
using namespace Theme;
constexpr int W = 240, H = 135;
constexpr uint32_t kHoldMs = 3000;  // A+B hold before bonds are cleared
constexpr int kSplashSteps = 5;

// ---- helpers --------------------------------------------------------------
int textWidth(const char* s, int size, int spacing) {
  int n = int(strlen(s));
  return n ? n * (6 * size + spacing) - spacing : 0;
}

// Letter-spaced Font0 text; ASCII only.
void label(M5Canvas& c, int x, int y, const char* s, uint16_t color, int size = 1,
           int spacing = 1) {
  c.setFont(&fonts::Font0);
  c.setTextSize(size);
  c.setTextColor(color);
  for (; *s; ++s) {
    c.setCursor(x, y);
    c.write(uint8_t(*s));
    x += 6 * size + spacing;
  }
}

void labelRight(M5Canvas& c, int right, int y, const char* s, uint16_t color, int size = 1,
                int spacing = 1) {
  label(c, right - textWidth(s, size, spacing), y, s, color, size, spacing);
}

void upper(char* out, size_t cap, const char* in) {
  size_t i = 0;
  for (; in && in[i] && i + 1 < cap; ++i) {
    char ch = in[i];
    out[i] = (ch >= 'a' && ch <= 'z') ? char(ch - 32) : ch;
  }
  out[i] = 0;
}

void square(M5Canvas& c, int x, int y, int s, uint16_t color) { c.fillRect(x, y, s, s, color); }

// L-shaped crosshair tick at a corner; dx/dy are +1/-1 pointing inward.
void tick(M5Canvas& c, int x, int y, int dx, int dy, uint16_t color) {
  c.drawFastHLine(dx > 0 ? x : x - 5, y, 6, color);
  c.drawFastVLine(x, dy > 0 ? y : y - 5, 6, color);
}

void cornerTicks(M5Canvas& c, int x0, int y0, int x1, int y1, uint16_t color) {
  tick(c, x0, y0, 1, 1, color);
  tick(c, x1, y0, -1, 1, color);
  tick(c, x0, y1, 1, -1, color);
  tick(c, x1, y1, -1, -1, color);
}

// Battery body, nub, fill and readout, right-aligned at `right`.
// Returns the left edge of the readout cluster so callers can place text beside it.
int drawBattery(M5Canvas& c, int right, int y, int pct, bool charging) {
  int x = right - 18;  // body 16x7 plus 2px nub, right aligned
  c.drawRect(x, y, 16, 7, DIM);
  c.fillRect(x + 16, y + 2, 2, 3, DIM);
  if (pct >= 0) {
    int w = (pct * 12 + 50) / 100;
    if (pct > 0 && w < 1) w = 1;
    c.fillRect(x + 2, y + 2, w, 3, charging ? ACCENT : (pct <= 15 ? ALERT : INK));
  }
  char buf[8];
  if (pct < 0) snprintf(buf, sizeof buf, "--");
  else snprintf(buf, sizeof buf, "%d%%", pct);
  labelRight(c, x - 4, y, buf, pct < 0 ? DIM : INK, 1, 0);
  int left = x - 4 - textWidth(buf, 1, 0);
  if (charging) {
    int chgRight = x - 10 - textWidth(buf, 1, 0);
    labelRight(c, chgRight, y, "CHG", ACCENT, 1, 1);
    left = chgRight - textWidth("CHG", 1, 1);
  }
  return left;
}
}  // namespace

// ---- Display --------------------------------------------------------------
void Display::begin(M5GFX& gfx, uint8_t brightness) {
  gfx_ = &gfx;
  gfx.setRotation(1);
  gfx.setBrightness(brightness);
  canvas_.setPsram(true);
  canvas_.setColorDepth(16);
  canvas_.createSprite(W, H);
  canvas_.fillScreen(BG);
  push();
}

void Display::push() { canvas_.pushSprite(gfx_, 0, 0); }

void Display::header(const char* title, const char* status, int battery, bool charging) {
  char buf[40];
  square(canvas_, 8, 5, 6, ACCENT);
  upper(buf, sizeof buf, title);
  label(canvas_, 18, 5, buf, DIM);
  int left = drawBattery(canvas_, W - 8, 4, battery, charging);
  if (status) labelRight(canvas_, left - 8, 5, status, DIM);
}

void Display::splash(const char* name, const char* version) {
  char title[32];
  upper(title, sizeof title, name);
  for (int s = 0; s <= kSplashSteps; ++s) {
    canvas_.fillScreen(BG);
    cornerTicks(canvas_, 2, 2, W - 3, H - 3, ACCENT);
    square(canvas_, 12, 12, 6, ACCENT);
    label(canvas_, 24, 12, title, INK, 1, 2);
    labelRight(canvas_, W - 12, 12, version ? version : "", DIM);
    canvas_.drawFastHLine(12, 26, W - 24, LINE);
    canvas_.drawRect(12, 112, W - 24, 8, LINE);
    canvas_.fillRect(14, 114, (W - 28) * s / kSplashSteps, 4, ACCENT);
    push();
    delay(s == kSplashSteps ? 100 : 120);
  }
}

void Display::holdProgress(const char* action, uint32_t remainingMs, bool done) {
  char buf[40];
  canvas_.fillScreen(BG);
  canvas_.fillRect(0, 0, W, 15, ACCENT);
  label(canvas_, 8, 4, "WARNING", BG, 1, 2);
  labelRight(canvas_, W - 8, 4, "MAINTENANCE", BG);
  cornerTicks(canvas_, 2, 18, W - 3, H - 3, ACCENT);
  label(canvas_, 14, 26, "RESET", INK, 3, 1);
  upper(buf, 12, action);
  label(canvas_, 14, 52, buf, INK, 3, 1);
  if (!done) {
    uint32_t left = remainingMs > kHoldMs ? kHoldMs : remainingMs;
    snprintf(buf, sizeof buf, "KEEP A+B HELD %lu", (unsigned long)((left + 999) / 1000));
    label(canvas_, 14, 92, buf, ACCENT);
    canvas_.drawRect(14, 106, W - 28, 8, LINE);
    canvas_.fillRect(16, 108, int((W - 32) * (kHoldMs - left) / kHoldMs), 4, ACCENT);
  } else {
    label(canvas_, 14, 88, "BONDS CLEARED. RELEASE A+B.", ACCENT);
    label(canvas_, 14, 102, "FORGET DEVICE ON HOST,", INK);
    label(canvas_, 14, 114, "THEN PAIR.", INK);
  }
  push();
}

void Display::fatal(const char* msg) {
  canvas_.fillScreen(BG);
  canvas_.fillRect(0, 0, W, 15, ALERT);
  label(canvas_, 8, 4, "FAULT", INK, 1, 2);
  labelRight(canvas_, W - 8, 4, "HALTED", INK);
  cornerTicks(canvas_, 2, 18, W - 3, H - 3, ALERT);
  char buf[40];  // wrap at 36 columns, 6 lines max
  const char* s = msg ? msg : "";
  for (int line = 0; line < 6 && *s; ++line) {
    size_t n = 0;
    while (*s && n < 36) buf[n++] = *s++;
    buf[n] = 0;
    label(canvas_, 14, 30 + line * 14, buf, INK, 1, 0);
  }
  push();
}
#endif  // FEATURE_DISPLAY
