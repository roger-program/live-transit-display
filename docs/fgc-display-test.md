# Local FGC physical-display preparation — 7 November 2026

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
