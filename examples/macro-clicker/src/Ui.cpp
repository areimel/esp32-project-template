#include "Ui.h"
#include <Arduino.h>
#include "Draw.h"
#include "Theme.h"

namespace {
using namespace Theme;
using namespace draw;
constexpr int W = 240, H = 135;

void frameFooter(M5Canvas& c, const char* left, const char* right) {
  c.drawFastHLine(6, 118, W - 12, LINE);
  if (left) label(c, 8, 124, left, DIM);
  if (right) labelRight(c, W - 8, 124, right, DIM);
}

void drawLinkTag(M5Canvas& c, LinkState link) {
  const char* tag = link == LinkState::SECURE      ? "SECURE"
                    : link == LinkState::HANDSHAKE ? "HANDSHAKE"
                                                   : "SEARCHING";
  int tw = textWidth(tag, 1, 1) + 12;
  int tx = W - 8 - tw;
  if (link == LinkState::SECURE) {
    c.fillRect(tx, 18, tw, 12, ACCENT);
    label(c, tx + 6, 20, tag, BG);
    return;
  }
  const bool on = (millis() / 500) & 1;
  const uint16_t col = link == LinkState::HANDSHAKE ? INK : DIM;
  c.drawRect(tx, 18, tw, 12, col == INK ? INK : LINE);
  label(c, tx + 6, 20, tag, col);
  if (on) square(c, tx - 7, 22, 4, ACCENT);
}

// ---- screens --------------------------------------------------------------
void drawHome(Display& display, const UiModel& m) {
  M5Canvas& c = display.canvas();
  char buf[32];
  const Profile* p =
      (m.profiles && m.profileIndex < m.profileCount) ? &m.profiles[m.profileIndex] : nullptr;

  snprintf(buf, sizeof buf, "PROFILE %02u/%02u", m.profileIndex + 1u, m.profileCount);
  display.header(buf, nullptr, m.battery, m.charging);

  upper(buf, 16, p ? p->name : "");
  label(c, 8, 17, buf, INK, 2, 0);
  drawLinkTag(c, m.link);
  c.drawFastHLine(6, 35, W - 12, LINE);

  // Grid: column heads, 2 rows x 3 gestures.
  const int gx = 24, cw = 72, headY = 40, row0 = 51, rh = 32;
  static const char* const heads[3] = {"TAP", "DBL", "HOLD"};
  for (int g = 0; g < 3; ++g) label(c, gx + g * cw + 8, headY, heads[g], DIM);
  c.drawFastHLine(6, 50, W - 12, LINE);
  c.drawFastHLine(6, row0 + rh, W - 12, LINE);
  for (int g = 0; g < 3; ++g) c.drawFastVLine(gx + g * cw - 2, 37, 80, LINE);
  for (int r = 0; r < 2; ++r) {
    int y = row0 + r * rh;
    label(c, 10, y + rh / 2 - 8, r ? "B" : "A", ACCENT, 2, 0);
    for (int g = 0; g < 3; ++g) {
      int x = gx + g * cw;
      const Macro* mc = p ? &p->slots[r][g] : nullptr;
      int cy = y + rh / 2 - 4;
      if (mc && mc->count > 0) {
        square(c, x + 3, cy + 2, 3, ACCENT);
        upper(buf, 12, mc->label);
        label(c, x + 8, cy, buf, INK, 1, 0);
      } else {
        c.drawFastHLine(x + 8, cy + 4, 8, LINE);  // unassigned: dim dash
      }
    }
  }
  cornerTicks(c, 2, 2, W - 3, H - 3, LINE);
  frameFooter(c, "A+B  PROFILES", "NO. 0001");
}

void drawPicker(M5Canvas& c, const UiModel& m) {
  char buf[24];
  square(c, 8, 5, 6, ACCENT);
  label(c, 18, 5, "SELECT PROFILE", INK);
  snprintf(buf, sizeof buf, "%u/%u", m.highlighted + 1u, m.profileCount);
  labelRight(c, W - 8, 5, buf, DIM);
  c.drawFastHLine(6, 16, W - 12, LINE);

  const int rowH = 19, visible = 5, top = 20;
  int first = 0;
  if (m.profileCount > visible) {
    first = int(m.highlighted) - visible / 2;
    if (first < 0) first = 0;
    if (first > m.profileCount - visible) first = m.profileCount - visible;
  }
  for (int i = 0; i < visible && first + i < m.profileCount; ++i) {
    int idx = first + i, y = top + i * rowH;
    bool hi = idx == m.highlighted;
    uint16_t fg = hi ? BG : INK, sub = hi ? BG : DIM;
    if (hi) c.fillRect(6, y, W - 12, rowH - 2, ACCENT);
    snprintf(buf, sizeof buf, "%02d", idx + 1);
    label(c, 12, y + 5, buf, sub);
    upper(buf, 16, m.profiles ? m.profiles[idx].name : "");
    label(c, 34, y + 1, buf, fg, 2, 0);
    if (idx == m.profileIndex) labelRight(c, W - 12, y + 5, "ACTIVE", hi ? BG : ACCENT);
  }
  if (first > 0) c.fillTriangle(W - 4, 18, W - 7, 22, W - 1, 22, DIM);
  if (first + visible < m.profileCount) c.fillTriangle(W - 4, 116, W - 7, 112, W - 1, 112, DIM);
  frameFooter(c, "B NEXT  A ENGAGE  A+B CANCEL", nullptr);
}

void drawTransmit(M5Canvas& c, const UiModel& m) {
  char buf[24];
  bool blink = (millis() / 400) & 1;
  c.fillRect(0, 0, W, 15, ACCENT);
  label(c, 8, 4, "TRANSMITTING", BG, 1, 2);
  if (blink) square(c, W - 14, 4, 6, BG);
  else c.drawRect(W - 14, 4, 6, 6, BG);

  upper(buf, 12, m.macroLabel);
  int size = textWidth(buf, 3, 0) <= W - 16 ? 3 : 2;
  labelCenter(c, W / 2, 36, buf, INK, size, 0);

  const int bx = 12, by = 78, bw = W - 24, bh = 12;
  c.drawRect(bx, by, bw, bh, LINE);
  int fill = m.total ? int((uint32_t(m.sent) * (bw - 4)) / m.total) : 0;
  if (fill > bw - 4) fill = bw - 4;
  c.fillRect(bx + 2, by + 2, fill, bh - 4, ACCENT);
  for (int i = 1; i < 4; ++i) c.drawFastVLine(bx + bw * i / 4, by + bh + 1, 3, LINE);
  snprintf(buf, sizeof buf, "%u / %u", m.sent, m.total);
  labelCenter(c, W / 2, 98, buf, INK);
  frameFooter(c, nullptr, nullptr);
  labelCenter(c, W / 2, 124, "PRESS ANY BUTTON TO ABORT", DIM);
}
}  // namespace

void Ui::render(const UiModel& m) {
  M5Canvas& c = display_.canvas();
  c.fillScreen(BG);
  switch (m.screen) {
    case Screen::HOME: drawHome(display_, m); break;
    case Screen::PICKER: drawPicker(c, m); break;
    case Screen::TRANSMIT: drawTransmit(c, m); break;
  }
  display_.push();
}
