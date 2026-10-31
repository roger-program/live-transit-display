#pragma once
#include "regular_font.h"

namespace provisioning {
// Prototype defaults only. Keep setup-network credentials separate from saved home Wi-Fi.
// Replace these defaults with provisioned per-device values before production.
inline constexpr char SetupSsid[] = "METRO-DISPLAY";
inline constexpr char SetupPassword[] = "BCNMETRO";
static_assert(sizeof(SetupPassword) >= 9 && sizeof(SetupPassword) <= 64,
              "Protected setup requires an 8-63 byte passphrase");

inline void renderSetupScreen(transit::Framebuffer &frame, const std::string &ssid,
                              const std::string &password) {
  frame.clear();
  typography::drawC2(frame, "1. WIFI", 0, 0, 0xffffff, 0, frame.width());
  typography::drawC2(frame, ssid, 0, 6, 0xffffff, 0, frame.width());
  typography::drawC2(frame, password, 0, 12, 0xffffff, 0, frame.width());
  // Five 5-pixel rows: one-pixel line gaps, four blank pixels between steps.
  typography::drawC2(frame, "2. ABRE WEB", 0, 21, 0xffffff, 0, frame.width());
  typography::drawC2(frame, "192.168.4.1", 0, 27, 0xffffff, 0, frame.width());
}
} // namespace provisioning
