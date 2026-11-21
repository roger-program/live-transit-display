#pragma once
#include <algorithm>
#include <cstdint>
#include <string_view>

namespace transit {
inline constexpr uint8_t PreviewMaximumRaw = 64;
inline constexpr uint64_t PreviewTtlMs = 30000;
struct PreviewCommand {
  uint64_t generation = 0;
  uint64_t remainingMs = 0;
  uint8_t raw = 0;
  bool active = false;
};
bool parsePreview(std::string_view json, std::string_view boot, PreviewCommand &out);
// Owned by the render/control mutex. Repeated or delayed responses cannot renew a lease.
class BrightnessPreview {
public:
  bool receive(const PreviewCommand &command, uint64_t requestAt, uint64_t receivedAt) {
    if (seen_ && command.generation < generation_)
      return false;
    if (!seen_ || command.generation > generation_) {
      seen_ = true;
      generation_ = command.generation;
      receivedAt_ = receivedAt;
      deadline_ = requestAt + std::min(command.remainingMs, PreviewTtlMs);
      active_ = command.active && command.raw <= PreviewMaximumRaw && receivedAt < deadline_;
      raw_ = command.raw;
      return true;
    }
    if (!command.active)
      active_ = false;
    return false;
  }
  void disconnect() { active_ = false; }
  bool active(uint64_t now) const { return active_ && now < deadline_; }
  uint8_t brightness(uint64_t now, uint8_t baseline) const { return active(now) ? raw_ : baseline; }
  uint64_t generation() const { return generation_; }
  uint64_t receivedAt() const { return receivedAt_; }
  bool seen() const { return seen_; }

private:
  bool seen_ = false, active_ = false;
  uint64_t generation_ = 0, deadline_ = 0, receivedAt_ = 0;
  uint8_t raw_ = 0;
};
} // namespace transit
