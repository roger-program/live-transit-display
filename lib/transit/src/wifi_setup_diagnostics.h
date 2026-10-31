#pragma once
#include <cstdint>
#include <string>
namespace provisioning {
inline constexpr char SetupUrl[] = "http://192.168.4.1/";
inline bool setupHost(const std::string &host) {
  return host == "192.168.4.1" || host == "192.168.4.1:80";
}
inline bool setupOrigin(const std::string &origin) {
  return origin.empty() || origin == "http://192.168.4.1" || origin == "http://192.168.4.1:80";
}
// Redirect only navigation/probe requests. Never turn API calls or POSTs into redirects.
inline bool setupPortalRedirect(const std::string &method, const std::string &host,
                                const std::string &path) {
  if (method != "GET" && method != "HEAD")
    return false;
  if (path == "/status" || path == "/connect" || path == "/complete")
    return false;
  return !setupHost(host) || path != "/";
}
// A closed captive window must not leave a successfully configured AP running forever.
inline uint64_t setupSuccessDeadline(uint64_t now, bool browserSubmission) {
  return now + (browserSubmission ? 30000 : 5000);
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
