#include "app_config.h"
#if APP_MODE == 3
#include "badge_contrast.h"
#include "display_driver.h"
#include <Arduino.h>
#include <esp_log.h>

namespace {
PrototypeDisplay display;
transit::Framebuffer frame(64, 32);
unsigned selected = 0;
bool swapped = false;
badgecontrast::Comparison comparison = badgecontrast::Comparison::BlackWhite;
void report() {
  Serial.printf("Badge contrast %u: %s #%06lx; left=%s right=%s; brightness=16\n", selected,
                badgecontrast::Samples[selected].label,
                static_cast<unsigned long>(badgecontrast::Samples[selected].background),
                badgecontrast::faceName(badgecontrast::faceFor(comparison, 0, swapped)),
                badgecontrast::faceName(badgecontrast::faceFor(comparison, 1, swapped)));
}
} // namespace
void setup() {
  static_assert(appcfg::Brightness == 16, "Keep experiment brightness at 16/255");
  static_assert(panelcfg::Width == 64 && panelcfg::Height == 32 && panelcfg::ChainLength == 1,
                "Experiment requires one 64x32 panel");
  esp_log_level_set("*", ESP_LOG_NONE);
  Serial.begin(115200);
  if (!display.begin()) {
    Serial.println("Display initialization failed");
    while (true)
      delay(1000);
  }
  report();
}
void loop() {
  while (Serial.available()) {
    const int key = Serial.read();
    if (key >= '0' && key <= '7')
      selected = key - '0';
    else if (key == 'n')
      selected = (selected + 1) % badgecontrast::Count;
    else if (key == 'p')
      selected = (selected + badgecontrast::Count - 1) % badgecontrast::Count;
    else if (key == 'a')
      comparison = badgecontrast::Comparison::BlackWhite;
    else if (key == 'b')
      comparison = badgecontrast::Comparison::BlackBold;
    else if (key == 'c')
      comparison = badgecontrast::Comparison::WhiteBold;
    else if (key == 's')
      swapped = !swapped;
    else if (key != 'i')
      continue;
    report();
  }
  badgecontrast::render(frame, selected, swapped, comparison);
  display.present(frame, appcfg::Brightness);
  delay(50);
}
#endif
