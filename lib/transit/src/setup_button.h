#pragma once
#include <cstdint>
namespace provisioning {
// GPIO polarity belongs at the hardware boundary; this state machine consumes pressed/released.
class SetupButton {
public:
  bool update(bool pressed, uint64_t now) {
    if (!initialized_) {
      initialized_ = true;
      raw_ = pressed;
      changedAt_ = now;
    }
    if (pressed != raw_) {
      raw_ = pressed;
      changedAt_ = now;
    }
    if (raw_ != stable_ && now - changedAt_ >= 100) {
      stable_ = raw_;
      heldAt_ = now;
      handled_ = false;
    }
    if (stable_ && !handled_ && now - heldAt_ >= 3000) {
      handled_ = true;
      return true;
    }
    return false;
  }
  bool pressed() const { return stable_; }

private:
  bool initialized_ = false, raw_ = false, stable_ = false, handled_ = false;
  uint64_t changedAt_ = 0, heldAt_ = 0;
};
} // namespace provisioning
