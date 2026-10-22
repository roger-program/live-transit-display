#include "app_config.h"
#include "brightness.h"
#include "display_driver.h"
#include "feed_client.h"
#include "fixture.h"
#include "renderer.h"
#include <Arduino.h>
#include <WiFi.h>
#include <atomic>
#include <esp_log.h>
#include <esp_sntp.h>
#include <esp_timer.h>
#include <sys/time.h>

namespace {
PrototypeDisplay display;
transit::ArrivalRenderer renderer;
transit::Framebuffer frame(panelcfg::Width *panelcfg::ChainLength, panelcfg::Height);
transit::Snapshot snapshot;
transit::Snapshot mailbox;
SemaphoreHandle_t mailboxMutex;
bool mailboxPending = false;
std::atomic<bool> synchronized{false};
uint64_t animationStart = 0;
uint8_t configuredBrightness = appcfg::Brightness;
std::string serialCommand;
bool serialOverflow = false;
void readBrightness() {
  while (Serial.available()) {
    const char ch = static_cast<char>(Serial.read());
    if (ch == '\n' || ch == '\r') {
      if (!serialOverflow && !serialCommand.empty()) {
        if (transit::brightnessPreset(serialCommand, configuredBrightness))
          Serial.printf("Brightness %u/255\n", configuredBrightness);
        else
          Serial.println("Brightness commands: b4, b8, b16, b32 (then Enter)");
      }
      serialCommand.clear();
      serialOverflow = false;
    } else if (serialCommand.size() < 8 && !serialOverflow)
      serialCommand += ch;
    else
      serialOverflow = true;
  }
}
unsigned diagnostic = 0;
uint64_t diagnosticStart = 0;

uint64_t uptimeMs() { return static_cast<uint64_t>(esp_timer_get_time() / 1000); }
int64_t utcMs() {
  timeval tv{};
  gettimeofday(&tv, nullptr);
  return static_cast<int64_t>(tv.tv_sec) * 1000 + tv.tv_usec / 1000;
}
bool clockValid() { return synchronized.load() && utcMs() >= 1704067200000LL; }
void printInfo() {
  Serial.printf("Firmware %s; flash=%u; psram=%u; panel=%s\n", appcfg::FirmwareVersion,
                ESP.getFlashChipSize(), ESP.getPsramSize(),
                panelcfg::Enabled ? "enabled" : "disabled");
}
void publish(transit::Snapshot &next) {
  xSemaphoreTake(mailboxMutex, portMAX_DELAY);
  mailbox = std::move(next);
  mailboxPending = true;
  xSemaphoreGive(mailboxMutex);
}
void networkTask(void *) {
  FeedClient client;
  std::vector<std::string> selectors(std::begin(appcfg::Stops), std::end(appcfg::Stops));
  if (!client.configured() || selectors.empty() || selectors.size() > transit::MaxStops) {
    transit::Snapshot next;
    next.error = transit::RequestError::NoConfig;
    publish(next);
    vTaskDelete(nullptr);
    return;
  }
  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  sntp_set_time_sync_notification_cb([](timeval *) { synchronized.store(true); });
  configTime(0, 0, "pool.ntp.org", "time.cloudflare.com");
  uint64_t reconnectAt = 0;
  uint64_t pollAt = 0;
  bool offlinePublished = false;
  while (true) {
    const uint64_t tick = uptimeMs();
    if (WiFi.status() != WL_CONNECTED) {
      if (!offlinePublished) {
        transit::Snapshot next;
        next.error = transit::RequestError::Network;
        publish(next);
        offlinePublished = true;
      }
      if (tick >= reconnectAt) {
        WiFi.begin(localcfg::WifiSsid, localcfg::WifiPassword);
        reconnectAt = tick + 15000;
      }
    } else if (!clockValid()) {
      transit::Snapshot next;
      next.error = transit::RequestError::Clock;
      publish(next);
    } else if (tick >= pollAt) {
      auto result = client.fetch(selectors);
      transit::Snapshot next;
      next.error = result.error;
      next.hasFeed = result.error == transit::RequestError::None;
      next.feed = std::move(result.feed);
      next.receivedAtMs = uptimeMs();
      publish(next);
      offlinePublished = false;
      pollAt = uptimeMs() + result.retryMs;
    }
    vTaskDelay(pdMS_TO_TICKS(250));
  }
}
} // namespace
void setup() {
  // Suppress SDK HTTP/TLS logs: they can contain URLs or raw exception details.
  esp_log_level_set("*", ESP_LOG_NONE);
  Serial.begin(115200);
  printInfo();
  if (APP_MODE != 1)
    Serial.println("Brightness: b4, b8, b16, b32 then Enter; default 16/255");
  if (!display.begin()) {
    Serial.println("Display initialization failed");
    while (true)
      delay(1000);
  }
  animationStart = uptimeMs();
  diagnosticStart = animationStart;
  if (APP_MODE == 2) {
    snapshot.hasFeed = transit::parseFeed(FixtureJson, snapshot.feed) == transit::ParseResult::Ok;
    snapshot.receivedAtMs = animationStart;
  } else if (APP_MODE == 0) {
    mailboxMutex = xSemaphoreCreateMutex();
    if (!mailboxMutex || xTaskCreate(networkTask, "feed", 16384, nullptr, 1, nullptr) != pdPASS)
      snapshot.error = transit::RequestError::Network;
  } else {
    Serial.println(
        "Diagnostics: 0 red, 1 green, 2 blue, 3 pixels, 4 rows, 5 columns, 6 text, 7 brightness");
  }
}
void loop() {
  const uint64_t tick = uptimeMs();
  if (APP_MODE != 1)
    readBrightness();
  uint8_t brightness = configuredBrightness;
  if (APP_MODE == 1) {
    while (Serial.available()) {
      int input = Serial.read();
      if (input == 'i')
        printInfo();
      if (input >= '0' && input <= '7') {
        diagnostic = input - '0';
        diagnosticStart = tick;
        Serial.printf("Diagnostic %u\n", diagnostic);
      }
    }
    brightness = transit::renderDiagnostic(frame, static_cast<transit::Diagnostic>(diagnostic),
                                           tick - diagnosticStart);
  } else {
    if (APP_MODE == 0 && mailboxMutex && xSemaphoreTake(mailboxMutex, 0) == pdTRUE) {
      if (mailboxPending) {
        if (snapshot.error != mailbox.error || snapshot.hasFeed != mailbox.hasFeed)
          animationStart = tick;
        snapshot = std::move(mailbox);
        mailboxPending = false;
      }
      xSemaphoreGive(mailboxMutex);
    }
    const int64_t now =
        APP_MODE == 2 ? snapshot.feed.generatedAt + static_cast<int64_t>(tick - animationStart)
                      : utcMs();
    const auto view = transit::viewAt(snapshot, now, tick, APP_MODE == 2 || clockValid());
    renderer.render(frame, view, now, tick - animationStart);
  }
  display.present(frame, brightness);
  delay(50); // 20 fps; network/TLS work runs separately from animation.
}
