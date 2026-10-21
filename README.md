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

Waveshare ESP32-S3-RGB-Matrix controller (user-reported QFN56, board v0.2), with 32 MB octal flash and 16 MB octal PSRAM. The generic DevKitC definition is overridden with `opi_opi`, OPI flash at 80 MHz, 32 MB flash and a 4 MB factory application partition. The application-size limit matches that partition; remaining flash is unallocated. PSRAM capacity is detected by the SDK at boot, not selected by an arbitrary size macro. Actual detected capacity and usable heap still require a physical boot test. The user-reported ROM log explicitly enables octal flash; this supersedes the earlier QIO assumption from Waveshare’s setup screenshot. Exact flash manufacturer/model is not yet identified.

Waveshare RGB Matrix P2.5 64×32: 160×80 mm, HUB75, 1/16 scan, 5 V / 2.5 A. Internal SRAM holds the double-buffered DMA data; PSRAM remains available to the app. Brightness stays at 16/255.

The official controller map is explicit in `include/panel_config.h`. Timing and panel IC remain unvalidated: SHIFTREG, requested HZ_8M, phase false, latch blanking 2. The pinned S3 driver implements that clock selection through a 10 MHz path. Only `hub75-diagnostic` and `hub75-fixture` enable physical output. The user reports the factory demonstration works; our firmware has not been flashed or physically tested.

## Build and offline checks

Install Python 3.11+ and a C++17 host compiler (macOS Command Line Tools or Linux GCC/Clang):

```sh
python3 -m venv .venv
. .venv/bin/activate
python -m pip install -r requirements-dev.txt
make check
make preview
```

Initial dependency installation needs internet access; subsequent tests use fixtures and make no feed/network requests. `make check` checks formatting/generated fixtures, runs native tests and builds all five firmware variants. `make format` formats owned C++ only. Host warnings are treated as errors. Open `artifacts/preview/index.html` to inspect C++ raster frames at 64×32 and 128×64.

| Environment | Behaviour |
| --- | --- |
| `esp32s3` | Live polling; `CONFIG` until local credentials/trust are supplied; panel disabled |
| `fixture` | Historical feed replay from `generatedAt`; no Wi-Fi or SNTP; becomes stale at 120 seconds; panel disabled |
| `diagnostic` | Serial-selected patterns; no network; panel disabled |
| `hub75-diagnostic` | Same diagnostics with Waveshare HUB75 output enabled |
| `hub75-fixture` | Existing historical feed replay with Waveshare HUB75 output enabled |

Build artifacts are `.pio/build/<environment>/{firmware.bin,firmware.elf,bootloader.bin,partitions.bin}`. Reviewed copies, if present, are under ignored `artifacts/firmware/`; they contain no real credentials. Prefer rebuilding for your board/settings rather than flashing an old copy.

## Exact first hardware bring-up

1. Verify the controller is the **ESP32-S3-RGB-Matrix**, not the separate 8×8 ESP32-S3-Matrix product. Recheck the serial port with `pio device list`. Build without uploading:
   ```sh
   python tools/generate_fixtures.py --check
   pio test -e native
   pio run -e hub75-diagnostic -e hub75-fixture
   ```
2. With power off, check the HUB75 input orientation and board connection. Official GPIOs: `R1/G1/B1=4/5/6`, `R2/G2/B2=7/15/16`, `A/B/C/D=18/8/3/42`, `LAT=40`, `OE=2`, `CLK=41`. GPIO3 is intentional board wiring. E routes to GPIO9, but is disabled (`-1`) for this 1/16-scan panel. No generic rewiring is needed with the matching controller connector.
3. Use regulated 5 V power with panel/controller headroom (recommend at least 3 A for one panel), check polarity, and use the board's intended power input/output arrangement. Use the Mac's data USB connection for programming/debugging; do not assume it supplies enough panel current. Do not apply a higher voltage to the panel, power it from 3.3 V, or improvise connections between independent supplies. The controller includes signal buffering; retain its HUB75 path rather than adding generic jumper wiring.
4. **Only after explicit flashing authorization**, upload diagnostics and open the monitor:
   ```sh
   pio run -e hub75-diagnostic -t upload --upload-port /dev/cu.usbmodem1101
   pio device monitor --port /dev/cu.usbmodem1101 --baud 115200
   ```
   Substitute the rediscovered port if different. Send `i` for the boot report: flash should be `33554432`, panel `enabled`; record the SDK-reported PSRAM capacity (physical capacity 16 MB; mapped/usable size can depend on the framework). Stop on initialization failures or unexpected memory detection. If USB does not enumerate, follow Waveshare's BOOT-on-connect procedure; reset after upload if needed.
5. Diagnostics start red at brightness 16. Send `0`, `1`, `2` for RGB; `3` for checkerboard/moving pixel; `4` and `5` for full row/column sweeps (6.4/12.8 seconds); `6` for text/corner markers. Check both halves, orientation, colours, missing/duplicated rows, flicker and ghosting. **Skip `7` initially**: it cycles full-white brightness 8/16/32/64 and overrides the normal 16 limit. Inspect the actual panel IC before changing SHIFTREG or timing; change only one variable at a time.
6. Close the monitor, then, only when authorized, upload the fixture:
   ```sh
   pio run -e hub75-fixture -t upload --upload-port /dev/cu.usbmodem1101
   pio device monitor --port /dev/cu.usbmodem1101 --baud 115200
   ```
   Existing replay makes no Wi-Fi/SNTP/feed requests. Check all six Metro/bus group pages, stop headers, countdowns, text reveal and transitions. The first Fondo page initially shows 2M/7M. At 120 seconds, verify STALE suppresses arrivals. Reset restarts the historical replay. Record settings, timestamps, photos and limitations in `docs/phase-1.md`.
7. Leave live networking, provisioning and OTA outside this bring-up. Ordinary environments keep physical output disabled.

Do not save credential-bearing logs or binaries in version control. Local secrets are embedded in firmware flash; compile-time configuration is a laboratory shortcut, not secure provisioning/storage.

## Display and state behaviour

The current browser reference is **128×64**, despite earlier 64×32 experiments. This embedded adaptation uses a 5×5 font, one direction per 64×32 page (two when height ≥54), two arrivals where they fit, stop/group order, a 3000 ms dwell + 300 ms horizontal push, fixed header during same-stop page pushes, and 700/600/700/600 ms overflow reveal. Accents are transliterated only for pixels; original UTF-8 labels/IDs stay intact. Mode badges use M/B or generic `?`; line colours come only from optional `lineColor`, with grey fallback.

Countdowns use absolute UTC milliseconds: positive <60 seconds is `DUE`, otherwise floor minutes; entries at/before now disappear. `S` means scheduled, `?` means unverified; missing legacy provenance is never realtime. `!` in the header marks partial coverage. Empty groups show `NO PRED`; failed stops show `UNAVAIL`; stale stops show `STALE`. When space is insufficient, a single intact countdown takes priority; extreme values use `99+M`, and an empty group's narrow slot uses `NONE`.

The local 120-second stale policy checks both `generatedAt` age and monotonic receipt age. It is an experiment policy, not proof of fresh vehicle data. Future feed clocks over five seconds ahead show `CLOCK`; minor skew is tolerated. Request failures immediately suppress predictions (`OFFLINE`, `AUTH`, `RETRY`, `FEED ERR`, `BAD FEED`). Polling is 30 seconds after completion, with 60-second failure/429 backoff and five minutes for 401/403; numeric `Retry-After` can extend it up to one hour. These intervals are not verified provider quotas.

Before continuing, resolve the exact board/driver IC and wiring, trusted HTTPS endpoint/root CA, clock bootstrap reliability and acceptable 64×32 readability. Per-unit credentials/configuration ownership and support/trust-root lifetime remain later lifecycle decisions. No provisioning, OTA, enclosure or PCB phase is included here.
