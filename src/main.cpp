#include "app_config.h"
#include "brightness.h"
#include "display_driver.h"
#include "feed_client.h"
#include "fixture.h"
#include "renderer.h"
#include "setup_button.h"
#include "wifi_setup.h"
#include <Arduino.h>
#include <WiFi.h>
#include <atomic>
#include <esp_log.h>
#include <esp_sntp.h>
#include <esp_timer.h>
#include <sys/time.h>

namespace {
PrototypeDisplay display;
WifiSetup wifiSetup;
constexpr uint8_t BootButtonPin = 0; // Waveshare's documented active-low BOOT input.
provisioning::SetupButton bootButton;
uint64_t lastButtonPollAt = 0;
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
  unsigned remaining = 64; // Continuous serial traffic must not starve the BOOT poll.
  while (remaining-- && Serial.available()) {
    const char ch = static_cast<char>(Serial.read());
    if (ch == '\n' || ch == '\r') {
      if (!serialOverflow && !serialCommand.empty()) {
        if (APP_MODE == 0 && serialCommand == "wifi")
          wifiSetup.request();
        else if (transit::brightnessPreset(serialCommand, configuredBrightness))
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
  // Safe summaries only: never print SSID, URL, token, body or SDK errors.
  static transit::RequestError lastError = transit::RequestError::None;
  if (next.hasFeed) {
    unsigned arrivals = 0;
    unsigned realtime = 0;
    for (const auto &stop : next.feed.stops)
      for (const auto &group : stop.groups)
        for (const auto &arrival : group.arrivals) {
          ++arrivals;
          if (arrival.provenance == transit::Provenance::Realtime)
            ++realtime;
        }
    Serial.printf("Feed OK; generatedAt=%lld; stops=%u; arrivals=%u; realtime=%u\n",
                  static_cast<long long>(next.feed.generatedAt),
                  static_cast<unsigned>(next.feed.stops.size()), arrivals, realtime);
  } else if (next.error != lastError)
    Serial.printf("Feed state: %s\n", transit::requestMessage(next.error));
  lastError = next.error;
  xSemaphoreTake(mailboxMutex, portMAX_DELAY);
  mailbox = std::move(next);
  mailboxPending = true;
  xSemaphoreGive(mailboxMutex);
}
void networkTask(void *) {
  FeedClient client;
  wifiSetup.begin();
  if (!client.begin()) {
    transit::Snapshot next;
    next.error = transit::RequestError::NoConfig;
    publish(next);
    while (true) {
      wifiSetup.tick(uptimeMs());
      vTaskDelay(pdMS_TO_TICKS(50));
    }
  }
  sntp_set_time_sync_notification_cb([](timeval *) { synchronized.store(true); });
  configTime(0, 0, "pool.ntp.org", "time.cloudflare.com");
  uint64_t pollAt = 0;
  bool offlinePublished = false;
  bool connected = false;
  bool waitingForClock = false;
  while (true) {
    const uint64_t tick = uptimeMs();
    wifiSetup.tick(tick);
    if (WiFi.status() != WL_CONNECTED) {
      if (connected)
        Serial.println("Wi-Fi disconnected; reconnecting");
      connected = false;
      waitingForClock = false;
      if (!offlinePublished) {
        transit::Snapshot next;
        next.error = transit::RequestError::Network;
        publish(next);
        offlinePublished = true;
      }
    } else {
      if (!connected) {
        Serial.println("Wi-Fi connected");
        connected = true;
        offlinePublished = false;
      }
      if (!clockValid()) {
        if (!waitingForClock) {
          transit::Snapshot next;
          next.error = transit::RequestError::Clock;
          publish(next);
          waitingForClock = true;
        }
      } else if (!wifiSetup.active() && tick >= pollAt) {
        auto result = client.fetch();
        transit::Snapshot next;
        next.error = result.error;
        next.hasFeed = result.error == transit::RequestError::None;
        next.feed = std::move(result.feed);
        next.receivedAtMs = uptimeMs();
        publish(next);
        offlinePublished = false;
        pollAt = uptimeMs() + result.retryMs;
        waitingForClock = false;
      }
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
    Serial.println("Brightness: b4, b8, b16, b32 then Enter; reset uses configured brightness");
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
    pinMode(BootButtonPin, INPUT_PULLUP);
    Serial.printf("BOOT: GPIO0 initial level=%s (active low)\n",
                  digitalRead(BootButtonPin) == LOW ? "LOW" : "HIGH");
    Serial.println("Hold BOOT for 3 seconds after startup, or enter wifi, for Wi-Fi setup");
    const bool wifiReady = wifiSetup.initialize();
    mailboxMutex = xSemaphoreCreateMutex();
    if (!wifiReady || !mailboxMutex ||
        xTaskCreate(networkTask, "feed", 16384, nullptr, 1, nullptr) != pdPASS)
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
  if (APP_MODE == 0) {
    if (lastButtonPollAt && tick - lastButtonPollAt > 250)
      Serial.printf("BOOT: polling delayed %llu ms\n",
                    static_cast<unsigned long long>(tick - lastButtonPollAt));
    lastButtonPollAt = tick;
    const bool wasPressed = bootButton.pressed();
    const bool activated = bootButton.update(digitalRead(BootButtonPin) == LOW, tick);
    if (bootButton.pressed() != wasPressed)
      Serial.println(bootButton.pressed() ? "BOOT: pressed" : "BOOT: released");
    if (activated) {
      Serial.println("BOOT: long press detected; requesting Wi-Fi setup");
      wifiSetup.request();
    }
  }
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
        transit::applySnapshot(snapshot, std::move(mailbox));
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
  if (APP_MODE == 0)
    wifiSetup.render(frame);
  display.present(frame, brightness);
  delay(50); // 20 fps; network/TLS work runs separately from animation.
}
