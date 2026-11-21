#include "brightness_configuration.h"
#include <ArduinoJson.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <ctime>

namespace transit {
namespace {
bool number(JsonVariantConst value, uint64_t max) {
  if (!value.is<double>())
    return false;
  const double n = value.as<double>();
  return std::isfinite(n) && n >= 0 && n <= static_cast<double>(max) && n == std::floor(n);
}
int weekday(int year, int month, int day) {
  constexpr int offsets[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
  if (month < 3)
    --year;
  return (year + year / 4 - year / 100 + year / 400 + offsets[month - 1] + day) % 7;
}
} // namespace
bool parseBrightnessSettings(std::string_view json, BrightnessSettings &out) {
  if (json.size() > MaxBrightnessRecordBytes)
    return false;
  StaticJsonDocument<6144> doc;
  if (deserializeJson(doc, json.data(), json.size()) || !doc.is<JsonObject>() ||
      !number(doc["configurationRevision"], 9007199254740991ULL) ||
      !number(doc["maximumRaw"], 64) || doc["maximumRaw"].as<unsigned>() == 0 ||
      !doc.as<JsonObjectConst>().containsKey("brightness"))
    return false;
  BrightnessSettings next;
  next.revision = doc["configurationRevision"].as<uint64_t>();
  next.maximumRaw = doc["maximumRaw"].as<uint8_t>();
  const auto config = doc["brightness"];
  if (!config.isNull()) {
    if (!config.is<JsonObject>() || !config["mode"].is<const char *>())
      return false;
    next.configured = true;
    const std::string_view mode = config["mode"].as<const char *>();
    if (mode == "manual") {
      if (config.size() != 2 || !number(config["level"], 100))
        return false;
      next.level = config["level"].as<uint8_t>();
    } else if (mode == "automatic") {
      if (config.size() != 3 || !config["timezone"].is<const char *>() ||
          std::strcmp(config["timezone"].as<const char *>(), "Europe/Madrid") ||
          !config["entries"].is<JsonArray>() || config["entries"].size() < 1 ||
          config["entries"].size() > 5)
        return false;
      next.automatic = true;
      next.count = config["entries"].size();
      for (uint8_t i = 0; i < next.count; ++i) {
        const auto entry = config["entries"][i];
        if (!entry.is<JsonObject>() || entry.size() != 2 || !entry["start"].is<const char *>() ||
            !number(entry["level"], 100))
          return false;
        const auto time = entry["start"].as<const char *>();
        if (std::strlen(time) != 5 || time[2] != ':' || time[0] < '0' || time[0] > '2' ||
            time[1] < '0' || time[1] > '9' || time[3] < '0' || time[3] > '5' || time[4] < '0' ||
            time[4] > '9')
          return false;
        const int minute =
            ((time[0] - '0') * 10 + time[1] - '0') * 60 + (time[3] - '0') * 10 + time[4] - '0';
        if (minute >= 1440)
          return false;
        next.entries[i] = {static_cast<uint16_t>(minute), entry["level"].as<uint8_t>()};
      }
      std::sort(
          next.entries.begin(), next.entries.begin() + next.count,
          [](const BrightnessChange &a, const BrightnessChange &b) { return a.minute < b.minute; });
      for (uint8_t i = 1; i < next.count; ++i)
        if (next.entries[i].minute == next.entries[i - 1].minute)
          return false;
    } else
      return false;
  }
  out = next;
  return true;
}
std::string brightnessRecord(const BrightnessSettings &settings) {
  StaticJsonDocument<1536> doc;
  doc["configurationRevision"] = settings.revision;
  doc["maximumRaw"] = settings.maximumRaw;
  if (!settings.configured)
    doc["brightness"] = nullptr;
  else {
    auto config = doc.createNestedObject("brightness");
    if (!settings.automatic) {
      config["mode"] = "manual";
      config["level"] = settings.level;
    } else {
      config["mode"] = "automatic";
      config["timezone"] = "Europe/Madrid";
      auto entries = config.createNestedArray("entries");
      for (uint8_t i = 0; i < settings.count; ++i) {
        auto entry = entries.createNestedObject();
        char time[6];
        std::snprintf(time, sizeof(time), "%02u:%02u", settings.entries[i].minute / 60,
                      settings.entries[i].minute % 60);
        entry["start"] = time;
        entry["level"] = settings.entries[i].level;
      }
    }
  }
  std::string record;
  serializeJson(doc, record);
  return record;
}
int madridMinuteAt(int64_t utcSeconds) {
  const time_t time = static_cast<time_t>(utcSeconds);
  tm utc{};
  if (!gmtime_r(&time, &utc))
    return -1;
  const int month = utc.tm_mon + 1, year = utc.tm_year + 1900;
  const int marchSunday = 31 - weekday(year, 3, 31), octoberSunday = 31 - weekday(year, 10, 31);
  // Europe/Madrid: current EU rules switch at 01:00 UTC on the last Sunday of March/October.
  const bool summer =
      (month > 3 && month < 10) ||
      (month == 3 &&
       (utc.tm_mday > marchSunday || (utc.tm_mday == marchSunday && utc.tm_hour >= 1))) ||
      (month == 10 &&
       (utc.tm_mday < octoberSunday || (utc.tm_mday == octoberSunday && utc.tm_hour < 1)));
  return (utc.tm_hour * 60 + utc.tm_min + (summer ? 120 : 60)) % 1440;
}
uint8_t brightnessRaw(uint8_t level, uint8_t maximum) {
  return static_cast<uint8_t>((static_cast<unsigned>(level) * maximum + 99) / 100);
}
uint8_t effectiveBrightness(const BrightnessSettings &settings, int64_t utcSeconds, bool validClock,
                            uint8_t fallback) {
  if (!settings.configured)
    return fallback;
  if (!settings.automatic)
    return brightnessRaw(settings.level, settings.maximumRaw);
  if (!validClock || settings.count == 0)
    return fallback;
  const int minute = madridMinuteAt(utcSeconds);
  if (minute < 0)
    return fallback;
  uint8_t index = settings.count - 1;
  for (uint8_t i = 0; i < settings.count; ++i)
    if (settings.entries[i].minute <= minute)
      index = i;
  return brightnessRaw(settings.entries[index].level, settings.maximumRaw);
}
} // namespace transit
