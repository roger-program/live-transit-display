#pragma once
#include "regular_font.h"

namespace provisioning {
// Prototype defaults only. Keep setup-network credentials separate from saved home Wi-Fi.
// Replace these defaults with provisioned per-device values before production.
inline constexpr char SetupSsid[] = "metro-display";
inline constexpr char SetupPassword[] = "bcnmetro";
static_assert(sizeof(SetupPassword) >= 9 && sizeof(SetupPassword) <= 64,
              "Protected setup requires an 8-63 byte passphrase");

// Setup-only lowercase: five-pixel bodies/ascenders, with two extra descender rows.
// Keep C2 normalization and all arrival/badge glyphs unchanged.
struct SetupGlyph {
  char character;
  int columns;
  uint8_t rows[7];
};
inline constexpr SetupGlyph SetupLowercase[] = {
    {'a', 3, {0, 0, 6, 5, 7}}, {'b', 3, {4, 4, 6, 5, 6}},       {'c', 3, {0, 0, 3, 4, 3}},
    {'d', 3, {1, 1, 3, 5, 3}}, {'e', 3, {0, 0, 7, 6, 3}},       {'i', 1, {1, 0, 1, 1, 1}},
    {'l', 2, {2, 2, 2, 2, 1}}, {'m', 5, {0, 0, 26, 21, 21}},    {'n', 3, {0, 0, 6, 5, 5}},
    {'o', 3, {0, 0, 7, 5, 7}}, {'p', 3, {0, 0, 6, 5, 6, 4, 4}}, {'r', 3, {0, 0, 3, 4, 4}},
    {'s', 3, {0, 0, 3, 2, 6}}, {'t', 3, {2, 7, 2, 2, 3}},       {'y', 3, {0, 0, 5, 5, 3, 1, 6}},
};
inline const SetupGlyph *setupLowercase(char character) {
  for (const auto &glyph : SetupLowercase)
    if (glyph.character == character)
      return &glyph;
  return nullptr;
}
inline int setupTextWidth(const std::string &text) {
  int width = 0;
  for (char character : text) {
    const auto *glyph = setupLowercase(character);
    width += (glyph ? glyph->columns : typography::c2Columns(character)) + 1;
  }
  return width ? width - 1 : 0;
}
inline void drawSetupCredential(transit::Framebuffer &frame, const std::string &text, int y,
                                int x = 0) {
  for (char character : text) {
    const auto *glyph = setupLowercase(character);
    if (glyph) {
      for (int row = 0; row < 7; ++row)
        for (int col = 0; col < glyph->columns; ++col)
          if (glyph->rows[row] & (1 << (glyph->columns - 1 - col)))
            frame.pixel(x + col, y + row, 0xffffff);
      x += glyph->columns + 1;
    } else {
      // Unsupported lowercase stays visibly unsupported rather than changing its case.
      const char supported = character >= 'a' && character <= 'z' ? '?' : character;
      typography::drawC2(frame, std::string(1, supported), x, y, 0xffffff, 0, frame.width());
      x += typography::c2Columns(supported) + 1;
    }
  }
}
inline void renderSetupScreen(transit::Framebuffer &frame, const std::string &ssid,
                              const std::string &password) {
  frame.clear();
  drawSetupCredential(frame, ssid, 0);
  typography::drawC2(frame, "PW", 0, 10, 0xffffff, 0, frame.width());
  drawSetupCredential(frame, password, 10, 14);
  typography::drawC2(frame, "OPEN HTTP", 0, 20, 0xffffff, 0, frame.width());
  typography::drawC2(frame, "192.168.4.1", 0, 26, 0xffffff, 0, frame.width());
}
} // namespace provisioning
