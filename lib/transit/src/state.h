#pragma once
#include "model.h"

namespace transit {
constexpr int64_t StaleAfterMs = 120000;
constexpr int64_t FutureToleranceMs = 5000;
enum class RequestError { None, NoConfig, Network, Auth, RateLimited, Http, InvalidFeed, Clock };
struct Snapshot {
  Feed feed;
  bool hasFeed = false;
  uint64_t receivedAtMs = 0;
  RequestError error = RequestError::None;
};
struct View {
  std::vector<Stop> stops;
  std::string message;
};
std::string countdown(const Arrival &arrival, int64_t now);
View viewAt(const Snapshot &snapshot, int64_t now, uint64_t uptime, bool clockValid);
const char *requestMessage(RequestError error);
} // namespace transit
