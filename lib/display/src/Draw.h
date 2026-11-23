#pragma once
#if FEATURE_DISPLAY
#include <M5GFX.h>
#include <stddef.h>
#include <stdint.h>

// Small drawing helpers for the 240x135 canvas, shared by Display and any app
// that composes its own screens. Letter-spaced Font0 text; ASCII only.
namespace draw {
int textWidth(const char* s, int size, int spacing);
void label(M5Canvas& c, int x, int y, const char* s, uint16_t color, int size = 1, int spacing = 1);
void labelRight(M5Canvas& c, int right, int y, const char* s, uint16_t color, int size = 1,
                int spacing = 1);
void labelCenter(M5Canvas& c, int cx, int y, const char* s, uint16_t color, int size = 1,
                 int spacing = 1);
void upper(char* out, size_t cap, const char* in);  // ASCII uppercase copy, always terminated
void square(M5Canvas& c, int x, int y, int s, uint16_t color);
// L-shaped crosshair ticks in the four corners of the given rectangle.
void cornerTicks(M5Canvas& c, int x0, int y0, int x1, int y1, uint16_t color);
}  // namespace draw
#endif  // FEATURE_DISPLAY
