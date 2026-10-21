#pragma once
#include "font.h"
#include "renderer.h"

namespace typography {
// Manually drawn diagnostic subset, not a generated browser table. Four-column
// square bowls retain two-column counters; one-pixel stems stay one pixel.
inline constexpr transit::Glyph LabelsC2[] = {
    {'A', {6, 9, 15, 9, 9}},  {'E', {15, 8, 14, 8, 15}}, {'I', {7, 2, 2, 2, 7}},
    {'L', {8, 8, 8, 8, 15}},  {'N', {9, 13, 11, 9, 9}},  {'O', {15, 9, 9, 9, 15}},
    {'P', {15, 9, 15, 8, 8}}, {'R', {15, 9, 15, 10, 9}}, {'S', {15, 8, 15, 1, 15}},
    {'T', {15, 4, 4, 4, 4}},  {'U', {9, 9, 9, 9, 15}},   {'V', {9, 9, 9, 10, 4}},
    {'Z', {15, 1, 6, 8, 15}},
};
// The existing compact question-mark design, not an implicit mixed-font fallback.
inline constexpr transit::Glyph C2Fallback = {'?', {6, 1, 2, 0, 2}};
inline const transit::Glyph &c2Glyph(char character) {
  for (const auto &glyph : LabelsC2)
    if (glyph.character == character)
      return glyph;
  return C2Fallback;
}
inline int c2Columns(char character) {
  return c2Glyph(character).character == '?' || character == 'I' ? 3 : 4;
}
inline bool c2Supports(const std::string &text) {
  for (char character : transit::pixelText(text))
    if (character != ' ' && c2Glyph(character).character == '?')
      return false;
  return true;
}
inline int c2Width(const std::string &text) {
  int x = 0, right = 0;
  for (char character : transit::pixelText(text)) {
    if (character == ' ') {
      x += 3;
      continue;
    }
    right = x + c2Columns(character);
    x = right + 1;
  }
  return right;
}
inline void drawC2(transit::Framebuffer &frame, const std::string &text, int x, int y,
                   transit::Color color, int left, int right, int offset = 0) {
  for (char character : transit::pixelText(text)) {
    if (character == ' ') {
      x += 3;
      continue;
    }
    const auto &glyph = c2Glyph(character);
    const int columns = c2Columns(character);
    for (int row = 0; row < 5; ++row)
      for (int col = 0; col < columns; ++col) {
        const int px = x + col - offset;
        if (px >= left && px < right && (glyph.rows[row] & (1 << (columns - 1 - col))))
          frame.pixel(px, y + row, color);
      }
    x += columns + 1;
  }
}
} // namespace typography
