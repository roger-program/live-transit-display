#pragma once
#include "brightness_configuration.h"
#include "brightness_preview.h"
#include <Arduino.h>
#include <atomic>
#include <freertos/semphr.h>

// Cached brightness policy and RAM-only preview; renderer remains sole output owner.
class PreviewClient {
public:
  bool begin();
  void available(bool value) { available_.store(value); }
  void clockReady(bool value) { clockReady_.store(value); }
  uint8_t brightness(uint64_t now, uint8_t baseline, bool recovery = false);
  void presented(uint64_t now);

private:
  struct Ack {
    uint64_t generation = 0, received = 0, applied = 0, receivedUtc = 0, appliedUtc = 0;
    uint8_t raw = 0;
    bool valid = false, active = false;
    uint64_t configurationRevision = 0;
    uint8_t maximumRaw = 36;
    bool configurationReady = false;
  };
  static void run(void *self);
  void task();
  SemaphoreHandle_t mutex_ = nullptr;
  std::atomic<bool> available_{false}, clockReady_{false};
  transit::BrightnessSettings settings_;
  bool settingsReady_ = false;
  transit::BrightnessPreview state_;
  Ack applying_, ack_;
  uint64_t receivedUtc_ = 0;
};
