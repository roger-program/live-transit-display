# Phase 1 brightness preview — local firmware work

2026-11-15, Europe/Madrid. User authorized local implementation/testing only. No hardware access, serial session, application flash, deployment or production configuration change. No schedule, timezone or persistent brightness settings.

## Source changes

- `lib/transit/src/brightness_preview.h/.cpp`: bounded JSON parser and pure RAM lease/generation state. Raw ceiling 16, lease <=15 s; repeated responses never renew, elapsed requests cannot extend expiry, canceled/expired/interrupted commands cannot replay. Fresh instance starts at normal baseline.
- `src/preview_client.h/.cpp`: independent bounded network task (12 KiB stack), random boot ID, 2 s idle / 1 s active polls after request completion, 2 s HTTP timeout, failure backoff/jitter/Retry-After. Network failure, setup/invalid clock or Wi-Fi loss disables preview. Mutex protects shared state; only render loop chooses/applies brightness. ACK after presentation includes exact generation/raw, monotonic/UTC timings and observed request/D1 counters. No ongoing idle D1 presence writes.
- `src/feed_client.h/.cpp`: reuse read-only identity/verified origin/CA/bearer behavior for fixed control paths. Control responses capped 1024 bytes, redirects prohibited, no SDK exceptions/credentials logged. Existing arrival fetch behavior remains unchanged.
- `src/main.cpp`: start independent preview task after existing setup; choose temporary brightness over current serial/compiled baseline and publish acknowledgement after `display.present`. No renderer/provenance/operator changes. Raw zero leaves normal CPU/network/frame loop running. Provisioning takes precedence and restores baseline.
- `test/test_core/test_main.cpp`: native parser/lease/zero/delay/supersession/disconnect/reboot regression cases.

Identity, Wi-Fi provisioning implementation, NVS records, partition layout, bootloader and transport parser/rendering are unchanged. Existing identity reader is reused, not rewritten; no new NVS write path. Serial baseline defaults to 4/255 and existing b4/b8/b16/b32 behavior survives.

## Local results

80 native tests passed; hub75-live built successfully (47,164 bytes static RAM, 964,301 program bytes). Formatting and whitespace checks pass. Static build data does not measure runtime task/TLS memory or panel output. Physical darkness, applied-light latency, reboot/expiry observations and transport continuity with parallel TLS remain pending.

## Integration, physical experiment and rollback

Central [experiment report](../../live-transit-feed/docs/brightness-preview-experiment-2026-11-15.md) contains API/security/metric semantics, local D1 measurements, ≥30 initial/100 subsequent physical sampling procedure, failure cases and separate deployment/flash authorization boundary. Browser/Worker/SNTP clock differences make cross-clock timing approximate; ACK proves driver application only, not photons. Normal baseline restoration is evaluated locally every frame even if network task blocks.

A later authorized rollout must retain current backup/identity/partition safeguards. Rollback uses previous application-only firmware and previous Worker version, after Stop/15-second expiry; no NVS erase, provisioning or partition change. Additive D1 experiment table can remain unused. Current source is **not flashed**; no hardware acceptance claimed.


## 2026-11-16 — Authorized application-only activation on A

User authorized focused production activation and application flashing, preserving all protected state and leaving slider available. Exact reviewed 964,672-byte candidate **f3a28399b088ba90346a213b993c9faae6fb6433dc1b8439e66bbd4cb828aa9d** written only at 0x10000 (00:06 Madrid). Fresh installed-image read matched both original4MiB backups; full post-write readback verifies candidate, FF padding and untouched tail. Protected prefix/identity byte-identical before00:07:11 restart. No NVS/provisioning/partition/bootloader change. Two four-stop Feed OK summaries and real baseline raw 4 render acknowledgement observed; monitor closed, A running. No experimental brightness command was sent; visible output, recovery and latency/calibration acceptance remain user-pending.

Rollback: restore only full 4 MiB application backup at 0x10000, SHA **98090fb3e7defabc78873c8109c91be01f26deb1d5be28560295826f8ad406cc**, verify protected regions before restart. Current backend7eb05a49… can revert to8fa8b1e6… while retaining additive0006. [Central execution, private backup location and rollback](../../live-transit-feed/docs/brightness-activation-2026-11-16.md). Earlier local-only sections retain their historical boundary; no source changes or rebuild in activation.


## 2026-11-21 — Second iteration, local only

User reports physical slider operation, acceptable ~1–2 s latency, visual off at 0%, insufficient raw 16 maximum, timed normal-output recovery and occasional misleading expired status. These are user observations, not a fresh hardware measurement. User subsequently explicitly requested a local 0–64 range and progressive 16/32/48/64 power validation; supply is the Mac USB-C port, with available/negotiated current still unknown.

Local source now caps preview at 64 and 30 s; the existing two-minute server session cap, polling, normal baseline, NVS/identity/partitions and transport renderer/feed are retained. ACK explicitly reports active versus normal output, including when both bytes equal4. Active-only 5 s acknowledgement refresh supports an 8 s freshness bound in the UI; normal/idle output adds no recurring ACK writes. A cached active ACK is not renewed after RAM expiry/disconnection while the renderer catches up. Existing render-side application timing is retained; refresh is a status observation, not a repeated application.

81 native tests and hub75-live build passed. The host tests exercise raw 16/32/48/64 and 30 s expiry, Stop/disconnect/reboot recovery. These are software checks: no voltage/current/drop/reset/flicker/thermal observations at higher brightness exist. No serial access, power change, deployment or flash. Compiled capacity is not electrical qualification.

Official panel documentation rechecked21 November 2026: [P2.5 specifications](https://docs.waveshare.com/RGB-Matrix-Px-64x32) give5 V/2.5 A, ≤12 W. [Controller guide](https://docs.waveshare.com/ESP32-S3-RGB-Matrix/Instructions-For-Use) demonstrates a separate USB-C charger and discusses insufficient supply; it does not validate this Mac/cable/board path. README's historical blanket16 setting was corrected to distinguish normal raw 4 from the local experimental64 ceiling.

For a later separately authorized physical test, establish the actual5 V power budget and measure at the panel input before raising output. Hold raw 16,32,48,64 individually (slider25/50/75/100%) for short observed intervals, recording loaded voltage/current, Wi-Fi/CPU peaks, resets, ghosting/flicker and connector/panel temperature; test representative transport patterns and separately authorized worst-case white load. Keep the regulated 5 V rail within actual documented component limits; never infer current from raw/255 or port capability. Stop and reduce output on significant voltage sag, supply/cable current limit, heating, reset or instability; use a verified correctly rated supply/path if budget is insufficient. No automatic diagnostic ramp. Confirm Stop and 30 s inactivity recovery at each stage; two-minute cap still applies.

Backend migration 0007 must precede the matching Worker and this firmware on a future authorized rollout; close preview while versions differ. Old ACKs remain unknown, never Active; old firmware still has its15 s/raw 16 limits until reflashed. Roll back firmware/Worker together after Stop/expiry; leave the compatible expanded temporary table rather than rewriting migration history. [Central experiment](../../live-transit-feed/docs/brightness-preview-experiment-2026-11-15.md#2026-11-21--second-iteration-local-only).


## 2026-11-21 — Authorized v2 application-only activation

Exact reviewed 964,912-byte hub75-live **6ab3ae1e1ad02de8382fd086528d5a4de58c018de198f342bc35629b7e8eba45** written only at 0x10000,15:01 Madrid. Fresh current v1 read matched two 4 MiB backups; full post-write readback validates candidate, FF padding and untouched tail. NVS/Wi-Fi/bootloader/helper/table prefix and device-auth byte-identical before 15:02:17 restart. Wi-Fi and two four-stop Feed OK summaries, panel enabled and real ACK raw 4/active=false observed; no captured brownout/panic/unexpected restart. Monitor closed, normal 4 left running. No preview commands or high-brightness stress.

V2 range 0–64/30 s timeout and 120 s cap now deployed. Higher-level power/voltage/reset/stability checks and visual status/expiry remain user-manual; normal render pipeline/feed and explicit normal-output ACK only verified here. Chrome computer access was not approved. Existing TRAM authentication-required outage was present before deployment and is unchanged; no transport/identity/configuration changes.

Rollback: private `../live-transit-feed/data/brightness-v2-rollout-2026-11-21/device/application-before-full.bin` and independent verify copy, 4,194,304 bytes/SHA **55e70bac5ee8ba03fd3726613a558ac2c540afaa1be926db3e6d3753125ba1c2**. Restore only factory at 0x10000 and preserve protected regions; reactivate prior Worker 7eb05a49… while retaining compatible migration 0007. [Central execution/evidence](../../live-transit-feed/docs/brightness-preview-experiment-2026-11-15.md#2026-11-21--controlled-v2-production-activation). No rollback performed; source/build unchanged from local 81-test preparation.
