#pragma once
namespace provisioning {
inline constexpr char WifiSuccessPage[] =
    "<!doctype html><html lang=en><head><meta charset=utf-8>"
    "<meta name=viewport content='width=device-width,initial-scale=1'>"
    "<title>Display Wi-Fi</title><style>"
    "body{font:18px system-ui;max-width:32rem;margin:2rem auto;padding:1rem}"
    "main{padding:2rem 0}h1{margin:0 0 2rem}p{margin:1.5rem 0;line-height:1.6}"
    "</style></head><body><main><h1>Connected!</h1>"
    "<p>Your display is ready.</p><p>You can close this page.</p>"
    "</main></body></html>";
} // namespace provisioning
