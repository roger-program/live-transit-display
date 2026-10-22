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
bool v23 = true;
unsigned variant = 0;
bool swapped = false;
badgecontrast::Comparison comparison = badgecontrast::Comparison::BlackWhite;
void report() {
  if (v23) {
    Serial.printf("V23 variant %c; background=#547B31; brightness=4/255\n", char('A' + variant));
    return;
  }
  Serial.printf("Badge contrast %u: %s #%06lx; left=%s right=%s; brightness=4\n", selected,
                badgecontrast::Samples[selected].label,
                static_cast<unsigned long>(badgecontrast::Samples[selected].background),
                badgecontrast::faceName(badgecontrast::faceFor(comparison, 0, swapped)),
                badgecontrast::faceName(badgecontrast::faceFor(comparison, 1, swapped)));
}
} // namespace
void setup() {
  static_assert(appcfg::Brightness == 4, "Keep experiment brightness at 4/255");
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
    if (key >= 'A' && key <= 'D') {
      v23 = true;
      variant = key - 'A';
    } else if (key == 'm') {
      v23 = !v23;
    } else if (key >= '0' && key <= '7')
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
  if (v23)
    badgecontrast::renderV23(frame, variant);
  else
    badgecontrast::render(frame, selected, swapped, comparison);
  display.present(frame, appcfg::Brightness);
  delay(50);
}
#endif
