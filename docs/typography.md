# Physical typography comparison (2026-10-21; C2 numerals/punctuation/Q update 2026-10-22)

Isolated `hub75-typography` PlatformIO environment for the Waveshare P2.5 64×32
panel. It uses PrototypeDisplay, the unchanged HUB75 driver/GPIO mapping, existing
OPI/PSRAM build configuration and brightness **16/255**, compile-time checked.
No Wi-Fi, feed client, live data, automatic cycle, brightness controls or settings
storage. Nothing has been flashed as part of preparing this update.

**Current focused experiment:** use samples **6–9** with `a` (operational A), `c`
(original C) and `e` (C2). Startup is A on sample 6. These samples preserve the
operational countdown raster identically across faces, isolating label typography.
Original samples 0–5 and their C glyph/countdown rendering remain unchanged.
Use `f` for the complete alphabet (10–12), `g` for added Barcelona destinations
(13–16), and `h` for Q/numerals/punctuation (17–26), then `n`/`p` to navigate
all 27 samples.
B/D remain accessible. The user reported B/C/D as more elegant but less readable
around 3 m on the physical panel. The user subsequently reported the original
C2 subset as the best appearance/readability balance so far; the new alphabet
was subsequently physically tested and looks good overall. This update adds
missing label numerals/punctuation and corrects Q; these new changes have not yet
been physically tested. Former question marks were missing glyphs, not failed designs.

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
| e | C2: geometric letters/digits/punctuation; three-pixel word-space advance |
| 0–9 | Directly select an existing sample, listed below |
| f | Select first alphabet group (10); n/p traverses 10–12 |
| g | Select first added destination (13); n/p traverses 13–16 |
| h | Select Q/digits/punctuation tests (17); n/p traverses 17–26 |
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
| 6 | Lesseps | L3 | Lesseps |
| 7 | Lesseps | L3 | La Pau |
| 8 | Lesseps | L3 | Trinitat Nova |
| 9 | Lesseps | L3 | Zona Universitària |
| 10 | Lesseps | L3 | ABCDEFGH |
| 11 | Lesseps | L3 | IJKLMNOPQ |
| 12 | Lesseps | L3 | RSTUVWXYZ |
| 13 | Lesseps | L3 | Bon Pastor |
| 14 | Lesseps | L3 | Collblanc |
| 15 | Lesseps | L3 | Badalona Pompeu Fabra |
| 16 | Lesseps | L3 | Gorg |
| 17 | Lesseps | L3 | QU QR QO |
| 18 | Lesseps | L3 | 01234 |
| 19 | Lesseps | L3 | 56789 |
| 20 | Lesseps | L3 | Aeroport T1 |
| 21 | Lesseps | L3 | Aeroport T2 |
| 22 | Lesseps | L3 | Plaça d’Espanya |
| 23 | Lesseps | L3 | Pg. de Gràcia |
| 24 | Lesseps | L3 | Sants/Estació |
| 25 | Lesseps | L3 | Can Marcet - Horta |
| 26 | Lesseps | L3 | O0 S5 I1 |

Samples 6–26 deliberately use one fixed header and L3 badge to hold content and
colour constant when switching faces. Their destination labels are reading stimuli,
including a self-name and La Pau, not claims about L3 service patterns.

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
- In original samples 0–5, as in the browser bitmap comparison, first-arrival digits use five-column
  COUNTDOWNS; second-arrival digits use compact FACES digits. Units use compact
  letters. The firmware has **one combined arrival field**, while the browser
  concept has separate first/second anchors. Natural advances change the ink
  positions, but the firmware's badge, available slot and right safe edge do not
  move. Both fixed values fit every sample in every variant.
- Firmware keeps one direction per page, a 53-pixel name slot at (10,2), a
  61-pixel destination slot at (2,21), and the existing arrival row at y=12.
  The browser study's two directions, thin rail and plain line identifier are
  deliberately not ported. Only three five-row text rectangles are redrawn for
  B/C/D in samples 0–5. Focused samples 6–26 replace only the name/destination
  slots; all countdown pixels remain bit-identical to A. C2 always uses this
  labels-only policy. Badge/background/separator/border pixels remain unchanged.
- The **existing firmware pixelText policy** applies to every variant: uppercase;
  common Catalan/Spanish Latin-1 accents and cedilla transliterate; combining
  accents are dropped; unknown Unicode codepoints become one `?`. It supports
  Ñ too. Browser compactText uses broader Unicode decomposition, converts curly
  apostrophes and en/em dashes to ASCII; firmware converts those punctuation
  codepoints to `?`. C2 now explicitly maps U+2019 to its apostrophe glyph
  before calling pixelText; A/B/C/D keep their original policy. New C2 fixtures
  exercise that curly apostrophe without modifying stored text or the production
  normalizer. Other curly punctuation/en/em dashes still report missing. Comma,
  exclamation and plus are operational glyphs but absent from the browser compact
  subset: B/C/D show its `?` fallback. None occurs in the samples. There is no new
  Unicode policy or mutation of stored labels.
- Firmware reveal uses the existing 700/600/700/600 ms eased traversal and true
  per-face width. Browser concept uses two-second endpoint holds and 150 ms
  integer steps. Paused phases preserve repeatability rather than matching the
  browser animation clock. Compare each face at the initial and endpoint phases;
  the same clock position can expose different substrings due to different widths.

## Physical method and findings

The user physically tested the original diagnostic and reported worse distance
readability for B/C/D despite preferring their contemporary appearance. This is
reported physical evidence, not a controlled recognition score. No new C2 physical
readability result is claimed before the operator uploads/runs this update.
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


## C2 construction, complete alphabet and measurements — 2026-10-22

Inspected the existing firmware/browser definitions and sibling measured report
`docs/typography-readability-2026-10-22.md` before implementation. C2 is isolated in
`include/typography_c2.h`; the imported C masks and importer are unmodified.
It is a manually drawn derivative of the local geometric experiment, with no
third-party outline font or additional licensing claim/dependency. No scaling,
dilation or uniformly bold rendering produces its masks.

**Coverage:** all uppercase A–Z and blank space. Original A/E/I/L/N/O/P/R/S/T/U/V/Z
masks are preserved byte-for-byte. All subsequently validated capitals are also
unchanged except the explicitly corrected Q described below. Added B/C/D/F/G/H/J/K/M/Q/W/X/Y, using the
original geometric C as the primary design reference. B/D retain cut corners;
C/F/G/H continue square strokes and counters; J/K/X/Y are manually placed hooks,
forks and diagonals. Q uses an in-cell tail because capital height remains five
pixels. M/W have five-column interiors to avoid compressing their inner strokes.
No automatic scaling, dilation, unrelated font substitution or uniform bolding.

Existing firmware lowercase-to-uppercase and Catalan/Spanish accent folding
remains: Ç→C, Ñ→N, À/Á→A, etc.; no separate lowercase/diacritic drawing is added.
Original UTF-8 strings remain intact in data and the serial report.

**Completed label coverage:** A–Z, 0–9, blank space, ASCII apostrophe `'`,
U+2019 right apostrophe `’`, ASCII hyphen `-`, period `.` and slash `/`. The four
punctuation masks are newly drawn within C2, not borrowed from a different font.
The existing compact `?` remains the visible fallback. Countdown/badge rendering
continues to use the unchanged operational font.

**Normalization before bitmap rendering:** all C2 label measurement, drawing and
missing-character audit use `c2Text`. It explicitly recognizes the three UTF-8
bytes of **U+2019 only**, mapping them to the same apostrophe bitmap used by ASCII
`'`. It then delegates to unchanged `transit::pixelText`, which uppercases ASCII,
folds common precomposed Latin-1 accents (À/Á→A, Ç→C, Ñ→N, etc.) and removes
U+0300–U+036F combining marks. Thus Plaça d’Espanya reaches the bitmap loop as
`PLACA D'ESPANYA`, and decomposed Gràcia as `GRACIA`. This is a narrow rendering
policy, not general Unicode transliteration. Original UTF-8 strings remain intact
in samples and serial output. C2's serial report announces this policy. A/B/C/D
and the production normalizer are unchanged: their U+2019 still becomes `?`.

**Remaining unsupported:** comma, colon, semicolon, exclamation, plus, brackets,
middle dot and other unimplemented punctuation/symbols; U+2018 left apostrophe,
en/em dashes and unsupported Unicode also remain missing. There is no automatic
straightening of other punctuation or unrelated glyph substitution. Missing C2
characters are reported explicitly as unique characters after normalization, then
visibly drawn as `?`. Unknown Unicode appears as `?` in that list, while the full
original label is printed separately. All current sample labels are now covered
by C2, including digits and punctuation in original samples 0–5.

| Property | A | C | C2 |
| --- | --- | --- | --- |
| Actual capital height | 5 px | 5 px | 5 px |
| Capital ink width | 5 px | 3 px, I=1 | 4 px; I=3, M/W=5 |
| Letter advance | 6 px | 4 px, I=2 | 5 px; I=4, M/W=6 |
| Space advance | 6 px | 1 px | 3 px |
| Typical empty columns between words | 7 | 2 | 4 |
| Focused-sample `2M 9M` raster width | 29 px | 29 px | 29 px |

C2 has single-pixel stems and crossbars. The square O has a deliberately open
2-column × 3-row counter; P/R have 2-column × 1-row counters, constrained by five
rows. A has a flat two-cell apex and two-column upper counter. R's diagonal leg,
N's joins and Z's short middle diagonal are individually placed; V has a single
lower point. I gains bars so it is not a bare one-column stick. None of the 26
masters contains a solid 2×2 block. All are five rows tall and reach both horizontal
edges of their declared ink width.

**Design compromises:** even-width T has a left-of-centre stem and V a slightly
asymmetric taper; exact bilateral symmetry cannot always coexist with a one-pixel
axis in four columns. Short P/R counters remain vertically cramped. I uses three
columns to keep its stem centred. The alphabet is complete, but C2 is not a complete transport font until its
numerals/punctuation are designed and physically validated. M/W intentionally
consume one extra column instead of thickening/compressing their interior joins.
C2 deliberately combines wider proportions and larger word spaces per the request;
a preference alone cannot identify which change caused it.

Full text widths measured by the actual C++ drawing functions on an unclipped
framebuffer (all measurements in native pixels):

| Label | A | C | C2 | C2 overflow in 61 px destination slot |
| --- | ---: | ---: | ---: | ---: |
| LESSEPS | 41 | 27 | 34 | 0 |
| LA PAU | 35 | 20 | 27 | 0 |
| TRINITAT NOVA | 77 | 44 | 60 | 0 |
| ZONA UNIVERSITARIA | 107 | 62 | 84 | 23 |

C2 LESSEPS is 34 px in the existing 53 px header slot. These are actual ink
extents, also checked against the width functions by native tests; they are not
fit-to-width scaling. Use `s` twice from `r` for the 1600 ms reveal endpoint.
Start comparisons with samples 6/7, which fit all three faces without motion;
then use 8/9 to assess density, word separation and reveal. Keep approximately
3 m distance, viewing angle, lighting and brightness 16 unchanged, vary order,
and record transcription errors/time and preference/tie.

**Countdown control:** A is still the direct operational renderer. On focused
samples C uses its unchanged original letter masks but the production countdown
pixels are left untouched, as they are in C2. This intentionally differs from
C's original samples, whose compact second digit/units remain available at 0–5.
No operational renderer code or badge font is modified to achieve this control.

Native tests cover original C raster hashes from before this update, operational
A equality, palette consistency, changed-pixel bounds, focused countdown equality,
C2 masks/counters/coverage, actual widths/full reveal and direct serial switching.
Hardware build outcomes are recorded in `docs/phase-1.md`; no upload is run by
these tests/builds, and no physical C2 readability result is asserted.


### Alphabet completion measurements and controls

The old four comparison widths above are unchanged. Additional measured C2 widths:

| Label | Native ink width | Reveal required in 61 px slot |
| --- | ---: | ---: |
| ABCDEFGH | 39 | No |
| IJKLMNOPQ | 44 | No |
| RSTUVWXYZ | 45 | No |
| Bon Pastor | 47 | No |
| Collblanc | 44 | No |
| Badalona Pompeu Fabra | 101 | 40 px |
| Gorg | 19 | No |

`e`, then `f`, shows the first alphabet group; two `n` commands expose the rest.
`g` jumps to Bon Pastor, followed by Collblanc, Badalona Pompeu Fabra and Gorg
through `n`. Use `r` to reset reveal and `s` twice to reach the endpoint for long
labels. Face changes a/b/c/d/e retain the selected sample and reveal phase.
Digits 0–9 still select only samples 0–9; punctuation commands cannot accidentally
select sample indices above nine. `i`/`?` repeats the active sample index and help.

New native checks preserve the original 13 C2 masks/widths, whole-frame A/B/C/D
snapshots across old samples, complete unique A–Z lookup, remaining missing
character reporting, group alphabet coverage, measured proportional widths,
countdown invariance and expanded serial navigation. Build/test results are
recorded in `docs/phase-1.md`. No flash or new physical glyph validation is
performed by this code change.


### Label numerals, punctuation and precise Q correction

New numeral masters follow the original geometric C construction at four-column
ink/five-pixel advance, except 1 uses three-column ink/four advance. All are five
rows tall with one-pixel stems. Zero has clipped corners so it differs from the
validated square O; 5 has shortened middle/bottom terminals so it differs from S.
These are deliberate per-glyph designs, not scaling/dilation/uniform bolding.
Sample 26 compares O0, S5 and I1 directly. Recognition under LED bloom remains a
physical question; no automatic substitution of these pairs is performed.

Punctuation uses natural ink widths with the same one-pixel inter-character gap:
apostrophe 2 columns/3 advance (two upper diagonal pixels), hyphen 3/4 (middle row),
period 1/2 (baseline row), slash 3/4 (five-row diagonal). Existing capital metrics,
including Q's 4-column ink/5 advance, and three-pixel space advance are unchanged.

Q changes **only** from `[15, 9, 9, 11, 1]` to `[15, 9, 9, 15, 2]`:

```text
Before     After
####       ####
#..#       #..#
#..#       #..#
#.##       ####
...#       ..#.
```

Using one-based bitmap coordinates: add row 4/column 2 to close the bowl; clear
row 5/column 4 and set row 5/column 3 to move the tail left one column. The top
three rows, five-row height and all other 25 validated capitals are unchanged.
The corrected bowl has a 2-column × 2-row closed counter above its tail.

New sample widths, measured by the actual C++ renderer without clipping:

| Sample | Label | C2 ink width |
| --- | --- | ---: |
| 17 | QU QR QO | 35 |
| 18 | 01234 | 23 |
| 19 | 56789 | 24 |
| 20 | Aeroport T1 | 51 |
| 21 | Aeroport T2 | 52 |
| 22 | Plaça d’Espanya | 70 |
| 23 | Pg. de Gràcia | 56 |
| 24 | Sants/Estació | 62 |
| 25 | Can Marcet - Horta | 83 |
| 26 | O0 S5 I1 | 33 |

`e`, `h` selects C2 on the Q test. `n`/`p` steps through these samples, `s` steps
paused reveal and `r` resets it. Other face/sample shortcuts remain unchanged.
Counted values stay operational 2M/9M at brightness 16/255 on all new samples.
No normal renderer, backend/Feed v1, colour or hardware configuration is changed.
Native regressions check the exact three-pixel Q edit, unchanged other capitals,
whole-frame A/B/C/D equality across all 17 previous samples, numeral/punctuation
coverage, explicit apostrophe handling, normalization, actual widths, period
baseline, existing clipping/reveal and countdown equality. Hardware/build outcomes
are recorded in `docs/phase-1.md`. No device is flashed during validation.
