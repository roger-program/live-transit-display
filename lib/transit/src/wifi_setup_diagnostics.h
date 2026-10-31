#pragma once
#include <cstdint>
#include <string>
namespace provisioning {
inline constexpr char SetupHostname[] = "metro.conf";
inline std::string setupAddressLowercase(std::string value) {
  for (char &c : value)
    if (c >= 'A' && c <= 'Z')
      c += 'a' - 'A';
  return value;
}
inline bool setupHost(const std::string &host) {
  const std::string value = setupAddressLowercase(host);
  return value == "192.168.4.1" || value == "192.168.4.1:80" || value == SetupHostname ||
         value == std::string(SetupHostname) + ":80";
}
inline bool setupOrigin(const std::string &origin) {
  const std::string value = setupAddressLowercase(origin);
  return value.empty() || (value.compare(0, 7, "http://") == 0 && setupHost(value.substr(7)));
}
// SSIDs are untrusted bytes. Keep serial output bounded and on one line.
inline std::string diagnosticSsid(const std::string &ssid) {
  std::string safe;
  for (size_t i = 0; i < ssid.size() && i < 32; ++i) {
    const unsigned char c = static_cast<unsigned char>(ssid[i]);
    if (c >= 32 && c <= 126 && c != '\\' && c != '"')
      safe += static_cast<char>(c);
    else {
      constexpr char hex[] = "0123456789ABCDEF";
      safe += "\\x";
      safe += hex[c >> 4];
      safe += hex[c & 15];
    }
  }
  return safe;
}
// Pinned ESP-IDF 4.4.7 Wi-Fi reason codes; none proves a wrong password alone.
inline const char *connectionFailure(uint16_t reason) {
  switch (reason) {
  case 201:
    return "Network not found. Check the SSID, range and 2.4 GHz availability.";
  case 2:
  case 15:
  case 202:
  case 204:
    return "Authentication or handshake failed. Check the password and router security.";
  case 203:
  case 205:
    return "Router connection failed. Check router access restrictions and signal.";
  case 200:
    return "Router signal was lost. Move the display closer and retry.";
  case 0:
    return "No connection and IP address within 20 seconds. Check network, password and router "
           "DHCP.";
  default:
    return "Wi-Fi disconnected before setup completed. Check router settings and retry.";
  }
}
} // namespace provisioning
