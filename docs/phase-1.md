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
