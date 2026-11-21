#pragma once
#include <array>
#include <cstdint>
#include <string>
#include <string_view>

namespace transit {
inline constexpr size_t MaxBrightnessRecordBytes = 4096;
struct BrightnessChange {
  uint16_t minute = 0;
  uint8_t level = 0;
};
struct BrightnessSettings {
  uint64_t revision = 0;
  uint8_t maximumRaw = 36;
  bool configured = false, automatic = false;
  uint8_t level = 0;
  std::array<BrightnessChange, 5> entries{};
  uint8_t count = 0;
};
bool parseBrightnessSettings(std::string_view json, BrightnessSettings &out);
std::string brightnessRecord(const BrightnessSettings &settings);
int madridMinuteAt(int64_t utcSeconds);
uint8_t brightnessRaw(uint8_t level, uint8_t maximum);
uint8_t effectiveBrightness(const BrightnessSettings &settings, int64_t utcSeconds, bool validClock,
                            uint8_t fallback);
} // namespace transit
