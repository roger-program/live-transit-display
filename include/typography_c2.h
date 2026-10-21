#pragma once
#include "font.h"
#include "renderer.h"

namespace typography {
// Manually drawn geometric capitals, referenced to the original C masters.
// Original 13 glyphs are unchanged. M/W use five columns for open interiors;
// other square bowls retain two-column counters and one-pixel stems.
inline constexpr transit::Glyph LabelsC2[] = {
    {'A', {6, 9, 15, 9, 9}},     {'B', {14, 9, 14, 9, 14}},   {'C', {15, 8, 8, 8, 15}},
    {'D', {14, 9, 9, 9, 14}},    {'E', {15, 8, 14, 8, 15}},   {'F', {15, 8, 14, 8, 8}},
    {'G', {15, 8, 11, 9, 15}},   {'H', {9, 9, 15, 9, 9}},     {'I', {7, 2, 2, 2, 7}},
    {'J', {7, 1, 1, 9, 15}},     {'K', {9, 10, 12, 10, 9}},   {'L', {8, 8, 8, 8, 15}},
    {'M', {17, 27, 21, 17, 17}}, {'N', {9, 13, 11, 9, 9}},    {'O', {15, 9, 9, 9, 15}},
    {'P', {15, 9, 15, 8, 8}},    {'Q', {15, 9, 9, 11, 1}},    {'R', {15, 9, 15, 10, 9}},
    {'S', {15, 8, 15, 1, 15}},   {'T', {15, 4, 4, 4, 4}},     {'U', {9, 9, 9, 9, 15}},
    {'V', {9, 9, 9, 10, 4}},     {'W', {17, 17, 21, 27, 17}}, {'X', {9, 6, 4, 6, 9}},
    {'Y', {9, 9, 6, 4, 4}},      {'Z', {15, 1, 6, 8, 15}},
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
  if (c2Glyph(character).character == '?' || character == 'I')
    return 3;
  return character == 'M' || character == 'W' ? 5 : 4;
}
// Unique normalized missing characters; never substitute another font silently.
inline std::string c2Missing(const std::string &text) {
  std::string missing;
  for (char character : transit::pixelText(text))
    if (character != ' ' && c2Glyph(character).character == '?' &&
        missing.find(character) == std::string::npos)
      missing += character;
  return missing;
}
inline bool c2Supports(const std::string &text) { return c2Missing(text).empty(); }
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
