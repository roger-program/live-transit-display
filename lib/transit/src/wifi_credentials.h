#pragma once
#include <cstdint>
#include <cstring>
#include <string>
namespace provisioning {
struct Credentials {
  uint32_t version = 1;
  char ssid[33]{};
  char password[65]{};
};
inline bool validCredentials(const std::string &ssid, const std::string &password) {
  if (ssid.empty() || ssid.size() > 32 || password.size() > 64 ||
      ssid.find('\0') != std::string::npos || password.find('\0') != std::string::npos)
    return false;
  if (password.empty())
    return true;
  if (password.size() < 8)
    return false;
  if (password.size() == 64)
    for (char c : password)
      if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F')))
        return false;
  return true;
}
inline bool validRecord(const Credentials &record) {
  return record.version == 1 && record.ssid[32] == 0 && record.password[64] == 0 &&
         validCredentials(record.ssid, record.password);
}
} // namespace provisioning
