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
  bool render(transit::Framebuffer &frame);

private:
  void start();
  void connect(const String &ssid, const String &password, uint64_t now);
  void message(const String &status);
  bool authorized();
  void sendPage(int code = 200, bool configuration = false);
  const char *webState() const;
  WebServer server_{IPAddress(192, 168, 4, 1), 80};
  Preferences storage_;
  SemaphoreHandle_t mutex_ = nullptr;
  std::atomic<bool> requested_{false};
  std::atomic<bool> provisioning_{false};
  std::atomic<uint16_t> disconnectReason_{0};
  uint16_t reportedReason_ = 0;
  // Wi-Fi, HTTP and persistence belong to the network task; render fields use mutex_.
  String savedSsid_, savedPassword_, candidateSsid_, candidatePassword_;
  String apName_, apPassword_, token_, status_, selectedSsid_;
  uint8_t activeSlot_ = 0;
  bool stored_ = false, setup_ = false, testing_ = false, success_ = false;
  bool attemptStarted_ = false, failed_ = false;
  bool successPageSent_ = false, browserSubmission_ = false;
  bool storageReady_ = false, visible_ = false;
  uint64_t attemptAt_ = 0, retryAt_ = 0, finishAt_ = 0, connectAt_ = 0;
};
