# Physical badge contrast experiment

Isolated `hub75-badge-contrast` environment; no network, Feed v1 input, automatic foreground selection or normal transit-renderer changes. Uses the existing bitmap drawText, RGB888 framebuffer and PrototypeDisplay/HUB75 adapter. Brightness is fixed at 16/255 (compile-time checked); neither serial controls nor this environment can select the existing brightness-ramp diagnostic.

## Build and run

From `/Users/rogerpau/mamp/live-transit-display`:

```sh
source .venv/bin/activate
python tools/generate_fixtures.py --check
python tools/format.py --check
pio test -e native
pio run -e hub75-badge-contrast
pio device list
```

**No upload has been performed for this experiment.** Only after separate authorization, with reviewed power/HUB75 connections and the correct enumerated port:

```sh
pio run -e hub75-badge-contrast -t upload --upload-port /dev/cu.usbmodem3101
pio device monitor --port /dev/cu.usbmodem3101 --baud 115200 --dtr 1 --rts 0
```

Substitute the actual port if different. USB reset/reconnection can lose the startup report; send lowercase `i` to repeat the current sample/foreground/brightness report. Commands are single characters, no Enter needed.

| Command | Background / action |
| --- | --- |
| 0 | L1 #CE1126 |
| 1 | L2 #93248F |
| 2 | L3 #1EB53A |
| 3 | L4 #F7A30E |
| 4 | L5 #005A97 |
| 5 | L9 #FB712B (Nord/Sud share colour) |
| 6 | L10 #00A6D6 (Nord/Sud share colour) |
| 7 | L11 #89B94C |
| n / p | Next / previous sample, wrapping |
| s | Swap black/white sides |
| i | Repeat serial report |

Starts on L1; holds the selected sample indefinitely. Uses exactly 64×32 pixels. BLACK/WHITE headings identify each side, including after swapping. Two equal 29×11 backgrounds occupy x=1–29 and x=34–62, y=10–20; the same line identifier is centred in both at y=13, with identical glyphs/spacing. N/S is shown below the L9/L10 pairs. No scrolling, paging dots or unrelated icons compete with the pair. Badge comparison is the same for both branches; branch suffixes are omitted to fit both labels equally.

Background data comes from the sibling backend's reviewed official TMB metadata (L1–L5 captured 2026-10-03; branches/L11 2026-10-21). The small table is offline diagnostic input only, never used by the production renderer or parser. See sibling `docs/evidence/tmb-metro-colours-2026-10-21/`. Canonical RGB inputs remain unchanged; existing driver CIE correction, PWM/bitplane processing and brightness 16 are deliberately retained.

## Interpretation and recording

Read each pair at the same distance/angle and ambient lighting. Compare speed of recognition, missing-looking strokes, white bloom, black cutout visibility and background hue. Swap sides to rule out viewing-position bias; do not increase brightness during the comparison. Record sample, preferred foreground (or tie/neither), distance, angle, ambient lighting and whether the preference survives swapping. Repeat under representative brighter and darker ambient conditions while holding device settings fixed.

This tests physical legibility, not brand compliance or calibrated luminance. The catalogue's COLOR_TEXT_LINIA differs from the user's visual reference for some lines; neither is applied automatically here. At 16/255, emitted light is not a calibrated 6.3% luminance and a photograph may clip or distort colour. Do not replace canonical background metadata with adjusted LED swatches. Choose any later generic contrast/calibration policy only after evaluating observations; this experiment does not select a production rule.

## Bottom-edge pixels in the normal renderer

`lib/transit/src/renderer.cpp` draws **service-page indicators**, not freshness/status indicators or unintended pixels. When the current stop has more than one service page, it draws up to width/3 one-pixel markers at `(1 + 3*i + transitionOffset, height - 2)`. At 64×32 the row is y=30, one above the bottom row. Current page: #AAFF66; other pages: #384438. The count is pages for the current stop, not total stops or total feed pages. Existing fixture: four Metro groups/pages, two bus groups/pages. Markers move with content during horizontal transitions; stale/empty views can reduce the stop to one page and remove the markers. Their deliberate low density explains the isolated coloured dots visible near the bottom-left. No normal-renderer code was changed.
