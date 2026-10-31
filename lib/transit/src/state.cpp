#include "state.h"
#include <algorithm>
#include <utility>

namespace transit {
const char *requestMessage(RequestError error) {
  switch (error) {
  case RequestError::NoConfig:
    return "CONFIG";
  case RequestError::Configuration:
    return "ADD STOPS";
  case RequestError::Network:
    return "OFFLINE";
  case RequestError::Auth:
    return "AUTH";
  case RequestError::RateLimited:
    return "RETRY";
  case RequestError::Http:
    return "FEED ERR";
  case RequestError::InvalidFeed:
    return "BAD FEED";
  case RequestError::Clock:
    return "CLOCK";
  case RequestError::None:
    return "UNAVAIL";
  }
  return "UNAVAIL";
}
std::string countdown(const Arrival &arrival, int64_t now) {
  const int64_t remaining = arrival.arrivalAt - now;
  if (remaining <= 0)
    return {};
  std::string prefix;
  if (arrival.provenance == Provenance::Scheduled)
    prefix = "S";
  else if (arrival.provenance == Provenance::Unknown)
    prefix = "?";
  return prefix + (std::to_string(remaining / 60000) + "'");
}
void applySnapshot(Snapshot &current, Snapshot next) {
  if (next.hasFeed && next.error == RequestError::None)
    current = std::move(next);
  else
    current.error = next.error;
}
View viewAt(const Snapshot &snapshot, int64_t now, uint64_t uptime, bool clockValid) {
  View view;
  const bool interrupted =
      snapshot.error == RequestError::Network || snapshot.error == RequestError::Http ||
      snapshot.error == RequestError::InvalidFeed || snapshot.error == RequestError::RateLimited;
  // Reuse the existing full-screen status renderer for one second every five.
  // Other frames retain the unmodified arrival layout and normal expiry rules.
  if (snapshot.error != RequestError::None &&
      (!interrupted || !snapshot.hasFeed || uptime % 5000 < 1000)) {
    view.message = requestMessage(snapshot.error);
    return view;
  }
  if (!clockValid || (snapshot.hasFeed && snapshot.feed.generatedAt - now > FutureToleranceMs)) {
    view.message = "CLOCK";
    return view;
  }
  if (!snapshot.hasFeed) {
    view.message = "UNAVAIL";
    return view;
  }
  const bool stale = now - snapshot.feed.generatedAt >= StaleAfterMs ||
                     uptime < snapshot.receivedAtMs ||
                     uptime - snapshot.receivedAtMs >= StaleAfterMs;
  view.stops = snapshot.feed.stops;
  for (auto &stop : view.stops) {
    bool healthy = stop.status == StopStatus::Ok || stop.status == StopStatus::Empty ||
                   stop.status == StopStatus::Partial || stop.status == StopStatus::Scheduled;
    if (stale)
      stop.status = StopStatus::Stale;
    if (stale || !healthy) {
      stop.groups.clear();
      stop.realtimeStatus = RealtimeStatus::Unavailable;
      continue;
    }
    bool future = false;
    for (auto &group : stop.groups) {
      auto &arrivals = group.arrivals;
      arrivals.erase(std::remove_if(arrivals.begin(), arrivals.end(),
                                    [now](const Arrival &a) { return a.arrivalAt <= now; }),
                     arrivals.end());
      future = future || !arrivals.empty();
    }
    if (!future && stop.status == StopStatus::Scheduled) {
      stop.status = StopStatus::Unavailable;
      stop.groups.clear();
      stop.realtimeStatus = RealtimeStatus::Unavailable;
    } else if (!future && stop.status == StopStatus::Ok) {
      stop.status = StopStatus::Empty;
      stop.realtimeStatus = RealtimeStatus::Empty;
    }
  }
  if (view.stops.empty())
    view.message = "ADD STOPS";
  return view;
}
} // namespace transit
