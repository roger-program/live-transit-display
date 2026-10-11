#include "feed_request.h"
#include <algorithm>
#include <cctype>

namespace transit {
std::string arrivalsPath(const std::vector<std::string> &selectors) {
  constexpr char Hex[] = "0123456789ABCDEF";
  std::string path = "/v1/arrivals";
  bool first = true;
  for (const auto &selector : selectors) {
    path += first ? "?stop=" : "&stop=";
    first = false;
    for (unsigned char c : selector) {
      if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~')
        path += char(c);
      else {
        path += '%';
        path += Hex[c >> 4];
        path += Hex[c & 15];
      }
    }
  }
  return path;
}
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
Feed selectStops(const Feed &feed, const std::vector<std::string> &selectors) {
  Feed selected;
  selected.generatedAt = feed.generatedAt;
  for (const auto &selector : selectors) {
    auto found = std::find_if(feed.stops.begin(), feed.stops.end(), [&selector](const Stop &stop) {
      return stop.mode + ":" + stop.id == selector;
    });
    if (found != feed.stops.end())
      selected.stops.push_back(*found);
    else {
      Stop missing;
      size_t colon = selector.find(':');
      missing.mode = selector.substr(0, colon);
      missing.id = selector.substr(colon + 1);
      selected.stops.push_back(std::move(missing));
    }
  }
  return selected;
}
} // namespace transit
