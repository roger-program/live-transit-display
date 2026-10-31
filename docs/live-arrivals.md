> Historical shared-token experiment. Superseded for live firmware on 2026-10-31 by [device-feed.md](device-feed.md). Do not follow the compiled token or stop-selection instructions below.

# First physical live-arrivals test — 25 October 2026

## What is implemented

Use the existing Waveshare ESP32-S3 RGB Matrix controller, HUB75 P2.5 64×32 panel,
USB-C data connection and tested power/wiring. The new `hub75-live` environment
adds physical panel output to the existing normal live mode. Pin mapping, driver,
C2 bitmap typography, line badges/colours, countdowns, pages and configured
brightness are unchanged. Fixture, badge, typography and hardware diagnostics
retain their own environments and entry points.

The existing background task connects to Wi-Fi, synchronizes UTC using SNTP and
requests the backend's normalized `/v1/arrivals` with repeated stop selectors.
Operator credentials and provider formats remain exclusively in the backend.
The device validates the HTTPS certificate/root and Bearer configuration, refuses
redirects, bounds the response to 64 KiB and uses a 10-second HTTP I/O timeout.
This timeout is not a guaranteed wall-clock bound for the entire multi-read TLS
transaction; network work runs separately from the 20-fps display loop.

Polling remains 30 seconds after completion; failures wait at least 60 seconds,
auth failures five minutes, and numeric Retry-After may extend the delay to one
hour. Reconnection does not bypass an existing server backoff. The inspected
backend has no arrivals cache or refresh hints; these are retained prototype
intervals, not a statement of operator quotas. Stop status and absolute UTC
arrival times retain their Feed v1 meanings; missing optional colour/provenance
fields retain existing fallbacks.

## Endpoint decision (before connecting)

Use an already deployed, trusted HTTPS origin if available. Set only its origin,
for example `https://live-transit-feed.roger-80a.workers.dev`, with no path, trailing slash, query or
credentials. The client builds `/v1/arrivals?stop=...` itself. Its Bearer token
must match that backend's FEED_API_TOKEN; never copy TMB_APP_ID/TMB_APP_KEY into
the firmware. The backend currently uses a shared experiment token, not a
provisioned per-device credential.

The sibling Node server listens on `127.0.0.1:8787`; the ESP32 cannot reach it via
localhost. The user-supplied HTTPS deployment was verified on 25 October 2026 (see the deployment gate below). **If only the local
server is available, stop here to agree on access before the first live test.**
The smallest compatible local approach is a temporary TLS reverse proxy on the
Mac to that loopback server, with a certificate trusted by `RootCa` and a hostname
reachable from the ESP32. Both Mac and ESP32 must join the phone hotspot and the
phone must permit peer traffic; its implementation may isolate clients. A
certificate must match the requested hostname (an IP requires an IP SAN).
This approach needs certificate/hostname setup and explicit local network
exposure. A temporary HTTPS tunnel is another option, but exposes the feed
through an external service and needs a separate decision. Plain LAN HTTP would
require changing the current HTTPS-only client and exposes its Bearer token on
the LAN; it is not enabled here. Nothing was deployed, tunneled, rebound or
exposed automatically.

## Configure, build, flash and observe (macOS)

1. Open the firmware repository and activate its existing environment:

   ```sh
   cd /Users/rogerpau/mamp/live-transit-display
   . .venv/bin/activate
   ```

2. If `include/secrets.h` does not exist, copy `include/secrets.h.example` to it.
   Edit it locally with your editor; do not paste credentials into chat or shell
   commands. It is already ignored. Confirm without printing its contents:

   ```sh
   git check-ignore include/secrets.h
   ```

   Set `WifiSsid` and `WifiPassword` for a 2.4 GHz hotspot. Enable the phone's
   2.4 GHz/compatibility mode where offered. Set `FeedOrigin`, `BearerToken` and
   `RootCa` (the trusted PEM root for that HTTPS service). Obtain the root through
   a trusted channel; do not disable certificate verification. Keep the phone
   hotspot active and allow internet access for SNTP and the chosen endpoint.
   Credentials are compiled into flash: keep secrets and built binaries private.

3. Set ordered selectors in `include/app_config.h` (`appcfg::Stops`). The existing
   `metro:6660122` and `bus:94` request Espanya Metro and bus stop 94. These are
   configurable example selections, not renderer routing rules. Preserve the
   working panel/brightness settings. Build and run host checks without flashing:

   ```sh
   python tools/generate_fixtures.py --check
   pio test -e native
   pio run -e hub75-live
   ```

4. Connect the existing controller over USB-C with the panel's tested power
   arrangement. Detect the actual port now:

   ```sh
   pio device list
   ```

   Compare with the controller unplugged/reconnected if ambiguous. Do not assume
   a previously used port. Assign the discovered port locally:

   ```sh
   PORT='/dev/cu.REPLACE_WITH_DETECTED_PORT'
   ```

5. When ready to flash the physical device yourself, close any serial monitor:

   ```sh
   pio run -e hub75-live -t upload --upload-port "$PORT"
   pio device monitor --port "$PORT" --baud 115200
   ```

   Reset after opening the monitor to see startup if needed. Firmware boot should
   report `panel=enabled` and the existing flash/PSRAM detection. Existing serial
   brightness commands `b4`, `b8`, `b16`, `b32` plus Enter still apply; reset uses
   `appcfg::Brightness` (currently 4). No port discovery or upload was performed
   automatically during development.

6. Expect `Wi-Fi connected`, then `Feed OK; generatedAt=...; stops=...;
   arrivals=...; realtime=...`. A startup `OFFLINE` or `CLOCK` while connecting or
   synchronizing is normal. Logs omit SSIDs, passwords, tokens, URLs, response
   bodies and raw SDK exceptions. `generatedAt` should advance on later fetches;
   the realtime count must be positive to establish that this response contains
   realtime predictions. A successful response with zero realtime entries can
   legitimately be empty, unavailable or scheduled. Check per-stop display status
   and compare the same selected stops with the existing live backend preview.
   Watch Metro and Bus pages cycle, countdowns decrease between fetches, expired
   entries disappear, and feed-provided badge colours remain unchanged.

7. Turn the hotspot off briefly, then on. During an interruption, valid cached
   pages remain until the original freshness deadline; one second every five
   displays `OFFLINE` (or the relevant request error). This uses the existing
   status screen and does not change arrival-page geometry. Predictions continue
   expiring locally. At 120 seconds from either generation or receipt, retained
   stops show `STALE` with predictions suppressed. Successful retrieval clears
   the error and replaces the snapshot. Observe recovery after reconnection,
   accounting for remaining request backoff. Record test times, selected stops,
   state transitions and limitations in `docs/phase-1.md`; do not save secrets.

## Distinguishing failures

| Serial/display state | Interpretation and next check |
| --- | --- |
| CONFIG | Missing/invalid Wi-Fi, origin, Bearer, CA or stop-count configuration; check ignored configuration locally |
| OFFLINE before Wi-Fi connected | Association is pending/failed; check 2.4 GHz hotspot, credentials, range and phone settings |
| CLOCK after Wi-Fi connected | Waiting for usable SNTP UTC; check hotspot internet/UDP time access; no arrivals are shown with an invalid clock |
| OFFLINE after Wi-Fi connected | HTTP transport/DNS/TLS failure is possible; check origin reachability, certificate/root/hostname and internet; SDK details are intentionally suppressed |
| AUTH | Backend returned HTTP 401/403; check feed token locally |
| RETRY | Backend returned HTTP 429; respect backoff |
| FEED ERR | Other non-success HTTP response, including redirects and server errors; inspect backend health separately |
| BAD FEED | Malformed, incompatible or oversized response; check normalized Feed v1 at the backend |
| Feed OK, but UNAVAIL/STALE/NO PRED | Transport succeeded; inspect per-stop source availability/freshness/selection rather than Wi-Fi |

## Validation limits

Native tests simulate Wi-Fi interruption through the same `Network` snapshot
transition used by the task; they do not emulate the radio, SNTP, TLS or GPIO.
Parser/HTTP policy tests and existing raster regressions remain offline. Physical
hotspot association, certificate chain, real upstream availability, heap headroom
and uninterrupted panel refresh still require the steps above. No live operator
requests, deployment or device flashing are part of automated verification.

## Historical deployment gate — earlier on 25 October 2026

The selected origin is `https://live-transit-feed.roger-80a.workers.dev`.
Authenticated Metro, Bus and mixed requests succeeded; both upstream diagnostics
reported HTTP 200 and predictions changed across reads about 47 seconds apart.
The actual firmware parser accepted the normalized live results. No upstream
connectivity failure was reproduced during this short observation.

However, this deployment omits arrival provenance, realtimeStatus, lineColor and
lineTextColor that the current backend checkout emits. Legacy compatibility
works, but the device will correctly show unverified `?` countdowns and fallback
badge colours. Do not infer realtime provenance or hardcode missing colours in
firmware. Before the intended live physical test, have the backend owner update
the deployment from the intended reviewed revision, then verify explicit
realtime provenance and selected colours. No deployment or flash was performed.
Until this difference is resolved, stop before the upload step above unless an
explicit legacy-response test is chosen. Sanitized request summaries are in the
sibling feed repository at `docs/evidence/deployed-feed-2026-10-25/`.


## Historical pre-flash check — before prototype exception, 25 October 2026

The historical backend-field gap above is resolved: production version
`2bd64122-96d6-4048-ac55-780d9842e1f6` was verified with explicit provenance,
realtimeStatus and selected Metro/TMB Bus colours. See the sibling feed
repository's production-alignment evidence. The current blockers are local:

- `include/secrets.h` is absent. Copy the ignored example locally, restrict its
  permissions, and set hotspot SSID/password, FeedOrigin, BearerToken and RootCa.
  FeedOrigin is `https://live-transit-feed.roger-80a.workers.dev`; BearerToken is
  the existing backend FEED_API_TOKEN value, without the `Bearer ` prefix.
- The pinned Arduino SDK has `CONFIG_MBEDTLS_HAVE_TIME_DATE` disabled. Its
  compiled certificate past/future check functions return zero unconditionally.
  NTP gating is present but does not activate these disabled library checks.
  Resolve certificate validity-period enforcement before declaring readiness;
  adding an application compiler flag cannot rebuild the precompiled library.
  Do not disable chain/hostname validation or change the renderer to work around it.

Native tests: 51 passed; hub75-live build, formatting and fixture checks passed.
USB enumeration detected `/dev/cu.usbmodem3101` (Espressif 303A:1001); no port was
opened and no device flashed. Re-enumerate if reconnected. The endpoint's
hostname/chain was verified from the Mac, including TLS 1.2 using only ISRG Root
X2; this does not verify ESP32 runtime behavior. The public self-signed root PEM
is available from https://letsencrypt.org/certs/isrg-root-x2.pem; paste its entire
PEM into the existing RootCa raw string. Root SHA-256:
`69729b8e15a86efc177a57afb7171dfc64add28c2fca8cf1507e34453ccb1470`.

Stop before the upload step until configuration and certificate-date checks are
resolved and revalidated. Actual Wi-Fi association, SNTP synchronization and
ESP32 TLS remain hardware-test observations, not host-test results.


## Current prototype configuration — 25 October 2026

The user explicitly accepts the installed framework's missing certificate-date
checks **only for the first physical prototype test**. This supersedes the
historical date-check upload blocker above. Full validity-period verification
remains an unresolved, mandatory production security requirement. The framework
was not rebuilt and no insecure TLS mode was enabled. Certificate-chain and
hostname checks, trusted root configuration and NTP-before-HTTPS remain intact.

`include/secrets.h` is now created from the existing example, ignored/untracked
and mode 0600. The existing HTTPS origin and official ISRG Root X2 PEM are set.
The root was downloaded from Let's Encrypt, matched the previously verified
SHA-256 fingerprint and successfully validated the endpoint's chain and hostname
with TLS 1.2 from the Mac. Do not mistake this for an ESP32 runtime test.

Fill these three empty values locally before uploading:

- WifiSsid: exact phone hotspot name; use its 2.4 GHz/compatibility setting.
- WifiPassword: the hotspot password.
- BearerToken: the existing backend FEED_API_TOKEN value, without `Bearer `.

No private value was printed, generated, copied from backend secrets, committed
or uploaded. Empty SSID/token deliberately fail configuration rather than using
a dummy credential. The header and compiled firmware will contain credentials
once you fill them; keep both private.

After saving valid local credentials, build/upload manually from the firmware
repository (close any serial monitor before upload):

```sh
. .venv/bin/activate
pio run -e hub75-live -t upload --upload-port /dev/cu.usbmodem3101
pio device monitor --port /dev/cu.usbmodem3101 --baud 115200
```

The port was re-enumerated for this preparation, not assumed from a prior run.
Recheck `pio device list` if USB is reconnected. No upload/reset/monitor was run
automatically. Until credentials are filled, the current build shows CONFIG.
After valid configuration expect Wi-Fi connected, optional CLOCK during SNTP,
then Feed OK summaries with generation time and realtime counts. The existing
Espanya Metro and Bus 94 pages, C2 type, badges/colours, paging, countdowns and
brightness 4/255 are unchanged. Runtime hotspot, SNTP, TLS and LED behavior remain
the physical-test milestone.
