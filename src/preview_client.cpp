#include "preview_client.h"
#include "feed_client.h"
#include <ArduinoJson.h>
#include <WiFi.h>
#include <esp_system.h>
#include <esp_timer.h>
#include <sys/time.h>

namespace {
uint64_t utcMs() {
  timeval tv{};
  gettimeofday(&tv, nullptr);
  return static_cast<uint64_t>(tv.tv_sec) * 1000 + tv.tv_usec / 1000;
}
uint64_t nowMs() { return static_cast<uint64_t>(esp_timer_get_time() / 1000); }
} // namespace
bool PreviewClient::begin() {
  mutex_ = xSemaphoreCreateMutex();
  return mutex_ && xTaskCreate(run, "preview", 12288, this, 1, nullptr) == pdPASS;
}
void PreviewClient::run(void *self) { static_cast<PreviewClient *>(self)->task(); }
uint8_t PreviewClient::brightness(uint64_t now, uint8_t baseline) {
  if (!mutex_)
    return baseline;
  xSemaphoreTake(mutex_, portMAX_DELAY);
  if (!available_.load() || WiFi.status() != WL_CONNECTED)
    state_.disconnect();
  applying_.generation = state_.generation();
  applying_.received = state_.receivedAt();
  applying_.receivedUtc = receivedUtc_;
  applying_.raw = state_.brightness(now, baseline);
  applying_.active = state_.active(now);
  applying_.valid = state_.seen();
  xSemaphoreGive(mutex_);
  return applying_.raw;
}
void PreviewClient::presented(uint64_t now) {
  if (!mutex_ || !applying_.valid)
    return;
  xSemaphoreTake(mutex_, portMAX_DELAY);
  if (!ack_.valid || ack_.generation != applying_.generation || ack_.raw != applying_.raw ||
      ack_.active != applying_.active) {
    ack_ = applying_;
    ack_.applied = now;
    ack_.appliedUtc = utcMs();
    ack_.valid = true;
  }
  xSemaphoreGive(mutex_);
}
void PreviewClient::task() {
  FeedClient client;
  if (!client.begin()) {
    vTaskDelete(nullptr);
    return;
  }
  char boot[33];
  for (unsigned i = 0; i < 4; ++i)
    snprintf(boot + i * 8, 9, "%08lx", static_cast<unsigned long>(esp_random()));
  bool registered = false;
  uint64_t pollAt = 0, ackAt = 0, activeHeartbeatAt = 0;
  uint64_t sentGeneration = UINT64_MAX;
  uint8_t sentRaw = 255;
  bool sentActive = false;
  unsigned failures = 0;
  uint64_t requests = 0, rowsRead = 0, rowsWritten = 0;
  const auto count = [&](const std::string &response) {
    ++requests;
    StaticJsonDocument<1536> doc;
    if (!deserializeJson(doc, response) && doc["metrics"]["measured"] == true) {
      rowsRead += doc["metrics"]["rowsRead"].as<uint64_t>();
      rowsWritten += doc["metrics"]["rowsWritten"].as<uint64_t>();
    }
  };
  while (true) {
    const uint64_t tick = nowMs();
    if (!available_.load() || WiFi.status() != WL_CONNECTED) {
      xSemaphoreTake(mutex_, portMAX_DELAY);
      state_.disconnect();
      xSemaphoreGive(mutex_);
      vTaskDelay(pdMS_TO_TICKS(100));
      continue;
    }
    if (tick >= pollAt) {
      std::string response;
      const uint64_t requestAt = nowMs();
      uint32_t retryMs = 0;
      int status = client.control(registered ? "/v1/device/control" : "/v1/device/control/boot",
                                  registered ? "GET" : "POST", boot, "", response, retryMs);
      count(response);
      bool ok = status == 200;
      if (ok && !registered) {
        registered = true;
        pollAt = nowMs();
        continue;
      }
      transit::PreviewCommand command;
      if (ok)
        ok = transit::parsePreview(response, boot, command);
      bool active = false;
      xSemaphoreTake(mutex_, portMAX_DELAY);
      if (ok && available_.load() && WiFi.status() == WL_CONNECTED) {
        if (state_.receive(command, requestAt, nowMs()))
          receivedUtc_ = utcMs();
      } else
        state_.disconnect();
      active = state_.active(nowMs());
      xSemaphoreGive(mutex_);
      if (ok)
        failures = 0;
      else
        ++failures;
      // A boot conflict fails closed. Never repeatedly re-register and fight a newer boot.
      const uint32_t interval = ok ? (active ? 1000 : 2000)
                                   : (status == 401 || status == 403 || status == 409
                                          ? 60000
                                          : std::min(30000u, 2000u << std::min(failures, 3u)));
      pollAt = nowMs() + std::max(interval, retryMs) + (ok ? 0 : esp_random() % 250);
    }
    Ack ack;
    xSemaphoreTake(mutex_, portMAX_DELAY);
    ack = ack_;
    // Do not renew an active ACK after RAM expiry/disconnection while render catches up.
    if (ack.active && !state_.active(nowMs()))
      ack.valid = false;
    xSemaphoreGive(mutex_);
    if (registered && ack.valid && nowMs() >= ackAt &&
        (ack.generation != sentGeneration || ack.raw != sentRaw || ack.active != sentActive ||
         (ack.active && nowMs() >= activeHeartbeatAt))) {
      StaticJsonDocument<768> doc;
      doc["generation"] = ack.generation;
      doc["raw"] = ack.raw;
      doc["active"] = ack.active;
      doc["receivedMs"] = ack.received;
      doc["appliedMs"] = ack.applied;
      doc["receivedUtcMs"] = ack.receivedUtc;
      doc["appliedUtcMs"] = ack.appliedUtc;
      doc["requests"] = requests;
      doc["rowsRead"] = rowsRead;
      doc["rowsWritten"] = rowsWritten;
      std::string body;
      serializeJson(doc, body);
      std::string response;
      uint32_t retryMs = 0;
      const int status =
          client.control("/v1/device/control/ack", "POST", boot, body, response, retryMs);
      count(response);
      if (status == 200) {
        sentGeneration = ack.generation;
        sentRaw = ack.raw;
        sentActive = ack.active;
        activeHeartbeatAt = nowMs() + 5000;
      }
      ackAt = nowMs() + std::max(status == 401 || status == 403 ? 60000u : 1000u, retryMs);
    }
    vTaskDelay(pdMS_TO_TICKS(25));
  }
}
