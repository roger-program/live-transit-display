#pragma once
#include "feed_parser.h"
#include "state.h"

namespace transit {
struct FeedResult {
  RequestError error = RequestError::None;
  Feed feed;
  uint32_t retryMs = 30000;
};
std::string arrivalsPath(const std::vector<std::string> &selectors);
bool validOrigin(const std::string &origin);
bool validBearer(const std::string &token);
FeedResult readResponse(int httpStatus, std::string_view body, const std::string &retryAfter);
Feed selectStops(const Feed &feed, const std::vector<std::string> &selectors);
} // namespace transit
