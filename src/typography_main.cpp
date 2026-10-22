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
  Serial.printf("Typography: %s; sample=%u; %s / %s / %s; phase=%llu ms; brightness=4/255\n",
                typography::faceName(selected.face), selected.sample, sample.stop, sample.line,
                sample.destination, static_cast<unsigned long long>(selected.phase()));
  if (selected.face == typography::Face::C2) {
    Serial.println("C2 label policy: uppercase/accent folding; U+2019 uses apostrophe bitmap");
    const auto missing = typography::c2Missing(std::string(sample.stop) + " " + sample.destination);
    if (!missing.empty())
      Serial.printf(
          "C2 missing label glyphs after normalization: %s (render as ?; no other font)\n",
          missing.c_str());
  }
  Serial.println("a/b/c/d/e: A/B/C/D/C2; 0-9: existing samples; f: alphabet; g: Barcelona; h: "
                 "Q/digits/punctuation; "
                 "n/p: all samples; s: reveal step; r: initial hold; i/?: report");
}
void show() {
  typography::render(frame, selected.face, selected.sample, selected.phase());
  display.present(frame, appcfg::Brightness);
  report();
}
} // namespace
void setup() {
  static_assert(appcfg::Brightness == 4, "Keep typography brightness at 4/255");
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
