#include "device_identity.h"
#include <ArduinoJson.h>
#include <cstring>

namespace transit {
bool parseDeviceIdentity(std::string_view json, DeviceIdentity &output) {
  if (json.empty() || json.size() > MaxIdentityBytes)
    return false;
  StaticJsonDocument<512> doc;
  if (deserializeJson(doc, json.data(), json.size()) || !doc.is<JsonObject>() || doc.size() != 2 ||
      !doc["deviceId"].is<const char *>() || !doc["token"].is<const char *>())
    return false;
  const auto id = doc["deviceId"].as<JsonString>();
  const auto token = doc["token"].as<JsonString>();
  if (id.size() != 36 || token.size() != 69 || std::strlen(id.c_str()) != id.size() ||
      std::strlen(token.c_str()) != token.size())
    return false;
  const std::string deviceId(id.c_str(), id.size());
  const std::string credential(token.c_str(), token.size());
  auto hex = [](char c) { return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'); };
  for (size_t i = 0; i < deviceId.size(); ++i) {
    const bool separator = i == 8 || i == 13 || i == 18 || i == 23;
    if (separator ? deviceId[i] != '-' : !hex(deviceId[i]))
      return false;
  }
  if (credential.compare(0, 5, "dev1_") != 0)
    return false;
  for (size_t i = 5; i < credential.size(); ++i)
    if (!hex(credential[i]))
      return false;
  output = {deviceId, credential};
  return true;
}
} // namespace transit
