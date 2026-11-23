#include "led_preview.h"
#include "panel_config.h"
#include "preview_frame_receiver.h"
#include <WiFi.h>
#include <cstring>
#include <esp_timer.h>

namespace {
constexpr size_t FrameBytes = panelcfg::Width * panelcfg::Height * 3;
constexpr uint64_t PreviewDurationMs = 30000;
uint64_t nowMs() { return static_cast<uint64_t>(esp_timer_get_time() / 1000); }
} // namespace

bool LedPreview::begin() {
  mutex_ = xSemaphoreCreateMutex();
  return mutex_ && xTaskCreate(run, "led-preview", 8192, this, 1, nullptr) == pdPASS;
}

void LedPreview::run(void *self) { static_cast<LedPreview *>(self)->task(); }

void LedPreview::task() {
  bool listening = false;
  while (true) {
    const bool shouldListen = WiFi.status() == WL_CONNECTED && available_.load();
    if (shouldListen && !listening) {
      const char *headers[] = {"Origin", "Content-Type", "X-LED-Width", "X-LED-Height",
                               "X-LED-Format"};
      server_.collectHeaders(headers, 5);
      server_.addHandler(&frameHandler_);
      server_.on("/api/preview", HTTP_OPTIONS, [this]() {
        if (server_.header("Origin") != "null") {
          server_.send(403, "text/plain", "Local editor origin required");
          return;
        }
        cors();
        server_.sendHeader("Access-Control-Allow-Methods", "POST, OPTIONS");
        server_.sendHeader("Access-Control-Allow-Headers",
                           "Content-Type, X-LED-Width, X-LED-Height, X-LED-Format");
        server_.sendHeader("Access-Control-Allow-Private-Network", "true");
        server_.send(204, "text/plain", "");
      });
      server_.on("/api/preview/stop", HTTP_OPTIONS, [this]() {
        if (server_.header("Origin") != "null") {
          server_.send(403, "text/plain", "Local editor origin required");
          return;
        }
        cors();
        server_.sendHeader("Access-Control-Allow-Methods", "POST, OPTIONS");
        server_.sendHeader("Access-Control-Allow-Private-Network", "true");
        server_.send(204, "text/plain", "");
      });
      server_.on("/api/preview/stop", HTTP_POST, [this]() { handleStop(); });
      server_.onNotFound([this]() {
        cors();
        server_.send(404, "text/plain", "Not found");
      });
      server_.begin();
      listening = true;
    } else if (!shouldListen && listening) {
      server_.stop();
      xSemaphoreTake(mutex_, portMAX_DELAY);
      active_ = false;
      xSemaphoreGive(mutex_);
      listening = false;
    }
    if (listening)
      server_.handleClient();
    else
      vTaskDelay(pdMS_TO_TICKS(100));
  }
}

bool LedPreview::FrameHandler::canHandle(HTTPMethod method, String uri) {
  if (method != HTTP_POST || uri != "/api/preview")
    return false;
  owner_.receiveValid_ = false;
  owner_.frameComplete_ = false;
  owner_.received_ = 0;
  return true;
}

bool LedPreview::FrameHandler::canRaw(String uri) { return uri == "/api/preview"; }

void LedPreview::FrameHandler::raw(WebServer &server, String, HTTPRaw &raw) {
  if (raw.status == RAW_START) {
    owner_.received_ = 0;
    owner_.frameComplete_ = false;
    owner_.receiveValid_ =
        server.header("Origin") == "null" &&
        server.clientContentLength() == static_cast<int>(FrameBytes) &&
        server.header("Content-Type") == "application/octet-stream" &&
        server.header("X-LED-Width") == "64" && server.header("X-LED-Height") == "32" &&
        server.header("X-LED-Format") == "RGB888";
  } else if (raw.status == RAW_WRITE && owner_.receiveValid_) {
    if (!preview::copyRawFrameChunk(owner_.staging_.data(), FrameBytes, raw.totalSize,
                                    raw.currentSize, raw.buf, owner_.received_)) {
      owner_.receiveValid_ = false;
      return;
    }
  } else if (raw.status == RAW_END) {
    owner_.receiveValid_ = owner_.receiveValid_ && owner_.received_ == FrameBytes;
    owner_.frameComplete_ = owner_.receiveValid_;
  } else if (raw.status == RAW_ABORTED) {
    owner_.receiveValid_ = false;
    owner_.frameComplete_ = false;
  }
}

bool LedPreview::FrameHandler::handle(WebServer &server, HTTPMethod, String) {
  owner_.cors();
  if (!owner_.frameComplete_ || !owner_.receiveValid_ || owner_.received_ != FrameBytes) {
    server.send(400, "text/plain", "Expected one 64x32 RGB888 frame");
    return true;
  }
  xSemaphoreTake(owner_.mutex_, portMAX_DELAY);
  std::memcpy(owner_.frame_.data(), owner_.staging_.data(), FrameBytes);
  owner_.expiresAt_ = nowMs() + PreviewDurationMs;
  owner_.active_ = true;
  xSemaphoreGive(owner_.mutex_);
  server.send(204, "text/plain", "");
  return true;
}

void LedPreview::cors() { server_.sendHeader("Access-Control-Allow-Origin", "null"); }

void LedPreview::handleStop() {
  if (server_.header("Origin") != "null") {
    server_.send(403, "text/plain", "Local editor origin required");
    return;
  }
  cors();
  xSemaphoreTake(mutex_, portMAX_DELAY);
  active_ = false;
  xSemaphoreGive(mutex_);
  server_.send(204, "text/plain", "");
}

void LedPreview::render(transit::Framebuffer &frame, uint64_t now, bool unavailable) {
  xSemaphoreTake(mutex_, portMAX_DELAY);
  if (active_ && now >= expiresAt_)
    active_ = false;
  if (active_ && !unavailable) {
    for (int y = 0; y < panelcfg::Height; ++y)
      for (int x = 0; x < panelcfg::Width; ++x) {
        const size_t i = (static_cast<size_t>(y) * panelcfg::Width + x) * 3;
        frame.pixel(x, y, (static_cast<uint32_t>(frame_[i]) << 16) |
                              (static_cast<uint32_t>(frame_[i + 1]) << 8) | frame_[i + 2]);
      }
  }
  xSemaphoreGive(mutex_);
}
