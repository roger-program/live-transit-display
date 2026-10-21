#pragma once
#include "renderer.h"

// Offline experiment inputs, not a production line-identity lookup.
namespace badgecontrast {
struct Sample {
  const char *label;
  transit::Color background;
};
// Backend-verified TMB COLOR_LINIA; L9/L10 branches share their parent colour.
inline constexpr Sample Samples[] = {{"L1", 0xce1126},  {"L2", 0x93248f}, {"L3", 0x1eb53a},
                                     {"L4", 0xf7a30e},  {"L5", 0x005a97}, {"L9", 0xfb712b},
                                     {"L10", 0x00a6d6}, {"L11", 0x89b94c}};
inline constexpr unsigned Count = sizeof(Samples) / sizeof(Samples[0]);
inline void render(transit::Framebuffer &frame, unsigned index, bool swapped = false) {
  frame.clear();
  const auto &sample = Samples[index % Count];
  for (int side = 0; side < 2; ++side) {
    const bool white = (side == 1) != swapped;
    const int x = side == 0 ? 1 : 34;
    transit::drawText(frame, white ? "WHITE" : "BLACK", x, 1, 0xffffff, x, x + 29);
    frame.rect(x, 10, 29, 11, sample.background);
    const int labelX = x + (29 - transit::textWidth(sample.label)) / 2;
    transit::drawText(frame, sample.label, labelX, 13, white ? 0xffffff : 0, x, x + 29);
  }
  if (index % Count == 5 || index % Count == 6)
    transit::drawText(frame, "N/S", 23, 25, 0xffffff, 0, 64);
}
} // namespace badgecontrast
