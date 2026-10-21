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
enum class Face { BlackRegular, WhiteRegular, WhiteBold };
enum class Comparison { BlackWhite, BlackBold, WhiteBold };
inline Face faceFor(Comparison comparison, int side, bool swapped) {
  const bool right = (side == 1) != swapped;
  if (comparison == Comparison::BlackWhite)
    return right ? Face::WhiteRegular : Face::BlackRegular;
  if (comparison == Comparison::BlackBold)
    return right ? Face::WhiteBold : Face::BlackRegular;
  return right ? Face::WhiteBold : Face::WhiteRegular;
}
inline const char *faceName(Face face) {
  return face == Face::BlackRegular ? "A BLK" : face == Face::WhiteRegular ? "B WHT" : "C BLD";
}
struct BoldGlyph {
  char character;
  uint8_t rows[7];
};
// Original hand-designed 6x7 diagnostic glyphs. Not dilation or copied font data.
// Two-column stems and open two-column counters; 7px advance keeps L10 within 29px.
inline constexpr BoldGlyph Bold[] = {
    {'L', {48, 48, 48, 48, 48, 63, 63}}, {'0', {30, 51, 51, 51, 51, 51, 30}},
    {'1', {12, 28, 12, 12, 12, 30, 30}}, {'2', {30, 51, 3, 6, 12, 24, 63}},
    {'3', {62, 3, 3, 30, 3, 3, 62}},     {'4', {51, 51, 51, 63, 3, 3, 3}},
    {'5', {63, 48, 48, 62, 3, 3, 62}},   {'9', {30, 51, 51, 31, 3, 3, 30}},
};
inline int boldWidth(const char *label) {
  int width = 0;
  for (; *label; ++label)
    width += 7;
  return width ? width - 1 : 0;
}
inline void drawBold(transit::Framebuffer &frame, const char *label, int x, int y) {
  for (; *label; ++label, x += 7)
    for (const auto &glyph : Bold)
      if (glyph.character == *label)
        for (int row = 0; row < 7; ++row)
          for (int col = 0; col < 6; ++col)
            if (glyph.rows[row] & (1 << (5 - col)))
              frame.pixel(x + col, y + row, 0xffffff);
}
inline void render(transit::Framebuffer &frame, unsigned index, bool swapped = false,
                   Comparison comparison = Comparison::BlackWhite) {
  frame.clear();
  const auto &sample = Samples[index % Count];
  for (int side = 0; side < 2; ++side) {
    const Face face = faceFor(comparison, side, swapped);
    const int x = side == 0 ? 1 : 34;
    transit::drawText(frame, faceName(face), x, 1, 0xffffff, x, x + 29);
    frame.rect(x, 10, 29, 11, sample.background);
    if (face == Face::WhiteBold) {
      drawBold(frame, sample.label, x + (29 - boldWidth(sample.label)) / 2, 12);
    } else {
      const int labelX = x + (29 - transit::textWidth(sample.label)) / 2;
      transit::drawText(frame, sample.label, labelX, 13, face == Face::WhiteRegular ? 0xffffff : 0,
                        x, x + 29);
    }
  }
  if (index % Count == 5 || index % Count == 6)
    transit::drawText(frame, "N/S", 23, 25, 0xffffff, 0, 64);
}
} // namespace badgecontrast
