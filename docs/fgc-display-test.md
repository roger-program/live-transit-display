# Local FGC physical-display preparation — 7 November 2026

**Current operator colour: #00834A/white, pending physical validation.** See [green update](#operator-green-and-physical-feedback--7-november-2026).

**Destination revision:** [Explicit destination rows](#explicit-destination-rows--7-november-2026) with the [consolidated provisional palette](#consolidated-provisional-palette--7-november-2026). Comparison sections below are historical; A/B/C and darker-background variants are removed.

Local implementation/build preparation only. **Not flashed or physically validated.** Backend code, deployment, device configuration and browser simulator are unchanged. Central tracking remains in the sibling feed repository.

## Minimal architecture change

Feed v1's required fields/version are unchanged. Firmware accepts an optional string `operator` on a stop, stored as `operatorId`. It is an experimental additive presentation extension used by the local fixture; older clients ignore it, and the current backend does not emit it. Missing values preserve existing M/B/? behavior. Unknown operators preserve that same fallback; malformed supplied types/oversized strings reject the feed transactionally using existing text bounds.

The renderer looks up a bounded compiled symbol registry by operator ID. It does not interpret FGC routes, station IDs, API formats or line-colour rules. The registry's FGC entry copies the already-reviewed 7×5 simulator mask (rows 22,41,85,74,52), white on #A1D341, centered in the existing 7×7 Metro operator slot at (1,1). Line colours/foregrounds continue to come from ordinary `lineColor` / `lineTextColor`. No font/accent changes.

## Isolated offline test

`hub75-fgc-fixture` extends the existing ESP32-S3 board/panel configuration and selects only `display_driver.cpp` and `fgc_fixture_main.cpp` as application sources. It uses the regular parser, state/freshness logic, pagination, countdowns and renderer. It never includes `app_config.h`/`secrets.h`, starts Wi-Fi/SNTP, reads device identity, runs provisioning or calls persistent-storage APIs. The pinned Arduino framework startup is unchanged: it initializes default NVS and retains its existing recovery behavior for broken/incompatible NVS. This is not a byte-for-byte flash-persistence guarantee; no new erase/reset path is introduced. Brightness is fixed at 4/255. Panel pins/timing and the partition CSV are unchanged; this project currently has a factory application and separate identity NVS, **no OTA slots**.

`fixtures/fgc.json` is explicitly synthetic, not live FGC data. Six PC pages use the selected palette: L6 #867EC4/white, L7 #AE621C/white, L8 #E27AB4/white, L12 #B6B3E1/black, S1 #DC6F2D/white, S2 #93BC44/white. PC displays PL. CATALUNYA; two PADUA L7 groups exercise distinct destinations. Minute predictions carry unknown provenance (`?`), not realtime. The normal 120-second stale policy suppresses them. No colour comparison or new visual experiment is included.

Controls at 115200 baud: `n` next page, `r` restart the synthetic replay, `i` repeat the safe report. Otherwise pages rotate normally. Reset restarts replay. The known 59-pixel PL. CATALUNYA label still uses existing scrolling in the 53-pixel native header; there is no claim that it fits without scrolling.

## Build and checks (no hardware operations)

From this firmware repository, with the existing virtual environment activated:

```sh
python tools/generate_fixtures.py --check
python tools/format.py --check
pio test -e native
python tools/test_device_identity.py
pio run -e hub75-fgc-fixture -e hub75-live -e hub75-fixture
```

`make check` also includes the new environment alongside all existing environments. No command above uploads or connects to the ESP32. Build artifacts remain under ignored `.pio/build/`.

## Simplest controlled test on the ESP32-S3 (manual only)

The following procedure is prepared for the user to execute later; it was **not executed** during implementation. Use the established Waveshare controller/panel/power setup. Close any serial monitor first. Keep identity/Wi-Fi NVS and partition layout intact; no erase, identity provisioning, partition migration or full-flash image is required.

1. Identify the currently connected ESP32-S3 port with `.venv/bin/pio device list`, then set `FGC_TEST_PORT` to that actual port. Do not reuse an old port blindly. Retain a known-good live application for restoration, or build `hub75-live` with the existing unchanged local production settings. The existing private recovery baseline/procedure remains applicable; see the sibling [controlled application-only update](../../live-transit-feed/docs/preflash-verification-2026-10-31.md).
2. Build the diagnostic above. Verify the installed partition table against this unchanged build **before writing**. These reads may reset the board into its loader but do not write flash. Read only the table, never the identity partition:

```sh
mkdir -p artifacts/fgc-test
FGC_ESPTOOL="$HOME/.platformio/packages/tool-esptoolpy/esptool.py"
.venv/bin/python "$FGC_ESPTOOL" --chip esp32s3 --port "$FGC_TEST_PORT" \
  --after no_reset read_flash 0x8000 0x1000 artifacts/fgc-test/installed-partitions.bin
.venv/bin/python - <<'PY'
from pathlib import Path
build = Path('.pio/build/hub75-fgc-fixture')
expected = (build / 'partitions.bin').read_bytes()
actual = Path('artifacts/fgc-test/installed-partitions.bin').read_bytes()
image = (build / 'firmware.bin').read_bytes()
assert len(actual) == 0x1000 and expected and actual[:len(expected)] == expected, 'Partition mismatch: STOP'
assert 0 < len(image) <= 0x400000, 'Application too large: STOP'
assert 0x10000 + ((len(image) + 4095) // 4096) * 4096 <= 0x410000, 'Identity boundary: STOP'
print('Table matches; application fits before device-auth. No write performed.')
PY
```

3. **Only when choosing to flash the diagnostic**, write the application alone at 0x10000, retaining flash settings. Do not substitute the ordinary multi-image upload command, rewrite partitions/bootloader/identity, or add erase options:

```sh
.venv/bin/python "$FGC_ESPTOOL" --chip esp32s3 --port "$FGC_TEST_PORT" --baud 460800 \
  --after hard_reset write_flash --flash_mode keep --flash_freq keep --flash_size keep \
  0x10000 .pio/build/hub75-fgc-fixture/firmware.bin
.venv/bin/pio device monitor --port "$FGC_TEST_PORT" --baud 115200
```

Confirm successful write verification. On boot, expect the synthetic/offline report, brightness 4/255, FGC symbol, existing paging and selected line data. Check six PC and two PADUA pages, the short-header reveal and separate L7 destinations. Wait at least 120 seconds and confirm STALE/no countdowns. `r` restarts the fixture. This validates local rendering/state behavior, not native API accuracy or production connectivity.

4. Close the monitor. Restore the known-good live application using the same application-only command with its `firmware.bin` path after confirming its size/layout. When using the rebuilt normal environment, that path is `.pio/build/hub75-live/firmware.bin`. Do not erase/reprovision either NVS partition. Restart and verify existing Wi-Fi/device-feed operation resumes. The diagnostic application does not modify stored credentials/configuration; actual persistence/recovery is a physical observation to record, not a build result.

If the table, chip, size, write verification or display initialization differs from expectations, stop. Record only safe observations and photos. No physical success, colour acceptance, live arrival accuracy or device recovery has yet been established by this work.

## Recorded preparation result

Final `make check`: formatting and generated-fixture checks passed; 74/74 native cases, 2/2 identity-image tests and 9/9 build environments passed. FGC/live compiled partition tables match, and the partition CSV is unchanged from HEAD. The application fits before device-auth. [Artifact size/hash and scope](evidence/fgc-display-preparation-2026-11-07.json). No device was accessed or flashed.

## Softer-white diagnostic revision — 7 November 2026

FGC diagnostic only: seven formerly white line-label foregrounds in `fixtures/fgc.json` now use #D9D9D9; L12 black and all backgrounds remain unchanged. The operator asset foreground uses #D9D9D9 only under the existing FGC_FIXTURE_TEST compile flag. Normal builds retain the original pure-white symbol and existing Metro/bus presentation. No renderer, typography, layout, prediction count, brightness or freshness change. Structural comparison against HEAD confirmed the fixture differs only in those seven foreground values.

The ? countdown prefix is intentional: all synthetic arrivals retain provenance unknown. `state.cpp::countdown` prefixes unknown predictions with ?, scheduled with S, realtime with no marker; the renderer also retains the unknown prefix for overflow labels. This is unrelated to font/accent replacement. No provenance/countdown changes were made.

Verification: 74/74 native tests passed, including unchanged default-symbol/fallback pixels and updated fixture palette assertions. Diagnostic compile-time assertion checks its softened symbol foreground. Formatting/generated-fixture checks and FGC build passed. Current binary `.pio/build/hub75-fgc-fixture/firmware.bin`: 308,544 bytes; SHA-256 `26efe8f54c249e07c31987dea3049ac13a0b1bd552223e3c26a8bfca04a316c2`. This supersedes the earlier diagnostic artifact hash, not its historical preparation record. No build of the live target, flash or deployment occurred. Exact known-good restore application remains separate and unchanged; see central phase-1 record. Next: separately authorized physical comparison.


## Background A/B diagnostic revision — 7 November 2026

Supersedes softer-white experiment for this offline diagnostic only. Restored #FFFFFF for all formerly white line foregrounds and FGC symbol, retaining L12 black, #B6B3E1, typography and existing renderer. The now-unnecessary diagnostic symbol colour conditional is removed; normal builds retain exactly their original white asset. All backgrounds except four explicit B duplicates are retained. B scales encoded RGB channels by 0.75 with nearest rounding (halves up); this is approximately 25% lower channel values, not a measured 25% reduction in panel luminance. No production/Metro/bus/backend changes.

| Line | A original | B darker |
| --- | --- | --- |
| L6 | #867EC4 | #655F93 |
| L8 | #E27AB4 | #AA5C87 |
| S1 | #DC6F2D | #A55322 |
| S2 | #93BC44 | #6E8D33 |

Navigation order (normal 3.3-second pages and existing n control): PC L6 A, L6 B, L7 unchanged (#AE621C), L8 A, L8 B, L12 unchanged (#B6B3E1/black), S1 A, S1 B, S2 A, S2 B, PADUA L7 Av. Tibidabo, PADUA L7 Barcelona - Placa Catalunya, then repeat. The diagnostic reports each current page/variant/background over serial, also on automatic transitions; i repeats the current report. No extra display text or layout changes.

Arrivals are explicitly simulated realtime-style predictions (provenance realtime and realtimeStatus available) solely in the offline fixture, so normal countdown formatting emits no ? or S prefix. Serial still states synthetic/offline/not live. This is not evidence of actual realtime FGC coverage. Existing generated timestamp, three predictions per group, prediction times and 120-second stale gate are unchanged; r restarts replay. More pages still fit in the unchanged stale window.

Verification: 74/74 native cases, formatting/generated-fixture checks and FGC-only build passed. Tests check exact palette/order, 12 pages, three unprefixed countdowns, matched A/B destinations/times, default operator/fallback pixels and stale suppression. Additional fixture check confirmed original prediction times and L12 presentation unchanged. No live build, flash or deployment. Binary `.pio/build/hub75-fgc-fixture/firmware.bin`: 310,544 bytes, SHA-256 `02c5e83a48637769294087b690c14344eb95a0df5e69701f0313b019826f7bd4`. Hardware A/B judgement remains pending.


## Foreground A/B/C diagnostic and symbol inspection — 7 November 2026

Supersedes background A/B for the current diagnostic. A #FFFFFF, B #999999, C #666666 apply to every formerly white line-label foreground and the FGC operator symbol. Original line backgrounds are restored for all variants: L6 #867EC4, L7 #AE621C, L8 #E27AB4, L12 #B6B3E1, S1 #DC6F2D, S2 #93BC44. L12 remains #000000 in all variants, while the operator symbol follows A/B/C even on L12 pages. Operator background remains #A1D341.

Implementation is fixture presentation data: three copies of the PC/PD pair, preserving canonical stop IDs/names, destinations and times; only diagnostic operator aliases differ. FGC-TEST-B/C symbol entries exist solely under FGC_FIXTURE_TEST and copy exactly the original mask. They are absent in normal builds. Shared renderer/driver, layout, fonts and global brightness 4/255 are unchanged. Three synthetic realtime-style predictions per group and the normal stale gate remain. No new generic Feed v1 fields or production behavior.

Existing n/automatic navigation cycles 24 pages: variant A PC L6/L7/L8/L12/S1/S2 then PADUA L7 Tibidabo/Catalunya; repeat the same eight pages for B, then C, then A. Serial identifies variant, foreground, station/line/destination and background for each page. Full automatic cycle is 79.2 seconds, inside the existing 120-second stale window; r restarts replay, i repeats the current report.

Verification: 74 native cases passed, including all original backgrounds, exact variant foregrounds, L12 black, 24-page mapping, unchanged destinations/times, three unprefixed countdowns, stale suppression, and absence of diagnostic operator aliases in default builds. Diagnostic compile-time assertions validate symbol foregrounds. Format/generated-fixture checks and FGC-only build passed. Current binary `.pio/build/hub75-fgc-fixture/firmware.bin`: 314,144 bytes; SHA-256 `1540e16c1d48675104b2d008621169d8876a5e19ce7479a1c227ad83370d2bc4`. No production build, flash/deployment or simulator work.

### Operator bitmap findings (inspection only)

User reports poor physical FGC-logo recognition independently of badge colours (7 November 2026). This is user-reported hardware feedback; no fresh hardware observation/photo analysis by the agent. No bitmap geometry/pattern redesign was performed.

Verified mask: width 7, height 5, rows decimal 22/41/85/74/52 (hex 16/29/55/4A/34), 16 lit pixels of 35. Pattern, # = foreground:

```text
..#.##.
.#.#..#
#.#.#.#
#..#.#.
.##.#..
```

The generic renderer places this at x=1..7, y=2..6 within the 7×7 background at (1,1), with one top/bottom background row and no horizontal inset. Metro uses its C2 M glyph, 5×5 rows 17/27/21/17/17, at x=2..6/y=2..6 inside the same 7×7 slot, with one-pixel margins on all sides. Metro is a letter approximation rather than a full logo. Both use integer framebuffer pixels and the same RGB888 driver; no scaling, filtering, antialiasing or added pixel spacing occurs. Existing raster tests verify the FGC mask's software placement. This establishes correct rendering of the stored mask, not faithful recognition of the reference logo.

Inspection/inference: the FGC mask has 12 orthogonally connected foreground components, joined into one only when diagonal adjacency counts; its zigzags rely heavily on corner contacts. Metro M has fewer such components (3) and long vertical strokes. Component count is descriptive, not a perceptual quality test. The reference mark's thick curved interlocking strokes are severely simplified at 7×5; whether this is recognizable is not established by bit correctness. Panel configuration identifies a P2.5 panel; physical LED gaps/optics and viewing distance may weaken diagonal joins, but current pitch/gaps, optical bloom and panel timing were not measured in this task. No source evidence of a symbol-specific spacing/scaling bug; overall physical display faults cannot be ruled out by source inspection alone.

Recommendation (not implemented): treat this as a representation/resolution problem, not a colour-only problem. At the fixed 7×7 badge size, reconsider whether the full FGC mark is an appropriate symbol. One bounded hand-tuned bitmap refinement could improve continuous strokes, but should not be assumed to preserve recognizable branding; if recognition remains poor, choose an explicitly approved simpler identifier or review a larger badge separately. No firmware layout or symbol redesign is authorized by this inspection.


## Consolidated provisional palette — 7 November 2026

User-confirmed provisional line palette following user-reported physical LED colour tests. Original backgrounds; selected foregrounds mirror backend FGC_PRESENTATION via ordinary lineColor/lineTextColor, not firmware colour-selection logic:

| Line | Background | Foreground |
| --- | --- | --- |
| L6 | #867EC4 | #FFFFFF |
| L7 | #AE621C | #FFFFFF |
| L8 | #E27AB4 | #000000 |
| L12 | #B6B3E1 | #000000 |
| S1 | #DC6F2D | #000000 |
| S2 | #93BC44 | #000000 |

FGC operator background is now provisional #006B3B, symbol #FFFFFF. The 7×5 mask, 7×7 slot, coordinates and dimensions are unchanged. Only registry presentation colour changes; shared renderer untouched. Private preview operator colour is sourced from shared backend presentation configuration. Feed v1 has no operator-colour fields, so firmware still uses a matching compiled asset registry; no wire extension was introduced.

Removed all background/foreground comparison pages, diagnostic symbol aliases and variant-only reporting/assertions. Navigation returns to eight pages: PC L6, L7, L8, L12, S1, S2, then PADUA L7 Tibidabo and L7 Catalunya. Serial n advances; automatic paging remains 3.3 seconds. Three predictions per group, times, synthetic realtime provenance, global brightness 4/255, normal 120-second stale gate, typography/layout and production configurations are unchanged. Serial continues identifying data as synthetic, not live. Metro/bus untouched.

Final verification: 74/74 native cases passed, including selected palette, eight-page ordering, absence of variant registry entries, unchanged mask pixels over new green, three unprefixed predictions and stale suppression. Formatting/generated-fixture checks and FGC-only build passed. Backend 218 tests/type/lint/format passed; explicit cross-repository comparison verified all eight fixture groups match shared presentation colours. Current binary `.pio/build/hub75-fgc-fixture/firmware.bin`: 309,184 bytes, SHA-256 `1652c8413ecbf210b19cbef40fd348e1127ed945b9408b815a63c50821a6fb3a`. Supersedes historical diagnostic hashes. No live replacement built, device connection/flash or deployment. Exact October 31 restore artifact remains separate/unchanged.

Outstanding: poor physical operator-symbol recognition (user-reported), bitmap/representation redesign pending; darker operator green is provisional and not newly hardware-validated. L12 typography review, native header overflow and deferred accents remain. Pàdua native live coverage/accuracy, quota accounting and licensing/attribution are independent of these colour tests. Next: separate symbol review, with no assumption that recolouring solves the shape problem.


## Explicit destination rows — 7 November 2026

Root cause: existing automatic wrapping accepts two rows only when both fit the bitmap viewport; otherwise the complete label scrolls on one line. Current Sabadell fixture fits SABADELL / PARC DEL NORD (39/60 px), hence its existing good two-row behavior. No provider-specific renderer is needed.

Backend exact-name presentation aliases insert a newline in the existing destination string; canonical destinations/identifiers and prediction semantics remain unchanged. Firmware destinationLines honors the explicit split before font normalization, flattens further breaks into row two and preserves the old implicit wrap behavior. Each row is clipped at x=1..62 and occupies y=20..24 or 26..30 below line badges/countdowns. Fitting rows stay stationary. Overflows use shared normalized progress, existing 700 ms leading/far-end pauses and 600 ms eased outward/return movement within the 3.3-second page cycle. Content/page changes or replay-clock reset restart the explicit reveal; it repeats if the same page remains. No FGC name matching in renderer, new wire fields, font changes or global timing changes.

Offline examples: PC S1 TERRASSA / NACIONS UNIDES (39/65 px); PC L8 MOLI NOU / CIUTAT COOPERATIVA (37/85 px); PADUA L7 BARCELONA / PL. CATALUNYA (44/59 px); PC S2 retains automatic SABADELL / PARC DEL NORD (39/60 px). Thus Moli's first row is fixed and second scrolls 23 pixels; Terrassa's second scrolls 3 pixels; Barcelona and Sabadell rows are fixed. Accents use existing folding; full destination information is retained apart from explicitly configured PL. abbreviation/punctuation presentation. No canonical-name mutation. Eight pages and three predictions, all timestamps/provenance, line/operator palette, panel brightness and layout remain unchanged; fixture structural comparison verified changes only to destination strings.

Tests: 75 native cases and 2 identity-image tests passed; format/generated-fixture checks and all 9 firmware builds passed. New pixel tests cover fixed/scrolling rows, two overflowing rows sharing timing, pauses, reset on in-place destination change, and no overlap with badge/predictions or panel edges. Existing Metro/bus raster/wrap tests pass unchanged. Backend 220 tests and type/lint/format passed; tests verify exact aliases preserve normalized data/predictions, while private preview compatibility respects newlines without a new visual experiment.

Current FGC binary `.pio/build/hub75-fgc-fixture/firmware.bin`: 310,512 bytes, SHA-256 `570fc8d0ff70c950b076624914e50918df23c719f12e997c7bfe8d91620edb82`. Older diagnostic hashes are historical. No flash/deployment or production settings changes; hardware readability remains unverified. The live target was compiled as part of required shared-renderer regression checks; this does not replace the exact known-good private October 31 restoration artifact.

Compatibility/limits: Feed v1 structure/version remain unchanged, but old renderers lack explicit-break semantics; keep this optional mapping confined to the experiment until separately authorized rollout with compatible firmware. Maximum two 5-pixel-high destination rows, 62-pixel width; very long rows reveal quickly within existing page timing, not an unlimited reading interval. Glyphs/accents and the poor FGC operator symbol remain separate issues. See sibling [destination presentation policy](../../live-transit-feed/docs/fgc-visual-identity.md#explicit-destination-rows--7-november-2026).

Files changed in firmware: lib/transit/src/renderer.h, lib/transit/src/renderer.cpp, fixtures/fgc.json, generated include/fixture.h, test/test_core/test_main.cpp, docs/fgc-display-test.md and docs/phase-1.md.


## Operator green and physical feedback — 7 November 2026

Updated common FGC firmware asset background #006B3B → #00834A; symbol foreground remains #FFFFFF. Both normal firmware and offline diagnostic use this same registry entry. Shared backend presentation colour matches; no new protocol field. Bitmap rows/dimensions, line colours, typography/layout, destinations, scrolling, brightness, three predictions and production settings are unchanged.

User-reported physical 64×32 validation, 7 November 2026: L8/S1/S2 black foregrounds accepted provisionally; generic two-line destination rendering and independent scrolling validated. Exact tested build/time/duration/photos not supplied; not agent-observed. Do not infer live-source/freshness/outage acceptance. New #00834A green awaits physical validation, and bitmap redesign/recognition remains pending.

Verification for this colour-only revision: 75 native tests (including exact operator pixels/fallback and unchanged destination regressions), 14 focused feed/presentation/preview tests, backend types/lint/format and firmware format/generated-fixture checks passed; diagnostic build passed. No normal/live target rebuild was needed; its shared asset source is updated, not its previous binary. Current diagnostic `.pio/build/hub75-fgc-fixture/firmware.bin`: 310,512 bytes; SHA-256 `752f8d4310096650d9753f8fd8a1a9e7b41123aac1da178943285f0982e2a3c5`. Prior artifact hashes are historical; exact known-good restore file is unchanged. No device connection, flash or deployment.


## Operator background #72900F — 7 November 2026

User refinement supersedes #00834A: common firmware operator asset and shared backend presentation now #72900F/white. Bitmap, dimensions, line colours and all rendering behavior unchanged. 75 native tests, 14 focused backend presentation/replay tests and formatting check passed; FGC diagnostic built. Current binary .pio/build/hub75-fgc-fixture/firmware.bin: 310512 bytes, SHA-256 54d277f3c1eb7088e4f2e1c3def41397f3f30ca56bc2a5b7c35f9f24bbe6f8eb. No flash/deployment; physical validation of this colour pending. Previously reported line/destination acceptance remains unchanged.


## Transition to normal live application (prepared locally)

Latest requested live palette uses operator #00834A/white; bitmap and all line/destination rendering remain unchanged. Prototype physical acceptance is user-reported, with minor visuals deferred. Do not use this offline fixture as the live application. Normal hub75-live is built against the shared renderer and existing authenticated device polling; no new provisioning or Wi-Fi setup. [Local build/evidence](phase-1.md#fgc-live-application-integration-preparation), [central activation/rollback gates](../../live-transit-feed/docs/fgc-live-integration.md). No flash/deployment performed.
