#include "feed_request.h"
#include <algorithm>
#include <cctype>

namespace transit {
bool validOrigin(const std::string &origin) {
  if (origin.rfind("https://", 0) != 0 || origin.size() <= 8 || origin.size() > 256)
    return false;
  for (size_t i = 8; i < origin.size(); ++i) {
    unsigned char c = origin[i];
    if (!(std::isalnum(c) || c == '-' || c == '.' || c == ':'))
      return false;
  }
  return true;
}
bool validBearer(const std::string &token) {
  if (token.empty() || token.size() > 1024)
    return false;
  bool padding = false;
  for (unsigned char c : token) {
    if (c == '=') {
      padding = true;
      continue;
    }
    if (padding ||
        !(std::isalnum(c) || c == '.' || c == '_' || c == '~' || c == '+' || c == '/' || c == '-'))
      return false;
  }
  return token.front() != '=';
}
FeedResult readResponse(int httpStatus, std::string_view body, const std::string &retryAfter) {
  FeedResult result;
  if (httpStatus == 401 || httpStatus == 403) {
    result.error = RequestError::Auth;
    result.retryMs = 300000;
  } else if (httpStatus == 409) {
    result.error = RequestError::Configuration;
    result.retryMs = 300000;
  } else if (httpStatus == 429) {
    result.error = RequestError::RateLimited;
    result.retryMs = 60000;
  } else if (httpStatus < 200 || httpStatus >= 300) {
    result.error = RequestError::Http;
    result.retryMs = 60000;
  } else if (parseFeed(body, result.feed) != ParseResult::Ok) {
    result.error = RequestError::InvalidFeed;
    result.retryMs = 60000;
  }
  // Support delta-seconds only; an HTTP-date falls back to conservative local backoff.
  if (result.error != RequestError::None && !retryAfter.empty()) {
    uint32_t seconds = 0;
    bool valid = retryAfter.size() <= 10;
    for (unsigned char c : retryAfter) {
      if (c < '0' || c > '9') {
        valid = false;
        break;
      }
      seconds = std::min(uint32_t(3600), seconds * 10 + c - '0');
    }
    if (valid)
      result.retryMs = std::max(result.retryMs, seconds * 1000);
  }
  return result;
}
} // namespace transit
