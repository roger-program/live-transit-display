#pragma once
#include "feed_parser.h"
#include "state.h"

namespace transit {
struct FeedResult {
  RequestError error = RequestError::None;
  Feed feed;
  uint32_t retryMs = 30000;
};
inline constexpr char DeviceFeedPath[] = "/v1/device/feed";
bool validOrigin(const std::string &origin);
bool validBearer(const std::string &token);
FeedResult readResponse(int httpStatus, std::string_view body, const std::string &retryAfter);
} // namespace transit
