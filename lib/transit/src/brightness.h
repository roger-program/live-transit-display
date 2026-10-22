#pragma once
#include <cstdint>
#include <string>
namespace transit {
// The output accepts any configured byte; serial presets are deliberately bounded.
inline bool brightnessPreset(const std::string &command, uint8_t &value) {
  if (command == "b4")
    value = 4;
  else if (command == "b8")
    value = 8;
  else if (command == "b16")
    value = 16;
  else if (command == "b32")
    value = 32;
  else
    return false;
  return true;
}
} // namespace transit
