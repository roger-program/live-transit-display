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
static_assert(transit::OperatorSymbols[0].foreground == 0xffffff,
              "FGC diagnostic symbol must use white foreground");
PrototypeDisplay display;
transit::Framebuffer frame(64, 32);
transit::ArrivalRenderer renderer;
transit::Snapshot snapshot;
uint64_t replayStart = 0;
uint64_t animationOffset = 0;
size_t reportedPage = static_cast<size_t>(-1);
constexpr const char *VariantLabels[] = {"A #FFFFFF", "B #999999", "C #666666"};
static_assert(transit::OperatorSymbols[1].foreground == 0x999999 &&
                  transit::OperatorSymbols[2].foreground == 0x666666,
              "Diagnostic symbol foreground variants must match the fixture");
uint64_t tickMs() { return static_cast<uint64_t>(esp_timer_get_time() / 1000); }
void report() {
  Serial.println("FGC synthetic fixture; 64x32; brightness=4/255; offline application; no "
                 "credential/configuration access");
  Serial.println("Synthetic realtime-style labels only; not live predictions.");
  Serial.println("r: restart replay; n: next page; i: report; stale after 120 seconds");
}
void resetReplay() {
  replayStart = tickMs();
  snapshot.receivedAtMs = replayStart;
  animationOffset = 0;
  reportedPage = static_cast<size_t>(-1);
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
    } else if (command == 'i') {
      report();
      reportedPage = static_cast<size_t>(-1);
    }
  }
  const uint64_t tick = tickMs();
  const uint64_t elapsed = tick - replayStart;
  // Same production parser/state/countdown/render path, using a synthetic
  // capture clock.
  const int64_t now = snapshot.feed.generatedAt + static_cast<int64_t>(elapsed);
  const auto view = transit::viewAt(snapshot, now, tick, true);
  const auto pages = transit::pagesFor(view, frame.height());
  if (!pages.empty()) {
    const size_t index = ((elapsed + animationOffset) / transit::PageDurationMs) % pages.size();
    if (index != reportedPage) {
      reportedPage = index;
      const auto &page = pages[index];
      if (page.groupCount && pages.size() == 24) {
        const auto &stop = view.stops[page.stopIndex];
        const auto &group = stop.groups[page.groupStart];
        Serial.printf("Page %u/%u: %s | %s | %s | %s | background %s | text %s\n",
                      static_cast<unsigned>(index + 1), static_cast<unsigned>(pages.size()),
                      VariantLabels[page.stopIndex / 2], stop.name.c_str(), group.line.c_str(),
                      group.destination.c_str(), group.lineColor.c_str(),
                      group.lineTextColor.c_str());
      } else
        Serial.println("STALE / no predictions; r restarts the synthetic comparison.");
    }
  }
  renderer.render(frame, view, now, elapsed + animationOffset);
  display.present(frame, TestBrightness);
  delay(20);
}
#endif
