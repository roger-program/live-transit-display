#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace transit {
enum class Provenance { Realtime, Scheduled, Unknown };
enum class StopStatus { Ok, Empty, Partial, Scheduled, Stale, Unavailable, Unknown, UpstreamError };
enum class RealtimeStatus { Available, Empty, Unavailable, Unknown };
struct Arrival {
  int64_t arrivalAt = 0;
  Provenance provenance = Provenance::Unknown;
  std::string identifierKind;
  std::string identifier;
};
struct Group {
  std::string line;
  std::string destination;
  std::string lineColor;
  std::vector<Arrival> arrivals;
};
struct Stop {
  std::string mode;
  std::string id;
  std::string name;
  bool nameKnown = false;
  StopStatus status = StopStatus::Unavailable;
  RealtimeStatus realtimeStatus = RealtimeStatus::Unknown;
  std::vector<Group> groups;
};
struct Feed {
  int64_t generatedAt = 0;
  std::vector<Stop> stops;
};
// Bounds are prototype resource limits, not additional v1 contract guarantees.
constexpr size_t MaxBodyBytes = 65536;
constexpr size_t MaxStops = 20;
constexpr size_t MaxGroups = 32;
constexpr size_t MaxArrivals = 32;
constexpr size_t MaxStringBytes = 256;
} // namespace transit
