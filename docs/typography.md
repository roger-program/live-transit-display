# Physical typography comparison (2026-10-21)

Isolated `hub75-typography` PlatformIO environment for the Waveshare P2.5 64×32
panel. It uses PrototypeDisplay, the unchanged HUB75 driver/GPIO mapping, existing
OPI/PSRAM build configuration and brightness **16/255**, compile-time checked.
No Wi-Fi, feed client, live data, automatic cycle, brightness controls or settings
storage. Nothing has been flashed as part of preparing this diagnostic.

## Build, upload and serial controls

From `/Users/rogerpau/mamp/live-transit-display`:

```sh
source .venv/bin/activate
python tools/generate_typography.py --check
python tools/generate_fixtures.py --check
python tools/format.py --check
pio test -e native
pio run -e hub75-typography
```

The importer check requires the existing sibling `live-transit-feed` directory;
normal builds and native tests use the checked-in generated header independently.
No additional package is required. To deliberately refresh the browser snapshot,
run the importer without `--check` and review its diff. Sources/hashes are included
in `include/typography_glyphs.h`.

**Upload instructions for the operator; not run during this task:**

```sh
pio device list
pio run -e hub75-typography -t upload --upload-port /dev/cu.usbmodemXXXX
pio device monitor --port /dev/cu.usbmodemXXXX --baud 115200 --dtr 1 --rts 0
```

Replace the example port with the enumerated controller port. Uploading replaces
the running application; the production build sources and other environments are
preserved. To restore the operational application later, upload its normal
`esp32s3` environment using the project's established procedure.

Single-character commands (lowercase, no Enter needed):

| Command | Action |
| --- | --- |
| a | A: actual operational 5×5 typography (startup default) |
| b | B: browser contemporary bitmap |
| c | C: browser geometric bitmap |
| d | D: browser condensed bitmap |
| 0–5 | Select a sample, listed below |
| n / p | Next / previous sample, wrapping |
| s | Next paused reveal phase: 0, 1000, 1600, 2300 ms, then 0 |
| r | Return to initial reveal hold (0 ms) |
| i / ? | Repeat current face, sample, reveal position, brightness and help |

Selections retain the same sample and reveal time when changing fonts. No glyph
label is added to the panel because that would consume layout space: the serial
report identifies the active face. USB reconnect may hide the startup report;
use `i`. Unknown characters/newlines are ignored. All selections are RAM-only.
Times remain **2M and 9M** so delay while judging a face cannot change the content.
Reveal uses the existing firmware easing/timing; `s` deliberately freezes useful
positions instead of automatically moving/paging the display.

| Sample | Stop | Badge | Destination |
| --- | --- | --- | --- |
| 0 | Lesseps | L3 | Zona Universitària |
| 1 | Passeig de Gràcia | L4 | Trinitat Nova |
| 2 | Collblanc | L9S | Aeroport T1 |
| 3 | Plaça d'Espanya | H8 | Parc de la Vall d'Hebron |
| 4 | Pg. Marítim - Hospital del Mar | V23 | Can Marcet - Horta |
| 5 | Gran Via - Balmes | 6 | Pg. Manuel Girona |

These are realistic Barcelona names and synthetic comparison inputs, not verified
current line/destination pairings or arrivals. Full names remain in the sample
model and serial report. Existing RGB values are copied from the current reviewed
firmware/backend policy: L3 #1EB53A/white, L4 #F7A30E/black,
L9S #FB712B/black, H8 #2D4191/white, V23 #547B31/white,
6 #C62A0F/white. Operational mode badges remain M red / B red with regular white
lettering. The previous badge-contrast experiment is not mixed into this study.
These offline colours are diagnostic inputs, not a new production lookup table.

## Baseline and browser differences

Inspection covered `lib/transit/src/{renderer.cpp,font.h}`, `panel_config.h`,
existing PlatformIO environments, the prior 6×7 bold badge diagnostic and its
hardware notes, plus sibling `scripts/concept/{render.ts,typography.ts,bdf.ts}`,
BDF references/notices and `identity/{fonts.ts,build.ts,README.md}`.

- **A is the true operational renderer**, called directly on a 64×32 framebuffer.
  It is not the browser study's compact 3×5 A. The production 5×5 masks, six-pixel
  advance, RGB colours, filled badges, spacing, right alignment, clipping and
  reveal remain unchanged. The normal renderer file is not edited.
- B/C/D use the **exact browser FACES masks** for A–Z and 0–9, compact punctuation
  from concept/render.ts and **exact COUNTDOWNS masks**. No BDF, outline font,
  scaling, antialiasing, dilation, alternative weight or new glyph is introduced.
  Five rows are plotted directly as binary pixels. These local repository masters
  retain their existing licensing status; no separate third-party font license
  is introduced. The previously examined BDF licenses are not needed here.
- B/C text is normally three columns plus a one-pixel gap, I one column, spaces
  zero ink columns plus a one-pixel advance. D narrows C/E/F/J/L/S to two columns
  using the browser's drawn masks. This is the browser's compact width policy,
  not the operational six-pixel monospaced advance. Text is left-aligned in the
  existing name/destination slots; the countdown field retains its right edge.
- As in the browser bitmap comparison, first-arrival digits use five-column
  COUNTDOWNS; second-arrival digits use compact FACES digits. Units use compact
  letters. The firmware has **one combined arrival field**, while the browser
  concept has separate first/second anchors. Natural advances change the ink
  positions, but the firmware's badge, available slot and right safe edge do not
  move. Both fixed values fit every sample in every variant.
- Firmware keeps one direction per page, a 53-pixel name slot at (10,2), a
  61-pixel destination slot at (2,21), and the existing arrival row at y=12.
  The browser study's two directions, thin rail and plain line identifier are
  deliberately not ported. Only three five-row text rectangles are redrawn for
  B/C/D; badge/background/separator/border pixels remain bit-identical to A.
- The **existing firmware pixelText policy** applies to every variant: uppercase;
  common Catalan/Spanish Latin-1 accents and cedilla transliterate; combining
  accents are dropped; unknown Unicode codepoints become one `?`. It supports
  Ñ too. Browser compactText uses broader Unicode decomposition, converts curly
  apostrophes and en/em dashes to ASCII; firmware converts those punctuation
  codepoints to `?`. Therefore fixtures use ASCII apostrophes/hyphens. Comma,
  exclamation and plus are operational glyphs but absent from the browser compact
  subset: B/C/D show its `?` fallback. None occurs in the samples. There is no new
  Unicode policy or mutation of stored labels.
- Firmware reveal uses the existing 700/600/700/600 ms eased traversal and true
  per-face width. Browser concept uses two-second endpoint holds and 150 ms
  integer steps. Paused phases preserve repeatability rather than matching the
  browser animation clock. Compare each face at the initial and endpoint phases;
  the same clock position can expose different substrings due to different widths.

## Physical method and findings

No physical readability result is claimed before an operator uploads/runs it.
At brightness 16, compare a/b/c at approximately 3 m with unchanged ambient light,
viewing angle and panel setup. Use d only as a density control. Record face,
sample, reveal phase, distance, ambient light, reading accuracy/time, confused
letters/numerals and preference/tie. Repeat in a different order. In particular,
watch square C bowls for bloom and B open terminals for missing-looking strokes.
The browser recommendation of C is a visual hypothesis, not a hardware finding.
A P2.5 five-row glyph is about 12.5 mm high; this does not establish legibility.

Production renderer, feed protocol/backend, data/configuration ownership, existing
diagnostic environments, brightness and panel wiring remain unchanged. This
build adds rendering capabilities only; it does not put ordinary transport data
or colours behind a new firmware-update requirement.
