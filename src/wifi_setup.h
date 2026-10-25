#pragma once
#include "framebuffer.h"
#include <Arduino.h>
#include <Preferences.h>
#include <WebServer.h>
#include <atomic>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

class WifiSetup {
public:
  // Initialize synchronization before starting the network task.
  bool initialize();
  void begin();
  bool active() const { return provisioning_; }
  void tick(uint64_t now);
  void request() { requested_ = true; }
  bool render(transit::Framebuffer &frame, uint64_t now);

private:
  void start(uint64_t now);
  void connect(const String &ssid, const String &password, uint64_t now);
  void message(const String &status);
  bool authorized();
  WebServer server_{IPAddress(192, 168, 4, 1), 80};
  Preferences storage_;
  SemaphoreHandle_t mutex_ = nullptr;
  std::atomic<bool> requested_{false};
  std::atomic<bool> provisioning_{false};
  // Wi-Fi, HTTP and persistence belong to the network task; render fields use mutex_.
  String savedSsid_, savedPassword_, candidateSsid_, candidatePassword_;
  String apName_, apPassword_, token_, status_;
  uint8_t activeSlot_ = 0;
  bool stored_ = false, setup_ = false, testing_ = false, success_ = false;
  bool storageReady_ = false, visible_ = false;
  uint64_t attemptAt_ = 0, retryAt_ = 0, setupAt_ = 0, finishAt_ = 0;
};
