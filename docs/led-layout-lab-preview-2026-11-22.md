# LED Layout Lab firmware preview — 2026-11-22

Implemented a local-only receiver for the static editor in the sibling
`live-transit-feed/experiments/display-visual-redesign-2026-11-22/` experiment.
The endpoint and wire format match its existing controls:

- `POST http://led-display.local/api/preview`
- Request body: exactly 6,144 binary bytes, row-major RGB888 for 64 × 32 pixels
- Required headers: `Content-Type: application/octet-stream`, `X-LED-Width: 64`,
  `X-LED-Height: 32`, and `X-LED-Format: RGB888`
- `POST /api/preview/stop` immediately clears the temporary frame and restores the
  regular live renderer

The receiver accepts the editor's `Origin: null` file-origin requests and its
CORS preflight. It stores the submitted image in RAM, overlays it after normal
arrival rendering, and restores live arrivals after 30 seconds or the stop call.
The server is available only while connected to Wi-Fi and outside Wi-Fi setup;
frame dimensions/headers are validated, no frame is persisted, and no feed or
backend behavior changes.

**Validation:** source inspection and `git diff --check` completed. The requested
`hub75-live` build could not run because PlatformIO (`pio`) is not installed in
this environment. The repository formatter also could not run because
`clang-format` is not installed. No tests, flash, device access, deployment or
production changes were performed. Physical rendering, timeout behavior and the
browser request remain unverified.

**Central tracking pending:** `live-transit-feed/PROJECT_STATUS.md` is readable
but the repository rejects writes in this session. Reconcile its **Current Focus /
Next Action** with this local firmware implementation and build limitation when
the feed repository is writable. Do not create a duplicate central tracker here.
