# Prototype 1 firmware experiment

Review/build date: **2026-10-11**, Europe/Madrid. Hardware has not arrived.

## Existing project inspected (read only)

Sibling `live-transit-feed` HEAD: `c6aabc8b96076eda4bbe31bdd87a85d0f1902965`.
Inspected `docs/feed-protocol-v1.md`, `docs/device-lifecycle.md`, the final backend/firmware boundary and historical raster sections in `docs/phase-1.md`, `scripts/feed/{protocol,model,auth}.ts`, public fixture, and `scripts/preview/{display,countdown,live,pixels,colors}.ts` plus relevant tests/README. No credential file was opened; no sibling build/script was run. Its initial working tree was clean.

**Verified code facts:** Feed v1 has UTC integer milliseconds (beyond 32 bits), opaque stop IDs, open mode strings, ordered stop results and line/destination groups without stable group identity. Names may be null; optional canonical line colours are renderer inputs. Current arrival provenance is required; old missing provenance must remain unverified. Unknown fields are ignored; unknown stop statuses suppress their predictions. HTTP failures are distinct from a 200 envelope with per-stop failures. Current authentication is a shared development Bearer token. `/v1/stops` is a protected catalogue; firmware uses ordered laboratory selectors and does not call it.

**Lifecycle boundary:** device identity, per-unit credentials, hardware model/revision registry, server-owned configuration and compatibility tracking are proposals, not implemented endpoints. Firmware sends the proposed non-secret `X-Firmware-Version` build label; the existing server does not record it. Firmware does not invent device IDs, use MAC addresses as identity, call `/v1/device/config`, register a unit or persist credentials/selections. Those decisions remain outside this experiment.

**Renderer discrepancy:** earlier experiments included 64×32; the current reference uses a canonical 128×64 with 256×128 density comparison. This prototype adapts its semantics rather than reducing the browser canvas. At 64×32 a single group gets a badge/countdown row and separate destination row. The current browser's horizontal push is retained; historical notes about vertical transitions have been superseded in code. Accent glyphs are intentionally simplified to base letters for 5-row readability; normalized strings remain intact. Bright badge colours use local luminance-based black/white text. No transport line-name palette is inferred.

## Framework evaluation and decision

| Candidate | Fit for this phase | Tradeoff |
| --- | --- | --- |
| PlatformIO + Arduino | Small Wi-Fi/SNTP application, C++ host tests, reproducible environment configuration and established HUB75 integration | Pinned PlatformIO Espressif32 uses Arduino 2.0.17 / IDF 4.4 SDK; not the latest Espressif SDK. SDK configuration is less flexible |
| Standalone ESP-IDF | Native Wi-Fi events, TLS/certificate bundle, memory/task configuration and current SDK control; library also supports IDF | More setup and component configuration for this experiment; no present need justifies migrating to it |

**Decision/recommendation:** PlatformIO + Arduino, with the ESP-IDF HTTP client underneath for validated TLS and explicit redirect control. Keep the parser/state/raster portable C++17, so host tests do not depend on either framework. Revisit the SDK choice based on actual memory, networking or panel limitations. This is a prototype decision, not a production support commitment.

No source/operator API logic or source credentials enter firmware. All feed compatibility tests are offline. Feed v1 required no redesign.

The partition table uses a 20 KiB NVS partition ending at `0xe000`, then a gap before the factory app at `0x10000`. The Arduino upload recipe writes its stock `boot_app0.bin` helper at `0xe000`; leaving that sector outside NVS avoids an upload overlap. There is no OTA data partition, update slot or OTA code. This was checked against the pinned framework's upload recipe without flashing hardware.

## Externally verified sources (2026-10-11)

- [Espressif WROOM-1/1U datasheet](https://www.espressif.com/sites/default/files/documentation/esp32-s3-wroom-1_wroom-1u_datasheet_en.pdf): WROOM-1-N16R8 has 16 MB quad SPI flash and 8 MB octal PSRAM. Its octal PSRAM reserves GPIO35/36/37. A module part number does not specify a carrier board or accessible pins.
- [PlatformIO board manifest](https://github.com/platformio/platform-espressif32/blob/v6.12.0/boards/esp32-s3-devkitc-1.json) and [Espressif32 documentation](https://docs.platformio.org/en/latest/platforms/espressif32.html): the generic board manifest is N8/no-PSRAM; the firmware explicitly overrides memory type, flash size and partitions rather than treating that default as N16R8. The installed framework's `tools/platformio-build-esp32s3.py` selects `qio_opi`; its SDK configuration enables octal PSRAM, boot initialization and malloc use above 4096 bytes. This was statically inspected, not measured on a module.
- [Waveshare product documentation](https://docs.waveshare.com/RGB-Matrix-Px-64x32): P2.5 64×32 is 160×80 mm, HUB75, 1/16 scan, 5 V/2.5 A, with an external supply and polarity checks. This establishes published specifications, not actual delivered-panel electrical measurements.
- [HUB75 DMA project](https://github.com/mrcodetastic/ESP32-HUB75-MatrixPanel-DMA), [pinned 3.0.14 release](https://github.com/mrcodetastic/ESP32-HUB75-MatrixPanel-DMA/releases/tag/3.0.14), [build options](https://github.com/mrcodetastic/ESP32-HUB75-MatrixPanel-DMA/blob/3.0.14/doc/BuildOptions.md): ESP32-S3 support, panel chaining, configurable pin map/timing, internal DMA buffering, optional octal-PSRAM DMA. The library warns about unsupported panel driver ICs and possible Wi-Fi interference. Master advertises 3.0.15, but the build uses released 3.0.14 at commit `f17fb7fe9d487e9643f919eb5aeedea8d9d1f8d7`, not moving master.
- [ESP-IDF HTTP client documentation](https://docs.espressif.com/projects/esp-idf/en/v4.4.7/esp32s3/api-reference/protocols/esp_http_client.html): HTTPS server certificate verification through a supplied CA certificate and explicit redirect control. The implementation supplies a root PEM and never disables certificate/hostname verification.

**Library selection:** maintained ESP32-S3 DMA support and chained/64×64 examples make HUB75 DMA a reasonable first integration. A conventional 64×32 1/16 scan needs A–D, no E and no virtual remapping. SHIFTREG is only a starting assumption until the delivered IC is inspected. PxMatrix is an alternative for unsupported panels; there is no evidence here that the exact arriving unit works with either library. Future 128×64/64×64 tests must explicitly configure scan/address lines, buffers and layout. No larger physical setup was validated.

## Implementation and resource policies

Portable model keeps `mode`, `stop`, name/null distinction, status, realtime coverage, groups, optional canonical colour, absolute arrivals/provenance and optional service/vehicle identifiers. Labels are not IDs; groups are never merged by label. Optional identifiers are kept as exact text (numeric IDs become decimal text); `error` is validated but not copied to pixels/logs. Unknown/debug fields at all levels are ignored within the resource ceiling.

Parser updates state transactionally. It rejects incompatible versions, missing/wrong required fields, unsafe/fractional timestamps, unordered arrivals, malformed provenance/colour and oversized input. Prototype ceilings: 64 KiB JSON body, 128 KiB JSON document, depth 16, 20 stops, 32 groups per stop, 32 arrivals per group, 256 bytes per string (optional identifiers 200). These are explicit resource limits, not v1 protocol restrictions. Responses beyond them fail closed with `BAD FEED`; this is a bounded consumer limitation, not a reason to redesign v1. Agree operational limits with the backend before expanding selections.

The renderer consumes a prepared `View`, receives wall-clock/animation times explicitly, and writes a generic RGB888 framebuffer. The display driver consumes pixels/brightness only. Geometry/font/paging policy remains in the renderer; scan/GPIO/clock/driver IC remains in `panel_config.h`. Framebuffer is 8192 bytes at 64×32; DMA is separately allocated by the library. PSRAM is enabled in the SDK; actual initialization and peak heap/stack with Wi-Fi, TLS, JSON and DMA together need measurement.

Network worker polls after completion, so requests never overlap. The main loop renders approximately 20 fps without waiting for TLS. Wi-Fi reconnect attempts are 15 seconds apart. Clock validity requires an actual SNTP synchronization callback and plausible UTC; unsynchronized time blocks HTTPS/feed display. Freshness is recalculated each frame with server cutoff and monotonic receipt age; server cutoff is not a sequence number or vehicle observation. Requests with errors immediately suppress old predictions, preserving no misleading successful display after failure. SNTP is unauthenticated; a robust long-term clock/trust-root bootstrap policy remains unresolved.

No insecure TLS fallback, redirects, credentials in query strings, raw HTTP/body/exception logs, or debug feed requests. The root CA and development settings are supplied in ignored local `secrets.h`. SDK HTTP/TLS logging is disabled. Local settings reside in compiled flash; neither secure storage nor provisioning is claimed. Live binaries built with local credentials must remain private and ignored.

## Validation method and observed results

All tests replay fixed JSON at an explicit historical cutoff, using the same parser/state/raster sources as firmware. `fixtures/feed-v1.json` is copied unchanged from the sibling's public historical fixture: Metro Espanya plus Bus 94, twelve arrivals/six groups, cutoff `1791030107985` (2026-10-03). It contains no source secrets/debug payloads. `fixtures/states.json` is synthetic and covers all statuses, generic mode, unknown status, scheduled and missing legacy provenance. The generated C++ header is checked against these JSON files.

Host tests cover protocol validation and limits, open modes/opaque IDs, optional fields, unknown fields/status, independent failed stops, string/numeric timestamp rejection, 64-bit precision, scheduled/unverified countdowns and exact expiry, 119999/120000 ms freshness, monotonic-age protection, future clock skew, request-level failure/backoff, URL encoding/Bearer header safety, selector ordering/missing stops, page capacities/order, text reveal/transliteration, deterministic raster/motion/header preservation and RGB/row/column/brightness diagnostics.

Raster previews are generated by compiling `tools/render_preview.cpp` against the actual portable C++ sources. It renders every historical/synthetic page at 64×32 and 128×64 for 0/1500/3150 ms, with a fixed UTC cutoff; `tools/preview.py` creates PNGs and a local contact sheet. This is software pixel evidence, not an LED photograph, brightness calibration or measured readability at viewing distance.

Final build/test sizes, timestamps and artifact hashes are recorded in `validation.json` after the last verification. No board was flashed, and no live feed/operator request was made.

An additional compile/link experiment temporarily supplied an ignored configuration with only `synthetic-build-only` values, `https://example.invalid` and a deliberately invalid CA string. It forces the HTTPS branch into the build, verifying link compatibility beyond the optimized unconfigured default. That build passed (870365 bytes application flash, 45468 bytes static RAM); it was never executed/flashed and does not validate TLS behaviour. The temporary header was removed and ordinary builds were repeated before packaging. No synthetic credentials are left in the delivered source or firmware.

## Unvalidated assumptions and next decisions

1. Actual carrier board, pin exposure, onboard peripheral conflicts and USB/boot behaviour. GPIO4–16 is proposed; PSRAM/flash, USB, UART and boot straps are avoided by design but not physically checked.
2. Exact Waveshare SKU/driver IC, connector/power polarity, 3.3 V input thresholds, level shifting, signal integrity, scan ordering, phase/blanking, refresh/flicker/ghosting, current and temperature. Use the README diagnostic sequence first; record actual evidence here.
3. Detected 16 MB flash/8 MB PSRAM and combined DMA/Wi-Fi/TLS memory/stack headroom, boot transients and long-duration stability. Static linker RAM/flash reports exclude dynamic allocations.
4. Certificate chain/root selection, endpoint availability, SNTP boot access/drift, clock-skew tolerance and safe 30-second laboratory polling policy. Delta-seconds `Retry-After` is supported; HTTP-date form falls back to the local backoff. Neither polling quotas nor multi-year TLS support are established.
5. 64×32 readability, destination scrolling delay and whether two full countdowns fit typical scheduled labels. The one-countdown fallback and accent transliteration are documented compromises to assess with hardware/users. Renderer-neutral state does not imply that every resolution is already a polished layout.
6. Later per-unit credential injection/recovery, stop/config ownership and device lifecycle policy described by the sibling proposal. Do not silently reinterpret the shared token as a permanent unit identity.

Provisioning UX, OTA, enclosure, production PCB, manufacturing and production transport licensing remain outside this implementation. The sibling feed project is unchanged.


## Waveshare controller support — 21 October 2026

**Scope:** adapt the existing firmware to the received Waveshare ESP32-S3-RGB-Matrix controller (user-reported QFN56, PCB v0.2, 32 MB flash/16 MB PSRAM) and P2.5 64×32 panel. The user reports that the factory demo works. This is user-observed hardware evidence, not a test of this firmware. No upload, flash erase, network provisioning, OTA or backend change is authorized in this step.

**Sources checked on 2026-10-21:**

- [Waveshare controller documentation](https://docs.waveshare.com/ESP32-S3-RGB-Matrix): N32R16, HUB75, USB programming and onboard buffering.
- [Official S3 GPIO definitions](https://github.com/waveshareteam/ESP32-S3-RGB-Matrix/blob/main/example/arduino_v3.3.7/01_SimpleTestShapes/platforms/esp32s3/esp32s3-default-pins.hpp): R1/G1/B1=4/5/6, R2/G2/B2=7/15/16, A/B/C/D=18/8/3/42, E=9, LAT/OE/CLK=40/2/41. E is disabled for this 1/16-scan panel. GPIO3 is intentional controller wiring, superseding the earlier generic proposed map.
- [Espressif board-support change](https://github.com/espressif/arduino-esp32/pull/12813): 32 MB QIO flash at 80 MHz, 16 MB OPI PSRAM through qio_opi, hardware USB CDC/JTAG. This does not imply that the dedicated board ID exists in the pinned PlatformIO platform.
- [Official panel specifications](https://docs.waveshare.com/RGB-Matrix-Px-64x32): matching size/resolution, 1/16 scan, 5 V / 2.5 A. Use regulated 5 V with controller headroom and correct polarity; Mac USB power headroom is unverified.
- [Pinned driver S3 implementation](https://github.com/mrcodetastic/ESP32-HUB75-MatrixPanel-DMA/blob/3.0.14/src/platforms/esp32s3/gdma_lcd_parallel16.cpp): existing HZ_8M selection uses a 10 MHz S3 clock path. No timing change is made.

**Changes:** explicit official GPIO map; flash-size setting 32MB; application-size limit 4 MB matching the unchanged factory partition; new hub75-fixture inheriting APP_MODE=2 and enabling physical output; standard make build/check targets include it. Existing qio_opi and BOARD_HAS_PSRAM enable SDK detection/initialization of octal PSRAM; no fictitious capacity macro is added. Physical capacity is 16 MB, but mapped/usable memory and successful initialization must be measured on boot. The 4 MB partition does not require allocating all flash. Renderer, Feed v1 parser, framebuffer adapter, double buffering, brightness 16/255, SHIFTREG, phase false, latch blanking 2 and HUB75 DMA 3.0.14 remain unchanged.

**Test method:** install pinned development tools in ignored .venv; check generated fixtures and formatting; run native tests; compile hub75-diagnostic and hub75-fixture without upload. No secrets header was present during inspection. Native tests use checked-in fixtures. Build success alone does not validate electrical timing, PSRAM initialization, USB enumeration or physical readability.

**Proposed physical procedure:** after separate upload authorization, run dim diagnostics 0–6 and check RGB, both halves, row/column sweeps and corners. Skip diagnostic 7 initially because it overrides brightness and cycles full white up to 64/255. Then run hub75-fixture: six existing Metro/bus group pages, countdowns and scrolling/push transitions, followed by STALE suppression at 120 seconds. Fixture time is generatedAt plus monotonic elapsed time; no Wi-Fi/SNTP is started. The first Fondo page starts at 2M/7M. Record timestamps, photos, memory reports, power and limitations here. Panel driver IC, exact revision correspondence and measured current remain unresolved; retain SHIFTREG until evidence justifies a change. Earlier generic N16R8 bring-up instructions are superseded by this section and the updated README.


**Observed validation, 2026-10-21 around 19:55 CEST (17:55 UTC):** local .venv installed PlatformIO 6.1.18 and clang-format 18.1.8 plus pinned build dependencies. Generated fixture consistency and owned-source formatting checks passed. All 20 native cases passed, including raster regression, parser/state and diagnostic tests. Both initial builds and a clean rebuild passed; clean logs contained no compiler warnings/errors, only the driver informational ESP32-S3 pragma. No project runtime source changed beyond panel_config.h.

| Environment | Application bytes / 4 MB partition | Static RAM bytes / 327680 |
| --- | --- | --- |
| hub75-diagnostic | 288629 | 18960 |
| hub75-fixture | 304677 | 18968 |

Esptool 4.9.0 image_info v2 verified both generated bootloaders: ESP32-S3, 32MB flash, 80 MHz, valid checksum/hash. Header mode is DIO for boot, as expected for this framework's QIO boot path; selected qio_opi SDK has CONFIG_ESPTOOLPY_FLASHMODE_QIO, CONFIG_SPIRAM_MODE_OCT, TYPE_AUTO, SIZE=-1, SPEED_80M and BOOT_INIT. The PlatformIO summary says 4MB Flash because its banner derives from maximum application size, which now correctly matches the factory partition; the actual boot image records 32MB. Neither message proves physical detection. Runtime SDK usable PSRAM capacity still awaits board execution. No compiler failures remain. No board was flashed, and /dev/cu.usbmodem1101 was absent during this session's port check; rediscover the port before upload. Historical validation.json is prior evidence, not the result of this board-support change.


## Octal flash startup correction — 21 October 2026

**Evidence and limits:** the user reported a physical upload followed by a pre-application startup assertion, `flash_ret == ESP_OK`, and ROM output `Octal Flash Mode Enabled` / `For OPI Flash, Use Default Flash Boot Mode`. This is direct board-reported evidence supporting octal flash boot, superseding the earlier QIO assumption. No saved esptool flash identification was found in repository evidence or the accessible prior build log. The listed USB devices did not include /dev/cu.usbmodem3101, so no fresh flash-ID query was possible. Manufacturer, JEDEC ID and exact flash model remain unverified; do not infer them from 32 MB capacity or QFN56 alone. [Waveshare specifications](https://docs.waveshare.com/ESP32-S3-RGB-Matrix) identify N32R16 / 32 MB flash and 16 MB PSRAM, but the [official Arduino settings screenshot](https://docs.waveshare.net/assets/images/arduino-setting-7da2b0cd5276763ed4b29895a013434d.webp) shows QIO; that published setting conflicts with this unit's reported ROM mode. [Espressif's OPI documentation](https://docs.espressif.com/projects/esp-idf/en/v5.2/esp32s3/api-reference/peripherals/spi_flash/spi_flash_optional_feature.html) distinguishes octal interfaces from capacity and lists supported chips; none was positively identified here. Sources rechecked 2026-10-21.

**Diagnosis:** qio_opi means quad flash and octal PSRAM, not octal flash. Inspection of installed Arduino 2.0.17 / IDF 4.4.7 confirms its qio_opi SDK lacks CONFIG_ESPTOOLPY_OCT_FLASH. opi_opi enables that flag independently of CONFIG_SPIRAM_MODE_OCT. The startup assertion occurs on esp_flash_init_default_chip before application initialization. The core-dump error is secondary: CRC32 of twelve zero bytes reproduces 0x7bd5c66f exactly, consistent with uninitialized in-memory core-dump configuration on an early panic. Existing SDK enables flash core dumps but the partition table has no dump partition; that limits panic recording and does not fix the flash-interface mismatch. No erase or partition redesign was performed. References: [startup](https://raw.githubusercontent.com/espressif/esp-idf/v4.4.7/components/esp_system/startup.c), [flash initialization](https://raw.githubusercontent.com/espressif/esp-idf/v4.4.7/components/spi_flash/esp_flash_spi_init.c), [core dump](https://raw.githubusercontent.com/espressif/esp-idf/v4.4.7/components/espcoredump/src/core_dump_flash.c).

**Minimal correction:** memory_type=opi_opi and flash_mode=opi. Retain 80 MHz, 32 MB flash size, auto-detected OPI PSRAM, 4 MB factory application partition, pinned platform/framework/HUB75 driver, renderer, GPIO mapping, protocol, timing and brightness 16/255. README corrects the previous quad-flash claim. Earlier QIO validation above is historical evidence of the failing configuration, not current guidance.

**Observed validation, 2026-10-21 around 20:07 CEST (18:07 UTC):** generated fixture consistency, formatting and diff-whitespace checks passed; all 20 native tests passed. Both hardware environments were cleaned and rebuilt with verbose output. No compiler warnings/errors. Diagnostic application: 290413 bytes flash, 19308 bytes static RAM; fixture: 306445 bytes flash, 19300 bytes static RAM. Logs prove both bootloaders are generated from bootloader_opi_80m.elf, compile includes select esp32s3/opi_opi/include, and linking selects -L.../esp32s3/opi_opi, with no old qio_opi SDK path. Selected SDK defines CONFIG_ESPTOOLPY_OCT_FLASH=1, FLASHMODE_OPI=1, FLASH_SAMPLE_MODE_DTR=1; PSRAM independently has MODE_OCT, TYPE_AUTO, SPEED_80M and BOOT_INIT. Both bootloader image headers encode 32 MB, 80 MHz, DOUT (mode 3); DOUT is the PlatformIO OPI image-header encoding, not evidence that quad SDK was selected. Built artifacts remain ignored. Configuration is consistent with the board-reported octal boot mode, but a successful physical boot and exact flash-chip identification remain unverified. No upload, erase or device write was performed.


## Panel green/blue order compensation — 21 October 2026

**User-observed physical results:** opi_opi firmware boots; flash/PSRAM are detected correctly; pixel, row and column diagnostics work; text displays HELLO 64 x 32; brightness is 16/255. Diagnostic 0 displays red correctly, 1 (logical green) displays blue, and 2 (logical blue) displays green. These observations supersede earlier statements that no physical firmware test had occurred. Exact numerical memory reports, instrument measurements and panel IC identification were not supplied.

**Investigation:** checked src/display_driver.cpp, diagnostic framebuffer tests and pinned HUB75 DMA 3.0.14 sources. Logical colours are RGB888: green=0x00ff00 and blue=0x0000ff. The adapter forwards RGB arguments in that order. The library routes logical G1/B1/G2/B2 directly to configured GPIOs and has no separate colour-order property. Existing map exactly matches the [official Waveshare pin definitions](https://github.com/waveshareteam/ESP32-S3-RGB-Matrix/blob/main/example/arduino_v3.3.7/01_SimpleTestShapes/platforms/esp32s3/esp32s3-default-pins.hpp), rechecked 2026-10-21: G1/B1=5/6 and G2/B2=15/16. Thus no discrepancy from published board GPIOs was found. Physical observations support an effective RBG order in the delivered controller/panel path. Software inspection cannot distinguish panel routing from connector/cable or controller-revision differences; claiming a proven panel PCB fault would overstate the evidence.

**Minimum correction:** swap logical green/blue GPIOs in panel_config.h for both halves: G1/B1=6/5 and G2/B2=16/15. Red, row/address/control pins remain unchanged. This is panel-specific compensation at the hardware boundary, not a correction to Waveshare's published map. No physical rewiring, colour transforms, dependency changes or additional configuration abstraction. Renderer, Feed v1 parser, framebuffer RGB semantics, diagnostics, timings, flash/PSRAM configuration and brightness 16/255 are unchanged. README distinguishes published wiring from effective logical mapping.

**Observed software validation, 2026-10-21 around 20:23 CEST (18:23 UTC):** fixture header consistency, formatting and diff-whitespace checks passed; all 20 native tests passed, including unchanged diagnostic RGB expectations and reviewed raster hashes. Both hub75-diagnostic and hub75-fixture builds passed with no compiler warnings/errors. Diagnostic: 290413 bytes application, 19308 bytes static RAM; fixture: 306445 bytes application, 19300 bytes static RAM. No tests were added that merely repeat the GPIO initializer. No device was flashed or otherwise operated in this change. Colour correction still requires physical verification: after separately authorized upload, repeat 0/1/2 and confirm red/green/blue across both halves at brightness 16. If only one half differs, investigate that half's physical routing rather than alter renderer colours.

## Normalized transport colours on the physical fixture — 21 October 2026

**Scope and source:** enrich the existing historical feed-v1 fixture's four Metro groups with L1 #CE1126 / L3 #1EB53A, matching the backend's verified source-code map in ../live-transit-feed/scripts/feed/tmb.ts and retained official metadata captured 2026-10-03. Backend verification now also covers L2/L4/L5/L9 branches/L10 branches/L11, but those services are absent from this fixture: no fabricated arrivals or new pages were added. Fixture timestamps, stop/source identities and arrival values are unchanged. Regenerated include/fixture.h using tools/generate_fixtures.py. The fixture is now an explicitly colour-enriched historical replay, not a byte-identical copy of the backend's older capture.

**Rendering policy:** explicit normalized lineColor takes precedence for every mode, including #000000. If absent, bus badges use existing bus-mode green #42AC52 as presentation only; other missing colours remain #777777. The fallback never populates or changes canonical lineColor. Bus metadata in the replay remains absent. No line-name parsing, Metro identity lookup table or Feed v1 change was introduced. RGB888 framebuffer and HUB75 adapter, display geometry/text/paging, GPIO green/blue compensation, brightness 16/255, pinned driver, memory and timing configuration are unchanged. The existing header badge reuses the same bus-green constant without changing its output.

**Observed software validation, 2026-10-21 around 20:51 CEST (18:51 UTC):** fixture consistency and owned C++ formatting checks passed. All 24 native tests passed. Four added cases cover all six fixture page colours through parser/state/renderer, explicit mixed-case RGB and black overriding unknown/bus fallbacks, unknown and missing-Metro metadata retaining grey, canonical bus metadata remaining absent after rendering, and exact RGB888 preservation through clear/pixel/rect operations. The existing reviewed raster regression retains its original neutral-colour baseline by explicitly setting those baseline groups grey; separate colour tests exercise the new colours. This preserves reviewed geometry hashes rather than accepting changed hashes without review.

Both hub75-diagnostic and hub75-fixture builds passed with no compiler warnings/errors. Diagnostic application uses 290409 bytes flash / 19308 bytes static RAM; fixture uses 306537 bytes flash / 19300 bytes static RAM. Diff-whitespace check passed. No device was flashed, no new operator calls were made and backend files were not modified in this step. Earlier physical observations are user-reported; this colour change awaits physical testing. After separately authorized upload of hub75-fixture, check L1 red, L3 green and bus 6/H8 green across all six pages at brightness 16, with existing scrolling and stale handling unchanged. Arbitrary lineColor values are supported generically; physical validation of other services requires representative normalized fixtures later.

## Isolated physical badge contrast experiment — 21 October 2026

**Implementation:** new hub75-badge-contrast environment selects its own source entry point (badge_contrast_main.cpp plus the unchanged display_driver.cpp), APP_MODE=3 and physical panel output. No main.cpp, normal renderer, Feed v1 parser, framebuffer, GPIO mapping, HUB75 driver, brightness configuration or timing changes. include/badge_contrast.h holds eight offline diagnostic inputs from verified backend colours, not a production identity table. It reuses drawText and the RGB888 framebuffer; all physical output still uses PrototypeDisplay. No network or feed initialization in this entry point. Compile-time checks require one 64×32 panel and brightness 16. Build/check Makefile targets include the new environment.

**Method:** equal side-by-side 29×11 backgrounds show identical 5-row line identifiers in black and white, with explicit headings and reversible sides. Static samples avoid scrolling/paging ambiguity. Starts L1; 0–7 selects L1–L5/L9/L10/L11, n/p moves, s swaps, i repeats the report after USB reconnect. L9/L10 shared branch colours use compact identifiers plus N/S footer. This comparison uses more vertical padding than the normal seven-row badge; same glyph engine and colour path, but final production-context readability still needs confirmation. Run instructions, exact pixels, official data provenance, environment controls and interpretation criteria are in docs/badge-contrast.md. No automatic production contrast choice is made and no LED swatches replace canonical colours.

**Bottom-edge finding:** normal renderer.cpp lines 102–106 deliberately draw per-stop service-page dots at x=1+3*i+transitionOffset, y=height-2 (row 30). Active #AAFF66, inactive #384438; count capped at width/3. Existing Metro fixture has four service pages; bus has two. These are neither status indicators nor artifacts. They move with pushed content and disappear when only one service page remains. Native assertions now verify active-dot movement across two pages and absence for a single-page stop.

**Observed validation:** generated fixture consistency, formatting and diff-whitespace checks pass. All 26 native cases pass, including full paired-badge pixel checks on all eight backgrounds in both side orders (identical glyph positions, exact black/white values, background preservation and panel boundaries), service-page indicators and unchanged production raster regression. Builds pass: hub75-diagnostic (290409 bytes flash / 19308 static RAM), hub75-fixture (306537 / 19300), hub75-badge-contrast (288217 / 19204). No compiler warnings/errors. No dependencies added, no device flashed, no backend changes. Physical contrast preferences remain unmeasured: record observations after separately authorized upload, keeping brightness 16 and distinguishing canonical official metadata, user visual reference and panel-specific legibility.

## Bold-white diagnostic typography — 21 October 2026

**User-observed motivation:** physical comparison favoured black text for readability while white was perceived as visually cleaner and aligned with some official badges. No quantitative readability measurements were supplied. Extend only the isolated experiment, not production contrast selection.

**Font investigation and implementation:** inspected current 5×5 glyph masters, sibling lightweight glyphs, 9×9 detail masters, 4×6 BDF and Tom Thumb. Medium/light assets do not provide a suitable bold face; three glyphs at the existing 9×9 system's 12-pixel advance exceed 29 pixels. Chose an original hand-designed 6×7 subset (L, 0–5, 9), not copied third-party data or dilation. Two-column stems, a two-column zero counter and separately drawn digits fit L10/L11 in a 20-pixel cell span. Existing regular A/B masks remain unchanged. No dependency/license additions; existing repository licensing unchanged. Source and candidate/licensing considerations are documented in docs/badge-contrast.md.

**Controls and compromises:** a selects A/B black regular vs white regular; b selects A/C black regular vs white bold; c selects B/C white regular vs white bold. 0–7 backgrounds, n/p, s and i retain prior functions. A BLK / B WHT / C BLD headings and serial reports identify faces. Brightness stays 16 and badges stay 29×11 in the same locations. Bold is two rows taller and one column wider per cell, so the experiment mixes size and weight; it cannot directly justify a production 7-row badge change. Normal renderer, Feed v1, GPIO, timing, driver and hardware configuration are untouched. Only isolated diagnostic assets/entry point, tests and documentation changed.

**Observed validation:** all 27 native tests pass, including all eight backgrounds, all three comparison pairs in both side orders, regular-mask parity, exact ink/background values, badge boundaries, larger bold ink area and open zero counter. The first new test incorrectly asserted an empty gap through the existing N/S footer; restricting that assertion to the badge region corrected the test without changing rendering. Existing fixture consistency, formatting and diff-whitespace checks pass. Compiled the actual C++ raster into a temporary 8×3 contact sheet and visually inspected it: labels fit, L10/L11 remain separated and counters open. Raster preview is not physical LED evidence. Hardware builds pass: diagnostic 290409 bytes flash/19308 static RAM; fixture 306537/19300; badge contrast 288569/19204. No compiler warnings/errors. No device was flashed. Physical preference between regular/bold white remains unresolved pending comparison at fixed brightness/ambient conditions.


## Final backend-owned badge colours — 21 October 2026

**Approved policy:** preserve Metro canonical backgrounds, with white regular text for L1/L2/L3/L5 and black for L4/L9/L10/L11 (both branches). Bus colours selected by user from TMB website reference: V #547B31, D #894796, H #2D4191, X #000000, Other verified TMB #C62A0F, all white text. These differ intentionally from API catalogue values. Backend owns identity/family matching; no production device identity lookup is added. Official metadata, selected display swatches and LED-specific adjustment remain distinct; no LED calibration transform is added.

**Implementation:** additive optional Feed v1 group lineTextColor validated as #RRGGBB and preserved in the portable model, like lineColor. Badge renderer uses explicit foreground before its unchanged weighted contrast threshold. Black (#000000) is not treated as absent. The normal renderer still uses existing regular glyphs, coordinates, padding, layout, paging/scroll timing and contrast fallback. Hardware pins, OPI configuration, driver, brightness 16 and double buffering are unchanged. Existing separate diagnostic environments, including experimental bold font, remain intact.

**Fixtures and tests:** historical feed-v1 fixture now matches the sibling enriched backend fixture (same six groups/timestamps/arrivals): L1/L3 white text; bus 6 red and H8 blue with white. Added synthetic policy stops to states.json for ten Metro services, five TMB bus categories and unknown-operator input lacking canonical background. Regenerated fixture.h with existing tool. Native tests cover explicit black/white on the same background with opaque mode, malformed foreground rejection, each selected Metro foreground, all bus backgrounds/white foregrounds, legacy unknown-operator green fallback and unchanged reviewed layout baseline. Early test sampled a blank cell of digit 6; changed it to verify actual white ink inside the badge rather than assume a particular glyph stroke.

**Observed validation:** fixture generation consistency and formatting passed; all 28 native tests passed. All six build environments passed: esp32s3, diagnostic, fixture, hub75-diagnostic, hub75-fixture and hub75-badge-contrast. No compiler warnings/errors. Hardware build application/static-RAM sizes: diagnostic 290409/19308 bytes, fixture 306993/19300 bytes, badge contrast 288569/19204 bytes. No device flashed or service deployed. Physical visual validation of selected foregrounds is pending; do not claim software raster checks establish panel readability. Unknown operator/background resolution remains backend-owned; absent metadata still renders through compatible presentation fallbacks.

## Bus stop header badge correction — 21 October 2026

Changed only the generic B badge alongside a bus stop name: background #C62A0F and explicit foreground #FFFFFF. This local mode-icon presentation choice is independent of backend-controlled individual line badges. Existing lineColor/lineTextColor handling, V/D/H/X/Other colours, legacy bus-line background fallback, regular bitmap font, layout, timing, protocol and hardware settings remain unchanged. No backend changes.

Validation: all 29 native cases passed; fixture consistency, formatting and diff-whitespace checks passed. The focused test checks the red/white header on all five bus line palettes and verifies an explicit black line foreground does not affect the white header. Updated the intentional bus full-raster baseline; recolouring only its header background back to the former green exactly reproduces the previous reviewed hash, verifying the rest of the frame is unchanged. All three hardware builds (hub75-diagnostic, hub75-fixture, hub75-badge-contrast) passed with no compiler warnings/errors. No device was flashed; physical confirmation remains pending.


## Isolated physical typography diagnostic — 21 October 2026

**Inspection/code facts:** the operational font is the 5×5 master with six-pixel
advance in `lib/transit/src/font.h`, used by ArrivalRenderer and regular badges.
This is a different baseline from the sibling browser concept's compact 3×5 A.
Reviewed both browser renderers, the identity FACES/COUNTDOWNS tables, existing
BDF/reference notices, all PlatformIO environments, hardware bring-up notes and
the previous 6×7 bold badge-contrast diagnostic. The latter remains separate.

**Implementation:** additive `hub75-typography` environment, APP_MODE=4, its own
source filter and serial entry point; original default/environment settings stay
unchanged. A directly invokes the production ArrivalRenderer. B/C/D first render
that baseline then clear/redraw only its three existing five-row text slots.
Badge geometry/regular lettering, separator/border pixels, palette, driver,
compensated GPIO mapping, OPI/PSRAM, double buffering and brightness 16/255 remain
unchanged. Serial a/b/c/d selects the face; 0–5 or n/p selects a sample; s steps
paused reveal times 0/1000/1600/2300 ms; r resets; i/? repeats state and help.
Each face holds identical synthetic 2M/9M predictions indefinitely. No Wi-Fi,
provider calls, configuration writes or device interaction is performed.

`tools/generate_typography.py` snapshots exact B/C/D masks, compact punctuation
and source hashes from `live-transit-feed/scripts/concept/identity/fonts.ts` and
`concept/render.ts`; --check detects drift without writing. Builds/tests need no
sibling checkout once the generated header is present. No new font/dependency or
font-license grant is introduced. See `docs/typography.md` for source provenance,
spacing/Unicode differences and upload instructions. Operational A retains 5×5
spacing, B/C/D use the actual browser compact advances; first arrival digits use
the browser five-column countdown masters, second digits its compact masters.
The firmware's one-direction layout/reveal timing is retained rather than porting
the browser concept composition. Unknown Unicode remains '?' under existing
firmware normalization; typographic apostrophes/dashes differ from browser ASCII
folding, so samples use ASCII punctuation. Full UTF-8 sample labels remain intact.

**Observed validation:** 35 native cases pass (29 unchanged core + six typography
cases). New checks cover exact operational baseline raster, unchanged pixels
outside text slots (including all badge pixels across every face/sample/phase),
fixture-matched badge backgrounds/foregrounds, selected exact browser glyph masks,
compact advances, accent/fallback handling, clipping, full reveal and serial
selection repeatability. Comparing diagnostic palettes with the reviewed state
fixture caught and corrected the initial L4 foreground to black; V23 uses the
existing V-family V31 fixture swatch. Typography source/hash parity, generated
fixture consistency, C++ formatting and diff whitespace checks pass. Hardware
builds for hub75-typography, hub75-diagnostic, hub75-fixture and
hub75-badge-contrast pass. The new diagnostic uses approximately 299 kB flash and
19.2 kB static RAM; these are build reports, not measured runtime heap/PSRAM use.

**Unresolved/physical method:** no upload, flashing, monitor connection or physical
readability test ran. Browser C preference is still a design hypothesis. At 3 m,
compare a/b/c in varying order with the same sample, reveal phase, lighting and
viewing angle; use D as the optional density control. Record transcription/time,
bloom, stroke loss, confusions and preference/tie. Current tests establish digital
raster isolation, not physical legibility. Ordinary transport data, colours and
configuration remain backend-owned; this isolated firmware rendering capability
experiment creates no new requirement to flash ordinary configuration changes.


## Focused geometric C2 diagnostic — 22 October 2026

**User-reported context:** original B/C/D looked more elegant/contemporary on the
physical P2.5 64×32 panel but were thinner/narrower and harder to read around 3 m.
The sibling measurement report identified horizontal compression and reduced
word separation; all original capitals remained five rows high. Inspected that
report, original glyphs/advances, renderer, diagnostic and tests before C2 work.

**Implementation facts:** added manually drawn `include/typography_c2.h` with
13 capitals A/E/I/L/N/O/P/R/S/T/U/V/Z. All are five rows tall; 12 are four columns
wide with five-pixel advances, barred I is three columns/four advance. Spaces
advance three pixels. Stems/crossbars are one pixel; square O counter is 2×3,
P/R counters 2×1. No solid 2×2 glyph blocks, horizontal scaling, dilation or uniform
bolding. Even-width T/V have deliberately asymmetric axis/taper compromises.
Other label characters use a visible three-column '?' fallback with a serial
warning; no narrow-font substitution. Existing uppercase/accent folding applies.
Full UTF-8 labels remain in sample data. This is not a complete production font.

**Controlled comparison:** original A remains a direct call to ArrivalRenderer;
imported C masks and complete original C frames for samples 0–5 are unchanged.
Added samples 6–9 (LESSEPS, LA PAU, TRINITAT NOVA, ZONA UNIVERSITARIA), each with
fixed Lesseps header/L3 badge and synthetic 2M/9M arrivals. These are reading
stimuli, not claims of actual L3 service patterns. Focused samples retain the
exact operational countdown pixels in A/C/C2 (also B/D), so only name/destination
text differs. Original samples retain their prior compact arrival rendering.
No production renderer, regular badge typography, stable palette, GPIO, HUB75
adapter, timing, brightness 16/255, PlatformIO environment or feed code changed.

**Serial:** a=A, c=C, e=C2; b/d retained. 6–9 select focused samples, 0–5 original
samples; n/p navigate all, s steps paused 0/1000/1600/2300 ms reveal, r resets and
i/? reports. Startup A/sample 6. Face switches retain content and reveal phase.
Legacy labels outside C2's subset produce '?' and a warning; use 6–9 for C2.

**Actual full ink widths, A/C/C2:** LESSEPS 41/27/34; LA PAU 35/20/27;
TRINITAT NOVA 77/44/60; ZONA UNIVERSITARIA 107/62/84 native pixels. C2's long
phrase needs 23 pixels of reveal in the existing 61-pixel destination slot.
Short samples 6/7 fit all three; use these first to remove scrolling as a confound.
The header remains in its 53-pixel slot. C2 combines width and word-space changes
as requested, so a preference cannot isolate their separate effects.

**Observed validation:** 39 native tests pass (29 core + 10 typography).
New coverage includes 24 golden original-C frames, C2 subset/dimensions/open
counters/no 2×2 blocks, actual drawn-vs-measured widths, normalization/fallback,
full reveal/clipping and exact focused-countdown equality. Existing tests verify
true operational A equality, unchanged non-text/badge pixels, palette parity
and direct serial switching. Browser import parity, fixture consistency, C++
formatting and whitespace checks pass. Native visual raster inspection also
confirmed the intended A/C/C2 label proportions and identical countdowns.

All five hardware builds pass: esp32s3, hub75-diagnostic, hub75-fixture,
hub75-badge-contrast and hub75-typography. New diagnostic: 299857 bytes flash,
19212 bytes static RAM (build reports, not runtime heap measurements).
No device upload, monitor connection or physical C2 observation ran. Upload
instructions and full limits are in `docs/typography.md`. No dependency or
external font resource added. Ordinary data, colours and configuration remain
backend responsibilities; this adds only isolated diagnostic rendering capability.


## C2 complete uppercase alphabet — 22 October 2026

**User-observed context:** the original C2 subset (`e`) has the best reported
balance of appearance/readability so far on the physical panel. Its B/C/D/G and
other question marks were absent glyphs in the 13-capital subset, not evidence
of unsuccessful glyph designs. New capital drawings are not yet physically
validated by this update.

**Inspected and implemented:** identified missing B/C/D/F/G/H/J/K/M/Q/W/X/Y in
`include/typography_c2.h`, referenced original C in `typography_glyphs.h`, and
manually completed A–Z. The existing 13 C2 masks and their widths are unchanged.
Capital height remains five pixels; 23 capitals have four-column ink/five advance,
I retains three-column ink/four advance, M/W use five-column ink/six advance for
open interior joins. Word-space advance stays three. No automatic scaling,
dilation, solid 2×2 blocks or uniformly bold stems. Original geometric references
include B/D cut corners, C/G square bowls, J hook, K/X/Y diagonals, M/W interior
joins and Q in-cell tail. No additional font source/license/dependency introduced.

**Samples and controls:** preserve all original samples 0–9; add alphabet groups
10 ABCDEFGH, 11 IJKLMNOPQ, 12 RSTUVWXYZ (39/44/45 px, all fit the 61-pixel
viewport), and 13 Bon Pastor (47 px), 14 Collblanc (44), 15 Badalona Pompeu Fabra
(101, 40 px overflow), 16 Gorg (19). These are offline reading stimuli on the fixed
L3 badge, not verified line/destination pairings. All new samples retain exact
operational countdown pixels, badges, colours, layout and brightness 16/255.

`e` selects C2; a/b/c/d retain A/B/C/D. `f` jumps to alphabet sample 10; `g` to
Barcelona sample 13; n/p navigates all 17 samples. Digits 0–9 retain their sample
selection. Restricting digit handling explicitly to 0–9 prevents ':'/'?' from
being misinterpreted as sample indices when the table exceeds ten entries.
s steps reveal, r resets, i/? reports. Face switches retain content/reveal phase.

**Remaining coverage:** C2 label digits 0–9 and label punctuation remain
unimplemented; existing compact '?' is only the fallback drawing. Unsupported
label characters are reported explicitly as unique missing characters after
existing firmware normalization, then visibly rendered '?'; no unrelated font
is silently used. Unknown Unicode becomes '?' in that audit, with full original
labels still present in serial output. Lowercase/common Catalan/Spanish accents
continue to uppercase/fold under the existing policy. Badge/countdown numerals
and punctuation use the unchanged operational font, not missing C2 glyphs.

**Observed validation:** 42 native cases pass (29 core + 13 typography). New
regressions preserve all 13 prior C2 masks and A/B/C/D frame snapshots across
all ten existing samples/four reveal phases; test complete distinct A–Z lookup,
proportional dimensions, alphabet-group coverage, missing numeral/punctuation
reports, actual raster widths, unchanged countdowns, existing colours/badges,
expanded sample navigation and help-command handling. All seven hardware builds
pass: esp32s3, diagnostic, fixture, hub75-diagnostic, hub75-fixture,
hub75-badge-contrast, hub75-typography. Diagnostic build: 300465 bytes flash,
19212 bytes static RAM (not runtime heap measurements). Browser font parity,
fixture consistency, formatting and diff whitespace checks pass. Native glyph
raster review confirms the intended proportions; physical validation remains
pending for the new 13 letters. No upload, device monitor connection or flash.

Production renderer, backend/Feed v1, canonical colour policy, GPIO/driver,
brightness and PlatformIO configuration remain unchanged. See `docs/typography.md`
for full character coverage, design compromises, controls and upload instructions.


## C2 label numerals/punctuation and precise Q correction — 22 October 2026

**User-observed context:** complete C2 A–Z looks good on the P2.5 panel; remaining
missing glyphs are label numerals/punctuation. The physical Q observation requested
closing its bowl and moving the bottom tail left one column. Inspected C2, the
original geometric C numeral masks, diagnostic controls and actual normalization
before changing anything. New drawings/Q correction are not yet physically tested.

**Exact Q change:** [15,9,9,11,1] → [15,9,9,15,2]. In one-based coordinates,
add row 4/column 2, clear row 5/column 4 and set row 5/column 3. Top three rows,
4-column ink/5-pixel advance and 5-row height remain unchanged. All other 25
validated capital masks and metrics are preserved byte-for-byte.

**Added label glyphs:** 0–9, ASCII apostrophe, ASCII hyphen, period and slash.
Digits are manually adapted from original geometric C to four-column ink/five
advance, except 1 at three/four. Zero has cut corners and 5 shortened terminals
to distinguish them from unchanged O/S. Apostrophe uses two columns/three advance,
hyphen three/four, period one/two and slash three/four. One-pixel inter-character
gaps and three-pixel word-space advance are unchanged. No scaling/dilation,
uniform bolding, unrelated font source or new dependency.

**Verified normalization:** existing transit::pixelText uppercases ASCII, folds
common precomposed Latin-1 accents and strips U+0300–U+036F combining marks before
bitmap iteration. It previously turned curly U+2019 into '?'. C2 alone now wraps
it with an explicit U+2019→ASCII apostrophe mapping, shared by measurement,
drawing and missing-character audit. Plaça d’Espanya therefore plots as
PLACA D'ESPANYA while retaining original UTF-8 sample/report strings. This policy
is announced in serial output and documented. A/B/C/D and the production
normalizer retain their existing behavior; no global punctuation policy changed.
Other punctuation (comma/colon/semicolon/exclamation/plus/brackets/middle dot),
U+2018, en/em dashes and unsupported Unicode remain explicitly reported/drawn '?'.
There is no silent substitution with another font.

**Physical samples/control:** `e`, `h` opens the new Q/numeral/punctuation set
17–26. Labels and C2 full ink widths: QU QR QO 35, 01234 23, 56789 24,
Aeroport T1 51, Aeroport T2 52, Plaça d’Espanya 70, Pg. de Gràcia 56,
Sants/Estació 62, Can Marcet - Horta 83, O0 S5 I1 33 pixels. n/p navigates all
27 samples; f/g, digits 0–9 and a/b/c/d/e retain existing functions. s/r step/reset
reveal; i/? reports active selection and missing characters after normalization.
Fixed L3 badge/colours/header and operational 2M/9M countdown pixels are unchanged
across the focused samples. Inputs are reading stimuli, not verified L3 routes.

**Observed validation:** 45 native tests pass (29 core + 16 typography), covering
precise Q bits, preservation of other 25 capitals, original A/B/C/D raster hashes
across all previous 17 samples/four reveal phases, label numeral/punctuation
coverage, U+2019/ASCII equivalence, precomposed/decomposed accents, remaining
unsupported reporting, true ink widths/period baseline, countdown equality,
regular badges/palette, clipping/reveal and serial navigation. All seven hardware
builds pass: esp32s3, diagnostic, fixture, hub75-diagnostic, hub75-fixture,
hub75-badge-contrast and hub75-typography. Diagnostic image: 302449 bytes flash,
19212 bytes static RAM (build reports). Import parity, fixture consistency,
formatting and diff whitespace checks pass. No device upload, monitor connection
or flash. No new physical readability result is claimed.

Only isolated diagnostic sources/tests and documentation changed. Operational
renderer/countdown/badge fonts, backend/Feed v1, stable colour policy, GPIO,
HUB75 driver/brightness 16, hardware configuration and PlatformIO environments
remain unchanged. Full bitmaps, coverage and upload instructions are in
`docs/typography.md`.


## Active regular typography and serial brightness — 2026-10-22

C2 is now the operational regular font for stop labels, destinations, mode and line badges, and status messages. Its X rows are exactly 9,9,6,9,9 (4×5), with a one-pixel inter-character gap. C2 masters live in lib/transit/src/regular_font.h; the former include/typography_c2.h is a research compatibility wrapper. Legacy bitmap primitives remain for unchanged countdowns, diagnostic patterns and intentional comparison tools. The partial-coverage exclamation mark remains the existing indicator. Feed Protocol v1, colours, page geometry, timings and default brightness remain unchanged. Narrower text changes measured badge widths and reveal distances as required by the selected font.

Offline fixture/live modes accept b4, b8, b16 or b32 followed by Enter over the 115200-baud serial monitor. For example: `.venv/bin/pio device monitor --port /dev/cu.usbmodem1101 --baud 115200`, then type `b8` and Enter. CR, LF and CRLF are accepted. Changes reach the existing DMA driver on the next frame (nominally within 50 ms); reset restores 16/255. Invalid commands do not change brightness. Diagnostic mode retains its existing serial controls and brightness sweeps. These presets are an isolated prototype input; configuredBrightness is an ordinary byte that can later be assigned by device configuration without changing the rendering/driver interface. Values are not calibrated luminance.

The sibling browser preview uses matching C2 masks, widths, spaces and apostrophe/accent folding. Detailed density scales ordinary C2 pixels by two; specialized detailed countdowns remain unchanged. Browser concept/identity typography comparisons are retained intentionally for research. Browser geometry, badge padding and countdown presentation still differ from the 64×32 firmware; glyph parity does not establish whole-frame equivalence. No hardware upload or physical readability measurement was performed.

Final validation on 2026-10-22: 48/48 native firmware tests passed; hub75-fixture build succeeded with 309585 bytes application flash and 19332 bytes static RAM. Backend 160/160 tests, TypeScript checking and lint passed. Firmware formatting and both repository diff whitespace checks passed. No flashing performed.

## Operational C2 and V23 contrast — 2026-10-22

Confirmed decision: all operational text, including countdown digits, M suffixes and partial-status markers, uses regular_font.h C2. Countdown widths and right alignment use C2 ink widths; the existing first-arrival fallback and clipping bounds remain. Legacy glyph helpers remain for isolated diagnostics. Browser operational glyphs use the matching C2 definitions and measured widths at both scales. Typography experiments remain available.

User-reported physical observations: brightness 4/255 is comfortable indoors at night with lights on and colours look correct. Complete darkness may require lower brightness or display-off; scheduling remains unresolved and is not implemented. Prototype default is now 4; serial brightness presets remain.

V23 experiment: build hub75-badge-contrast. Starts variant A; uppercase A/B/C/D select #FFFFFF/#D9D9D9/#BFBFBF/#000000 on unchanged #547B31. Uses the operational renderer with normalized Feed v1 lineColor/lineTextColor, no production identity rule. Press m to toggle the earlier Metro comparison. Fixed brightness 4/255. Evaluate under the same ambient light, recording readability and glare for each variant before selecting a production foreground. Production V-series foreground remains unchanged. No API calls or new externally verified facts. Software raster/build results do not establish physical readability. No ESP32 flashed.


## Darker V23 diagnostic backgrounds — 2026-10-22

User-observed preceding physical test at 4/255: #FFFFFF too bright; #D9D9D9 and #BFBFBF improved balance but remained difficult to read; black unsatisfactory. These observations motivate a new isolated experiment, not a production palette decision.

Replaced diagnostic A–D pairs with background/foreground #547B31/#FFFFFF, #466729/#D9D9D9, #385322/#D9D9D9 and #30481D/#BFBFBF. V23 remains operational C2 via normalized Feed v1 lineColor/lineTextColor. Shared diagnostic sample definitions drive both rendering and serial reporting. Each selection prints variant, both colours and global brightness 4/255. Uppercase controls and prior Metro comparison remain. Production renderer, palettes and backend unchanged. No external facts verified or provider calls made. Physical readability of darker green remains unresolved; compare under fixed viewing conditions. No ESP32 flashed.

Observed software validation: all 49 native firmware tests passed, including exact C2 V23 ink and background pixels for all four pairs and unchanged operational raster regressions. hub75-badge-contrast build passed. Firmware formatting and diff whitespace checks passed. These checks do not establish physical LED readability; no upload or flash performed.


## Saturated V-series diagnostic comparison — 2026-10-23

User-reported physical observations, Waveshare P2.5 64×32 HUB75 at brightness 4/255: comfortable indoors; original #547B31/white difficult to read; #385322/#D9D9D9 more readable but almost grey, losing green identity. No quantitative measurements supplied.

Replaced only isolated diagnostic A–D inputs: A #385322 (previous best reference), B #286B12, C #207A10, D #185C08; all foregrounds #D9D9D9. Existing operational C2 V23 rendering via normalized lineColor/lineTextColor, uppercase selection, serial colour/brightness report and default 4/255 remain. The diagnostic has no brightness-adjustment control; other environments' controls are untouched. Production renderer, Metro/bus colours and backend unchanged. No dependencies or operator rules added.

Method for the pending physical test: compare each pair under fixed ambient light, distance and angle; record readability and recognizable green separately. More saturated candidates improving both is a hypothesis, not a verified outcome. No external facts verified or provider requests made; no ESP32 flashed.

Observed validation: all 49 native firmware tests passed, including exact C2 badge foreground/background checks for all four pairs and unchanged operational raster regressions. hub75-badge-contrast built successfully. Firmware formatting and diff whitespace checks passed. No upload performed; physical readability and green identity remain pending evaluation.


## Physical live-feed integration — 25 October 2026

Inspection found an existing Arduino/PlatformIO Wi-Fi/HTTPS/background-task
pipeline, bounded ArduinoJson Feed v1 parser, SNTP clock, local countdowns and
isolated renderer/diagnostics. Reused these. Added panel-enabled hub75-live,
last-valid-feed merge on transient request failures (original generation/receipt
age preserved), a periodic error screen using the unchanged renderer, and safe
connection/feed summaries. AUTH/CONFIG/CLOCK still suppress predictions. Existing
120-second freshness and polling/backoff policies remain. No provider policy,
new dependency or contract change. Normal typography, badge colours, paging,
countdown pixels and configured brightness 4/255 remain.

Observed validation at 2026-10-25T09:24:55Z: 51 native tests passed (previous 49
plus interruption/recovery and local-expiry cases). Updated the existing request
error test to assert its warning-screen phase; all established raster and
comparison tests pass. All eight environments built: esp32s3, diagnostic,
fixture, hub75-diagnostic, hub75-fixture, hub75-badge-contrast, hub75-live and
hub75-typography. C++ formatting and generated fixture checks passed. Sibling
backend: all 160 tests and type/lint/format checks passed. PlatformIO cache and
local backend test listener access required sandbox permission. Builds used no
real credentials. Wi-Fi interruption tests simulate the network task's error
snapshot; they do not test a physical radio or TLS session.

Direct live endpoint verified 2026-10-25T09:27:08.443Z–09:28:25.097Z:
https://live-transit-feed.roger-80a.workers.dev. Auth/catalogue/Metro/Bus/mixed
checks succeeded; diagnostic upstream responses were HTTP 200 for both modes.
Prediction sets changed across requests. Actual firmware parser accepted four
normalized captures in a temporary host harness. No upstream connectivity issue
was reproduced. Deployed responses omit provenance/realtimeStatus/selected
colours present in the current backend checkout; likely deployment drift, exact
revision unconfirmed. Legacy unknown-provenance and colour fallbacks must remain.
Recommendation: owner updates the intended backend revision and repeats checks
before the intended physical test. No deployment or flash is automatic/authorized.
See sibling feed docs/phase-1.md and sanitized evidence/deployed-feed-2026-10-25.

Physical hotspot association, SNTP/CA bootstrap, heap headroom and uninterrupted
LED output remain unverified. docs/live-arrivals.md records secure local
configuration, chosen HTTPS origin, dynamically detected USB port, manual
build/upload/monitor commands, state interpretation and interruption tests.


## Pre-flash readiness inspection — 25 October 2026

User requested inspection/tests/build and explicitly prohibited flashing. No
renderer, font, colour, layout, backend or firmware source change was made.
Read docs/live-arrivals.md, app/network/TLS/panel configuration; git checkout was
clean before this documentation update. PlatformIO enumerated one Espressif USB
JTAG/serial device (303A:1001) at /dev/cu.usbmodem3101. Enumeration only: no serial
monitor, boot/reset, upload or flash operation was run.

Observed: include/secrets.h does not exist; its path is ignored and untracked.
All five local values therefore compile as empty fallback settings. The example
already contains the correct public feed origin, but SSID/password, Feed token
and root PEM still require local configuration. Do not expose credentials in
commands, logs, chat, evidence or commits. No secret file was created automatically.

Observed software validation: all 51 native tests passed, hub75-live built
successfully (45,256 bytes static RAM, 727,109 bytes flash), C++ formatting and
generated fixture checks passed. Build success without secrets does not imply
network readiness: the current device would show CONFIG. Hardware authentication
and hotspot association cannot be established from this host build.

Code inspection confirms SNTP callback installation before configTime, with an
atomic synchronized flag plus minimum UTC epoch required before client.fetch.
The HTTPS client supplies cert_pem, leaves hostname skipping disabled and refuses
redirects; no insecure mode was introduced. Physical NTP completion is untested.

Important additional finding: pinned Arduino 2.0.17 / Espressif32 6.12.0 SDK
esp32s3/sdkconfig explicitly says CONFIG_MBEDTLS_HAVE_TIME_DATE is not set;
the selected opi_opi SDK header has HAVE_TIME but no HAVE_TIME_DATE. Inspecting
the actual libmbedx509.a with the installed Xtensa objdump confirmed both
mbedtls_x509_time_is_past and mbedtls_x509_time_is_future unconditionally return
zero. Thus certificate notBefore/notAfter enforcement is disabled in the linked
library, despite NTP gating. This qualifies the earlier generic description of
certificate validation: chain/hostname checks are configured, validity dates are
not enforced. Do not claim full TLS readiness or fix this with only an application
-D flag; use a reviewed library/framework correction. No framework replacement
or precompiled SDK modification was performed during this readiness inspection.

Official behavior verified 2026-10-25:
https://docs.espressif.com/projects/esp-idf/en/v4.4.7/esp32s3/api-reference/kconfig.html#config-mbedtls-have-time-date
states the option controls certificate valid-from/valid-to checks. These checks
must be enabled in the TLS library build, or equivalent verified enforcement
provided before treating the build as ready.

Direct TLS observation 2026-10-25 from the Mac: the public feed hostname and
certificate chain validate using system trust, and a separate root-only TLS 1.2
connection also validates. Observed leaf issuer Let's Encrypt YE2, leaf expiry
2026-12-16T15:02:36Z; verified root ISRG Root X2 (ECDSA P-384). The selected SDK
enables TLS 1.2 and that curve. Saved only the public root in a temporary local
file for configuration; no credential or raw exception output. Root SHA-256:
69729b8e15a86efc177a57afb7171dfc64add28c2fca8cf1507e34453ccb1470.
Official root source verified 2026-10-25:
https://letsencrypt.org/certificates/ and
https://letsencrypt.org/certs/isrg-root-x2.pem.
Mac success is not proof of the ESP32 handshake, NTP or future CDN chain stability.

Readiness: not ready to flash. Configure the ignored local header, resolve the
precompiled TLS date-check omission, and rerun the readiness checks. The prior
backend-field blocker is resolved by production version
2bd64122-96d6-4048-ac55-780d9842e1f6; the backend was not modified in this task.


## Prototype-only TLS exception and local preparation — 25 October 2026

The user explicitly accepted the previously documented missing TLS validity-date
checks for this first physical prototype test only, and prohibited framework
rebuilds or insecure TLS modes. Full certificate notBefore/notAfter enforcement
remains mandatory before production. This supersedes the earlier prototype
readiness blocker, not the production security requirement. Chain and hostname
validation, the pinned framework and NTP-before-HTTPS logic remain unchanged.

Created ignored/untracked include/secrets.h from the existing example with mode
0600. Configured only the existing public HTTPS origin and verified ISRG Root X2
PEM. Wi-Fi SSID, Wi-Fi password and existing Feed API token are deliberately empty
for the user to enter locally; no new token or Cloudflare secret mutation. Empty
configuration fails closed as CONFIG. No credential value was printed or saved
in evidence, and no backend, Feed v1, renderer, typography, colour policy,
brightness, hardware pin or diagnostic code was changed.

Public CA source verified 2026-10-25:
https://letsencrypt.org/certs/isrg-root-x2.pem. Download used system-verified
HTTPS, then SHA-256 matched the previously verified ISRG Root X2 fingerprint:
69729b8e15a86efc177a57afb7171dfc64add28c2fca8cf1507e34453ccb1470.
A root-only TLS 1.2 connection to
https://live-transit-feed.roger-80a.workers.dev verified hostname and chain from
the Mac, without sending any token. This does not verify the ESP32 handshake or
remove the accepted device-side validity-date limitation.

PlatformIO re-enumerated the Espressif USB JTAG/serial unit at
/dev/cu.usbmodem3101. Enumeration only: no serial open/reset, upload or flash.
Current manual commands and expected LED/serial behavior are in live-arrivals.md.
Remaining private configuration and radio/SNTP/TLS operation must be verified
in the physical test. Builds made with empty credentials are not live-ready.

Validation after local header creation: all 51 native firmware tests passed;
hub75-live build passed (45,256 bytes static RAM; 727,109 bytes flash). Formatting,
generated fixtures and diff whitespace checks passed. The installed RootCa PEM
was parsed successfully and its fingerprint rechecked; header remains ignored,
untracked and mode 0600, with all three private fields empty. Git diff confirms
only documentation changed among tracked files; app/panel configuration, normal
renderer, fonts, palettes, brightness and diagnostics remain unchanged. Rebuild
after entering the private values; the manual upload command performs that build.
No ESP32 flash, reset, serial-monitor connection or backend change was made.


## Persistent Wi-Fi and protected local setup — 25 October 2026

Implemented in the existing live firmware; no partition, environment, renderer,
Feed v1 contract, backend, TLS policy, deployment or device flash changed. The
new WifiSetup module owns station retries, a ten-minute WPA-protected SoftAP and
a self-contained mobile HTTP form bound to the AP address. It supports async
network discovery/manual SSID, bounded candidate association, failure/retry,
committed NVS credentials and shutdown five seconds after success. Feed work
pauses during setup; normal reconnects continue without credential erasure.

Credentials are versioned blobs in the existing NVS transit-wifi namespace.
Inactive-slot write/readback precedes active-slot commit, retaining the previous
record. Saved settings take priority; compiled development Wi-Fi is tested once
only when the namespace has no records. FeedClient no longer requires a compiled
Wi-Fi SSID. No passwords are logged. NVS encryption/production security and the
Arduino default-NVS auto-erasure caveat remain unresolved; no destructive NVS
recovery was introduced.

BOOT GPIO0 runtime long-press (release after startup, then hold three seconds)
and the development serial command wifi + Enter reopen setup without clearing
saved credentials. Waveshare documents a programmable BOOT button and its
official music-player example defines BOARD_BUTTON_PIN 0. Source URLs,
verification date, enclosure/access limitations, migration behaviour and all six
manual acceptance scenarios are in [Wi-Fi setup](wifi-setup.md). Actual button,
phone handoff, AP channel changes, radio, stack/heap and power-cut NVS tests are
pending; no physical test or board connection was performed.

Before rebuilding, preserved the prior local hub75-live application (886,448
bytes), bootloader and partition binary in ignored artifacts/wifi-before-change;
added the matching stock helper, build settings/table and verified SHA-256
manifest. Artifacts may contain embedded secrets and remain private. This is not
a device readback. Application-only wired restoration at 0x10000 and optional
full-image recovery offsets are documented, not executed.

Final software verification: 58/58 native cases passed (54 existing, four new
credential/record validation cases); all eight existing hardware environments
built successfully. Final hub75-live: 932,649 application bytes, 46,492 static
RAM bytes. Formatting, fixture consistency, diff whitespace and archive hashes
passed. The partition CSV, PlatformIO settings and generated live partition
binary match the preserved versions. Native cases do not emulate Wi-Fi, HTTP
or NVS power-loss behaviour. No upload, erase, reset, repartition or deployment.


## Focused Wi-Fi onboarding UX — 25 October 2026

User requested simpler prototype credentials and a persistent readable setup
screen. AP defaults are now lowercase metro-display / bcnmetro, separately
configurable in lib/transit/src/wifi_setup_screen.h. The random HTTP form token
remains. Five fixed C2 rows show SSID, PW plus password, LOWERCASE, OPEN HTTP and
192.168.4.1 at y=0/6/12/20/26. This explicitly handles uppercase-only C2 glyphs
without changing transport fonts or the renderer. No paging or scrolling.

Removed setup inactivity expiry; the AP/page and credentials remain until verified
Wi-Fi is saved and the existing five-second success shutdown completes. Prior
NVS records, candidate verification, retry behaviour, migration fallback, and
BOOT/serial recovery are unchanged. Reboot can recover the previous saved network
when setup is abandoned. No partition, backend, environment or TLS changes.

Verification: 59/59 native cases passed, including setup text coverage/width at
64×32; hub75-live built with 932,353 application bytes and 46,484 static RAM bytes.
These are software observations, not physical readability or radio tests. Updated
docs/wifi-setup.md covers the no-timeout manual check and configurable prototype
settings. No flash, erase, device connection or deployment performed.


## Wi-Fi submission diagnostics and response ordering — 25 October 2026

Investigated the observed setup form submission failure by tracing the existing
HTTP handler, pinned WebServer/WiFiSTA implementation, AP/STA association, timeout
and NVS activation. Found WiFi.disconnect/begin ran before the 303 response,
creating a response/channel-switch race. This is a verified code ordering defect
and plausible symptom explanation, not a hardware-reproduced root cause.

POST now returns a complete HTTP 200 progress page before the network task starts
the queued connection after one second. Refresh explicitly uses GET /, avoiding
POST resubmission or a missing GET /connect handler. Begin status is not treated
as final asynchronous failure. The connection budget remains 20 seconds, timed
from the real attempt. Valid explicit :80 Host/Origin values are accepted without
allowing foreign origins or dropping token/interface checks.

Added safe serial receipt/SSID/attempt/result/disconnect-reason/save/exit events,
HTTP rejection categories and NVS failure stages. SSIDs are bounded/escaped; no
passwords, form tokens, raw SDK exceptions, headers or request bodies are logged.
Reason-specific retry pages preserve prior credentials. Shared-channel changes
can still disconnect a phone; the page documents rejoining the setup AP.

Sources checked 2026-10-25: installed Arduino 2.0.17 WiFiSTA/WebServer sources and
IDF 4.4.7 home-channel documentation linked in docs/wifi-setup.md. That guide also
records the full request-to-persistence trace, limitations and exact reproduction
steps including incorrect password, default-port URL, successful save, reboot
and router-channel/DHCP failure checks. Physical verification remains pending.

Validation: 62/62 native cases passed; hub75-live built (936,301 application bytes,
46,516 static RAM bytes). Formatting, fixture consistency and diff whitespace
passed. No partition, NVS schema, transport renderer, recovery controls, backend
or TLS policy change. No flash, erase, deployment or device connection.
