#include "display_driver.h"

bool PrototypeDisplay::begin() {
  if (!panelcfg::Enabled)
    return true; // Null sink: compile/replay without driving GPIO.
  HUB75_I2S_CFG config(panelcfg::Width, panelcfg::Height, panelcfg::ChainLength, panelcfg::Pins);
  config.driver = panelcfg::Driver;
  config.i2sspeed = panelcfg::Clock;
  config.clkphase = panelcfg::ClockPhase;
  config.latch_blanking = panelcfg::LatchBlanking;
  config.double_buff = true;
  panel_.reset(new MatrixPanel_I2S_DMA(config));
  if (!panel_->begin())
    return false;
  panel_->setBrightness8(0);
  panel_->clearScreen();
  return true;
}
void PrototypeDisplay::present(const transit::Framebuffer &frame, uint8_t brightness) {
  if (!panel_)
    return;
  if (brightness != brightness_) {
    panel_->setBrightness8(brightness);
    brightness_ = brightness;
  }
  for (int y = 0; y < frame.height(); ++y)
    for (int x = 0; x < frame.width(); ++x) {
      const auto color = frame.pixel(x, y);
      panel_->drawPixelRGB888(x, y, color >> 16, color >> 8, color);
    }
  panel_->flipDMABuffer();
}
