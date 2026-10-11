#include "feed_parser.h"
#include <ArduinoJson.h>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>

namespace transit {
namespace {
struct JsonInput {
  std::string_view input;
  size_t position = 0;
  int read() {
    return position < input.size() ? static_cast<unsigned char>(input[position++]) : -1;
  }
  size_t readBytes(char *buffer, size_t length) {
    const size_t count = std::min(length, input.size() - position);
    std::memcpy(buffer, input.data() + position, count);
    position += count;
    return count;
  }
};
bool text(JsonVariantConst value, std::string &out) {
  if (!value.is<const char *>())
    return false;
  auto input = value.as<JsonString>();
  if (input.size() > MaxStringBytes || std::strlen(input.c_str()) != input.size())
    return false;
  out.assign(input.c_str(), input.size());
  return true;
}
bool timestamp(JsonVariantConst value, int64_t &out) {
  if (!value.is<double>())
    return false;
  const double n = value.as<double>();
  if (!std::isfinite(n) || std::floor(n) != n || std::abs(n) > 8640000000000000.0)
    return false;
  out = static_cast<int64_t>(n);
  return true;
}
StopStatus status(const std::string &value) {
  if (value == "ok")
    return StopStatus::Ok;
  if (value == "empty")
    return StopStatus::Empty;
  if (value == "partial")
    return StopStatus::Partial;
  if (value == "scheduled")
    return StopStatus::Scheduled;
  if (value == "stale")
    return StopStatus::Stale;
  if (value == "unknown")
    return StopStatus::Unknown;
  if (value == "upstream_error")
    return StopStatus::UpstreamError;
  return StopStatus::Unavailable;
}
bool color(const std::string &value) {
  if (value.size() != 7 || value[0] != '#')
    return false;
  for (size_t i = 1; i < value.size(); ++i)
    if (!std::isxdigit(static_cast<unsigned char>(value[i])))
      return false;
  return true;
}
} // namespace

ParseResult parseFeed(std::string_view json, Feed &output) {
  if (json.size() > MaxBodyBytes)
    return ParseResult::ResourceLimit;
  // A fixed ceiling also bounds unknown/debug extensions. No silent truncation.
  DynamicJsonDocument doc(128 * 1024);
  JsonInput input{json};
  const auto error = deserializeJson(doc, input, DeserializationOption::NestingLimit(16));
  if (error == DeserializationError::NoMemory)
    return ParseResult::ResourceLimit;
  if (error || !doc.is<JsonObject>())
    return ParseResult::Invalid;
  for (size_t i = input.position; i < json.size(); ++i)
    if (json[i] != ' ' && json[i] != '\t' && json[i] != '\r' && json[i] != '\n')
      return ParseResult::Invalid;
  if (!doc["version"].is<int>() || doc["version"].as<int>() != 1)
    return ParseResult::UnsupportedVersion;
  Feed feed;
  if (!timestamp(doc["generatedAt"], feed.generatedAt) || !doc["stops"].is<JsonArray>())
    return ParseResult::Invalid;
  if (doc["stops"].size() > MaxStops)
    return ParseResult::ResourceLimit;
  for (JsonVariantConst item : doc["stops"].as<JsonArrayConst>()) {
    Stop stop;
    std::string label;
    if (!item.is<JsonObjectConst>() || !text(item["mode"], stop.mode) ||
        !text(item["stop"], stop.id) || !text(item["status"], label) ||
        !item["groups"].is<JsonArrayConst>() || !item.as<JsonObjectConst>().containsKey("name"))
      return ParseResult::Invalid;
    stop.status = status(label);
    if (!item["name"].isNull()) {
      if (!text(item["name"], stop.name))
        return ParseResult::Invalid;
      stop.nameKnown = true;
    }
    if (item.as<JsonObjectConst>().containsKey("realtimeStatus")) {
      if (!text(item["realtimeStatus"], label))
        return ParseResult::Invalid;
      if (label == "available")
        stop.realtimeStatus = RealtimeStatus::Available;
      else if (label == "empty")
        stop.realtimeStatus = RealtimeStatus::Empty;
      else if (label == "unavailable")
        stop.realtimeStatus = RealtimeStatus::Unavailable;
    }
    if (item.as<JsonObjectConst>().containsKey("error")) {
      if (!item["error"].is<JsonObjectConst>() || !text(item["error"]["code"], label) ||
          !text(item["error"]["message"], label))
        return ParseResult::Invalid;
    }
    if (item["groups"].size() > MaxGroups)
      return ParseResult::ResourceLimit;
    for (JsonVariantConst entry : item["groups"].as<JsonArrayConst>()) {
      Group group;
      if (!entry.is<JsonObjectConst>() || !text(entry["line"], group.line) ||
          !text(entry["destination"], group.destination) || !entry["arrivals"].is<JsonArrayConst>())
        return ParseResult::Invalid;
      if (entry.as<JsonObjectConst>().containsKey("lineColor") &&
          (!text(entry["lineColor"], group.lineColor) || !color(group.lineColor)))
        return ParseResult::Invalid;
      if (entry["arrivals"].size() > MaxArrivals)
        return ParseResult::ResourceLimit;
      int64_t previous = INT64_MIN;
      for (JsonVariantConst prediction : entry["arrivals"].as<JsonArrayConst>()) {
        Arrival arrival;
        if (!prediction.is<JsonObjectConst>() ||
            !timestamp(prediction["arrivalAt"], arrival.arrivalAt) || arrival.arrivalAt < previous)
          return ParseResult::Invalid;
        previous = arrival.arrivalAt;
        if (prediction.as<JsonObjectConst>().containsKey("provenance")) {
          if (!text(prediction["provenance"], label))
            return ParseResult::Invalid;
          if (label == "realtime")
            arrival.provenance = Provenance::Realtime;
          else if (label == "scheduled")
            arrival.provenance = Provenance::Scheduled;
          else if (label != "unknown")
            return ParseResult::Invalid;
        }
        if (prediction.as<JsonObjectConst>().containsKey("sourceIdentifier")) {
          const auto identifier = prediction["sourceIdentifier"];
          if (!identifier.is<JsonObjectConst>() ||
              !text(identifier["kind"], arrival.identifierKind) ||
              (arrival.identifierKind != "service" && arrival.identifierKind != "vehicle"))
            return ParseResult::Invalid;
          if (identifier["id"].is<int64_t>()) {
            int64_t numeric;
            if (!timestamp(identifier["id"], numeric))
              return ParseResult::Invalid;
            arrival.identifier = std::to_string(numeric);
          } else if (!text(identifier["id"], arrival.identifier))
            return ParseResult::Invalid;
          if (arrival.identifier.empty() || arrival.identifier.size() > 200)
            return ParseResult::Invalid;
        }
        group.arrivals.push_back(std::move(arrival));
      }
      stop.groups.push_back(std::move(group));
    }
    feed.stops.push_back(std::move(stop));
  }
  output = std::move(feed);
  return ParseResult::Ok;
}
} // namespace transit
