#pragma once
#include "device_identity.h"
#include "feed_request.h"

class FeedClient {
public:
  bool begin();
  bool configured() const;
  transit::FeedResult fetch() const;

private:
  transit::DeviceIdentity identity_;
  bool identityReady_ = false;
};
