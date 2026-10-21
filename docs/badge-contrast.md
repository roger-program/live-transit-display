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
| a | A/B: black regular vs white regular (startup default) |
| b | A/C: black regular vs white bold |
| c | B/C: white regular vs white bold |
| s | Swap the two selected faces |
| i | Repeat serial report |

Starts on L1; holds the selected sample indefinitely. Uses exactly 64×32 pixels. A BLK / B WHT / C BLD headings identify each face, including after swapping. Two equal 29×11 backgrounds occupy x=1–29 and x=34–62, y=10–20. Regular text is centred at y=13 (5 rows); designed bold text is centred at y=12 (7 rows). Both regular variants retain identical glyphs/spacing. N/S is shown below the L9/L10 pairs. No scrolling, paging dots or unrelated icons compete with the pair. Badge comparison is the same for both branches; branch suffixes are omitted to fit both labels equally.

Background data comes from the sibling backend's reviewed official TMB metadata (L1–L5 captured 2026-10-03; branches/L11 2026-10-21). The small table is offline diagnostic input only, never used by the production renderer or parser. See sibling `docs/evidence/tmb-metro-colours-2026-10-21/`. Canonical RGB inputs remain unchanged; existing driver CIE correction, PWM/bitplane processing and brightness 16 are deliberately retained.

## Interpretation and recording

Read each pair at the same distance/angle and ambient lighting. Compare speed of recognition, missing-looking strokes, white bloom, black cutout visibility and background hue. Swap sides to rule out viewing-position bias; do not increase brightness during the comparison. Record sample, preferred foreground (or tie/neither), distance, angle, ambient lighting and whether the preference survives swapping. Repeat under representative brighter and darker ambient conditions while holding device settings fixed.

This tests physical legibility, not brand compliance or calibrated luminance. The catalogue's COLOR_TEXT_LINIA differs from the user's visual reference for some lines; neither is applied automatically here. At 16/255, emitted light is not a calibrated 6.3% luminance and a photograph may clip or distort colour. Do not replace canonical background metadata with adjusted LED swatches. Choose any later generic contrast/calibration policy only after evaluating observations; this experiment does not select a production rule.

## Bottom-edge pixels in the normal renderer

`lib/transit/src/renderer.cpp` draws **service-page indicators**, not freshness/status indicators or unintended pixels. When the current stop has more than one service page, it draws up to width/3 one-pixel markers at `(1 + 3*i + transitionOffset, height - 2)`. At 64×32 the row is y=30, one above the bottom row. Current page: #AAFF66; other pages: #384438. The count is pages for the current stop, not total stops or total feed pages. Existing fixture: four Metro groups/pages, two bus groups/pages. Markers move with content during horizontal transitions; stale/empty views can reduce the stop to one page and remove the markers. Their deliberate low density explains the isolated coloured dots visible near the bottom-left. No normal-renderer code was changed.


## Typography extension

A is the unchanged production 5×5 black bitmap; B is the same bitmap in white. C is a new original hand-designed **6×7 white bold diagnostic subset** in `include/badge_contrast.h`, covering L, 0–5 and 9 (every character used by the eight samples). It is not a general font and unsupported glyphs are not intended inputs. Stems and counters were designed per glyph rather than generated by pixel dilation: L has a two-column stem/two-row foot, zero keeps a two-column counter, nine keeps its bowl opening, and 1/3 remain distinct. Fixed 7-pixel advance gives L10/L11 a 20-pixel cell span inside the unchanged 29-pixel badge. No resampling, anti-aliasing or changes to normal glyph masters.

Existing candidates inspected: the current 5×5 font in `lib/transit/src/font.h`; sibling `scripts/concept/typography.ts` lightweight glyphs; `scripts/preview/pixels.ts` 9×9 detail masters; and previously evaluated `scripts/concept/fonts/{4x6,tom-thumb}.bdf`. The 4×6 and Tom Thumb files identify medium weight and target compact/light typography; lightweight glyphs are not bold candidates. The 9×9 detail system uses 12-pixel advances, so L10/L11 cannot fit the badge without altering its spacing/assets. None was copied into this extension. The existing 5×5 face remains the control; the new 6×7 subset is original repository work, with no third-party font data, new dependency or license introduced. Repository licensing was not changed. The sibling BDF attribution/license notes remain there; those permissions are not needed for this original subset.

**Compromise:** C changes glyph size and width as well as weight; this is not a controlled same-size weight-only comparison. The badge/background stays identical, with two blank rows above/below bold and three above/below regular. Earlier 29×11 experimental badges remain taller than normal seven-row production badges. A preferred bold result therefore does not establish that it fits production layout unchanged. Evaluate recognition and stroke/counter clarity, not just visual density. Compare all three pairs at the same brightness, distance, angle and ambient light, swap sides, and record tie/neither outcomes too. The panel may bloom adjacent white pixels even where a digital raster counter is open.

All old controls remain: 0–7 selects backgrounds, n/p navigates, s swaps sides and i reports the active faces after USB reconnect. a/b/c changes only the comparison pair. No automatic cycling or brightness change.
