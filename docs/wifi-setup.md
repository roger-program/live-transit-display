# Local Wi-Fi setup — 25 October 2026

## Implemented behaviour

The existing live firmware now uses a transport-independent `WifiSetup` module.
No new firmware environment, partition, external library or deployment was added.
The renderer, Feed v1 parser, upstream/backend settings, colour policy and isolated
diagnostics are unchanged. Setup overlays instructions using existing drawing
primitives. Normal feed fetching resumes when setup closes.

Saved Wi-Fi credentials take priority. Without a valid saved record, the device
starts a protected lowercase `metro-display` AP with prototype password `bcnmetro`.
On an untouched namespace, if compiled development Wi-Fi credentials are present, it also tests them once and saves them only after a
successful association and DHCP address. This transitional migration keeps existing
development builds usable. Empty compiled Wi-Fi fields are supported; feed origin,
feed authentication and CA configuration still use the existing local header.

The 64×32 matrix shows a fixed screen containing the SSID, password, LOWERCASE
instruction, OPEN HTTP and browser address. C2 glyphs are uppercase, so LOWERCASE
explicitly applies to both credentials. There is no scrolling or rotating screen.
Join that AP in phone Settings, accept remaining connected
without internet if prompted, then explicitly open **http://192.168.4.1**.
There is no captive-portal DNS/redirect service. The self-contained page has a
network dropdown, manual SSID, password field and status/retry flow. A background
scan populates the initial list; refresh the page after scanning. A failed
connection triggers another scan. Hidden SSIDs can be entered manually.

Connection tests allow 20 seconds. Success means association to the requested
SSID and a nonzero station IP, not internet, DNS or feed availability. The status
page refreshes while testing. On success, credentials are saved and the AP/server
close after five seconds; reconnect the phone to its usual network. AP channel
changes during home-network association can interrupt the phone connection; the
matrix and retry/rejoin path are required physical checks.

Setup and its on-screen credentials remain available until configuration succeeds;
there is no inactivity timeout. Rebooting returns to previously saved credentials
if replacement has not succeeded. A normal network outage
never opens setup or deletes credentials: retries continue every 15 seconds.
Reboot and normal non-erasing firmware writes preserve NVS. Setup requests can be
delayed by an in-flight bounded feed request; drawing remains in the main loop.

## Storage and security boundary

The existing default NVS partition is unchanged. Namespace `transit-wifi` stores
version-1 credential blobs in `slot0`/`slot1`, plus the `active` slot key. A candidate
is kept in RAM until its connection succeeds, written to the inactive slot, read
back and compared, then activated with a separate committed slot selector. Failed
association never writes NVS; incomplete candidate writes leave the previous
selected slot in place. The old slot remains after activation. No Wi-Fi erase or
factory-reset endpoint is added. SSIDs/passwords are not printed or echoed into
HTML. Submitted passwords travel only over the protected local setup network.

The setup AP uses the shared prototype password `bcnmetro` and accepts one client.
`SetupSsid` and `SetupPassword` in `lib/transit/src/wifi_setup_screen.h` are the
configurable defaults; use device-specific credentials before production. Choose
values that fit the fixed C2 rows (64 pixels); the password must be 8–63 bytes.
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

After normal startup, release BOOT once, then hold it for three seconds to enter
setup without removing the saved network. Do not hold BOOT while resetting for
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
