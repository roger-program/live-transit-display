#pragma once
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>

namespace panelcfg {
// PROPOSED ONLY: review exact development-board schematic before enabling.
// Module model does not establish development-board pin exposure/peripheral use.
#ifndef ENABLE_PANEL_OUTPUT
#define ENABLE_PANEL_OUTPUT 0
#endif
inline constexpr bool Enabled = ENABLE_PANEL_OUTPUT != 0;
inline constexpr uint16_t Width = 64, Height = 32;
inline constexpr uint8_t ChainLength = 1;
// HUB75: R1 G1 B1 R2 G2 B2 A B C D E LAT OE CLK.
// Avoid flash/PSRAM pins 26..37, USB 19/20, UART 43/44, boot straps 0/3/45/46.
inline constexpr HUB75_I2S_CFG::i2s_pins Pins = {4, 5, 6, 7, 8, 9, 10, 11, 12, 13, -1, 14, 15, 16};
inline constexpr auto Driver = HUB75_I2S_CFG::SHIFTREG;
inline constexpr auto Clock = HUB75_I2S_CFG::HZ_8M;
inline constexpr bool ClockPhase = false;
inline constexpr uint8_t LatchBlanking = 2;
// Internal SRAM DMA for this small panel; octal PSRAM is enabled for the app.
// SPIRAM_FRAMEBUFFER and larger matrices are later measurable experiments.
} // namespace panelcfg
