#pragma once
#include <string>
#include <string_view>

namespace transit {
struct DeviceIdentity {
  std::string deviceId;
  std::string token;
};
constexpr size_t MaxIdentityBytes = 256;
// Transactional: invalid records leave the destination untouched.
bool parseDeviceIdentity(std::string_view json, DeviceIdentity &output);
} // namespace transit
