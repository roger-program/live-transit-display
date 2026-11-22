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

**Validation update — 2026-11-23:** restored the ignored local
`include/secrets.h` using the documented feed origin and the public ISRG Root X2
certificate from the local OpenSSL trust bundle. The CA fingerprint matched the
previously documented value; Wi-Fi fields remain empty and the header is mode
0600. `hub75-live` built successfully with PlatformIO. The binary contains the
configured feed origin, the build dependency confirms it included the local
header, and the LED preview receiver object and `/api/preview` route are present.
Firmware image: 978,336 bytes; SHA-256
`8ea4e7272b302c476baa7cb2660c94d705087a4a6b7fc9b6a4c8f0c77334a6f1`. No flash,
device access, deployment or production changes were performed. Physical
rendering, timeout behavior and the browser request remain unverified.

**Central tracking pending:** `live-transit-feed/PROJECT_STATUS.md` is readable
but outside this session's writable workspace. Reconcile its **Current Focus /
Next Action** to record the successful configured build and its size/hash, and
that physical validation remains pending. Do not create a duplicate central
tracker here.
