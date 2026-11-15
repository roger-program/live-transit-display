#pragma once
#include "device_identity.h"
#include "feed_request.h"

class FeedClient {
public:
  bool begin();
  bool configured() const;
  transit::FeedResult fetch() const;
  int control(const char *path, const char *method, const char *boot, const std::string &body,
              std::string &response, uint32_t &retryMs) const;

private:
  transit::DeviceIdentity identity_;
  bool identityReady_ = false;
};
