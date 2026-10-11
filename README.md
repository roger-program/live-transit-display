# live-transit-display

Prototype firmware for a Barcelona public-transport arrival display. Feed Protocol v1 remains unchanged; transport-provider integration stays in the sibling `live-transit-feed` repository. No provisioning, device registry or OTA is implemented.

## Architecture

`network/config → HTTPS feed client → normalized state → countdown/freshness → renderer → display driver`

- `src/main.cpp`: Wi-Fi reconnect, SNTP clock, background polling and main render loop. A mutex-protected mailbox keeps blocking TLS work out of animation.
- `src/feed_client.*`: certificate-validated HTTPS, Bearer header, redirect refusal and bounded response capture.
- `lib/transit/src/`: portable C++17 parser, request policy, model, countdown/state logic, bitmap renderer and RGB framebuffer. No Arduino, GPIO or provider API formats in this layer.
- `src/display_driver.*`: HUB75 DMA adapter with double buffering; consumes only pixels and brightness.
- `include/app_config.h`, `panel_config.h`: laboratory stop selection and isolated panel geometry/GPIO/timing. `secrets.h` is local and ignored.
- `fixtures/`, `test/`, `tools/`: historical/synthetic replay data, host tests and actual C++ raster previews.

PlatformIO + Arduino was chosen over standalone ESP-IDF for the small prototype setup, existing Wi-Fi support and straightforward HUB75 integration. ESP-IDF gives finer SDK/configuration control; it is worth revisiting if measured constraints require it. This build pins Espressif32 6.12.0 / Arduino 2.0.17, ArduinoJson 6.21.5 and HUB75 DMA 3.0.14. Detailed reasoning, source review and test evidence: [docs/phase-1.md](docs/phase-1.md).

## Prototype hardware

ESP32-S3-WROOM-1-N16R8: 16 MB **quad flash**, 8 MB **octal PSRAM**, Wi-Fi. The generic DevKitC board definition is overridden with `qio_opi`, 16 MB flash and a 4 MB factory application partition. Remaining flash is unallocated. The build prints detected flash/PSRAM sizes at boot; actual detection awaits hardware.

Waveshare RGB Matrix P2.5 64×32: 160×80 mm, HUB75, 1/16 scan, 5 V. `ESP32-HUB75-MatrixPanel-DMA` provides an ESP32-S3 DMA driver and panel chaining. For this prototype its DMA buffers use internal SRAM; enabled PSRAM is available to application allocations. Larger panels require separate memory/refresh/layout measurements.

**The GPIO map, driver IC, clock phase, latch timing and electrical behaviour are unvalidated.** All ordinary environments disable panel output. `hub75-diagnostic` explicitly enables the proposed map; only flash that variant after checking the exact controller board and delivered panel.

## Build and offline checks

Install Python 3.11+ and a C++17 host compiler (macOS Command Line Tools or Linux GCC/Clang):

```sh
python3 -m venv .venv
. .venv/bin/activate
python -m pip install -r requirements-dev.txt
make check
make preview
```

Initial dependency installation needs internet access; subsequent tests use fixtures and make no feed/network requests. `make check` checks formatting/generated fixtures, runs native tests and builds all four firmware variants. `make format` formats owned C++ only. Host warnings are treated as errors. Open `artifacts/preview/index.html` to inspect C++ raster frames at 64×32 and 128×64.

| Environment | Behaviour |
| --- | --- |
| `esp32s3` | Live polling; `CONFIG` until local credentials/trust are supplied; panel disabled |
| `fixture` | Historical feed replay from `generatedAt`; no Wi-Fi or SNTP; becomes stale at 120 seconds; panel disabled |
| `diagnostic` | Serial-selected patterns; no network; panel disabled |
| `hub75-diagnostic` | Same diagnostics with proposed HUB75 output enabled |

Build artifacts are `.pio/build/<environment>/{firmware.bin,firmware.elf,bootloader.bin,partitions.bin}`. Reviewed copies, if present, are under ignored `artifacts/firmware/`; they contain no real credentials. Prefer rebuilding for your board/settings rather than flashing an old copy.

## Exact first hardware bring-up

1. With panel disconnected, identify the development-board model/schematic and verify the module is **WROOM-1-N16R8**, not a WROOM-2/N16R8V. Build/flash `diagnostic` through the correct USB connector:
   ```sh
   pio run -e diagnostic -t upload --upload-port <PORT>
   pio device monitor --port <PORT> --baud 115200
   ```
   If the board does not enumerate, use its documented BOOT/RESET sequence. USB CDC uses GPIO19/20; the proposed HUB75 map avoids them. Send `i` to repeat the safe boot report; verify flash `16777216`, PSRAM `8388608`, panel `disabled`. Stop and resolve mismatches before continuing. Actual USB behaviour is untested.
2. Power everything off. Inspect the panel's input connector orientation, exact SKU/driver IC and controller pin exposure. Compare with `panel_config.h`: `R1/G1/B1=4/5/6`, `R2/G2/B2=7/8/9`, `A/B/C/D=10/11/12/13`, `LAT=14`, `OE=15`, `CLK=16`, `E=-1`. These are ESP32 GPIO numbers, not connector positions. Check the actual HUB75 pin labels; do not copy a generic ESP32 pinout.
3. Confirm the external regulated 5 V supply and connector polarity with a meter before attaching the panel. Waveshare specifies 5 V / 2.5 A for this panel; use a suitably rated supply/wiring. Power the panel through its power connector, with a common signal ground to the controller. Do not power the panel from the controller's USB/3.3 V rail or apply 5 V to ESP32 GPIO. Review the delivered panel's input thresholds; use suitable 3.3 V→5 V signal buffering if required. Direct 3.3 V operation is **not validated**.
4. Wire the reviewed map to the panel **input** while unpowered. Flash `hub75-diagnostic`, then reset/open the serial monitor as above. It starts red at brightness 16/255. This variant really drives GPIO; do not flash it onto an unreviewed board. A boot message reporting display initialization failure requires investigation before proceeding.
5. Send the single ASCII digits `0`, `1`, `2`: verify uniform red, green and blue, including both panel halves. Send `3` for checkerboard/moving pixel; `4` for a moving row; `5` for a moving column. Observe a full sweep (rows 6.4 seconds, columns 12.8 seconds). Check orientation, duplicated/missing rows, colour order and ghosting. `6` shows text and opposite-corner markers. `7` cycles full-white brightness **8, 16, 32, 64** every two seconds; measure current/voltage and watch resets/flicker before increasing brightness further.
6. If patterns fail, power off and check wiring/ground/power/level shifting first. Confirm driver-IC support, then vary one setting at a time in `panel_config.h` (currently SHIFTREG, 8 MHz, phase false, latch blanking 2). Record photos/settings/measurements in `docs/phase-1.md`; do not call a visually plausible pattern a timing/electrical validation.
7. Once diagnostics are sound, explicitly enable `ENABLE_PANEL_OUTPUT=1` in the `fixture` build flags, rebuild and flash `fixture`. Check stop/group paging, both countdowns, text reveal, scheduled markers in host fixtures and stale suppression at 120 seconds. Record results. Leave live networking until this succeeds.
8. For a laboratory live test, copy `include/secrets.h.example` to ignored `include/secrets.h`; supply laboratory Wi-Fi, an HTTPS origin without path/query/userinfo, the development Bearer token and trusted root-CA PEM. Review `app_config.h` stop selectors. Explicitly enable panel output for `esp32s3`, rebuild/flash it, and verify SNTP, successful feed parsing, disconnect/reconnect, expired arrivals, stale data and request errors. Never copy TMB/operator credentials. No real network test has yet run on this firmware.

Do not save credential-bearing logs or binaries in version control. Local secrets are embedded in firmware flash; compile-time configuration is a laboratory shortcut, not secure provisioning/storage.

## Display and state behaviour

The current browser reference is **128×64**, despite earlier 64×32 experiments. This embedded adaptation uses a 5×5 font, one direction per 64×32 page (two when height ≥54), two arrivals where they fit, stop/group order, a 3000 ms dwell + 300 ms horizontal push, fixed header during same-stop page pushes, and 700/600/700/600 ms overflow reveal. Accents are transliterated only for pixels; original UTF-8 labels/IDs stay intact. Mode badges use M/B or generic `?`; line colours come only from optional `lineColor`, with grey fallback.

Countdowns use absolute UTC milliseconds: positive <60 seconds is `DUE`, otherwise floor minutes; entries at/before now disappear. `S` means scheduled, `?` means unverified; missing legacy provenance is never realtime. `!` in the header marks partial coverage. Empty groups show `NO PRED`; failed stops show `UNAVAIL`; stale stops show `STALE`. When space is insufficient, a single intact countdown takes priority; extreme values use `99+M`, and an empty group's narrow slot uses `NONE`.

The local 120-second stale policy checks both `generatedAt` age and monotonic receipt age. It is an experiment policy, not proof of fresh vehicle data. Future feed clocks over five seconds ahead show `CLOCK`; minor skew is tolerated. Request failures immediately suppress predictions (`OFFLINE`, `AUTH`, `RETRY`, `FEED ERR`, `BAD FEED`). Polling is 30 seconds after completion, with 60-second failure/429 backoff and five minutes for 401/403; numeric `Retry-After` can extend it up to one hour. These intervals are not verified provider quotas.

Before continuing, resolve the exact board/driver IC and wiring, trusted HTTPS endpoint/root CA, clock bootstrap reliability and acceptable 64×32 readability. Per-unit credentials/configuration ownership and support/trust-root lifetime remain later lifecycle decisions. No provisioning, OTA, enclosure or PCB phase is included here.
