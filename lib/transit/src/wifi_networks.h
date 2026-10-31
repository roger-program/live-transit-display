#pragma once
#include <algorithm>
#include <string>
#include <vector>

namespace provisioning {
struct WifiNetwork {
  std::string ssid;
  int rssi;
};
// SSIDs are case-sensitive byte strings. Never merge similar-looking names.
inline void addWifiNetwork(std::vector<WifiNetwork> &networks, const std::string &ssid, int rssi) {
  if (ssid.empty())
    return;
  for (auto &network : networks)
    if (network.ssid == ssid) {
      network.rssi = std::max(network.rssi, rssi);
      return;
    }
  networks.push_back({ssid, rssi});
}
inline void sortWifiNetworks(std::vector<WifiNetwork> &networks) {
  std::sort(networks.begin(), networks.end(), [](const WifiNetwork &a, const WifiNetwork &b) {
    return a.rssi != b.rssi ? a.rssi > b.rssi : a.ssid < b.ssid;
  });
}
} // namespace provisioning
