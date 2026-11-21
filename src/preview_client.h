#pragma once
#include "brightness_preview.h"
#include <Arduino.h>
#include <atomic>
#include <freertos/semphr.h>

// Experimental RAM-only control. No Wi-Fi or NVS writes; renderer remains sole output owner.
class PreviewClient {
public:
  bool begin();
  void available(bool value) { available_.store(value); }
  uint8_t brightness(uint64_t now, uint8_t baseline);
  void presented(uint64_t now);

private:
  struct Ack {
    uint64_t generation = 0, received = 0, applied = 0, receivedUtc = 0, appliedUtc = 0;
    uint8_t raw = 0;
    bool valid = false, active = false;
  };
  static void run(void *self);
  void task();
  SemaphoreHandle_t mutex_ = nullptr;
  std::atomic<bool> available_{false};
  transit::BrightnessPreview state_;
  Ack applying_, ack_;
  uint64_t receivedUtc_ = 0;
};
