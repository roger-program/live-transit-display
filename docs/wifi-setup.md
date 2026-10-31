# Local Wi-Fi setup — 25 October 2026

## Implemented behaviour

The existing live firmware now uses a transport-independent `WifiSetup` module.
No new firmware environment, partition, external library or deployment was added.
The renderer, Feed v1 parser, upstream/backend settings, colour policy and isolated
diagnostics are unchanged. Setup overlays instructions using existing drawing
primitives. Normal feed fetching resumes when setup closes.

Saved Wi-Fi credentials take priority. Without a valid saved record, the device
starts a protected `METRO-DISPLAY` AP with prototype password `BCNMETRO`.
On an untouched namespace, if compiled development Wi-Fi credentials are present, it also tests them once and saves them only after a
successful association and DHCP address. This transitional migration keeps existing
development builds usable. Empty compiled Wi-Fi fields are supported; feed origin,
feed authentication and CA configuration still use the existing local header.

The 64×32 matrix uses the existing uppercase C2 font with this fixed layout:

```text
1. WIFI
METRO-DISPLAY
BCNMETRO

2. ABRE WEB
METRO.CONF
```

Measured rows are respectively 27, 63, 40, 48 and 47 pixels wide, each five pixels
high. Row origins are y=0, 6, 12, 21 and 27, occupying exactly 32 pixels. There is
one blank row between lines and four blank rows between steps; no text is scaled,
scrolled or rotated. Credentials match the actual SoftAP values.

Join that AP in phone Settings, accept remaining connected without internet if
prompted, then explicitly open **http://metro.conf/**. If hostname resolution fails,
open **http://192.168.4.1/** (or **http://192.168.4.1:80/**) while still joined to
METRO-DISPLAY. Include `http://` so the browser opens an address rather than a search
or HTTPS connection. No internet, public domain, external backend or redirect is
required. The self-contained page has a
network dropdown, manual SSID, password field and status/retry flow. A background
scan populates the initial list; refresh the page after scanning. A failed
connection triggers another scan. Hidden SSIDs can be entered manually. Exact SSID
duplicates are merged using their strongest RSSI, then ordered strongest first.
The existing limit is 40 unique networks, applied after merging and sorting.
Names differing in case, whitespace or punctuation remain distinct. The password
is hidden by default; an accessible eye button toggles visibility without changing
its value or how credentials are submitted, validated or stored. The inline SVG eye
shows a pupil initially (Show password), and a diagonal strike when visible
(Hide password); button sizing and styling are unchanged.

Form submission returns a complete progress page before the connection is
started on a later network-task tick (at least one second after queueing).
Progress refreshes use GET `/`, never a repeat POST. Connection tests allow 20
seconds from the actual connection attempt. Success means association to the requested
SSID and a nonzero station IP, not internet, DNS or feed availability. The status
page refreshes while testing. After a verified save, arrivals resume automatically
and the browser receives a dedicated Connected! page without a form. For browser
setup, the AP/server close five seconds after that success page is served. If the
phone has disconnected, rejoin the setup AP to receive the page; until then the
AP remains available while arrivals run. Compiled-credential migration has no
browser to wait for and closes the AP after five seconds. AP channel
changes during home-network association can interrupt the phone connection; the
matrix and retry/rejoin path are required physical checks.

Setup and its on-screen credentials remain available until configuration succeeds;
there is no inactivity timeout. Rebooting returns to previously saved credentials
if replacement has not succeeded. A normal network outage
never opens setup or deletes credentials: retries continue every 15 seconds.
Reboot and normal non-erasing firmware writes preserve NVS. Setup requests can be
delayed by an in-flight bounded feed request; drawing remains in the main loop.

## Local hostname and physical verification — 31 October 2026

The previous implementation had no DNS server. Setup now uses DNSServer bundled
with the pinned Arduino-ESP32 framework (no added package). After SoftAP starts,
UDP port 53 resolves `metro.conf` to the setup IP; the library folds case, so
`METRO.CONF` and mixed-case queries work. AP DHCP advertises the setup IP as DNS;
station DNS is untouched. One DNS datagram is processed per network-task tick,
with no polling/wait loop. HTTP Host and Origin checks accept the hostname,
case-insensitively and with optional `:80`, alongside the existing numerical IP.
Tokens, interface checks and relative browser requests remain in place.

DNS remains available during Configuration, Connecting, Result and Retry,
including the existing success-acknowledgement/grace interval. It stops before
HTTP/AP shutdown, and starts again on the next setup session. A DNS startup or
DHCP DNS-offer failure logs a safe fallback message; IP-based HTTP stays enabled.
No wildcard DNS, HTTPS listener, automatic captive-portal redirect or external
redirect is introduced. The bundled DNS library also recognizes a www-prefixed
query, but HTTP deliberately accepts only `metro.conf` and the setup IP.

Physical procedure (pending; do not assume host tests validate mobile DNS):

1. Enter setup mode. Confirm the static five-line layout is readable, especially
   the 63-pixel-wide SSID, with the four-pixel gap between steps.
2. Connect a phone to **METRO-DISPLAY** using **BCNMETRO**. Stay connected when the
   phone reports no internet. Forget/rejoin any cached setup network if necessary
   to renew its DHCP/DNS settings.
3. Open **http://metro.conf/** and confirm Configuration loads. Repeat with
   **http://METRO.CONF/** and a mixed-case hostname.
4. Open **http://192.168.4.1/** and confirm the same interface still loads.
   Use this numerical address as the fallback if the hostname fails.
5. From the hostname page, try an invalid home password, verify Connecting then
   Retry, and confirm the selected SSID remains. Retry with valid home credentials,
   verify Connected, and confirm arrivals resume and the setup AP disappears after
   acknowledgement plus the existing five-second grace interval.
6. Re-enter setup and confirm hostname/IP access works again. Repeat on available
   iOS/Android phones, with mobile data and VPN/private DNS settings noted. From a
   test computer joined to the AP, optionally query its IP directly as DNS before
   and after setup exits to check DNS startup/shutdown and uppercase A queries.

Limits: phones may bypass DHCP DNS through encrypted/private DNS, VPNs or mobile
routing, cache an earlier failure, upgrade HTTP to HTTPS, or leave a network without
internet. The IP fallback avoids DNS but still requires the phone to stay joined.
The pinned DNS library handles simple single-question packets without additional
records and returns IPv4 answers; EDNS/IPv6-specific resolver behavior is unverified.
These changes have been compiled and host-tested, not physically verified.

## Storage and security boundary

The existing default NVS partition is unchanged. Namespace `transit-wifi` stores
version-1 credential blobs in `slot0`/`slot1`, plus the `active` slot key. A candidate
is kept in RAM until its connection succeeds, written to the inactive slot, read
back and compared, then activated with a separate committed slot selector. Failed
association never writes NVS; incomplete candidate writes leave the previous
selected slot in place. The old slot remains after activation. No Wi-Fi erase or
factory-reset endpoint is added. Submitted SSIDs are logged only as bounded,
escaped single-line diagnostics. Passwords and form tokens are never logged or
echoed into HTML. Submitted passwords travel only over the protected local setup
network.

The setup AP uses the shared prototype password `BCNMETRO` and accepts one client.
`SetupSsid` and `SetupPassword` in `lib/transit/src/wifi_setup_screen.h` are the
configurable defaults; use device-specific credentials before production. Choose
uppercase values supported by C2 that fit the fixed rows (SSID: 64 pixels; password:
64 pixels). C2 normalizes lowercase to uppercase, so future
credentials must match their displayed case. The password must be 8–63 bytes.
The random per-session form token remains unchanged.
HTTP binds to the AP address and checks the local destination, Host, Origin and a
random form token; the page is not cached and rejects framing. It uses no CDN,
cloud service or internet assets. This is a prototype WPA-protected HTTP portal,
not production provisioning assurance or encrypted NVS. The bundled WebServer
parser is unchanged; authenticated nearby clients still have its request/parser
resource limits. No TLS settings were changed; the previously documented
certificate-date validation exception remains.

The pinned Arduino core can erase default NVS on some startup initialization
errors. Normal outages/updates do not do this, but this implementation cannot
promise survival of core recovery erasure, flash corruption or an explicit erase.
A separate durable partition/encrypted production store remains future work.
Never auto-erase NVS to recover a failed Preferences open.

## Physical control and temporary recovery

Waveshare documents onboard RST and programmable BOOT controls:
https://docs.waveshare.com/ESP32-S3-RGB-Matrix (checked 2026-10-25).
Waveshare’s official music-player example defines `BOARD_BUTTON_PIN 0`, supporting
the GPIO0 mapping used here:
https://github.com/waveshareteam/ESP32-S3-RGB-Matrix/blob/main/example/arduino_v3.3.7/09_Music_Player/09_Music_Player.ino
(checked 2026-10-25). Verify actual button operation and physical access on the
received PCB during the manual test. It does
not decide an enclosure control arrangement. RESET restarts the board; it is not
used as a settings button. Espressif documents GPIO0 low at reset as download mode:
https://docs.espressif.com/projects/esptool/en/latest/esp32s3/advanced-topics/boot-mode-selection.html
(checked 2026-10-25).

After normal startup, hold BOOT for three seconds (plus about 100 ms debounce)
to enter setup without removing the saved network. No preliminary release is
required by the application handler. Do not hold BOOT while resetting for
this feature: that can enter the ROM downloader instead. If BOOT is inaccessible,
open the development USB serial monitor at 115200 and send `wifi` followed by
Enter. Existing brightness commands remain. Serial recovery does not print AP or
home-network passwords. There is no serial credential-entry interface.

## Preserved build and restoration — instructions only

Before any rebuild, the existing local live firmware, bootloader and partition
binary were copied to ignored `artifacts/wifi-before-change/`. The matching
framework helper, partition CSV, PlatformIO settings and `sha256.json` are also
preserved there. The archived application was 886,448 bytes. This is the existing
local build, not a device readback or proof it exactly matches the flashed image.
The archive can contain embedded private credentials; keep it private and do not
commit or attach it. No flash operation has been run.

Because partitions/bootloader settings are unchanged, restoring only the archived
application at **0x10000** is sufficient for the planned application-only test.
With later explicit authorization, rediscover the correct port, verify archive
hashes and use the pinned esptool, for example from the repository root:

```sh
# Example only: replace PORT with the verified device port. This writes firmware.
.venv/bin/python /Users/rogerpau/.platformio/packages/tool-esptoolpy/esptool.py \
  --chip esp32s3 --port PORT write_flash \
  --flash_mode keep --flash_freq keep --flash_size keep \
  0x10000 artifacts/wifi-before-change/firmware.bin
```

Do not run erase-flash. If bootloader/table restoration becomes necessary, the
archived offsets are bootloader **0x0000**, partitions **0x8000**, helper **0xe000**,
application **0x10000**. Review that separate recovery operation first; the archive
does not contain an NVS backup. The old application ignores the new namespace and
uses its original compiled network settings. Board BOOT/RESET download recovery
may be needed if normal USB enumeration fails.

## Manual acceptance procedure — not performed

Use a spare/test unit where possible. Keep the archived build and verified port
available before separately authorizing an upload. Do not erase the working unit.

1. **Initial setup:** on a unit with no `transit-wifi` records and blank compiled
   Wi-Fi fields, boot. Verify matrix instructions, protected AP join, HTTP page
   without mobile data/internet, network scan and manual hidden-SSID entry. Test
   iPhone Safari and Android Chrome. Separately verify compiled-credential migration
   on an unconfigured test unit.
2. **Successful connection:** submit valid 2.4 GHz network credentials. Verify
   progress/success, saved result, AP/server shutdown and live arrivals. Check long
   names, statuses, paging and serial brightness against the existing renderer.
3. **Reboot/reconnect:** power cycle. Verify no setup AP, automatic association and
   live feed. A non-erasing firmware update should preserve the same saved network.
4. **Incorrect password:** with working saved credentials, enter setup, try a wrong
   password, wait for failure, then retry correctly. Reboot after failure and confirm
   the old network still works. On the spare, cut power during candidate storage
   and activation; verify reboot selects a complete verified record.
5. **Network unavailable:** turn off the configured AP. Verify continuing retries,
   existing offline/stale rendering and no automatic provisioning/reset. Restore
   the AP and verify recovery without entering credentials again.
6. **Return to setup:** after startup release BOOT, hold three seconds, and verify
   setup. Repeat through `wifi` + Enter. Leave setup open longer than ten minutes and
   confirm SSID/password remain visible, the AP/page remain available and a failed
   connection can still be retried. Reboot before a successful replacement and
   confirm old-network recovery. Check GPIO0 mapping and access with the actual mounted panel. Verify
   holding the button during reset is not mistaken for this application feature.

Record timestamps, phone/router models, memory headroom, AP channel handoff,
password/code legibility and any disconnects without recording credentials.
Software tests/builds cannot establish physical radio or persistence behaviour.

## Software verification

58/58 native cases passed (54 existing plus four credential-validation cases).
All eight existing hardware environments built successfully. The final live build used 932,649 application bytes and 46,492 static
RAM bytes (linker reports, not runtime peak heap/stack measurements). Formatting,
fixture consistency and diff whitespace passed. Native tests cover credential
boundaries, raw PSKs, embedded nulls and stored version/termination; they do not
emulate NVS commits, WebServer or radio association. Manual tests above remain
pending. Archive hashes and unchanged partition/settings files were checked.

## Focused prototype onboarding refinement — 25 October 2026

Changed only setup-network defaults, fixed matrix instructions and setup lifetime.
The five C2 rows are at y=0/6/12/20/26; all fit 64×32 without clipping. The
13-character SSID fills 63 pixels and the password row includes a PW label.
Credential storage/verification, reconnection, HTTP form protection and BOOT/serial
recovery are unchanged. Setup closes after the existing success delay and normal
arrivals resume. Fixed-screen fit/coverage regression added; 59/59 native tests
passed and hub75-live built (932,353 application bytes, 46,484 static RAM bytes).
Physical readability, association and long-idle checks remain manual. No flashing.

## Submission failure investigation — 25 October 2026

**Verified code defect:** the old POST handler invoked WiFi.disconnect/begin
before sending its 303 response. Joining home Wi-Fi can change the shared AP/STA
channel, interrupting the phone before that response arrives. The pinned IDF
documents station priority for the shared channel:
https://docs.espressif.com/projects/esp-idf/en/v4.4.7/esp32s3/api-guides/wifi.html#home-channel
(checked 2026-10-25). This is a plausible cause of the reported submit failure,
not a physically reproduced root cause. A delay reduces the response/radio race;
it cannot guarantee every phone survives a channel switch.

**Other failure points:** strict Host comparison previously rejected the valid
explicit :80 form. Host and Origin now allow only the setup address with absent
or explicit default HTTP port; cross-origin protection and the random form token
remain. Missing/stale token, wrong interface and unexpected Host/Origin now log
the rejection category without raw headers, URLs or submitted secrets. The
bundled WebServer must successfully parse the request before our handler logs
it; absence of the request log means browser/network/parser failure remains
possible. No SDK HTTP/TLS debug logging was enabled.

A successfully parsed POST now logs receipt and a sanitized SSID, validates the
form and queues the candidate. It returns an HTTP 200 progress page, then starts
the connection after the response handler returns. The timer starts at the actual
WiFi.begin call. Disconnect reason codes arrive through an atomic handoff from
the Wi-Fi event task; intentional reason 8 is ignored. Begin's immediate status
is logged, but is not treated as a definitive asynchronous connection result.
The 20-second budget remains unchanged. Reason codes are hints: authentication
or handshake failure does not prove that a password is wrong.

After association to the candidate SSID and DHCP, NVS write/readback/activation
are unchanged, with specific failure-stage diagnostics. Only a completed save
starts the five-second shutdown delay. Failure leaves the AP, retry form, matrix
credentials and previous saved credentials intact. Error text distinguishes
network-not-found, authentication/handshake, router/signal and timeout/DHCP.
Rejoin the setup AP and open its address again if the phone loses the page.

### Reproduce and verify after a separately authorized manual upload

1. Preserve the current working build before any upload; use the restoration
   procedure above and confirm the selected device/port. No upload was performed
   in this investigation. Open USB serial at **115200**.
2. Boot normally, release BOOT, then hold it for three seconds, or send `wifi`
   followed by Enter. Expect `Wi-Fi setup: AP and configuration page ready`.
3. Join **metro-display** with **bcnmetro**, retain the connection without internet,
   and open **http://192.168.4.1/** in a normal browser. Choose a known 2.4 GHz home
   network and deliberately enter a wrong password of at least eight characters.
4. Submit once. Expect, in order, `HTTP configuration request received`,
   `SSID received`, `progress response sent; connection queued`, then
   `connection attempt started`. A 403 rejection instead prints its category;
   invalid input prints that no connection was started. No request log at all
   isolates the failure before the parsed POST handler. Do not enable raw SDK logs.
5. Allow roughly **21 seconds plus page refresh time**. Expect a disconnect reason
   where supplied, then `connection failed` with status/reason and a useful page
   error. The form must remain retryable. If disconnected, rejoin metro-display
   and reopen the address. Try **http://192.168.4.1:80/** as well.
6. Submit the correct password. Expect `connection successful; DHCP address
   acquired`, `credentials saved successfully`, then `setup mode exited; returning
   to arrivals`. If NVS fails, the specific write/readback/activation stage is
   printed and setup remains open. Confirm live arrivals resume.
7. Reboot and confirm saved-network reconnection. Repeat on a router channel other
   than the initial setup channel 1 to exercise channel handoff. On a test network,
   disable DHCP or move the selected AP out of range to exercise timeout/network
   errors. Capture only these safe diagnostics and phone/router model details.

**Software results:** 62/62 native cases passed; hub75-live built with 936,301
application bytes and 46,516 static RAM bytes. New regressions cover default-port
Host/Origin handling, rejected foreign origins, bounded/control-escaped SSIDs and
actionable disconnect-reason classification. Formatting, fixtures and whitespace
checks passed. Tests do not simulate WebServer sockets, radio/channel changes or
NVS failure injection. Exact hardware root cause and end-to-end fix remain to be
verified by the steps above. No flash, erase, deployment or device connection.

## Dedicated success page and BOOT investigation — 25 October 2026

The success page is a separate, minimal mobile document with the exact text:
“Connected!”, “Your display is ready.”, “You can close this page.” There is no form,
selector, input, button, troubleshooting text or refresh redirect. It uses the
existing system-font/spacing style with a generous content inset. The success
branch is reached only after candidate association, DHCP and the existing
inactive-record/readback/activation NVS sequence have all succeeded. Failures
retain the existing form and retry/error flow.

For browser submissions, saving no longer starts a timer that can expire before
the next browser request. The AP shutdown timer is armed after serving the first
success page, with five seconds for receipt/rendering. Arrivals and saved-network
retries resume immediately after saving. If the browser never returns after a
channel switch, the AP remains available for the success page; reboot uses the
saved network normally. Repeated success requests do not keep extending the
shutdown window. This preserves the browser opportunity, but actual socket
delivery/phone rendering still requires a physical check.

Hardware mapping was rechecked against Waveshare's official example on
2026-10-25:
https://raw.githubusercontent.com/waveshareteam/ESP32-S3-RGB-Matrix/main/example/arduino_v3.3.7/09_Music_Player/09_Music_Player.ino
It defines BOARD_BUTTON_PIN 0, INPUT_PULLUP and a pressed state when the read
level is false. GPIO0 is unchanged and no configured HUB75 pin overlaps it.
The existing main loop samples nominally every 50 ms plus drawing/serial work;
HTTP/feed requests run in a separate task. Inspection found no network fetch
running directly in the button polling loop.

Verified software limitations were an initial-release gate (a button held before
the first poll was ignored), lack of debounce, and an unbounded serial input-drain
loop that could starve later polling under continuous input. Replaced the gate
with a 100 ms debounced, one-shot three-second hold; initial held state is handled,
short release glitches do not reset a hold, and release rearms it. Serial draining
is now bounded to 64 bytes/frame. These limitations do not establish the reported
physical fault's cause. The existing wifi + Enter recovery command is unchanged.

Serial now reports initial GPIO0 HIGH/LOW, debounced BOOT pressed/released,
long-press activation and polling gaps above 250 ms. After an authorized manual
upload, test during normal arrivals with the monitor at 115200:

1. Expect GPIO0 initial HIGH with no button held. Press BOOT and expect
   `BOOT: pressed`; hold about 3.1 seconds and expect
   `BOOT: long press detected; requesting Wi-Fi setup`, then the AP-ready log.
2. If no pressed/released logs occur, verify the actual accessible BOOT control,
   PCB revision and GPIO0 electrical levels. Do not substitute RESET or remap a
   GPIO based solely on the symptom. A button held during reset can enter ROM
   download mode before application polling starts. Enclosure access is unverified.
3. If pressed/released logs occur but activation fails, capture their timing and
   any `BOOT: polling delayed` lines. An in-flight feed request may delay the
   network task acting on an already logged request, but should not block polling.
4. Submit a wrong password and confirm the retry form remains. Then submit the
   correct password and verify that only the three success lines remain. Arrivals
   should resume, followed by the success-page-sent log and AP closure no sooner
   than five seconds afterward. Test rejoining the AP after a channel switch.
5. Confirm serial wifi recovery and reboot using saved credentials still work.

Final checks: 65/65 native cases passed, including debounce/initial hold/one-shot
rearming and success-page exclusions; hub75-live built with 937,361 application
bytes and 46,540 static RAM bytes. Formatting, fixture consistency and whitespace
checks passed. No hardware interaction, flashing, erasure or deployment.

## Browser configuration, connecting and result states — 25 October 2026

User reports provisioning now works on the physical device. This refinement
changes only browser presentation, polling and success-render acknowledgement;
station association, NVS write/readback/activation, BOOT detection, serial recovery
and transport rendering are preserved. Earlier sections record historical flows;
the state flow below supersedes their meta-refresh/status-form descriptions.

- Configuration retains scanned-network selection, manual SSID and password entry.
- Submit immediately removes the form and displays “Connecting…” and “Setting up
  your display.” with a small CSS spinner, respecting reduced-motion preferences.
  No diagnostics, timers, countdowns or buttons appear in progress.
- Success displays only “Connected!”, “Your display is ready.” and “You can close
  this page.” Failure displays “Couldn’t connect”, “Check your Wi-Fi password and
  try again.” and a Try again button. Retry restores the configuration form and
  selected SSID, but always clears the password.

The same nonce-protected script runs on the local device page without internet,
frameworks or external assets. It submits credentials in a POST body, immediately
removes the form, then polls GET /status about once per second. The endpoint
returns only one state: configuration, connecting, failed or success; no SSID,
password, reason code or raw diagnostic. AP-interface, Host, Origin and random
setup-token checks apply. Poll/ack tokens use X-Setup-Token headers, never URLs.
CSP allows only same-origin connections and the per-session nonce script.

Transient failed requests or malformed/interrupted responses retain the progress
screen and retry quietly; requests have an internal four-second abort budget
with no user-facing timeout. If the submission response is lost, the browser
polls rather than automatically sending credentials again. A status indicating
configuration means the submission was not accepted and shows the retry state.
A stale session reloads the root page for a new token.

The device retains the chosen SSID separately from the cleared candidate. The
browser also remembers only that SSID in sessionStorage when available, covering
a submission that never reached the device. Selected values are escaped in
server HTML; browser restoration assigns input/select values directly. Changing
the selected network clears a previous manual name so it cannot override the
new choice. Passwords are never echoed into HTML, returned from status, persisted
in browser storage or logged. Only the submission body contains the password.

Success still requires completed association, DHCP and committed NVS activation.
After rendering success through two animation frames, the browser POSTs /complete
with the session token. Firmware checks success again, then arms the existing
five-second AP shutdown grace once. Interrupted acknowledgements retry quietly;
repeated acknowledgements do not extend shutdown. GET /status and serving HTML
alone cannot trigger closure. Arrivals resume after the verified save as before.
If the phone loses its AP connection, automatic polling resumes when connectivity
returns; a phone that switches away may still need a manual rejoin. This does not
make radio channel changes invisible on every phone.

### Verification

66/66 native tests passed. hub75-live built with 942,493 application bytes and
46,556 static RAM bytes. Formatting, fixture consistency and whitespace passed.
The new progress/failure markup checks reject forms/inputs/diagnostics in those
states. The previous success-state exclusion check remains.

Run `node tools/test_wifi_web.mjs` for the retained, dependency-free browser-script
regression harness (Node built-ins only; no firmware/frontend dependency). It
evaluates the actual embedded script against a minimal mocked browser, checking
immediate form removal, interrupted poll recovery, failure/retry, SSID restoration,
success paint/ack ordering, interrupted acknowledgement recovery, lost-submission
handling and password isolation. It does not simulate phone radio behaviour or
prove visual rendering on Safari/Chrome. Existing C++ checks remain runnable
through the normal PlatformIO native test command.

After a separately authorized upload, join metro-display, open its local page and
submit a wrong password. Confirm only progress is visible, followed by the exact
failure screen. Select Try again and verify the chosen SSID remains while the
password is empty. Submit correctly and confirm the exact success state, then
AP closure after the success-rendered serial log plus at least five seconds.
Temporarily disconnect/rejoin the phone during progress and verify quiet polling
recovery. Reboot and verify the saved network; confirm BOOT and wifi serial
recovery still activate setup. None of these physical checks was run for this
refinement. No flash, erase, deployment or device connection was performed.
