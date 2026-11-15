#include "brightness_preview.h"
#include <ArduinoJson.h>
#include <cmath>
#include <string>

namespace transit {
bool parsePreview(std::string_view json, std::string_view boot, PreviewCommand &out) {
  if (json.size() > 1024)
    return false;
  StaticJsonDocument<1536> doc;
  if (deserializeJson(doc, json.data(), json.size()) || !doc.is<JsonObject>())
    return false;
  const auto number = [](JsonVariantConst v) {
    if (!v.is<double>())
      return false;
    const double n = v.as<double>();
    return std::isfinite(n) && n >= 0 && n <= 9007199254740991.0 && std::floor(n) == n;
  };
  if (!doc["boot"].is<const char *>() || std::string_view(doc["boot"].as<const char *>()) != boot ||
      !number(doc["generation"]) || !number(doc["serverNow"]) || !number(doc["expiresAt"]) ||
      !doc.as<JsonObjectConst>().containsKey("raw"))
    return false;
  PreviewCommand next;
  next.generation = doc["generation"].as<uint64_t>();
  if (!doc["raw"].isNull()) {
    if (!number(doc["raw"]) || doc["raw"].as<unsigned>() > PreviewMaximumRaw)
      return false;
    next.raw = doc["raw"].as<uint8_t>();
    const auto server = doc["serverNow"].as<uint64_t>();
    const auto expires = doc["expiresAt"].as<uint64_t>();
    next.remainingMs = expires > server ? std::min(expires - server, PreviewTtlMs) : 0;
    next.active = next.remainingMs > 0;
  }
  out = next;
  return true;
}
} // namespace transit
