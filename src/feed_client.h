#pragma once
#include "feed_request.h"

class FeedClient {
public:
  bool configured() const;
  transit::FeedResult fetch(const std::vector<std::string> &selectors) const;
};
