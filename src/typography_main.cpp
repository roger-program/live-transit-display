#include "app_config.h"
#if APP_MODE == 4
#include "display_driver.h"
#include "typography_diagnostic.h"
#include <Arduino.h>
#include <esp_log.h>

namespace {
PrototypeDisplay display;
transit::Framebuffer frame(64, 32);
typography::Selection selected;
void report() {
  const auto &sample = typography::Samples[selected.sample];
  Serial.printf("Typography: %s; sample=%u; %s / %s / %s; phase=%llu ms; brightness=16/255\n",
                typography::faceName(selected.face), selected.sample, sample.stop, sample.line,
                sample.destination, static_cast<unsigned long long>(selected.phase()));
  if (selected.face == typography::Face::C2 &&
      (!typography::c2Supports(sample.stop) || !typography::c2Supports(sample.destination)))
    Serial.println("C2 subset: unsupported label characters render as ?; use focused samples 6-9");
  Serial.println("a: A; c: C; e: C2 (b/d retained); 0-9 or n/p: sample; s: reveal step; r: initial "
                 "hold; i/?: report");
}
void show() {
  typography::render(frame, selected.face, selected.sample, selected.phase());
  display.present(frame, appcfg::Brightness);
  report();
}
} // namespace
void setup() {
  static_assert(appcfg::Brightness == 16, "Keep typography brightness at 16/255");
  static_assert(panelcfg::Enabled && panelcfg::Width == 64 && panelcfg::Height == 32 &&
                    panelcfg::ChainLength == 1,
                "Typography diagnostic requires one enabled 64x32 panel");
  esp_log_level_set("*", ESP_LOG_NONE);
  Serial.begin(115200);
  if (!display.begin()) {
    Serial.println("Display initialization failed");
    while (true)
      delay(1000);
  }
  show();
}
void loop() {
  while (Serial.available())
    if (selected.command(Serial.read()))
      show();
  delay(20);
}
#endif
