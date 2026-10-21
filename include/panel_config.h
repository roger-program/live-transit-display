#pragma once
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>

namespace panelcfg {
// Waveshare ESP32-S3-RGB-Matrix controller; physical timing/IC validation pending.
#ifndef ENABLE_PANEL_OUTPUT
#define ENABLE_PANEL_OUTPUT 0
#endif
inline constexpr bool Enabled = ENABLE_PANEL_OUTPUT != 0;
inline constexpr uint16_t Width = 64, Height = 32;
inline constexpr uint8_t ChainLength = 1;
// HUB75: R1 G1 B1 R2 G2 B2 A B C D E LAT OE CLK.
// Official board wiring includes GPIO3. E is GPIO9 on the board, unused for 1/16 scan.
inline constexpr HUB75_I2S_CFG::i2s_pins Pins = {4, 5, 6, 7, 15, 16, 18, 8, 3, 42, -1, 40, 2, 41};
inline constexpr auto Driver = HUB75_I2S_CFG::SHIFTREG;
inline constexpr auto Clock = HUB75_I2S_CFG::HZ_8M;
inline constexpr bool ClockPhase = false;
inline constexpr uint8_t LatchBlanking = 2;
// Internal SRAM DMA for this small panel; octal PSRAM is enabled for the app.
// SPIRAM_FRAMEBUFFER and larger matrices are later measurable experiments.
} // namespace panelcfg
