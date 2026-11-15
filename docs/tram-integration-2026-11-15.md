# TRAM identity — local validation, 15 November 2026

Local implementation only. No flash, serial access, device change or physical
validation. Backend/source/activation scope lives in the central
[TRAM record](../../live-transit-feed/docs/tram-integration-2026-11-15.md).

Added `TRAM` to the existing operator symbol registry: provisional 5×5 white T
on teal `#007e84`, centred in the established 7×7 header slot. Native line colours
continue arriving through Feed v1. No parser, renderer, layout, scrolling,
provenance policy, Wi-Fi, device identity or partition changes. This glyph/palette
is a prototype choice, not official brand approval.

A targeted host test parses mode `tram` and operator `TRAM`, renders the symbol
with the standard renderer and checks every pixel in/outside the header slot.
The prior FGC fixture's registry-size assertion now expects two operators.

Validation: `.venv/bin/pio test -e native`: **77/77 passing**;
`.venv/bin/pio run -e hub75-live`: success, **954,137 bytes** application
(22.7% of application capacity). Both edited C++ files clang-formatted;
`git diff --check` passes. Local build only; image was not flashed or physically
accepted. Existing generic v1 behaviour is retained, including the scheduled S
marker policy (suppression remains FGC-only).

Next: separately authorised application-only flash following existing safety
procedures, then physical mixed-operator symbol/destination/countdown checks.
No independent central tracking files are created here.
