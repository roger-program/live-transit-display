#pragma once
#include <cstdint>
#if __has_include("secrets.h")
#include "secrets.h"
#else
namespace localcfg {
inline constexpr char WifiSsid[] = "";
inline constexpr char WifiPassword[] = "";
inline constexpr char FeedOrigin[] = "";
inline constexpr char RootCa[] = "";
} // namespace localcfg
#endif
#ifndef APP_MODE
#define APP_MODE 0 // 0: live feed; 1: hardware diagnostics; 2: offline replay
#endif
namespace appcfg {
inline constexpr char FirmwareVersion[] = "prototype-0.1.0";
inline constexpr uint8_t Brightness = 4; // 0..255, start low; not calibrated luminance.
} // namespace appcfg
