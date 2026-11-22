#pragma once
#include "framebuffer.h"
#include <WebServer.h>
#include <array>
#include <atomic>
#include <freertos/semphr.h>

class LedPreview {
public:
  bool begin();
  void available(bool value) { available_.store(value); }
  void render(transit::Framebuffer &frame, uint64_t now, bool unavailable);

private:
  class FrameHandler final : public RequestHandler {
  public:
    explicit FrameHandler(LedPreview &owner) : owner_(owner) {}
    bool canHandle(HTTPMethod method, String uri) override;
    bool canRaw(String uri) override;
    bool handle(WebServer &server, HTTPMethod method, String uri) override;
    void raw(WebServer &server, String uri, HTTPRaw &raw) override;

  private:
    LedPreview &owner_;
  };

  static void run(void *self);
  void task();
  void handleStop();
  void cors();
  WebServer server_{80};
  FrameHandler frameHandler_{*this};
  SemaphoreHandle_t mutex_ = nullptr;
  std::array<uint8_t, 64 * 32 * 3> frame_{};
  std::array<uint8_t, 64 * 32 * 3> staging_{};
  std::atomic<bool> available_{false};
  size_t received_ = 0;
  uint64_t expiresAt_ = 0;
  bool receiveValid_ = false, frameComplete_ = false;
  bool active_ = false;
};
