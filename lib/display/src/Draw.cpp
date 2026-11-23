#include "Draw.h"
#if FEATURE_DISPLAY
#include <string.h>

namespace draw {
int textWidth(const char* s, int size, int spacing) {
  int n = int(strlen(s));
  return n ? n * (6 * size + spacing) - spacing : 0;
}

void label(M5Canvas& c, int x, int y, const char* s, uint16_t color, int size, int spacing) {
  c.setFont(&fonts::Font0);
  c.setTextSize(size);
  c.setTextColor(color);
  for (; *s; ++s) {
    c.setCursor(x, y);
    c.write(uint8_t(*s));
    x += 6 * size + spacing;
  }
}

void labelRight(M5Canvas& c, int right, int y, const char* s, uint16_t color, int size,
                int spacing) {
  label(c, right - textWidth(s, size, spacing), y, s, color, size, spacing);
}

void labelCenter(M5Canvas& c, int cx, int y, const char* s, uint16_t color, int size,
                 int spacing) {
  label(c, cx - textWidth(s, size, spacing) / 2, y, s, color, size, spacing);
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

namespace {
// dx/dy are +1/-1 pointing inward.
void tick(M5Canvas& c, int x, int y, int dx, int dy, uint16_t color) {
  c.drawFastHLine(dx > 0 ? x : x - 5, y, 6, color);
  c.drawFastVLine(x, dy > 0 ? y : y - 5, 6, color);
}
}  // namespace

void cornerTicks(M5Canvas& c, int x0, int y0, int x1, int y1, uint16_t color) {
  tick(c, x0, y0, 1, 1, color);
  tick(c, x1, y0, -1, 1, color);
  tick(c, x0, y1, 1, -1, color);
  tick(c, x1, y1, -1, -1, color);
}
}  // namespace draw
#endif  // FEATURE_DISPLAY
