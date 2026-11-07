#ifdef FGC_FIXTURE_TEST
#include "display_driver.h"
#include "feed_parser.h"
#include "fixture.h"
#include "operator_symbols.h"
#include "renderer.h"
#include <Arduino.h>
#include <esp_log.h>
#include <esp_timer.h>

namespace {
// Deliberately no app_config/secrets, Wi-Fi, provisioning or persistent
// storage.
constexpr uint8_t TestBrightness = 4;
static_assert(transit::OperatorSymbols[0].foreground == 0xd9d9d9,
              "FGC diagnostic symbol must use the softer foreground");
PrototypeDisplay display;
transit::Framebuffer frame(64, 32);
transit::ArrivalRenderer renderer;
transit::Snapshot snapshot;
uint64_t replayStart = 0;
uint64_t animationOffset = 0;
uint64_t tickMs() { return static_cast<uint64_t>(esp_timer_get_time() / 1000); }
void report() {
  Serial.println("FGC synthetic fixture; 64x32; brightness=4/255; offline application; no "
                 "credential/configuration access");
  Serial.println("r: restart replay; n: next page; i: report; stale after 120 seconds");
}
void resetReplay() {
  replayStart = tickMs();
  snapshot.receivedAtMs = replayStart;
  animationOffset = 0;
  report();
}
} // namespace
void setup() {
  static_assert(panelcfg::Enabled && panelcfg::Width == 64 && panelcfg::Height == 32 &&
                    panelcfg::ChainLength == 1,
                "FGC fixture requires one enabled 64x32 panel");
  esp_log_level_set("*", ESP_LOG_NONE);
  Serial.begin(115200);
  if (!display.begin() ||
      transit::parseFeed(FgcFixtureJson, snapshot.feed) != transit::ParseResult::Ok) {
    Serial.println("FGC fixture initialization failed");
    while (true)
      delay(1000);
  }
  snapshot.hasFeed = true;
  resetReplay();
}
void loop() {
  unsigned remaining = 32;
  while (remaining-- && Serial.available()) {
    const int command = Serial.read();
    if (command == 'r')
      resetReplay();
    else if (command == 'n') {
      const uint64_t elapsed = tickMs() - replayStart + animationOffset;
      animationOffset += transit::PageDurationMs - elapsed % transit::PageDurationMs;
    } else if (command == 'i')
      report();
  }
  const uint64_t tick = tickMs();
  const uint64_t elapsed = tick - replayStart;
  // Same production parser/state/countdown/render path, using a synthetic
  // capture clock.
  const int64_t now = snapshot.feed.generatedAt + static_cast<int64_t>(elapsed);
  const auto view = transit::viewAt(snapshot, now, tick, true);
  renderer.render(frame, view, now, elapsed + animationOffset);
  display.present(frame, TestBrightness);
  delay(20);
}
#endif
