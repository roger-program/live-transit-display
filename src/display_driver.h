#pragma once
#include "framebuffer.h"
#include "panel_config.h"
#include <memory>

class PrototypeDisplay final : public transit::DisplayDriver {
public:
  bool begin() override;
  void present(const transit::Framebuffer &frame, uint8_t brightness) override;

private:
  std::unique_ptr<MatrixPanel_I2S_DMA> panel_;
  uint8_t brightness_ = 0;
};
